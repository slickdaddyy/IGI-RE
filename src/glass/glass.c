#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "glass/glass.h"
#include "hook.h"

FN_EXTERN(RocketPod_GetArgSize);
FN_EXTERN(Task_FalseHandler);
FN_EXTERN(Task_Nothing);

ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (void *blast, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4FC860, uint8_t, PhysicsObj_GetHitEvent, (void));
ORIG(0x44BD70, void, GlassShatter_SpawnFrom, (void **model, Matrix *m, Vec3d *pos, Vec3d *from, float force,
                                               float scale));

/* "Glass" (0x148), a pane that shatters once it took enough damage */
typedef struct Glass {
    RigidDynCubeObj o;
    MagicPart m;        /* 0xF0 */
    float damage;       /* 0x13C */
    float strength;     /* 0x140 */
    char broken;        /* 0x144 */
    char unk145[3];
} Glass;

_Static_assert(sizeof(Glass) == 0x148, "Glass");

/* a bullet hit (Game event 9) */
typedef struct HitArg {
    void *hit;
    float damage;
    Vec3f dir;
} HitArg;

/* an explosion (Game event 11) */
typedef struct BlastArg {
    int unk0;
    Vec3d *pos;
    int unk8;
    int unkC;
    float force;
} BlastArg;

/* something ran into the glass (event from 0x4FC860) */
typedef struct TouchArg {
    RigidDynCubeObj *by;
    int unk4;
    int handled;
} TouchArg;

extern uint16_t gGlassType;

extern float kThreeQuarters;
extern float kHitForce;  /* 0.007 */

#define Task_Nothing (FN(0x4D3710, Task_Nothing))
#define sub_427D70 (FN(0x427D70, RocketPod_GetArgSize))
#define sub_45F920 (FN(0x45F920, Task_FalseHandler))

void GlassShatter_Spawn(void **model, Vec3f *dir, Vec3d *pos, Matrix *m, void *hit);

void Glass_Create(Glass *g, MagicArg *arg)
{
    void **model;
    float scale, radius;

    QTaskList_Add(Game_GetTaskList3(), g);
    model = VirModel_Find(arg->model);
    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    memcpy(g->m.offset, &arg->offset, sizeof(g->m.offset));
    g->m.local = arg->local;
    RigidDynCubeObj_Init(arg->world, g, model);
    scale = obj_scale(&g->o);
    radius = VirModel_GetRadius(g->o.model) * scale;
    DynCube_Add(arg->world, g, &arg->pos, radius, 2);
    g->o.matrix = arg->matrix;
    g->damage = 0;
    g->strength = 0.75f;
    g->strength = Random_Float(Game_GetRandom()) * kThreeQuarters + g->strength;
    g->broken = 0;
    g->m.data = arg->data;
    g->m.owner = arg->owner;
    RigidDynCubeObj_CreateMagicObjs(g, g, arg->owner);
}
HOOK(0x4497F0, Glass_Create);

void Glass_Delete(Glass *g)
{
    QTaskList_Remove(Game_GetTaskList3(), g);
    DynCube_Remove((int)g->o.world, g);
}
HOOK(0x449950, Glass_Delete);

void Glass_Run(Glass *g);
void Glass_Hit(Glass *g, HitArg *arg);
void Glass_Blast(Glass *g, BlastArg *arg);
void Glass_Touch(Glass *g, TouchArg *arg);

void Glass_Open(void)
{
    gGlassType = TaskType_New("Glass", sizeof(Glass), MagicObj_GetType(), 0);
    TaskType_SetHandler(gGlassType, 0, Glass_Create);
    TaskType_SetHandler2(gGlassType, 1, Glass_Run);
    TaskType_SetHandler2(gGlassType, 2, Glass_Delete);
    TaskType_SetHandler(gGlassType, RigidDynCubeObj_GetEvent0(), Task_Nothing);
    TaskType_SetHandler(gGlassType, RigidDynCubeObj_GetEvent1(), sub_427D70);
    TaskType_SetHandler(gGlassType, Game_GetEvent9(), Glass_Hit);
    TaskType_SetHandler(gGlassType, Game_GetEvent11(), Glass_Blast);
    TaskType_SetHandler(gGlassType, PhysicsObj_GetHitEvent(), Glass_Touch);
    TaskType_SetHandler(gGlassType, AIDetection_GetEvent0(), sub_45F920);
    Script_RegisterConstant("TASKTYPE_GLASS", gGlassType);
}
HOOK(0x449970, Glass_Open);

void Glass_Run(Glass *g)
{
    EACH_CHILD(&g->o.task, 1);
    if (g->broken)
        Task_Delete(g);
}
HOOK(0x449A80, Glass_Run);

void Glass_Hit(Glass *g, HitArg *arg)
{
    long double d = arg->damage + g->damage;
    Vec3f dir;

    g->damage = d;
    if (d >= g->strength && !g->broken) {
        g->broken = 1;
        dir.x = arg->dir.x * kHitForce;
        dir.y = arg->dir.y * kHitForce;
        dir.z = arg->dir.z * kHitForce;
        GlassShatter_Spawn(g->o.model, &dir, &g->o.pos, &g->o.matrix, arg->hit);
    }
}
HOOK(0x449B00, Glass_Hit);

/* damage from an explosion, measured at the model's centre */
void Glass_Blast(Glass *g, BlastArg *arg)
{
    float *m = g->o.matrix.m;
    float *c = (float *)(*(char **)(*(char **)((char *)g->o.model[0] + 0xC) + 0xC) + 0x30);
    long double x, f;
    float y, z;
    Vec3d p;

    x = (m[2] * c[2] + m[1] * c[1]) + c[0] * m[0];
    y = (m[5] * c[2] + m[4] * c[1]) + m[3] * c[0];
    z = (m[8] * c[2] + m[7] * c[1]) + m[6] * c[0];
    p.x = x + g->o.pos.x;
    p.y = y + g->o.pos.y;
    p.z = z + g->o.pos.z;
    f = Game_BlastFalloff(arg, g, &p, c[3], 0);
    g->damage = f + g->damage;
    if (fne(f, kZero) && !g->broken) {
        g->broken = 1;
        /* 409.6 */
        GlassShatter_SpawnFrom(g->o.model, &g->o.matrix, &g->o.pos, arg->pos, arg->force, 0x1.99999ap+8f);
    }
}
HOOK(0x449BA0, Glass_Blast);

void Glass_Touch(Glass *g, TouchArg *arg)
{
    arg->handled = 1;
    if (!g->broken) {
        g->damage = g->strength;
        g->broken = 1;
        /* 204.8 */
        GlassShatter_SpawnFrom(g->o.model, &g->o.matrix, &g->o.pos, &arg->by->pos, 12288.0f, 0x1.99999ap+7f);
    }
}
HOOK(0x449CB0, Glass_Touch);

void Glass_Close(void)
{
    TaskType_Free(gGlassType);
}
HOOK(0x449D10, Glass_Close);

uint16_t Glass_GetType(void)
{
    return gGlassType;
}
HOOK(0x449D20, Glass_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53351C) float kThreeQuarters INIT(0.75f);
ALIAS(kBrakeWing, kThreeQuarters, 0);
DATA(5338B8) float kHitForce INIT(0.007f);
BSS(57C220) uint16_t gGlassType;
PAD(igb, 57C220, 57C222, 0x2);
