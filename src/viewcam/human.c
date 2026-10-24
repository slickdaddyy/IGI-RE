/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "viewcam/viewcam.h"
#include "material/materialtypes.h"
#include "humanai/humanaitypes.h"
#include "statgun/statguntypes.h"
#include "config/configtypes.h"

FN_EXTERN(crt_CIpow);

/* something a human stands on and moves with */
typedef struct Platform {
    Task task;
    double pos[3];
    char unk38[0x38];
    Matrix rot;
} Platform;

/* sent to every task in the game's first task list */
typedef struct HumanPosMsg {
    Human *h;
    int unk4;
    double pos[3];
    float radius;
} HumanPosMsg;

/* asks the hit task to put a decal on itself */
typedef struct DecalMsg {
    void *hit;
    int unk4;
    void *texture;
    float size;
} DecalMsg;

/* passed by the particle system when a particle hits something */
typedef struct ParticleHit {
    CollHit *hit;
    int unk4;
    double pos[3];
    double prev[3];
} ParticleHit;

ORIG(0x401AE0, Task **, TaskRef_Add, (Task *t));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x406220, PlayerProfile *, Profile_GetActive, (void));
ORIG(0x413C80, WeaponType *, Weapon_GetType, (Task *w));
ORIG(0x4167D0, uint8_t, Game_GetEvent0, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x4168B0, QTaskList *, Game_GetTaskList1, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416A70, Task *, Game_GetPlayer, (int i));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416FB0, int, Game_IsRaining, (void));
ORIG(0x450CE0, void, HumanAI_Broadcast, (AIMessage *noise));
ORIG(0x460BF0, uint8_t, Human_GetEvent0, (void));
ORIG(0x460C60, QTaskList *, Human_GetList, (void));
ORIG(0x462390, void, Human_ProbeGround, (Human *h, double *pos, double *to, float *vel, unsigned *bits, float dist));
ORIG(0x47E6F0, uint8_t, BulletHole_GetEvent, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4B47C0, unsigned, Random_Int, (void *rng, unsigned n));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, double *pos, float radius));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *hits, double *from, double *to, int count, uint8_t event,
                                  void *filter, unsigned flags));
ORIG(0x4D38C0, void, BoneObj_Update, (Matrix *m, void *anim));
ORIG(0x4D64C0, int, Anim_GetId, (void *anim, int track));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, double *pos));
ORIG(0x4E6C60, void, Sound_SetVolume, (Task *sound, float a, float level));
ORIG(0x4E6CA0, void, Sound_SetPitch, (Task *sound, float pitch));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4FDF00, Task *, Particle2D_New, (void *root, void *world, void *sprite, float *dir, float *color, float *size,
                                    float b, int flags, int frames, float e, int f, int count, void *callback));
ORIG(0x4FE120, void, Particle2D_Add, (Task *ps, double *pos, float *dir, float size, float a, float b, float c, int d,
                                  int e));
ORIG(0x4FE210, void, Particle2D_Emit, (Task *ps, double *pos, float *dir, float spread, float a, float b, float c,
                                  float d, int e, float f, int g, int h));

extern const int gFootstepSounds[];
extern char *gBloodSprite1;
extern char *gBloodSprite2;
extern char *gBloodHole1;
extern char *gBloodHole2;
extern char *gBloodHole3;
extern char *gBloodHole4;

#define ANIM_WALK 0x3F2

static const double kOtherHeight = 0x1.ccccccccccccdp+12; /* 7372.8 */
static const double kOtherRadius = 0x1.199999999999ap+11; /* 2252.8 */

/* fistp with the current rounding mode, low 32 bits */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* the C runtime's x to the power y */
static __attribute__((noinline)) long double crt_pow(long double x, long double y)
{
    long double r;

    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "r"(FN(0x4A5760, crt_CIpow)) : "st(1)", "eax", "ecx", "edx", "memory");
    return r;
}

/* keeps `to` away from other humans standing near it; out = to - from */
void Human_AvoidHumans(Human *h, double *from, double *to, float *out)
{
    QTaskList *list = Human_GetList();
    int i;

    for (i = 0; i < list->count; i++) {
        Human *o = (Human *)list->items[i];
        double d[3], len;
        long double dz, l;

        if (!o->vulnerable || o->dead)
            continue;
        d[0] = to[0] - o->pos[0];
        d[1] = to[1] - o->pos[1];
        dz = (long double)to[2] - o->pos[2];
        if (dz < 0.0)
            dz = -dz;
        if (dz >= kOtherHeight)
            continue;
        d[2] = 0.0;
        l = Vec3d_Length(d);
        len = l;
        if (l >= kOtherRadius)
            continue;
        d[0] = from[0] - o->pos[0];
        d[2] = 0.0;
        d[1] = from[1] - o->pos[1];
        if (Vec3d_Length(d) > len) {
            to[0] = from[0];
            to[1] = from[1];
        }
    }
    out[0] = to[0] - from[0];
    out[1] = to[1] - from[1];
    out[2] = to[2] - from[2];
}
HOOK(0x462BA0, Human_AvoidHumans);

/* moves the human by its animation's root motion, in platform space */
void Human_MoveOnPlatform(Human *h)
{
    Platform *p = (Platform *)h->attach;
    const float *r = p->rot.m;
    const float *m = h->delta;
    long double a0, l0, l1, l2;
    float a1, a2;
    double w[3], pos[3];

    a0 = (r[0] * m[0] + m[1] * r[3]) + m[2] * r[6];
    a1 = (m[1] * r[4] + r[1] * m[0]) + r[7] * m[2];
    a2 = (m[1] * r[5] + r[2] * m[0]) + m[2] * r[8];
    l0 = a0 + h->attachPos[0];
    h->attachPos[0] = l0;
    l1 = a1 + h->attachPos[1];
    h->attachPos[1] = l1;
    l2 = a2 + h->attachPos[2];
    h->attachPos[2] = l2;
    w[0] = (r[1] * l1 + r[0] * l0) + r[2] * l2;
    w[1] = (r[5] * l2 + r[3] * l0) + r[4] * l1;
    w[2] = (r[8] * l2 + r[6] * l0) + r[7] * l1;
    pos[0] = w[0] + p->pos[0];
    pos[1] = w[1] + p->pos[1];
    pos[2] = w[2] + p->pos[2];
    DynCube_Move(Game_GetWorld(), h, pos, h->radius);
}
HOOK(0x462CF0, Human_MoveOnPlatform);

void Human_UpdateWeapon(Human *h)
{
    static const double kCookMin = 3.0, kCookMax = 60.0, kCookScale = 50.0;
    static const double kCookPow = 0x1.6666666666666p-1; /* 0.7 */
    static const float kCookLinear = 0x1.99999ap-4f;    /* 0.1 */
    static const float kRecoilFade = 0x1.ccccccp-1f;    /* 0.9 */
    HumanView *v = h->view;
    WeaponType *info;
    WeaponUseMsg msg;
    Task *w;
    double eye[3];
    float vel[3];
    Matrix cam;

    h->fired = 0;
    w = WeaponSlots_Get(h->weapons, 0);
    if (!w)
        return;
    info = Weapon_GetType(w);
    HumanView_GetEye(eye, h->view);
    HumanView_GetMatrix(&cam, h->view);
    eye[0] = 0.0f + eye[0];
    vel[0] = h->outVel[0];
    vel[1] = h->outVel[1];
    vel[2] = h->outVel[2];
    eye[1] = 0.0f + eye[1];
    eye[2] = 0.0f + eye[2];
    if (info->category == WEAPON_KIND_GRENADE) {
        long double t = h->cookTime, k;
        float tf;

        if (kCookMin > t)
            t = kCookMin;
        else if (kCookMax < t)
            t = kCookMax;
        tf = t;
        k = (crt_pow(t * kCookScale, kCookPow) + tf * kCookLinear) * info->throwSpeed;
        vel[0] = k * cam.m[1];
        vel[1] = k * cam.m[4];
        vel[2] = k * cam.m[7];
    }
    msg.fire = h->fire;
    msg.eye = eye;
    msg.view = &cam;
    msg.active = h->weaponReady;
    msg.alt = h->altFire;
    msg.vel = vel;
    msg.zoomed = v->zoomed;
    msg.shot = 0;
    msg.unk10 = 0;
    msg.recoilPitch = 0.0f;
    msg.recoilYaw = 0.0f;
    msg.h = h;
    msg.aimError = h->aimError;
    gTaskHandlers[Human_GetEvent0()][w->type](w, &msg);
    h->fire = 0;
    h->altFire = 0;
    h->fired = msg.shot;
    v->recoilPitch = (msg.recoilPitch + v->recoilPitch) * kRecoilFade;
    v->recoilYaw = (msg.recoilYaw + v->recoilYaw) * kRecoilFade;
}
HOOK(0x462E50, Human_UpdateWeapon);

void Human_SendPosition(Human *h)
{
    HumanPosMsg msg;
    int i;

    msg.h = h;
    memcpy(msg.pos, h->pos, sizeof(msg.pos));
    msg.radius = h->radius;
    for (i = 0; i < Game_GetTaskList1()->count; i++) {
        uint16_t type = Game_GetTaskList1()->items[i]->type;
        TaskHandler fn = gTaskHandlers[Game_GetEvent0()][type];

        fn(Game_GetTaskList1()->items[i], &msg);
    }
}
HOOK(0x463080, Human_SendPosition);

int Human_GetState(Human *h)
{
    return h->sm.state;
}
HOOK(0x463110, Human_GetState);

int Human_GetIndex(Human *h)
{
    return h->index;
}
HOOK(0x463120, Human_GetIndex);

int Human_GetTeam(Human *h)
{
    return h->team;
}
HOOK(0x463130, Human_GetTeam);

float *Human_GetMatrix(Human *h)
{
    return h->matrix.m;
}
HOOK(0x463140, Human_GetMatrix);

char Human_IsPlayer(Human *h)
{
    return (Task *)h == Game_GetPlayer(0);
}
HOOK(0x463150, Human_IsPlayer);

char Human_IsDead(Human *h)
{
    return h->dead;
}
HOOK(0x463170, Human_IsDead);

char Human_IsCrouching(Human *h)
{
    return h->crouch;
}
HOOK(0x463180, Human_IsCrouching);

char Human_IsActive(Human *h)
{
    return h->vulnerable;
}
HOOK(0x463190, Human_IsActive);

void Human_SetPosition(Human *h, double *pos)
{
    DynCube_Move(Game_GetWorld(), h, pos, h->radius);
    HumanView_UpdatePosition(h->view);
    memcpy(h->groundPos, pos, 2 * sizeof(double));
    h->slide[0] = 0.0f;
    h->slide[1] = 0.0f;
    h->slide[2] = 0.0f;
    h->push[0] = 0.0f;
    h->push[1] = 0.0f;
    h->push[2] = 0.0f;
}
HOOK(0x4631A0, Human_SetPosition);

void Human_SetYaw(Human *h, float yaw)
{
    h->baseZ = yaw;
    h->rotZ = yaw;
    Matrix_SetRotZ(&h->matrix, yaw);
    BoneObj_Update(&h->matrix, h->anim);
}
HOOK(0x463220, Human_SetYaw);

void Human_SnapToGround(Human *h)
{
    float zero[3];
    double pos[3];

    memcpy(pos, h->pos, sizeof(pos));
    zero[0] = 0.0f;
    zero[1] = 0.0f;
    zero[2] = 0.0f;
    Human_ProbeGround(h, pos, pos, zero, &h->sm.bits, 0x1.f4p+21f);
    Human_SetPosition(h, pos);
}
HOOK(0x463260, Human_SnapToGround);

void Human_LeaveGround(Human *h)
{
    h->sm.bits &= ~4u;
    Human_SetGround(h, NULL);
    h->ground.found = 0;
    h->movePos[0] = 0.0f;
    h->movePos[1] = 0.0f;
    h->movePos[2] = 0.0f;
}
HOOK(0x4632D0, Human_LeaveGround);

/* keeps the position and rotation relative to p */
void Human_SetPlatform(Human *h, Task *t)
{
    Platform *p = (Platform *)t;
    const float *r = p->rot.m;
    const float *m = h->matrix.m;
    long double dx, dy, dz;
    double l[3];
    Matrix rel;

    h->attach = t;
    dx = (long double)h->pos[0] - p->pos[0];
    dy = (long double)h->pos[1] - p->pos[1];
    dz = (long double)h->pos[2] - p->pos[2];
    l[0] = (r[3] * dy + r[6] * dz) + r[0] * dx;
    l[1] = (r[4] * dy + r[7] * dz) + r[1] * dx;
    l[2] = (r[5] * dy + r[8] * dz) + r[2] * dx;
    memcpy(h->attachPos, l, sizeof(l));
    rel.m[0] = (r[1] * m[1] + r[2] * m[2]) + r[0] * m[0];
    rel.m[1] = (r[4] * m[1] + r[5] * m[2]) + r[3] * m[0];
    rel.m[2] = (r[7] * m[1] + r[8] * m[2]) + r[6] * m[0];
    rel.m[3] = (r[2] * m[5] + r[0] * m[3]) + r[1] * m[4];
    rel.m[4] = (r[5] * m[5] + r[3] * m[3]) + r[4] * m[4];
    rel.m[5] = (r[8] * m[5] + r[6] * m[3]) + r[7] * m[4];
    rel.m[6] = (r[1] * m[7] + r[2] * m[8]) + r[0] * m[6];
    rel.m[7] = (r[4] * m[7] + r[5] * m[8]) + r[3] * m[6];
    rel.m[8] = (r[7] * m[7] + r[8] * m[8]) + r[6] * m[6];
    rel.count = h->matrix.count + 1 + p->rot.count;
    h->attachMatrix = rel;
    Human_HideShadow(h);
}
HOOK(0x463310, Human_SetPlatform);

void Human_ClearPlatform(Human *h)
{
    h->attach = NULL;
    Human_ShowShadow(h);
}
HOOK(0x463550, Human_ClearPlatform);

void Human_DisableCollision(Human *h)
{
    h->unk694 = 1;
}
HOOK(0x463570, Human_DisableCollision);

void Human_EnableCollision(Human *h)
{
    h->unk694 = 0;
}
HOOK(0x463580, Human_EnableCollision);

void Human_SetExactMotion(Human *h)
{
    h->deltaScaled = 1;
}
HOOK(0x463590, Human_SetExactMotion);

void Human_ClearExactMotion(Human *h)
{
    h->deltaScaled = 0;
}
HOOK(0x4635A0, Human_ClearExactMotion);

void Human_LockInput(Human *h)
{
    memset(h->prevInput, 0, sizeof(h->prevInput));
    memset(h->input, 0, sizeof(h->input));
    h->inputLocked = 1;
}
HOOK(0x4635B0, Human_LockInput);

void Human_UnlockInput(Human *h)
{
    h->inputLocked = 0;
}
HOOK(0x4635E0, Human_UnlockInput);

void Human_FreezeView(Human *h)
{
    h->frozenView = 1;
}
HOOK(0x4635F0, Human_FreezeView);

void Human_UnfreezeView(Human *h)
{
    h->frozenView = 0;
}
HOOK(0x463600, Human_UnfreezeView);

void Human_BeginUse(Human *h)
{
    h->unk7B8 = 1;
}
HOOK(0x463610, Human_BeginUse);

void Human_EndUse(Human *h)
{
    h->unk7B8 = 0;
    h->unk7B9 = 1;
}
HOOK(0x463620, Human_EndUse);

void Human_SetGround(Human *h, Task *t)
{
    if (h->groundRef) {
        TaskRef_Remove(h->groundRef);
        h->groundRef = NULL;
    }
    if (t)
        h->groundRef = TaskRef_Add(t);
}
HOOK(0x463640, Human_SetGround);

Task *Human_GetGround(Human *h)
{
    return h->groundRef ? *h->groundRef : NULL;
}
HOOK(0x463680, Human_GetGround);

/* the ladder, wire or zipline being used */
void Human_SetClimbObject(Human *h, Task *t)
{
    if (h->climbRef) {
        TaskRef_Remove(h->climbRef);
        h->climbRef = NULL;
    }
    if (t)
        h->climbRef = TaskRef_Add(t);
}
HOOK(0x4636A0, Human_SetClimbObject);

Task *Human_GetClimbObject(Human *h)
{
    return h->climbRef ? *h->climbRef : NULL;
}
HOOK(0x4636E0, Human_GetClimbObject);

/* the object the use key acts on */
void Human_SetAttached(Human *h, Task *t)
{
    if (h->attachedRef) {
        TaskRef_Remove(h->attachedRef);
        h->attachedRef = NULL;
    }
    if (t)
        h->attachedRef = TaskRef_Add(t);
}
HOOK(0x463700, Human_SetAttached);

Task *Human_GetAttached(Human *h)
{
    return h->attachedRef ? *h->attachedRef : NULL;
}
HOOK(0x463740, Human_GetAttached);

void HumanViewCam(Human *h, int mode)
{
    h->camMode = mode;
}
HOOK(0x463760, HumanViewCam);

void Human_ApplyGravity(Human *h)
{
    static const float kDamp = 0x1.fae148p-1f;   /* 0.99 */
    static const float kGravity = 0x1.52f77ep+6f; /* 84.74 */

    h->vel[0] = h->vel[0] * kDamp;
    h->vel[1] = h->vel[1] * kDamp;
    h->vel[2] = (h->vel[2] - kGravity) * kDamp;
}
HOOK(0x463770, Human_ApplyGravity);

/* plays name + 1..n through the view, at most once per 30 to 60 ticks */
void Human_SayTimed(Human *h, const char *name, int n)
{
    Task *old = NULL;
    char buf[32];
    int r, now;

    if (Flow_GetFrames() < h->voiceTime)
        return;
    if (h->voiceRef) {
        old = *h->voiceRef;
        if (old)
            Sound_Stop(old);
        TaskRef_Remove(h->voiceRef);
        h->voiceRef = NULL;
    }
    r = x87_fistp(Random_Float(Game_GetRandom()) * -30.0f);
    now = Flow_GetFrames();
    h->voiceTime = 30 - r + now;
    crt_sprintf(buf, "%s%d", name, Random_Int(Game_GetRandom(), n) + 1);
    Sound_Play(h->view, buf);
    if (old)
        h->voiceRef = TaskRef_Add(old);
}
HOOK(0x4637C0, Human_SayTimed);

/* plays name + 0..n-1 */
void Human_Say(Human *h, const char *name, int n)
{
    char buf[32];

    if (h->voiceRef) {
        if (*h->voiceRef)
            Sound_Stop(*h->voiceRef);
        TaskRef_Remove(h->voiceRef);
        h->voiceRef = NULL;
    }
    crt_sprintf(buf, "%s%d", name, Random_Int(Game_GetRandom(), n));
    Sound_Play(h, buf);
}
HOOK(0x4638A0, Human_Say);

void Human_Footstep(Human *h)
{
    static const float kQuietLevel = 0x1.51eb86p-1f; /* 0.66 */
    static const float kPitchLow = 0x1.99999ap-3f;   /* 0.2 */
    char *ground;
    char buf[128];
    int id;
    float pitch;
    Task *s;

    if (h->animEvent.sound != ANIM_WALK || h->animEvent.anim != Anim_GetId(h->anim, 0) || !h->ground.found)
        return;
    ground = (char *)h->ground.material;
    id = gFootstepSounds[h->footstep];
    h->footstep = (h->footstep + 1) % 12;
    if (!ground)
        return;
    pitch = (Random_Float(Game_GetRandom()) * 0.5f + 1.0f) - kPitchLow;
    s = Sound_Play(Level_GetDynamic(), ground + 0x54 + id * 0x20);
    if (s) {
        if (Human_IsCrouching(h))
            Sound_SetVolume(s, -1.0f, kQuietLevel);
        Sound_SetPos(s, &h->ground.pos.x);
        Sound_SetPitch(s, pitch);
    }
    if (Game_IsRaining()) {
        crt_sprintf(buf, "walk_rain_%1d", id + 1);
        s = Sound_Play(Level_GetDynamic(), buf);
        if (s) {
            Sound_SetPos(s, &h->ground.pos.x);
            Sound_SetPitch(s, pitch);
        }
    }
    if (!Human_IsCrouching(h)) {
        AIMessage noise;

        noise.radius = *(float *)(ground + 0x50) * 4096.0f;
        noise.team = h->team;
        noise.pos = &h->ground.pos;
        noise.type = 10;
        noise.flags = 0x46;
        noise.from = (Task *)h;
        noise.about = (Task *)h;
        HumanAI_Broadcast(&noise);
    }
}
HOOK(0x463920, Human_Footstep);

static void normalize3(float *d)
{
    if (!(x87_eq(d[0], 0.0f) && x87_eq(d[1], 0.0f) && x87_eq(d[2], 0.0f))) {
        long double x = d[0], y = d[1], z = d[2];
        long double inv = 1.0 / x87_sqrt((x * x + y * y) + z * z);

        d[0] = x * inv;
        d[1] = y * inv;
        d[2] = z * inv;
    }
}

/* blood from a hit at hit->pos going along dir */
void Human_Bleed(Human *h, const float *dir, CollHit *src)
{
    static const double kSplatDist = 12288.0;
    static const float kSplatSize = 0x1.333334p+9f;  /* 614.4 */
    static const float kSpread = 0x1.aceeap-5f;      /* 0.0524 */
    static const float kSpreadBase = 0x1.a3667ap-7f; /* 0.0128 */
    double *pos = &src->pos.x;
    double from[3], to[3], ys, zs;
    float d[3], a[3], color[3], size[2], zero[3];
    struct {
        int unk0;
        Human *ignore;
    } filter;
    CollHit hit;
    Task *ps;
    void *world;
    uint8_t ev;
    int n;

    if (!Profile_GetActive()->blood)
        return;
    d[0] = dir[0];
    d[1] = dir[1];
    d[2] = dir[2];
    normalize3(d);
    {
        long double xs = d[0] * kSplatDist;

        ys = d[1] * kSplatDist;
        zs = d[2] * kSplatDist;
        memcpy(from, pos, sizeof(from));
        filter.ignore = h;
        to[0] = from[0] + xs;
        to[1] = from[1] + ys;
        to[2] = from[2] + zs;
    }
    ev = Game_GetEvent8();
    Coll_Lines(Game_GetWorld(), &hit, from, to, 1, ev, &filter, 0x20000000);
    if (hit.found && hit.material && !hit.back && *(int *)((char *)hit.material->params + 0x58)) {
        DecalMsg msg;
        TaskHandler fn;
        float sz = (Random_Float(Game_GetRandom()) + 1.0f) * kSplatSize;
        unsigned k = Random_Int(Game_GetRandom(), 2);
        void *tex;

        if (k == 0)
            tex = gBloodHole1 + 0x18;
        else if (k == 1)
            tex = gBloodHole2 + 0x18;
        else
            tex = *(void **)&sz;
        msg.hit = &hit;
        msg.texture = tex;
        msg.unk4 = 0;
        msg.size = sz;
        fn = gTaskHandlers[BulletHole_GetEvent()][hit.task->type];
        if (fn)
            fn(hit.task, &msg);
    }

    a[0] = 0.0f;
    a[1] = 0.0f;
    a[2] = 0.0f;
    color[0] = color[1] = color[2] = 127.0f;
    size[0] = size[1] = 0x1.333334p+10f; /* 1228.8 */
    if (!h->bloodRef || !(ps = *h->bloodRef)) {
        int frames = 15 / *(short *)(*(char **)(gBloodSprite1 + 0x24) + 4);

        world = Game_GetWorld();
        ps = Particle2D_New(Level_GetDynamic(), world, gBloodSprite1 + 0x18, a, color, size, 409600.0f, 0x1080000,
                        frames, 0.0f, 0, 2, NULL);
        if (h->bloodRef)
            TaskRef_Remove(h->bloodRef);
        h->bloodRef = TaskRef_Add(ps);
    }
    memcpy(from, pos, sizeof(from));
    zero[0] = 0.0f;
    zero[1] = 0.0f;
    zero[2] = 0.0f;
    Particle2D_Add(ps, from, zero, 1.0f, 0.0f, 0.0f, 1.0f, 15, 0);

    a[0] = 0.0f;
    a[1] = 0.0f;
    a[2] = -0x1.dbbe2ap+3f; /* -14.87 */
    size[0] = size[1] = 0x1.99999ap+6f; /* 102.4 */
    if (!h->dropRef || !(ps = *h->dropRef)) {
        world = Game_GetWorld();
        ps = Particle2D_New(Level_GetDynamic(), world, gBloodSprite2 + 0x18, a, color, size, 409600.0f, 0x80000, 0,
                        0.0f, 0, 10, Human_BloodDrop);
        if (h->dropRef)
            TaskRef_Remove(h->dropRef);
        h->dropRef = TaskRef_Add(ps);
    }
    for (n = Random_Int(Game_GetRandom(), 3) + 3; n > 0; n--) {
        long double r = Random_Float(Game_GetRandom()) - 0.5f;

        Particle2D_Emit(ps, pos, d, 0x1.657186p-3f, 379.0f, 606.0f, 1.0f, (r + r) * kSpread + kSpreadBase, 0, 1.0f, 30, 7);
    }

    d[0] = dir[0];
    d[1] = dir[1];
    d[2] = dir[2];
    normalize3(d);
    d[0] = -d[0];
    d[1] = -d[1];
    d[2] = -d[2];
    for (n = Random_Int(Game_GetRandom(), 3) + 2; n > 0; n--) {
        long double r = Random_Float(Game_GetRandom()) - 0.5f;

        Particle2D_Emit(ps, pos, d, 0x1.38c354p+0f, 37.0f, 113.0f, 1.0f, (r + r) * kSpread + kSpreadBase, 0, 1.0f, 30, 7);
    }
}
HOOK(0x463AD0, Human_Bleed);

/* particle callback: a blood drop that lands leaves a decal */
void Human_BloodDrop(void *arg)
{
    static const float kDropSize = 0x1.99999ap+7f; /* 204.8 */
    static const float kDropBase = 0x1.99999ap+6f; /* 102.4 */
    ParticleHit *p = arg;
    CollHit *hit = p->hit;
    long double dx, dy, dz;
    DecalMsg msg;
    unsigned k;
    float sz;

    if (!gTaskHandlers[BulletHole_GetEvent()][hit->task->type])
        return;
    dx = (long double)p->pos[0] - p->prev[0];
    dy = (long double)p->pos[1] - p->prev[1];
    dz = (long double)p->pos[2] - p->prev[2];
    normalize3(&hit->normal.x);
    if ((dz * hit->normal.z + dy * hit->normal.y) + dx * hit->normal.x >= 0.0f)
        return;
    sz = Random_Float(Game_GetRandom()) * kDropSize + kDropBase;
    k = Random_Int(Game_GetRandom(), 2);
    msg.texture = k == 0 ? gBloodHole3 + 0x18 : k == 1 ? gBloodHole4 + 0x18 : *(void **)&sz;
    msg.hit = hit;
    msg.unk4 = 0;
    msg.size = sz;
    gTaskHandlers[BulletHole_GetEvent()][hit->task->type](hit->task, &msg);
}
HOOK(0x4640B0, Human_BloodDrop);

void Human_HideShadow(Human *h)
{
    h->unk7E8 = 0;
}
HOOK(0x464230, Human_HideShadow);

void Human_ShowShadow(Human *h)
{
    h->unk7E8 = 1;
}
HOOK(0x464240, Human_ShowShadow);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD_CONST(igd, 53F318, 53F318, 0x30) INIT({0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05});
ALIAS(gFootstepSounds, pad_53F318, 0);
BSS(5BDAD0) char *gBloodSprite2;
BSS(5BDAD0) char *gBloodSprite1;
BSS(5BDB04) char *gBloodHole4;
BSS(5BDB04) char *gBloodHole1;
BSS(5BDB10) char *gBloodHole2;
BSS(5BDB10) char *gBloodHole3;
