/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "viewcam/viewcam.h"

FN_EXTERN(HumanAttach_Detect);
FN_EXTERN(SubLevel_Run);
FN_EXTERN(Task_FalseHandler);

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct CollHit {
    Task *task;
    int unk4;
    double pos[3];
    char unk20[0x38];
    int found;
    int unk5C;
    double dist;
    char unk68[8];
} CollHit;

/* the second word is the object the line ignores */
typedef struct CollFilter {
    uint32_t unk0;
    Human *ignore;
} CollFilter;

/* where sounds are heard from */
typedef struct SoundEar {
    double *pos;
    Rot *rot;
    int unk8;
} SoundEar;

ORIG(0x4012A0, Task *, Task_New, (Task *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4148F0, long double, Vec3d_PitchAngle, (Vec3d v));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x46C060, Task *, Computer_Get, (void));
ORIG(0x477C20, uint8_t, Gun_GetEvent2, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4B3320, char, Vec3d_IsZero, (const double *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Rot *m, float ax, float ay, float az));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Rot *m, float a));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, double *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, double *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *hits, double *from, double *to, int count, uint8_t event,
                                  CollFilter *filter, unsigned flags));
ORIG(0x4CDB20, void, Coll_LinesEx, (void *world, CollHit *hits, double *from, double *to, int count, uint8_t event,
                                    CollFilter *filter, unsigned flags, int unk));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void *, VirModel_Find, (const char *name));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (float *out, float *unk, void *anim));
ORIG(0x4D3210, void, BoneObj_GetEvents, (int *out, void *anim));
ORIG(0x4D38C0, void, BoneObj_Update, (Rot *m, void *anim));
ORIG(0x4D6110, void, Anim_PlayNow, (void *anim, int a, int b, int c));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4D9620, void, BoneDynCubeObj_Init, (void *world, void *obj, int a, void *model));
ORIG(0x4D96F0, void, BoneDynCubeObj_Deinit, (void *obj));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E81F0, char *, Viewport_GetCurrentCamera, (void));
ORIG(0x4EC0E0, void, AnimTrigger_Fire, (int bone, void *obj, double *pos));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4F2310, void, Weapon_Move, (Task *w, double *pos, Rot *m));

extern uint16_t gHumanViewType;
extern int gHumanMaskType;

#define TASK_RUN_CHILDREN (FN(0x488700, SubLevel_Run))
#define HANDLER_45F920 (FN(0x45F920, Task_FalseHandler))
#define HANDLER_465B80 (FN(0x465B80, HumanAttach_Detect))

#define PI_F 0x1.921fb6p+1f
#define HALF_PI_F 0x1.921fb6p+0f

static const double kFovX = 0x1.921fb6p-1; /* 0.785398 */
static const double kFovY = 0x1.0c1524p-1; /* 0.523599 */

static long double x87_tan(long double x)
{
    __asm__("fptan\n\tfstp %%st(0)" : "+t"(x));
    return x;
}

int Human_GetMaskType(void)
{
    return gHumanMaskType;
}
HOOK(0x464250, Human_GetMaskType);

void HumanView_Init(void)
{
    uint16_t parent = BoneDynCubeObj_GetType();

    gHumanViewType = TaskType_New("HumanView", sizeof(HumanView), parent, 0, 0);
    TaskType_SetHandler(gHumanViewType, 0, HumanView_CreateHandler);
    TaskType_SetHandler2(gHumanViewType, 1, TASK_RUN_CHILDREN);
    TaskType_SetHandler2(gHumanViewType, 2, HumanView_DeleteHandler);
    TaskType_SetHandler(gHumanViewType, 4, HumanView_EnterHandler);
    TaskType_SetHandler(gHumanViewType, World_GetEvent1(), HumanView_LeaveWorldHandler);
    TaskType_SetHandler(gHumanViewType, World_GetEvent0(), HumanView_JoinWorldHandler);
    TaskType_SetHandler(gHumanViewType, 7, NULL);
    TaskType_SetHandler(gHumanViewType, Game_GetEvent7(), HANDLER_45F920);
    TaskType_SetHandler(gHumanViewType, Game_GetEvent8(), HANDLER_45F920);
    TaskType_SetHandler(gHumanViewType, Game_GetEvent13(), HANDLER_45F920);
    TaskType_SetHandler(gHumanViewType, AIDetection_GetEvent0(), HANDLER_465B80);
    TaskType_SetHandler(gHumanViewType, Sound_GetInfoEvent(), HumanView_SoundInfoHandler);
}
HOOK(0x464260, HumanView_Init);

void HumanView_CreateHandler(HumanView *v)
{
    v->unkD8 = 0;
    v->inWorld = 0;
    HumanView_ClearAngles(v);
    memset(v->target, 0, sizeof(v->target));
    v->unk138 = 0;
    v->unk139 = 1;
    v->camOverride = 0;
    v->hasTarget = 0;
    memset(v->offset, 0, sizeof(v->offset));
    memset(v->unk1A8, 0, sizeof(v->unk1A8));
    v->shake = 0.0f;
    v->dip = 0.0f;
    HumanView_ClearWeaponAngles(v);
    v->zoomed = 0;
    v->bone = -1;
    v->duck = 0.0f;
    v->duckNow = 0.0f;
    v->fovX = x87_tan(kFovX);
    v->fovY = x87_tan(kFovY);
}
HOOK(0x4643A0, HumanView_CreateHandler);

void HumanView_DeleteHandler(HumanView *v)
{
    if (v->inWorld) {
        v->inWorld = 0;
        BoneDynCubeObj_Deinit(v);
        DynCube_Remove(Game_GetWorld(), v);
    }
}
HOOK(0x464470, HumanView_DeleteHandler);

/* gun event 2 reply: where the held weapon sits (HumanBoneMsg in humanbase) */
typedef struct WeaponPlace {
    int firstPerson;
    int unk4;
    double pos[3];
    Rot rot;
} WeaponPlace;

/* camera of the player being shown: also moves the held weapon */
void HumanView_EnterHandler(HumanView *v, void *arg)
{
    Human *h = (Human *)v->task.parent;
    char *cam = *(char **)(Game_Get() + 0x5C);
    Task *w;
    WeaponPlace msg;

    if (*(char **)(Viewport_GetCurrentCamera() + 0x54) != cam || **(Human ***)(cam + 0x68) != h || *(int *)(cam + 0xE8) != 1)
        return;
    if (Computer_Get())
        return;
    HumanView_ApplyRot(v);
    gTaskHandlers[4][BoneDynCubeObj_GetType()](v, arg);
    w = WeaponSlots_Get(h->weapons, 0);
    if (!w)
        return;
    msg.firstPerson = 1;
    gTaskHandlers[Gun_GetEvent2()][h->task.type](h, &msg);
    Weapon_Move(w, msg.pos, &msg.rot);
    gTaskHandlers[4][RigidDynCubeObj_GetType()](w, NULL);
}
HOOK(0x4644A0, HumanView_EnterHandler);

void HumanView_LeaveWorldHandler(HumanView *v, void **world)
{
    if (v->inWorld) {
        BoneDynCubeObj_Deinit(v);
        DynCube_Remove(*world, v);
        v->inWorld = 0;
    }
}
HOOK(0x464580, HumanView_LeaveWorldHandler);

void HumanView_JoinWorldHandler(HumanView *v)
{
    if (v->unkD8)
        HumanView_AddToWorld(v);
}
HOOK(0x4645B0, HumanView_JoinWorldHandler);

void HumanView_SoundInfoHandler(HumanView *v, void *arg)
{
    SoundEar *ear = arg;

    ear->pos = v->eye;
    ear->rot = &v->cam;
    ear->unk8 = 0;
}
HOOK(0x4645D0, HumanView_SoundInfoHandler);

void HumanView_Free(void)
{
    TaskType_Free(gHumanViewType);
}
HOOK(0x464600, HumanView_Free);

Task *HumanView_New(Task *parent)
{
    Task *t;

    if (Mem_GetMode())
        return Task_New(parent, gHumanViewType, NULL);
    Mem_SetDefaultMode();
    t = Task_New(parent, gHumanViewType, NULL);
    Mem_ClearMode();
    return t;
}
HOOK(0x464610, HumanView_New);

void HumanView_AddToWorld(HumanView *v)
{
    double zero[3] = { 0.0, 0.0, 0.0 };
    void *model;

    v->unkD8 = 1;
    v->inWorld = 1;
    model = VirModel_Find("005_01_1");
    BoneDynCubeObj_Init(Game_GetWorld(), v, 5, model);
    DynCube_Add(Game_GetWorld(), v, zero, v->radius, 2);
    Anim_PlayNow(v->anim, 0, 2, 0);
    v->unkD4 = 0;
}
HOOK(0x464660, HumanView_AddToWorld);

void HumanView_LockPitch(HumanView *v)
{
    v->unk138 = 1;
}
HOOK(0x464700, HumanView_LockPitch);

void HumanView_UnlockPitch(HumanView *v)
{
    v->unk138 = 0;
}
HOOK(0x464710, HumanView_UnlockPitch);

void HumanView_ClearAngles(HumanView *v)
{
    v->pitch = 0.0f;
    v->pitchInput = 0.0f;
    v->recoilPitch = 0.0f;
    v->yaw = 0.0f;
    v->unk130 = 0.0f;
    v->recoilYaw = 0.0f;
}
HOOK(0x464720, HumanView_ClearAngles);

void HumanView_ClearWeaponAngles(HumanView *v)
{
    v->weaponPitch = 0.0f;
    v->weaponPitchTarget = 0.0f;
    v->weaponYaw = 0.0f;
    v->weaponYawTarget = 0.0f;
}
HOOK(0x464750, HumanView_ClearWeaponAngles);

void HumanView_SetWeaponTarget(HumanView *v, float pitch, float yaw, float pitchSpeed, float yawSpeed)
{
    v->weaponYawTarget = yaw;
    v->weaponPitchSpeed = pitchSpeed;
    v->weaponPitchTarget = pitch;
    v->weaponYawSpeed = yawSpeed;
}
HOOK(0x464770, HumanView_SetWeaponTarget);

void HumanView_SetWeaponAngles(HumanView *v, float pitch, float yaw)
{
    v->weaponPitchTarget = pitch;
    v->weaponYawTarget = yaw;
    v->weaponPitch = pitch;
    v->weaponYaw = yaw;
}
HOOK(0x4647A0, HumanView_SetWeaponAngles);

char HumanView_WeaponAtTarget(HumanView *v)
{
    return x87_eq(v->weaponPitch, v->weaponPitchTarget) && x87_eq(v->weaponYaw, v->weaponYawTarget);
}
HOOK(0x4647D0, HumanView_WeaponAtTarget);

/* *a = *a clamped to -pi/2 .. pi/2; NaN gives -pi/2 */
static void clamp_half_pi(float *a)
{
    if (!(*a >= -HALF_PI_F))
        *a = -HALF_PI_F;
    else if (*a > HALF_PI_F)
        *a = HALF_PI_F;
}

void HumanView_UpdateView(HumanView *v, Human *h)
{
    static const float kSlowLimit = 0x1.eb851ep-5f; /* 0.06 */
    static const double kSlowScale = 0x1.0aaaaaaaaaaabp+4; /* 16.67 */
    static const float kZoomIn = 0x1.eb851ep-1f;    /* 0.96 */
    static const float kZoomOut = 0x1.0aaaacp+0f;   /* 1.0417 */
    static const float kTurnStep = 0x1.aceeap-5f;   /* 3 degrees */
    float fov = x87_tan(kFovX), s, t;
    long double a;

    if (x87_eq(v->fovX, fov))
        s = (h->input[1] + h->prevInput[1]) * 0.5;
    else
        s = h->input[1];
    if (v->unk138 || !Vec3d_IsZero(v->target))
        s = 0.0f;
    a = s * PI_F * v->fovX;
    t = s;
    if (!(t >= 0.0f))
        t = -t;
    if (!(t >= kSlowLimit))
        a = a / (2.0 - t * kSlowScale);
    a = a + v->pitchInput;
    v->pitchInput = a;
    if (!(a >= -HALF_PI_F))
        v->pitchInput = -HALF_PI_F;
    else if (a > HALF_PI_F)
        v->pitchInput = HALF_PI_F;
    v->pitch = v->recoilPitch + v->pitchInput;
    if (!x87_eq(h->input[16], 0.0f)) {
        v->fovX = v->fovX * kZoomIn;
        v->fovY = v->fovY * kZoomIn;
    }
    if (!x87_eq(h->input[17], 0.0f)) {
        v->fovX = v->fovX * kZoomOut;
        v->fovY = v->fovY * kZoomOut;
    }
    if (!x87_eq(h->input[18], 0.0f)) {
        v->fovX = fov;
        v->fovY = x87_tan(kFovY);
    }
    if (!Vec3d_IsZero(v->target)) {
        Vec3d d;
        long double y, n;
        float p;

        memcpy(&d, v->target, sizeof(d));
        p = Vec3d_PitchAngle(d);
        memcpy(&d, v->target, sizeof(d));
        y = Vec3d_YawAngle(d);
        clamp_half_pi(&p);
        if (!(y >= -HALF_PI_F))
            y = -HALF_PI_F;
        else if (y > HALF_PI_F)
            y = HALF_PI_F;
        if (v->hasTarget) {
            v->yaw = y;
            v->pitch = p;
        } else {
            if (!(v->pitch >= p)) {
                n = v->pitch + kTurnStep;
                v->pitch = n;
                if (n > p)
                    v->pitch = p;
            } else if (v->pitch > p) {
                n = v->pitch - kTurnStep;
                v->pitch = n;
                if (!(n >= p))
                    v->pitch = p;
            }
            if (y > v->yaw) {
                n = v->yaw + kTurnStep;
                v->yaw = n;
                if (n > y)
                    v->yaw = y;
            } else if (!(y >= v->yaw)) {
                n = v->yaw - kTurnStep;
                v->yaw = n;
                if (!(n >= y))
                    v->yaw = y;
            }
        }
    }
    clamp_half_pi(&v->pitch);
    clamp_half_pi(&v->yaw);
}
HOOK(0x464800, HumanView_UpdateView);

/* moves *cur towards target by speed */
static void approach(float *cur, float target, float speed)
{
    long double n;

    if (!(*cur >= target)) {
        n = speed + *cur;
        *cur = n;
        if (n > target)
            *cur = target;
    } else if (*cur > target) {
        n = *cur - speed;
        *cur = n;
        if (!(n >= target))
            *cur = target;
    }
}

static long double shake_random(void)
{
    long double r = Random_Float(Game_GetRandom()) - 0.5f;

    return r + r;
}

void HumanView_UpdateBody(HumanView *v, Human *h)
{
    static const double kDipSide = 0x1.3333333333333p-4;    /* 0.075 */
    static const float kDipStep = 0x1.aceeap-3f;            /* 12 degrees */
    static const double kWallGap = 0x1.999999999999ap+7;    /* 204.8 */
    static const float kDuckReach = 2048.0f;
    static const double kDuckBase = 2048.0;
    static const float kDuckIn = 0x1.c71c72p+6f;  /* 113.78 */
    static const float kDuckOut = 0x1.2f684cp+6f; /* 75.85 */
    float shake[3] = { 0.0f, 0.0f, 0.0f };
    float e0, e1, e2, o[3];
    long double z;
    double eye[3], start[3], end[3];
    CollFilter filter;
    CollHit hit;
    uint8_t ev;
    void *world;

    e0 = h->pitch + v->pitch;
    e1 = 0.0f;
    e2 = h->yaw + v->yaw;
    if (!x87_eq(v->shake, 0.0f)) {
        e0 = v->shakeAngle * v->shake * shake_random() + e0;
        e1 = v->shakeAngle * v->shake * shake_random();
        e2 = v->shakeAngle * v->shake * shake_random() + e2;
    }
    Matrix_SetEuler(&v->cam, e0, e1, e2);
    Human_GetEyePos(eye, h);
    o[0] = v->offset[0];
    o[1] = v->offset[1];
    o[2] = v->offset[2];
    z = o[2];
    if (!x87_eq(v->shake, 0.0f)) {
        long double n;

        o[0] = v->shakeAmount * v->shake * shake_random() + o[0];
        z = v->shakeAmount * v->shake * shake_random() + o[2];
        if (!(v->shake >= 0.0f)) {
            n = v->shakeStep + v->shake;
            v->shake = n;
            if (n > 0.0f)
                v->shake = 0.0f;
        } else if (v->shake > 0.0f) {
            n = v->shake - v->shakeStep;
            v->shake = n;
            if (!(n >= 0.0f))
                v->shake = 0.0f;
        }
    }
    if (!x87_eq(v->dip, 0.0f)) {
        long double s = x87_sin(v->dipPhase), n;

        z = z + v->dip * s;
        shake[2] = v->dip * kDipSide * s;
        n = v->dipPhase + kDipStep;
        v->dipPhase = n;
        if (n >= PI_F)
            v->dip = 0.0f;
    }

    /* keep the eye out of walls */
    if (!(x87_eq(o[0], 0.0f) && x87_eq(o[1], 0.0f) && x87_eq(z, 0.0f))) {
        long double a = o[1] * v->cam.m[1] + o[0] * v->cam.m[0];
        float b = o[1] * v->cam.m[4] + o[0] * v->cam.m[3];

        memcpy(start, eye, sizeof(start));
        end[0] = a + eye[0];
        end[1] = b + eye[1];
        end[2] = z + eye[2];
        memcpy(&filter.unk0, &o[0], 4);
        filter.ignore = h;
        ev = Game_GetEvent8();
        world = Game_GetWorld();
        Coll_LinesEx(world, &hit, start, end, 1, ev, &filter, 0xFFFFFFFF, 1);
        if (hit.found) {
            double d[3], len;
            long double t;

            d[0] = hit.pos[0] - start[0];
            d[1] = hit.pos[1] - start[1];
            d[2] = hit.pos[2] - start[2];
            len = Vec3d_Length(d);
            Vec3d_Normalize(d);
            t = len - kWallGap;
            if (!(t >= 0.0))
                t = 0.0;
            d[0] = t * d[0];
            d[1] = t * d[1];
            eye[0] = d[0] + start[0];
            eye[1] = d[1] + start[1];
            eye[2] = t * d[2] + start[2];
        } else {
            memcpy(eye, end, sizeof(eye));
        }
    }
    memcpy(v->eye, eye, sizeof(eye));

    /* duck the weapon when something is in front of it */
    if (v->inWorld) {
        long double a = v->cam.m[1] * kDuckReach;
        float b = v->cam.m[4] * kDuckReach, c = v->cam.m[7] * kDuckReach, d, step;

        filter.unk0 = (uint32_t)(uintptr_t)&v->cam.m[2];
        memcpy(start, eye, sizeof(start));
        end[0] = a + eye[0];
        filter.ignore = h;
        end[1] = b + eye[1];
        end[2] = c + eye[2];
        ev = Game_GetEvent8();
        world = Game_GetWorld();
        Coll_Lines(world, &hit, start, end, 1, ev, &filter, 0xFFFFFFFF);
        if (hit.found)
            v->duck = -(kDuckBase - hit.dist);
        else
            v->duck = 0.0f;
        step = x87_eq(v->duck, 0.0f) ? kDuckOut : kDuckIn;
        approach(&v->duckNow, v->duck, step);
        d = v->duckNow;
        shake[0] = d * v->cam.m[1];
        shake[1] = d * v->cam.m[4];
        c = d * v->cam.m[7];
        shake[2] = c + shake[2];
    }

    if (!HumanView_WeaponAtTarget(v)) {
        approach(&v->weaponPitch, v->weaponPitchTarget, v->weaponPitchSpeed);
        approach(&v->weaponYaw, v->weaponYawTarget, v->weaponYawSpeed);
    }

    if (v->inWorld) {
        const float *c = v->cam.m, *r, *w;
        double pos[3];
        long double x;
        float y, zz;
        Rot m, rot;

        Matrix_SetEuler(&m, v->weaponPitch, 0.0f, v->weaponYaw);
        if (v->zoomed)
            Matrix_SetRotX(&m, -HALF_PI_F);
        rot.m[0] = (m.m[3] * c[1] + m.m[0] * c[0]) + m.m[6] * c[2];
        rot.m[1] = (m.m[4] * c[1] + m.m[1] * c[0]) + m.m[7] * c[2];
        rot.m[2] = (m.m[5] * c[1] + m.m[2] * c[0]) + m.m[8] * c[2];
        rot.m[3] = (m.m[3] * c[4] + m.m[6] * c[5]) + m.m[0] * c[3];
        rot.m[4] = (m.m[4] * c[4] + m.m[7] * c[5]) + m.m[1] * c[3];
        rot.m[5] = (m.m[5] * c[4] + m.m[8] * c[5]) + m.m[2] * c[3];
        rot.m[6] = (m.m[6] * c[8] + m.m[0] * c[6]) + m.m[3] * c[7];
        rot.m[7] = (m.m[7] * c[8] + m.m[1] * c[6]) + m.m[4] * c[7];
        rot.m[8] = (m.m[8] * c[8] + m.m[2] * c[6]) + m.m[5] * c[7];
        rot.serial = v->cam.serial + m.serial + 1;
        v->rotDirty = 1;
        v->rot = rot;
        BoneObj_GetRootMotion(v->weaponPos, v->unk1FC, v->anim);
        v->bone = -1;
        BoneObj_GetEvents(&v->unk208, v->anim);
        r = v->rot.m;
        if (v->bone != -1) {
            const float *b = v->bonePos;

            x = (r[2] * b[2] + r[1] * b[1]) + b[0] * r[0];
            y = (r[5] * b[2] + r[4] * b[1]) + r[3] * b[0];
            zz = (r[8] * b[2] + r[7] * b[1]) + r[6] * b[0];
            pos[0] = x + v->pos[0];
            pos[1] = y + v->pos[1];
            pos[2] = zz + v->pos[2];
            AnimTrigger_Fire(v->bone, v, pos);
        }
        HumanView_GetEye(eye, h->view);
        w = v->weaponPos;
        x = (r[2] * w[2] + r[1] * w[1]) + r[0] * w[0];
        y = (r[5] * w[2] + r[4] * w[1]) + r[3] * w[0];
        zz = (r[8] * w[2] + r[7] * w[1]) + r[6] * w[0];
        x = x + eye[0];
        eye[1] = y + eye[1];
        eye[2] = zz + eye[2];
        pos[0] = shake[0] + x;
        pos[1] = shake[1] + eye[1];
        pos[2] = shake[2] + eye[2];
        DynCube_Move(Game_GetWorld(), v, pos, v->radius);
    }
}
HOOK(0x464BD0, HumanView_UpdateBody);

void HumanView_ApplyRot(HumanView *v)
{
    if (v->rotDirty) {
        BoneObj_Update(&v->rot, v->anim);
        v->rotDirty = 0;
    }
}
HOOK(0x465700, HumanView_ApplyRot);

/* places the view model at the eye plus the weapon offset */
void HumanView_UpdatePosition(HumanView *v)
{
    const float *r = v->rot.m, *w = v->weaponPos;
    double pos[3];
    long double z;
    float x, y;

    if (!v->inWorld)
        return;
    HumanView_GetEye(pos, v);
    x = (w[2] * r[2] + w[1] * r[1]) + w[0] * r[0];
    y = (w[2] * r[5] + w[1] * r[4]) + w[0] * r[3];
    z = (w[2] * r[8] + w[1] * r[7]) + w[0] * r[6];
    pos[0] = x + pos[0];
    pos[1] = y + pos[1];
    pos[2] = z + pos[2];
    DynCube_Move(Game_GetWorld(), v, pos, v->radius);
}
HOOK(0x465730, HumanView_UpdatePosition);

void HumanView_GetEye(double *out, HumanView *v)
{
    memcpy(out, v->eye, sizeof(v->eye));
}
HOOK(0x465800, HumanView_GetEye);

void Human_GetEyePos(double *out, Human *h)
{
    static const double kStand = 0x1.6147ae147ae14p+11; /* 2826.24 */
    static const double kCrouch = 0x1.6666666666666p+10; /* 1433.6 */

    out[2] = kStand;
    if (Human_IsCrouching(h))
        out[2] = kCrouch;
    out[0] = h->pos[0];
    out[1] = h->pos[1];
    out[2] = h->pos[2] + out[2];
}
HOOK(0x465820, Human_GetEyePos);

void HumanView_GetMatrix(Rot *out, HumanView *v)
{
    *out = v->cam;
}
HOOK(0x465880, HumanView_GetMatrix);

void HumanView_GetForward(float *out, HumanView *v)
{
    out[0] = v->cam.m[1];
    out[1] = v->cam.m[4];
    out[2] = v->cam.m[7];
}
HOOK(0x4658A0, HumanView_GetForward);

void HumanView_SetCamera(HumanView *v, const double *pos, const Rot *rot)
{
    v->camOverride = 1;
    memcpy(v->camPos, pos, sizeof(v->camPos));
    v->camRot = *rot;
}
HOOK(0x4658D0, HumanView_SetCamera);
