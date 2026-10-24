#include <string.h>

#include "hook.h"
#include "igi.h"
#include "ammotype/ammotype.h"
#include "qasm/qasmtypes.h"

/* "GunShot" (0xC38 bytes): the bullets in flight of one gun and the
   tracer trails they leave. */

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x413B20, uint8_t, Weapon_GetFireEvent, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x460C30, uint8_t, Human_GetGunshotEvent, (void));
ORIG(0x460C60, QTaskList *, Human_GetList, (void));
ORIG(0x47B750, void, GunShot_MoveBullets, (GunShot *s));
ORIG(0x497F20, void, TransContext_Project, (float *screen, float *depth, Vec3d *pos));
ORIG(0x49A430, void, PolyList_SetPos, (PolyObj *p, Vec3d *pos));
ORIG(0x49A450, PolyObj *, PolyList_New, (void *world, void *owner, Vec3d *pos, float *matrix, ShotPoly *polys,
                                       int a, int b, int c, int d, int e, int f));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B47C0, int, Random_Int, (void *rng, int n));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4E0E20, void, Line3D_Draw, (Trail *t, Vec3d *b, float *c0, float a0, float *c1, float a1));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));

extern uint8_t gGunShotEvent;
extern uint16_t gGunShotType;
extern int gRicochetTime;
extern int gRicochetDelay;
extern const char *gRicochetSounds[];
extern void (*gRenderHandlers[])(void *task, void *arg);

extern const float kZero;
extern const float kOne;
extern const float kHalf;
extern const float k4096;    /* 4096 */
extern const double kNearPad;      /* 2^28 */
extern const double kNearDist;     /* 16384 */
extern const double kOneD;
extern const float kTrailWidth;    /* 286.72 */
extern const float kColorScale;    /* 1/255 */
extern const float kMaxDepth;      /* 2048000 */
extern const float kDepthFade;
extern const float kDepthFade2;    /* 0.1 */
extern const float kMinAlpha;      /* 0.2 */

#define GUN_AMMO(gun) (*(AmmoType **)((char *)(gun) + 0x12C))

void GunShot_Create(GunShot *s);
void GunShot_Run(GunShot *s);
void GunShot_Delete(GunShot *s);
void GunShot_Render(GunShot *s);
void GunShot_FireHandler(void *t, FireArgs *m);
void GunShot_UpdateTrails(GunShot *s);
void GunShot_AddTrailPoly(GunShot *s, int i, float a0, float a1, float r0, float g0, float b0, float r1, float g1,
                          float b1, int front);
void GunShot_RemoveTrailPoly(GunShot *s, int i);

static float clamp01(float v)
{
    if (kZero > v)
        return 0;
    if (!(kOne >= v))
        return 1.0f;
    return v;
}

/* draws the tracer of each trail: a line when both ends are in front of
   the camera, a triangle when it crosses the camera plane */
void GunShot_Render(GunShot *s)
{
    Task *gun = *s->gun;
    int i;

    if (gun && GUN_AMMO(gun) && GUN_AMMO(gun)->tracer) {
        AmmoType *at = GUN_AMMO(gun);
        float d[3];

        d[0] = rf((at->color1[0] - at->color0[0]) * kColorScale);
        d[1] = rf((at->color1[1] - at->color0[1]) * kColorScale);
        d[2] = rf((at->color1[2] - at->color0[2]) * kColorScale);
        for (i = 0; i < s->trailCount; i++) {
            Trail *t = s->trailList[i];
            long double f0 = t->from / t->len;
            float f1 = rf(t->to / t->len);
            float c0[3], c1[3], a0, a1, depth0, depth1, scr0[2], scr1[2];
            int k;

            c0[0] = rf(GUN_AMMO(gun)->color0[0] * kColorScale + f0 * d[0]);
            c0[1] = rf(GUN_AMMO(gun)->color0[1] * kColorScale + f0 * d[1]);
            c0[2] = rf(GUN_AMMO(gun)->color0[2] * kColorScale + f0 * d[2]);
            for (k = 0; k < 3; k++)
                c0[k] = clamp01(c0[k]);
            c1[0] = rf(GUN_AMMO(gun)->color0[0] * kColorScale + f1 * d[0]);
            c1[1] = rf(GUN_AMMO(gun)->color0[1] * kColorScale + f1 * d[1]);
            c1[2] = rf(GUN_AMMO(gun)->color0[2] * kColorScale + f1 * d[2]);
            for (k = 0; k < 3; k++)
                c1[k] = clamp01(c1[k]);
            a0 = clamp01(rf(t->from / t->len));
            a1 = clamp01(rf(t->to / t->len));
            TransContext_Project(scr0, &depth0, &t->a);
            TransContext_Project(scr1, &depth1, &t->b);
            if ((depth1 < kZero) != (depth0 < kZero)) {
                if (!t->hasPoly)
                    GunShot_AddTrailPoly(s, i, a0, a1, c0[0], c0[1], c0[2], c1[0], c1[1], c1[2], depth0 > kZero);
            } else {
                long double fade;

                if (kZero > depth1)
                    depth1 = 0;
                else if (!(kMaxDepth >= depth1))
                    depth1 = 2048000.0f;
                if (kZero > depth0)
                    depth0 = 0;
                else if (!(kMaxDepth >= depth0))
                    depth0 = 2048000.0f;
                fade = (kOne - depth0 * kDepthFade * kDepthFade2) * a0;
                a0 = rf(fade);
                a1 = rf((kOne - depth1 * kDepthFade * kDepthFade2) * a1);
                if (!(fade > kMinAlpha))
                    a0 = kMinAlpha;
                if (!(a1 > kMinAlpha))
                    a1 = kMinAlpha;
                Line3D_Draw(t, &t->b, c0, a0, c1, a1);
            }
            s->trailList[i]->life = 0;
        }
    }
    if (s->poly->count > 0) {
        PolyList_SetPos(s->poly, &s->trails[0].a);
        gRenderHandlers[s->poly->task.type](s->poly, 0);
    }
}
HOOK(0x47ABD0, GunShot_Render);

uint8_t GunShot_GetEvent(void)
{
    return gGunShotEvent;
}
HOOK(0x47B100, GunShot_GetEvent);

GunShot *GunShot_New(void *parent, void *gun, void *b)
{
    GunShot *s;

    Mem_PushMode(1);
    s = (GunShot *)Task_New(parent, gGunShotType, NULL);
    if (gun)
        s->gun = TaskRef_Add(gun);
    if (b)
        s->ref70 = TaskRef_Add(b);
    Mem_PopMode();
    return s;
}
HOOK(0x47B110, GunShot_New);

/* deletes the shot now, or once its bullets are gone */
void GunShot_Release(GunShot *s, int v)
{
    if (s->active) {
        s->released = v;
        return;
    }
    Task_Delete(s);
}
HOOK(0x47B170, GunShot_Release);

void GunShot_AddBullet(GunShot *s, Vec3d *pos, Vec3f *dir, char *type, int a5, int a6)
{
    Bullet *b;
    int i;

    if (s->active >= 15) {
        s->total++;
        return;
    }
    b = &s->bullets[0];
    for (i = 0; i < 15; i++) {
        b = &s->bullets[i];
        if (!b->used)
            break;
    }
    s->activeList[s->active] = b;
    b->used = 1;
    b->pos = *pos;
    b->dir = *dir;
    b->ricocheted = 0;
    memcpy(&b->unk38, type + 0x654, 4);
    b->speed = *(float *)(type + 0x650) * k4096;
    memcpy(&b->pierce, type + 0x658, 4);
    b->unk44 = a6;
    b->pierced = 0;
    b->type = type;
    b->unk30 = a5;
    s->active++;
    s->total++;
}
HOOK(0x47B190, GunShot_AddBullet);

void GunShot_SetUnk78(GunShot *s, Vec3d *v)
{
    s->unk78 = *v;
}
HOOK(0x47B270, GunShot_SetUnk78);

/* bullets of this frame passing within 16384 of pos: writes the closest
   point of each path to out, returns how many */
int GunShot_FindNear(Vec3d *out, int max, GunShot *s, Vec3d *pos)
{
    int count = 0;
    int i;

    for (i = 0; i < s->active; i++) {
        Bullet *b;
        long double sx, hx, hy, hz, r2, num, den, t, dz;
        float fdy, fdz;
        double vx, vy, vz, dx2, dy2, dz2, tx, ty;
        Vec3d mid, bp;

        if (count >= max)
            return count;
        b = s->activeList[i];
        if (!b->used)
            continue;
        sx = (long double)b->dir.x * b->speed;
        fdy = rf((long double)b->dir.y * b->speed);
        fdz = rf((long double)b->dir.z * b->speed);
        vx = rd(sx);
        vy = rd((long double)b->dir.y * b->speed);
        vz = fdz;
        hx = sx * kHalf;
        hy = fdy * kHalf;
        hz = fdz * kHalf;
        mid.x = hx + b->pos.x;
        mid.y = hy + b->pos.y;
        mid.z = hz + b->pos.z;
        dx2 = rd(hx);
        dy2 = hy;
        dz2 = hz;
        r2 = (long double)dx2 * dx2 + (long double)dy2 * dy2;
        r2 = r2 + (long double)dz2 * dz2;
        if (!(r2 + kNearPad > Vec3d_DistanceSq(pos, &mid)))
            continue;
        bp = s->activeList[i]->pos;
        num = ((long double)pos->z - bp.z) * vz;
        num = num + ((long double)pos->y - bp.y) * vy;
        num = num + ((long double)pos->x - bp.x) * vx;
        den = (long double)vz * vz + (long double)vy * vy;
        den = den + (long double)vx * vx;
        t = num / den;
        tx = rd(t * vx);
        ty = rd(t * vy);
        dz = t * vz;
        out->x = bp.x + tx;
        out->y = bp.y + ty;
        out->z = bp.z + dz;
        if (Vec3d_Distance(out, pos) < kNearDist) {
            out++;
            count++;
        }
    }
    return count;
}
HOOK(0x47B290, GunShot_FindNear);

void GunShot_Init(void)
{
    gGunShotType = TaskType_New("GunShot", sizeof(GunShot), Task_GetRootType(), 0);
    gGunShotEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gGunShotType, 0, GunShot_Create);
    TaskType_SetHandler2(gGunShotType, 1, GunShot_Run);
    TaskType_SetHandler2(gGunShotType, 2, GunShot_Delete);
    TaskType_SetHandler(gGunShotType, 4, GunShot_Render);
    TaskType_SetHandler(gGunShotType, Weapon_GetFireEvent(), GunShot_FireHandler);
    Script_RegisterConstant("TASKTYPE_GUNSHOT", gGunShotType);
}
HOOK(0x47B4B0, GunShot_Init);

void GunShot_Create(GunShot *s)
{
    static const float identity[10] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0};
    float m[10];
    int i;

    memset(&s->unk78, 0, sizeof(s->unk78));
    s->released = 0;
    s->gun = NULL;
    s->ref70 = NULL;
    s->total = 0;
    s->active = 0;
    s->trailRate = 10;
    s->unkA0 = 0;
    memset(s->activeList, 0, sizeof(s->activeList));
    for (i = 0; i < 15; i++)
        s->bullets[i].used = 0;
    s->trailCount = 0;
    for (i = 0; i < 5; i++) {
        s->trails[i].used = 0;
        s->trails[i].life = 0;
        s->trails[i].from = 0;
        s->trails[i].to = 0;
    }
    memset(s->trailList, 0, sizeof(s->trailList));
    memcpy(m, identity, sizeof(m));
    s->poly = PolyList_New(Game_GetWorld(), s, &s->pos, m, s->polys, 0, 0, 0, 0, 0, 0);
    DynCube_ListAdd(Game_GetWorld(), s, 0);
}
HOOK(0x47B560, GunShot_Create);

void GunShot_Run(GunShot *s)
{
    if (s->active) {
        Task *msg[3];
        int i;

        msg[0] = &s->task;
        msg[1] = (Task *)s->gun;
        msg[2] = (Task *)s->ref70;
        for (i = 0; i < Human_GetList()->count; i++) {
            uint16_t type = Human_GetList()->items[i]->type;
            TaskFn fn = HANDLER(Human_GetGunshotEvent(), type);

            fn(Human_GetList()->items[i], msg);
        }
    }
    GunShot_MoveBullets(s);
    GunShot_UpdateTrails(s);
    if (!s->active && s->released && !s->trailCount)
        Task_Delete(s);
}
HOOK(0x47B690, GunShot_Run);

void GunShot_PlayRicochet(Vec3d *pos)
{
    const char *name;
    Task *snd;

    if (gRicochetTime != -1) {
        int d;

        if ((int)((unsigned)Flow_GetFrames() - gRicochetTime) < 0)
            d = (int)((unsigned)gRicochetTime - Flow_GetFrames());
        else
            d = (int)((unsigned)Flow_GetFrames() - gRicochetTime);
        if (d <= gRicochetDelay)
            return;
    }
    name = gRicochetSounds[Random_Int(Game_GetRandom(), 4)];
    snd = Sound_Play(Level_GetDynamic(), name);
    if (snd)
        Sound_SetPos(snd, pos);
    gRicochetTime = Flow_GetFrames();
    gRicochetDelay = Random_Int(Game_GetRandom(), 6) + 6;
}
HOOK(0x47C330, GunShot_PlayRicochet);

void GunShot_Delete(GunShot *s)
{
    if (s->gun) {
        TaskRef_Remove(s->gun);
        s->gun = NULL;
    }
    if (s->ref70) {
        TaskRef_Remove(s->ref70);
        s->ref70 = NULL;
    }
    DynCube_ListRemove(Game_GetWorld(), s, 0);
}
HOOK(0x47C3D0, GunShot_Delete);

/* a weapon fired: adds a bullet to the weapon's GunShot */
void GunShot_FireHandler(void *t, FireArgs *m)
{
    void *type = m->type;
    int a5 = m->unk24;
    GunShot *src[2];
    Vec3f dir;

    HANDLER(gGunShotEvent, m->weapon->type)(m->weapon, src);
    memcpy(&dir.x, &m->matrix->m[1], 4);
    memcpy(&dir.y, &m->matrix->m[4], 4);
    memcpy(&dir.z, &m->matrix->m[7], 4);
    GunShot_AddBullet(src[0], m->pos, &dir, type, a5, m->unk2C);
}
HOOK(0x47C420, GunShot_FireHandler);

void GunShot_Close(void)
{
    TaskEvent_Free(gGunShotEvent);
    TaskType_Free(gGunShotType);
}
HOOK(0x47C4A0, GunShot_Close);

void GunShot_AddTrail(GunShot *s, Vec3d *a, Vec3d *b, int from, int to, int len, int life)
{
    Trail *t;
    int i;

    for (i = 0; i < 5; i++)
        if (!s->trails[i].used)
            break;
    if (i == 5)
        return;
    t = &s->trails[i];
    s->trailList[s->trailCount] = t;
    s->trailCount++;
    t->a = *a;
    t->b = *b;
    memcpy(&t->from, &from, 4);
    t->life = life;
    t->used = 1;
    memcpy(&t->to, &to, 4);
    memcpy(&t->len, &len, 4);
    t->hasPoly = 0;
}
HOOK(0x47C4C0, GunShot_AddTrail);

/* drops trails whose life ran out, then counts the others down */
void GunShot_UpdateTrails(GunShot *s)
{
    int i;

    for (i = 0; i < s->trailCount; i++) {
        Trail **p = &s->trailList[i];

        while ((*p)->life == 0) {
            if ((*p)->hasPoly)
                GunShot_RemoveTrailPoly(s, i);
            (*p)->used = 0;
            s->trailCount--;
            *p = s->trailList[s->trailCount];
            s->trailList[s->trailCount] = NULL;
            if (i >= s->trailCount)
                break;
        }
    }
    for (i = 0; i < s->trailCount; i++)
        s->trailList[i]->life--;
}
HOOK(0x47C560, GunShot_UpdateTrails);

/* a triangle from the trail to a point beside its start or end, for trails
   that cross the camera plane */
void GunShot_AddTrailPoly(GunShot *s, int i, float a0, float a1, float r0, float g0, float b0, float r1, float g1,
                          float b1, int front)
{
    Trail *t = s->trailList[i];
    PolyObj *poly = s->poly;
    Vec3d *o = &s->trails[0].a;
    long double bz, inv, nx;
    float dx, dy, lx, ly, lz, ny, nz;
    int j, n;

    if (poly->count == 10)
        return;
    t->v[0][0] = rf(t->a.x - o->x);
    t->v[0][1] = rf(t->a.y - o->y);
    t->v[0][2] = rf(t->a.z - o->z);
    t->v[1][0] = rf(t->b.x - o->x);
    t->v[1][1] = rf(t->b.y - o->y);
    bz = (long double)t->b.z - o->z;
    t->v[1][2] = rf(bz);
    dx = rf(t->v[1][0] - t->v[0][0]);
    dy = rf(t->v[1][1] - t->v[0][1]);
    /* the trail direction turned 90 degrees in the xy plane */
    lx = rf(kZero - dy);
    ly = rf(-(kZero - dx));
    lz = 0;
    if (lx == 0 && ly == 0)
        return;
    inv = kOneD / __builtin_sqrtl((long double)lx * lx + (long double)ly * ly + (long double)lz * lz);
    lx = rf(lx * inv);
    ly = rf(ly * inv);
    lz = rf(lz * inv);
    nx = lx * kTrailWidth;
    ny = rf(ly * kTrailWidth);
    nz = rf(lz * kTrailWidth);
    if (front) {
        t->v[2][0] = nx + t->v[1][0];
        t->v[2][1] = ny + t->v[1][1];
        t->v[2][2] = nz + t->v[1][2];
    } else {
        t->v[2][0] = nx + t->v[0][0];
        t->v[2][1] = ny + t->v[0][1];
        t->v[2][2] = nz + t->v[0][2];
    }
    memcpy(&t->color[0][0], &r0, 4);
    memcpy(&t->color[0][1], &g0, 4);
    memcpy(&t->color[0][2], &b0, 4);
    memcpy(&t->color[1][0], &r1, 4);
    memcpy(&t->color[1][1], &g1, 4);
    memcpy(&t->color[1][2], &b1, 4);
    memcpy(&t->color[2][0], &r1, 4);
    memcpy(&t->color[2][1], &g1, 4);
    memcpy(&t->color[2][2], &b1, 4);
    memset(t->uv, 0, sizeof(t->uv));
    t->normal[1] = 0;
    t->normal[2] = 1.0f;
    t->normal[0] = 0;
    t->poly = poly->count;
    t->hasPoly = 1;
    t->alpha[0] = 0;
    t->alpha[1] = 0.5f;
    t->alpha[2] = front ? 0.5f : 0;
    for (j = 0; j < 3; j++) {
        s->polys[poly->count].color[j] = t->color[j];
        s->polys[poly->count].v[j] = t->v[j];
        s->polys[poly->count].uv[j][0] = t->uv[j][0];
        s->polys[poly->count].uv[j][1] = t->uv[j][1];
        s->polys[poly->count].alpha[j] = t->alpha[j];
    }
    n = poly->count;
    s->polys[n].unk4C = 0;
    s->polys[poly->count].unk48 = 0;
    memcpy(s->polys[poly->count].normal, t->normal, sizeof(t->normal));
    poly->count++;
}
HOOK(0x47C610, GunShot_AddTrailPoly);

/* moves the last triangle into the slot of trail i's triangle */
void GunShot_RemoveTrailPoly(GunShot *s, int i)
{
    Trail *t = s->trailList[i];
    PolyObj *poly = s->poly;
    int slot = t->poly;
    int n = s->trailCount;
    int j;

    t->hasPoly = 0;
    poly->count--;
    s->polys[slot] = s->polys[poly->count];
    for (j = 0; j < n; j++) {
        if (s->trailList[j]->hasPoly && s->trailList[j]->poly == poly->count) {
            s->trailList[j]->poly = slot;
            return;
        }
    }
}
HOOK(0x47C990, GunShot_RemoveTrailPoly);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5333C4) const float kMinAlpha INIT(0.2f);
ALIAS(kFifth, kMinAlpha, 0);
DATA(534158) const float kDepthFade INIT(4.882813e-07f);
DATA(534158) const float kMaxDepth INIT(2.048e+06f);
DATA(534158) const float kColorScale INIT(0.003921569f);
PAD_CONST(igd, 534158, 534164, 0x4);
DATA(534158) const double kNearDist INIT(16384.0);
DATA(534158) const double kNearPad INIT(268435456.0);
DATA(534158) const float kTrailWidth INIT(286.72f);
PAD_CONST(igd, 534158, 53417C, 0x4) INIT({0xA6, 0x9B, 0xC4, 0x41});
ALIAS(kGridMin, kNearPad, 0);
DATA(540834) int gRicochetTime INIT(-1);
PAD(igd, 54083C, 54083C, 0xBC) INIT({0x70, 0x08, 0x54, 0x00, 0x64, 0x08, 0x54, 0x00, 0x58, 0x08, 0x54, 0x00, 0x4C, 0x08, 0x54, 0x00, 0x72, 0x69, 0x63, 0x63, 0x6F, 0x63, 0x68, 0x65, 0x74, 0x5F, 0x34, 0x00, 0x72, 0x69, 0x63, 0x63, 0x6F, 0x63, 0x68, 0x65, 0x74, 0x5F, 0x33, 0x00, 0x72, 0x69, 0x63, 0x63, 0x6F, 0x63, 0x68, 0x65, 0x74, 0x5F, 0x32, 0x00, 0x72, 0x69, 0x63, 0x63, 0x6F, 0x63, 0x68, 0x65, 0x74, 0x5F, 0x31, 0x00, 0x54, 0x41, 0x53, 0x4B, 0x54, 0x59, 0x50, 0x45, 0x5F, 0x47, 0x55, 0x4E, 0x53, 0x48, 0x4F, 0x54, 0x00, 0x00, 0x00, 0x00, 0x47, 0x75, 0x6E, 0x53, 0x68, 0x6F, 0x74, 0x00, 0x41, 0x6D, 0x6D, 0x6F, 0x54, 0x79, 0x70, 0x65, 0x5F, 0x4F, 0x70, 0x65, 0x6E, 0x28, 0x29, 0x3A, 0x20, 0x43, 0x6F, 0x75, 0x6C, 0x64, 0x6E, 0x27, 0x74, 0x20, 0x6C, 0x6F, 0x61, 0x64, 0x20, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x3A, 0x20, 0x25, 0x73, 0x00, 0x00, 0x00, 0x61, 0x6D, 0x6D, 0x6F, 0x2E, 0x71, 0x73, 0x63, 0x00, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x41, 0x6D, 0x6D, 0x6F, 0x54, 0x79, 0x70, 0x65, 0x00, 0x00, 0x4C, 0x4F, 0x43, 0x41, 0x4C, 0x3A, 0x61, 0x6D, 0x6D, 0x6F, 0x00, 0x00, 0x41, 0x4D, 0x4D, 0x4F, 0x5F, 0x49, 0x44, 0x5F, 0x2E, 0x2A});
ALIAS(gRicochetSounds, pad_54083C, 0);
BSS(5BE3CC) uint8_t gGunShotEvent;
PAD(igb, 5BE3CC, 5BE3CD, 0x1);
BSS(5BE3CC) uint16_t gGunShotType;
PAD(igb, 5BE3CC, 5BE3D0, 0x4);
BSS(5BE3CC) int gRicochetDelay;
PAD(igb, A982E0, A982E0, 0x4200);
ALIAS(gRenderHandlers, pad_A982E0, 0);
