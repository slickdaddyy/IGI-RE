/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "orig.h"
#include "vehbase/vehbase.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (Task *parts, void *obj, void *owner));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *owner));
ORIG(0x4EE0E0, uint8_t, PhysicsObjType_GetParseEvent, (void));
ORIG(0x4EE0F0, uint8_t, PhysicsObjType_GetSizeEvent, (void));
ORIG(0x4FB5E0, uint16_t, PhysicsObj_GetType, (void));
ORIG(0x4FB5F0, void, PhysicsObj_Init, (void *obj));
ORIG(0x4FB980, void, PhysicsObj_Free, (void *obj));
ORIG(0x5031F0, void, Vehicle_ParseConfig, (void *cfg, int *next, void *args));
ORIG(0x5042B0, char, AreaActivate_Contains, (void *criteria, Vec3d *pos));
ORIG(0x504A30, void *, AreaActivate_AddCriteria, (const char *name, void *fn));
ORIG(0x504A90, void, AreaActivate_RemoveCriteria, (void *criteria));

extern uint16_t gVehicleType;
extern uint8_t gVehicleExplodeEvent;
extern uint8_t gVehicleEvent203;
extern void *gVehicleCriteria;

/* every vehicle, kept by 0x4F4870 */
typedef struct VehicleList {
    int count;
    int unk4;
    Vehicle *items[];
} VehicleList;
extern VehicleList *gVehicles;

/* PhysType arguments event */
typedef struct ParseMsg {
    void *cfg;
    void *args;
    int next;
} ParseMsg;

/* criteria callback: counts matching vehicles */
typedef struct CountMsg {
    void *criteria;
    int count;
} CountMsg;

uint8_t Vehicle_GetSaveEvent(void);

/* Vehicle is abstract: subtypes must set these three */
void Vehicle_CreateHandler(Vehicle *v)
{
    Error_Show("Vehicle create handler must be overloaded");
    for (;;)
        ;
}
HOOK(0x4F4160, Vehicle_CreateHandler);

void Vehicle_DeleteHandler(Vehicle *v)
{
    Error_Show("Vehicle delete handler must be overloaded");
    for (;;)
        ;
}
HOOK(0x4F4170, Vehicle_DeleteHandler);

void Vehicle_RunHandler(Vehicle *v)
{
    Error_Show("Vehicle run handler must be overloaded");
    for (;;)
        ;
}
HOOK(0x4F4180, Vehicle_RunHandler);

void Vehicle_GetInfo(Vehicle *v, ObjInfo *out);
void Vehicle_CollFilter(Vehicle *v, void **msg);
void Vehicle_SaveState(Vehicle *v, void **msg);
void Vehicle_ParseArgs(void *task, ParseMsg *msg);
void Vehicle_GetConfigSize(void *task, int *out);
void Vehicle_Activate(Vehicle *v, void *msg);
void Vehicle_Deactivate(Vehicle *v, void *msg);
void Vehicle_CountInCriteria(CountMsg *msg);

void Vehicle_Open(void)
{
    gVehicleType = TaskType_New("Vehicle", 0x748, PhysicsObj_GetType(), 0);
    TaskType_SetHandler(gVehicleType, 0, Vehicle_CreateHandler);
    TaskType_SetHandler2(gVehicleType, 2, Vehicle_DeleteHandler);
    TaskType_SetHandler2(gVehicleType, 1, Vehicle_RunHandler);
    TaskType_SetHandler(gVehicleType, 7, Vehicle_GetInfo);
    TaskType_SetHandler(gVehicleType, 12, Vehicle_CollFilter);
    TaskType_SetHandler(gVehicleType, Vehicle_GetSaveEvent(), Vehicle_SaveState);
    TaskType_SetHandler(gVehicleType, PhysicsObjType_GetParseEvent(), Vehicle_ParseArgs);
    TaskType_SetHandler(gVehicleType, PhysicsObjType_GetSizeEvent(), Vehicle_GetConfigSize);
    TaskType_SetHandler(gVehicleType, World_GetEvent0(), Vehicle_Activate);
    TaskType_SetHandler(gVehicleType, World_GetEvent1(), Vehicle_Deactivate);
    gVehicleCriteria = AreaActivate_AddCriteria("CRITERIA_VEHICLE", Vehicle_CountInCriteria);
    gVehicleExplodeEvent = TaskEvent_Alloc(0);
    gVehicleEvent203 = TaskEvent_Alloc(0);
}
HOOK(0x4F4190, Vehicle_Open);

void Vehicle_GetInfo(Vehicle *v, ObjInfo *out)
{
    float s;

    out->pos = v->obj.pos;
    out->velocity = v->velocity;
    out->matrix = v->obj.matrix;
    s = max_scale(&v->obj);
    out->unk50 = 0;
    out->radius = VirModel_GetRadius(v->obj.model) * s;
}
HOOK(0x4F42D0, Vehicle_GetInfo);

/* slot 12: msg is {char result, Task *task} */
void Vehicle_CollFilter(Vehicle *v, void **msg)
{
    *(char *)msg = msg[1] != v;
}
HOOK(0x4F4390, Vehicle_CollFilter);

void Vehicle_ParseArgs(void *task, ParseMsg *msg)
{
    Vehicle_ParseConfig(msg->cfg, &msg->next, msg->args);
}
HOOK(0x4F43B0, Vehicle_ParseArgs);

void Vehicle_GetConfigSize(void *task, int *out)
{
    *out = 0x118;
}
HOOK(0x4F43D0, Vehicle_GetConfigSize);

void Vehicle_Activate(Vehicle *v, void *msg)
{
    v->active = 1;
    HANDLER(World_GetEvent0(), RigidDynCubeObj_GetType())(v, msg);
    if (v->obj.attached)
        BROADCAST(v->obj.attached, World_GetEvent0(), msg);
}
HOOK(0x4F43E0, Vehicle_Activate);

void Vehicle_Deactivate(Vehicle *v, void *msg)
{
    v->active = 0;
    if (v->obj.attached)
        BROADCAST(v->obj.attached, World_GetEvent1(), msg);
    HANDLER(World_GetEvent1(), RigidDynCubeObj_GetType())(v, msg);
}
HOOK(0x4F44A0, Vehicle_Deactivate);

void Vehicle_CountInCriteria(CountMsg *msg)
{
    int i, n;

    if (!gVehicles || (n = gVehicles->count) <= 0)
        return;
    for (i = 0; i < n; i++)
        if (AreaActivate_Contains(msg->criteria, &gVehicles->items[i]->obj.pos))
            msg->count++;
}
HOOK(0x4F4560, Vehicle_CountInCriteria);

void Vehicle_Close(void)
{
    TaskType_Free(gVehicleType);
    AreaActivate_RemoveCriteria(gVehicleCriteria);
    TaskEvent_Free(gVehicleExplodeEvent);
    TaskEvent_Free(gVehicleEvent203);
}
HOOK(0x4F45B0, Vehicle_Close);

uint16_t Vehicle_GetType(void)
{
    return gVehicleType;
}
HOOK(0x4F45F0, Vehicle_GetType);

void Vehicle_ResetControls(Vehicle *v)
{
    memset(v->turn, 0, sizeof(v->turn));
    v->speed = 0;
    memset(v->steer, 0, sizeof(v->steer));
    v->target_speed = 0;
    v->brake = 0;
}
HOOK(0x4F4600, Vehicle_ResetControls);

void Vehicle_DeleteParts(Vehicle *v);

void Vehicle_Free(Vehicle *v)
{
    Vehicle_DeleteParts(v);
    PhysicsObj_Free(v);
}
HOOK(0x4F4640, Vehicle_Free);

void Vehicle_RunParts(Vehicle *v)
{
    Task *parts = v->obj.attached;

    if (parts)
        HANDLER(1, parts->type)(parts);
}
HOOK(0x4F4660, Vehicle_RunParts);

void Vehicle_NewParts(Vehicle *v);

void Vehicle_Init(Vehicle *v)
{
    PhysicsObj_Init(v);
    Vehicle_NewParts(v);
    Vehicle_ResetControls(v);
    v->active = 1;
}
HOOK(0x4F4680, Vehicle_Init);

/* msg is {VehicleState *state, unused, Vec3d *pos} */
void Vehicle_SaveState(Vehicle *v, void **msg)
{
    VehicleState *s = msg[0];

    s->velocity = v->velocity;
    s->pos = v->obj.pos;
    *(Vec3d *)msg[2] = v->obj.pos;
    s->matrix = v->obj.matrix;
    memcpy(&s->speed, &v->speed, 8 * sizeof(float));
}
HOOK(0x4F46E0, Vehicle_SaveState);

void Vehicle_LoadState(Vehicle *v, void **msg)
{
    VehicleState *s = msg[0];

    v->velocity = s->velocity;
    v->obj.matrix = s->matrix;
    memcpy(&v->speed, &s->speed, 8 * sizeof(float));
}
HOOK(0x4F4780, Vehicle_LoadState);

void Vehicle_NewParts(Vehicle *v)
{
    v->obj.attached = MagicObjContainer_New(NULL, v);
    RigidDynCubeObj_CreateMagicObjs(v->obj.attached, v, v);
}
HOOK(0x4F4800, Vehicle_NewParts);

void Vehicle_DeleteParts(Vehicle *v)
{
    if (v->obj.attached) {
        Task_Delete(v->obj.attached);
        v->obj.attached = NULL;
    }
}
HOOK(0x4F4820, Vehicle_DeleteParts);

uint8_t Vehicle_GetExplodeEvent(void)
{
    return gVehicleExplodeEvent;
}
HOOK(0x4F4850, Vehicle_GetExplodeEvent);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E200) uint16_t gVehicleType INIT(385);
DATA(54E200) uint8_t gVehicleExplodeEvent INIT(255);
DATA(54E200) uint8_t gVehicleEvent203 INIT(255);
ALIAS(gVehicleClearHitEvent, gVehicleEvent203, 0);
BSS(A76C40) VehicleList *gVehicles;
PAD(igb, A76C40, A76C44, 0x4);
BSS(A76C40) void *gVehicleCriteria;
