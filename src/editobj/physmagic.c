/* PhysicsMagicObj: a PhysicsObj that follows its parent at a fixed offset, like MagicObj. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "editobj/editobj.h"

FN_EXTERN(MagicObj_RunHandler);

/* "PhysicsMagicObj" (0x768), a PhysicsObj (0x720) */
typedef struct PhysicsMagicObj {
    Obj obj;
    char unkF0[0x720 - 0xF0];
    Vec3d offset;
    Matrix local;
    int unk760;
    int unk764;
} PhysicsMagicObj;

/* the create argument, shared with MagicObj */
typedef struct MagicCreateArg {
    const char *model;
    void *world;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    int unk88;
} MagicCreateArg;

/* slot 12: may this object hit `other` */
typedef struct HitCheckArg {
    char result;
    char pad[3];
    void *other;
} HitCheckArg;

_Static_assert(offsetof(PhysicsMagicObj, local) == 0x738, "local");
_Static_assert(sizeof(PhysicsMagicObj) == 0x768, "PhysicsMagicObj");
_Static_assert(offsetof(MagicCreateArg, unk88) == 0x88, "MagicCreateArg");

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, int c));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D0950, long double, VirModel_GetRadius2, (void **model, int unk));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4EE140, void *, PhysicsObjType_Find, (const char *name));
ORIG(0x4FB5E0, uint16_t, PhysicsObj_GetType, (void));
ORIG(0x4FB5F0, void, PhysicsObj_Init, (void *obj));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *obj, void *phys));

#define MAGICOBJ_RUN (FN(0x4D9970, MagicObj_RunHandler))

extern uint16_t gPhysicsMagicObjType;

void PhysicsMagicObj_Create(PhysicsMagicObj *o, MagicCreateArg *arg);
void PhysicsMagicObj_Move(PhysicsMagicObj *o, MoveArg *arg);
void PhysicsMagicObj_Slot12(Obj *o, HitCheckArg *arg);

void PhysicsMagicObj_Open(void)
{
    uint16_t t = TaskType_New("PhysicsMagicObj", sizeof(PhysicsMagicObj), PhysicsObj_GetType(), 0, 0);

    gPhysicsMagicObjType = t;
    TaskType_SetHandler2(t, 1, MAGICOBJ_RUN);
    TaskType_SetHandler(gPhysicsMagicObjType, 0, PhysicsMagicObj_Create);
    TaskType_SetHandler(gPhysicsMagicObjType, MagicObj_GetMoveEvent(), PhysicsMagicObj_Move);
    TaskType_SetHandler(gPhysicsMagicObjType, 12, PhysicsMagicObj_Slot12);
    Script_RegisterConstant("TASKTYPE_PHYSICSMAGICOBJ", gPhysicsMagicObjType);
}
HOOK(0x503D60, PhysicsMagicObj_Open);

void PhysicsMagicObj_Create(PhysicsMagicObj *o, MagicCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);
    float s;

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    RigidDynCubeObj_Init(arg->world, o, model);
    s = max_scale(&o->obj);
    DynCube_Add(arg->world, o, &arg->pos, VirModel_GetRadius2(o->obj.model, 2) * s, 2);
    o->obj.matrix = arg->matrix;
    o->offset = arg->offset;
    o->local = arg->local;
    o->unk760 = arg->unk88;
    PhysicsObj_Init(o);
    PhysicsObj_SetPhysType(o, PhysicsObjType_Find(arg->model));
    RigidDynCubeObj_CreateMagicObjs(o, o, arg->unk88);
}
HOOK(0x503DF0, PhysicsMagicObj_Create);

/* The parent moved: place the object at its offset and pass the move on */
void PhysicsMagicObj_Move(PhysicsMagicObj *o, MoveArg *arg)
{
    const float *m = arg->matrix->m;
    const float *l = o->local.m;
    double d0, d1, d2;
    Vec3d pos;
    Matrix r;
    MoveArg next;
    float s;

    d0 = m[2] * o->offset.z + m[1] * o->offset.y + m[0] * o->offset.x;
    d1 = m[5] * o->offset.z + m[4] * o->offset.y + m[3] * o->offset.x;
    d2 = m[8] * o->offset.z + m[7] * o->offset.y + m[6] * o->offset.x;
    pos.x = d0 + arg->pos->x;
    pos.y = d1 + arg->pos->y;
    pos.z = d2 + arg->pos->z;
    s = max_scale(&o->obj);
    DynCube_Move(o->obj.world, o, &pos, VirModel_GetRadius(o->obj.model) * s);
    m = arg->matrix->m;
    r.m[0] = l[3] * m[1] + m[0] * l[0] + m[2] * l[6];
    r.m[1] = l[4] * m[1] + m[0] * l[1] + m[2] * l[7];
    r.m[2] = l[5] * m[1] + m[0] * l[2] + m[2] * l[8];
    r.m[3] = l[3] * m[4] + m[5] * l[6] + m[3] * l[0];
    r.m[4] = l[4] * m[4] + m[5] * l[7] + m[3] * l[1];
    r.m[5] = l[5] * m[4] + m[5] * l[8] + m[3] * l[2];
    r.m[6] = l[3] * m[7] + m[8] * l[6] + m[6] * l[0];
    r.m[7] = l[4] * m[7] + m[8] * l[7] + m[6] * l[1];
    r.m[8] = l[5] * m[7] + m[8] * l[8] + m[6] * l[2];
    r.count = arg->matrix->count + o->local.count + 1;
    o->obj.matrix = r;
    if (!o->obj.task.head->next || !o->obj.task.head)
        return;
    next.pos = &o->obj.pos;
    next.matrix = &o->obj.matrix;
    CALL_CHILDREN(&o->obj.task, MagicObj_GetMoveEvent(), &next);
}
HOOK(0x503F20, PhysicsMagicObj_Move);

void PhysicsMagicObj_Slot12(Obj *o, HitCheckArg *arg)
{
    if (o != arg->other && *(void **)((char *)o + 0x130) != arg->other) {
        arg->result = 1;
        return;
    }
    arg->result = 0;
}
HOOK(0x504250, PhysicsMagicObj_Slot12);

void PhysicsMagicObj_Close(void)
{
    TaskType_Free(gPhysicsMagicObjType);
    gPhysicsMagicObjType = 0x181;
}
HOOK(0x504280, PhysicsMagicObj_Close);

uint16_t PhysicsMagicObj_GetType(void)
{
    return gPhysicsMagicObjType;
}
HOOK(0x5042A0, PhysicsMagicObj_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E66C) uint16_t gPhysicsMagicObjType INIT(385);
PAD(igd, 54E66C, 54E66E, 0x2);
