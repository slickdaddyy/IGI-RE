/* MagicObj: a RigidDynCubeObj that follows its parent at a fixed offset. */
#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "igi.h"
#include "haze/haze.h"

FN_EXTERN(PhysicsMagicObj_Slot12);
FN_EXTERN(Task_Nothing);
FN_EXTERN(VehiclePart_Delete);

ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType_, (int a, int b));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void *unused));
ORIG(0x4C5700, uint8_t, RigidDynCubeObj_GetEvent3, (void *unused));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, int c));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D0950, long double, VirModel_GetRadius_, (void **model, int unk));

#define MAGIC_HANDLER_504250 (FN(0x504250, PhysicsMagicObj_Slot12))
#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))
#define VEHICLEPART_DELETE (FN(0x426860, VehiclePart_Delete))

extern uint16_t gMagicType;
extern uint8_t gMagicMoveEvent;
extern uint8_t gMagicEvent1;

void MagicObj_RunHandler(MagicObj *o);
void MagicObj_EnterHandler(MagicObj *o);
void MagicObj_CreateHandler(MagicObj *o, MagicCreateArg *arg);
void MagicObj_MoveHandler(MagicObj *o, MagicMoveArg *arg);

static float max_scale(const MagicObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

/* Runs every child */
void MagicObj_RunHandler(MagicObj *o)
{
    Task *n = o->task.head;

    if (!n->next)
        return;
    while (n) {
        void (*h)(void *);

        gTaskStack[gTaskStackTop++] = n->next && n->next->next ? n->next : NULL;
        h = (void (*)(void *))gTaskHandlers[1][((Task *)n)->type];
        if (h)
            h(n);
        n = gTaskStack[--gTaskStackTop];
    }
}
HOOK(0x4D9970, MagicObj_RunHandler);

void MagicObj_EnterHandler(MagicObj *o)
{
    gTaskHandlers[4][RigidDynCubeObj_GetType()](o, NULL);
}
HOOK(0x4D99E0, MagicObj_EnterHandler);

void MagicObj_Open(void)
{
    gMagicType = TaskType_New("MagicObj", sizeof(MagicObj), RigidDynCubeObj_GetType_(0, 0), 0, 0);
    gMagicMoveEvent = TaskEvent_Alloc(0);
    gMagicEvent1 = TaskEvent_Alloc(0);
    TaskType_SetHandler(gMagicType, 4, NULL);
    TaskType_SetHandler(gMagicType, RigidDynCubeObj_GetEvent2(MagicObj_EnterHandler), MagicObj_EnterHandler);
    TaskType_SetHandler2(gMagicType, 1, MagicObj_RunHandler);
    TaskType_SetHandler(gMagicType, 0, MagicObj_CreateHandler);
    TaskType_SetHandler(gMagicType, MagicObj_GetMoveEvent(), MagicObj_MoveHandler);
    TaskType_SetHandler(gMagicType, 12, MAGIC_HANDLER_504250);
    TaskType_SetHandler(gMagicType, RigidDynCubeObj_GetEvent3(TASK_NOTHING), TASK_NOTHING);
    TaskType_SetHandler2(gMagicType, 2, VEHICLEPART_DELETE);
    Script_RegisterConstant("TASKTYPE_MAGICOBJ", gMagicType);
}
HOOK(0x4D9A00, MagicObj_Open);

void MagicObj_CreateHandler(MagicObj *o, MagicCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);
    float s;

    if (!model)
        Warning_Show("VirModel \"%s\" not available", arg->model);
    RigidDynCubeObj_Init(arg->world, o, model);
    s = max_scale(o);
    DynCube_Add(arg->world, o, &arg->pos, VirModel_GetRadius_(o->model, o->unkD4) * s, o->unkD4);
    o->matrix = arg->matrix;
    o->offset = arg->offset;
    o->localMatrix = arg->local;
    o->unk130 = (int)arg->owner;
    if (model)
        RigidDynCubeObj_CreateMagicObjs(o, o, o->unk130);
}
HOOK(0x4D9B10, MagicObj_CreateHandler);

/* The parent moved: place the object at its offset and pass the move on */
void MagicObj_MoveHandler(MagicObj *o, MagicMoveArg *arg)
{
    const float *m = arg->matrix->m;
    const float *l = o->localMatrix.m;
    double d0, d1, d2;
    Vec3d pos;
    Matrix r;
    MagicMoveArg next;
    Task *n;
    float s;

    d0 = m[2] * o->offset.z + m[1] * o->offset.y + m[0] * o->offset.x;
    d1 = m[5] * o->offset.z + m[4] * o->offset.y + m[3] * o->offset.x;
    d2 = m[8] * o->offset.z + m[7] * o->offset.y + m[6] * o->offset.x;
    pos.x = d0 + arg->pos->x;
    pos.y = d1 + arg->pos->y;
    pos.z = d2 + arg->pos->z;
    s = max_scale(o);
    DynCube_Move(o->world, o, &pos, VirModel_GetRadius(o->model) * s);
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
    r.count = arg->matrix->count + o->localMatrix.count + 1;
    o->matrix = r;
    n = o->task.head;
    if (!n->next)
        return;
    next.pos = &o->pos;
    next.matrix = &o->matrix;
    while (n) {
        HzHandler h;

        gTaskStack[gTaskStackTop++] = n->next && n->next->next ? n->next : NULL;
        h = gTaskHandlers[MagicObj_GetMoveEvent()][((Task *)n)->type];
        if (h)
            h(n, &next);
        n = gTaskStack[--gTaskStackTop];
    }
}
HOOK(0x4D9C30, MagicObj_MoveHandler);

void MagicObj_Close(void)
{
    TaskType_Free(gMagicType);
    gMagicType = 0x181;
    TaskEvent_Free(gMagicMoveEvent);
    gMagicMoveEvent = 0xFF;
    TaskEvent_Free(gMagicEvent1);
    gMagicEvent1 = 0xFF;
}
HOOK(0x4D9F60, MagicObj_Close);

uint16_t MagicObj_GetType(void)
{
    return gMagicType;
}
HOOK(0x4D9FA0, MagicObj_GetType);

uint8_t MagicObj_GetMoveEvent(void)
{
    return gMagicMoveEvent;
}
HOOK(0x4D9FB0, MagicObj_GetMoveEvent);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(548648) uint16_t gMagicType INIT(385);
DATA(548648) uint8_t gMagicMoveEvent INIT(255);
DATA(548648) uint8_t gMagicEvent1 INIT(255);
