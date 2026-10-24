/* M203: the rifle grenade that flies until it hits something. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "guncasing/guncasing.h"

FN_EXTERN(Game_CollFilterSkip3);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x4168F0, void *, Game_GetCutsceneList, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4169D0, char *, Game_AddSprite, (const char *name));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x480FE0, void, Explosion_Spawn, (Vec3d *pos, int a, int b, float c, float d, float e, float f, int g, int h,
                                       CollHit *hit, const char *sprite, const char *sound, int j, int k, float l,
                                       int m));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *hits, Vec3d *from, Vec3d *to, int count, uint8_t event,
                                  CollFilter *filter, unsigned flags));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4FDF00, void *, Particle2D_New, (void *root, void *world, void *sprite, Vec3f *dir, Vec3f *color, float *size,
                                   float b, int flags, int frames, float e, int f, int count, void *callback));
ORIG(0x4FE120, void, Particle2D_Add, (void *smoke, Vec3d *pos, Vec3f *dir, float size, float a, float b, float c, int d,
                                  int e));

extern char *gM203Sprite;
extern uint16_t gM203Type;

extern float kM203Drag;    /* 0.995 */
extern float kM203Gravity;
extern double kPuffRise;
extern double kPuffRiseMin;
extern float kKmh;
extern float kPerFrameHour;
extern double kPuffSize;
extern float kHalf;
extern float kPuffSpin;
extern float kPuffSpinBase;

#define Game_NullHandler (FN(0x416A90, Game_CollFilterSkip3))

void M203_CreateHandler(M203 *g);
void M203_RunHandler(M203 *g);
void M203_DeleteHandler(M203 *g);

void M203_Open(void)
{
    uint16_t t = TaskType_New("M203", 0x128, RigidDynCubeObj_GetType(), 0, 0);

    gM203Type = t;
    TaskType_SetHandler(t, 0, M203_CreateHandler);
    TaskType_SetHandler2(gM203Type, 1, M203_RunHandler);
    TaskType_SetHandler2(gM203Type, 2, M203_DeleteHandler);
    TaskType_SetHandler(gM203Type, Game_GetEvent7(), Game_NullHandler);
    Script_RegisterConstant("TASKTYPE_M203", gM203Type);
    gM203Sprite = Game_AddSprite("smoke1.spr");
}
HOOK(0x476110, M203_Open);

void M203_CreateHandler(M203 *g)
{
    Vec3d zero = {0, 0, 0};
    Vec3f dir, color;
    float size[2], s, radius;
    void *world;

    QTaskList_Add(Game_GetCutsceneList(), g);
    RigidDynCubeObj_Init(Game_GetWorld(), g, NULL);
    s = max_scale(&g->o);
    radius = VirModel_GetRadius(g->o.model) * s;
    DynCube_Add(Game_GetWorld(), g, &zero, radius, 2);
    g->owner = NULL;
    g->other = NULL;
    dir.x = dir.y = dir.z = 0;
    color.x = color.y = color.z = 127.0f;
    size[0] = size[1] = 614.4f;
    world = Game_GetWorld();
    g->smoke = Particle2D_New(Level_GetDynamic(), world, gM203Sprite + 0x18, &dir, &color, size, 819200.0f,
                         0x1000008, 0, 0.1f, 0, 0x14, 0);
}
HOOK(0x4761B0, M203_CreateHandler);

void M203_RunHandler(M203 *g)
{
    Vec3d from, to, pos;
    CollFilter filter;
    CollHit hit;
    Vec3f dir;
    long double vx, r;
    float vy, vz, size, spin, s, radius;
    uint8_t ev;

    vx = g->vel.x * kM203Drag;
    g->vel.x = vx;
    from = g->pos;
    vy = g->vel.y * kM203Drag;
    g->vel.y = vy;
    vz = (g->vel.z - kM203Gravity) * kM203Drag;
    g->vel.z = vz;
    filter.task = &g->o.task;
    g->pos.x = vx + g->pos.x;
    g->pos.y = vy + g->pos.y;
    g->pos.z = vz + g->pos.z;
    to = g->pos;
    filter.owner = g->owner ? *g->owner : NULL;
    filter.other = g->other ? *g->other : NULL;
    filter.unk10 = 1.0f;
    ev = Game_GetEvent7();
    Coll_Lines(Game_GetWorld(), &hit, &from, &to, 1, ev, &filter, -1);
    if (hit.found) {
        Explosion_Spawn(&hit.pos, 1, 1, g->power, g->damage * kMeter, 0, 4096.0f, 3, 1, &hit, "1009_01_1",
                        "explo_proxy", 0x15, 2, 102400.0f, 0);
        Task_Delete(g);
        return;
    }
    s = max_scale(&g->o);
    radius = VirModel_GetRadius(g->o.model) * s;
    DynCube_Move(Game_GetWorld(), g, &g->pos, radius);
    pos = g->pos;
    dir.x = 0;
    dir.y = 0;
    r = Random_Float(Game_GetRandom());
    dir.z = (r * kPuffRise + kPuffRiseMin) * kKmh * kPerFrameHour;
    r = Random_Float(Game_GetRandom());
    size = (r + kOneD) * kPuffSize;
    r = Random_Float(Game_GetRandom()) - kHalf;
    spin = (r + r) * kPuffSpin + kPuffSpinBase;
    Particle2D_Add(g->smoke, &pos, &dir, size, spin, 0, 1.0f, 7, 7);
}
HOOK(0x476340, M203_RunHandler);

void M203_DeleteHandler(M203 *g)
{
    QTaskList_Remove(Game_GetCutsceneList(), g);
    DynCube_Remove(Game_GetWorld(), g);
    if (g->owner) {
        TaskRef_Remove(g->owner);
        g->owner = NULL;
    }
    if (g->other) {
        TaskRef_Remove(g->other);
        g->other = NULL;
    }
}
HOOK(0x476600, M203_DeleteHandler);

void M203_Close(void)
{
    TaskType_Free(gM203Type);
}
HOOK(0x476660, M203_Close);

M203 *M203_New(void *parent, Task *owner, Task *other, const Vec3d *pos, const Matrix *matrix, const Vec3f *vel,
               const char *model, float damage, float power)
{
    M203 *g;

    Mem_PushMode(1);
    g = (M203 *)Task_New(parent, gM203Type, 0);
    if (owner)
        g->owner = TaskRef_Add(owner);
    if (other)
        g->other = TaskRef_Add(other);
    g->pos = *pos;
    g->o.matrix = *matrix;
    g->vel = *vel;
    RigidDynCubeObj_SetModel(g, VirModel_Find(model));
    g->damage = damage;
    g->power = power;
    Mem_PopMode();
    return g;
}
HOOK(0x476670, M203_New);
