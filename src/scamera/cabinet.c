#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "scamera/scamera.h"

FN_EXTERN(Cabinet_AddSymbols);
FN_EXTERN(Cabinet_CheckUse);
FN_EXTERN(Cabinet_Delete);
FN_EXTERN(Cabinet_GetParams);
FN_EXTERN(Cabinet_ParamsChanged);
FN_EXTERN(Cabinet_RemoveSymbols);
FN_EXTERN(Cabinet_Run);
FN_EXTERN(Cabinet_Use);
FN_EXTERN(RocketPod_GetArgSize);
FN_EXTERN(Task_Nothing);
FN_EXTERN(VehiclePart_Delete);
FN_EXTERN(Cabinet_DrawIcon);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4DCCB0, void, Sector_AddObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E0830, uint16_t, SectorInteriorObject_GetType, (void));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1210, uint8_t, Level_GetEventDA8, (void));
ORIG(0x4F1220, uint8_t, Level_GetEventDAC, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

extern void *gCurrentWorld;
extern uint16_t gDrawerType;
extern void *gCabinetParams;
extern uint16_t gCabinetType;

/* handlers that live outside this folder */
#define Cabinet_Run (FN(0x444590, Cabinet_Run))
#define Cabinet_Delete (FN(0x444660, Cabinet_Delete))
#define Cabinet_GetParams (FN(0x4446D0, Cabinet_GetParams))
#define Cabinet_ParamsChanged (FN(0x4446E0, Cabinet_ParamsChanged))
#define Cabinet_CheckUse (FN(0x444860, Cabinet_CheckUse))
#define RocketPod_GetArgSize (FN(0x427D70, RocketPod_GetArgSize)) /* writes 4 */
#define Cabinet_Use (FN(0x444940, Cabinet_Use))
#define Cabinet_DrawIcon (FN(0x4449A0, Cabinet_DrawIcon))
#define Task_Nothing (FN(0x4D3710, Task_Nothing))
#define Cabinet_AddSymbols (FN(0x4449C0, Cabinet_AddSymbols))
#define Cabinet_RemoveSymbols (FN(0x444A70, Cabinet_RemoveSymbols))
#define VehiclePart_Delete (FN(0x426860, VehiclePart_Delete))

extern float kDrawerOpenStep; /* 153.6 */
extern float kDrawerOpen; /* 1843.2 */
extern float kDrawerCloseStep; /* 102.4 */
extern float kZero;

void Cabinet_Create(Cabinet *c);
void Drawer_Create(Drawer *d, DrawerArg *arg);
void Drawer_Run(Drawer *d);

#define FIELD(name) ((void *)offsetof(Cabinet, name))

void Cabinet_Open(void)
{
    gCabinetType = TaskType_New("Cabinet", sizeof(Cabinet), SectorInteriorObject_GetType(), 0);
    TaskType_SetHandler(gCabinetType, 0, Cabinet_Create);
    TaskType_SetHandler2(gCabinetType, 1, Cabinet_Run);
    TaskType_SetHandler2(gCabinetType, 2, Cabinet_Delete);
    TaskType_SetHandler(gCabinetType, Param_GetTableEvent(), Cabinet_GetParams);
    TaskType_SetHandler(gCabinetType, Param_GetChangedEvent(), Cabinet_ParamsChanged);
    TaskType_SetHandler(gCabinetType, Game_GetEvent2(), Cabinet_CheckUse);
    TaskType_SetHandler(gCabinetType, Game_GetEvent3(), RocketPod_GetArgSize);
    TaskType_SetHandler(gCabinetType, Game_GetEvent4(), Cabinet_Use);
    TaskType_SetHandler(gCabinetType, Game_GetEvent6(), Cabinet_DrawIcon);
    TaskType_SetHandler(gCabinetType, Game_GetEvent9(), Task_Nothing);
    TaskType_SetHandler2(gCabinetType, Level_GetEventDA8(), Cabinet_AddSymbols);
    TaskType_SetHandler2(gCabinetType, Level_GetEventDAC(), Cabinet_RemoveSymbols);
    TaskType_SetHandler(gCabinetType, ComputerObject_GetEvent0(), Terminal_MapHandler);
    gCabinetParams = ParamTable_New(4);
    ParamTable_Add(gCabinetParams, NULL, FIELD(place), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gCabinetParams, NULL, FIELD(yaw), Param_Angle(), 7, NULL);
    ParamTable_Add(gCabinetParams, NULL, FIELD(model), Param_String16(), 7, NULL);
    ParamTable_Add(gCabinetParams, NULL, FIELD(searchTime), Param_Real32(), 7, NULL);
    gDrawerType = TaskType_New("Drawer", sizeof(Drawer), MagicObj_GetType(), 0);
    TaskType_SetHandler(gDrawerType, 0, Drawer_Create);
    TaskType_SetHandler2(gDrawerType, 1, Drawer_Run);
    TaskType_SetHandler2(gDrawerType, 2, VehiclePart_Delete);
    Script_RegisterConstant("TASKTYPE_DRAWER", gDrawerType);
}
HOOK(0x443ED0, Cabinet_Open);

void Drawer_Create(Drawer *d, DrawerArg *arg)
{
    void **model = VirModel_Find(arg->model);
    float scale;

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    RigidDynCubeObj_Init(arg->world, d, model);
    scale = sobj_scale(&d->o);
    DynCube_Add(arg->world, d, &arg->pos, VirModel_GetRadius(d->o.model) * scale, 1);
    d->o.matrix = arg->matrix;
    d->cabinet = arg->cabinet;
    d->base = arg->pos;
    d->offset.x = 0;
    d->offset.y = 0;
    d->offset.z = 0;
}
HOOK(0x444150, Drawer_Create);

/* slides along its y axis while the cabinet opens or closes */
void Drawer_Run(Drawer *d)
{
    Cabinet *c = d->cabinet;
    float *m = d->o.matrix.m;
    long double t, x;
    float y, z, scale;
    Vec3d p;

    if (c->opening) {
        t = d->offset.y + kDrawerOpenStep;
        d->offset.y = t;
        if (t > kDrawerOpen) {
            d->offset.y = kDrawerOpen;
            c->moving = 0;
        }
    }
    if (c->closing) {
        t = d->offset.y - kDrawerCloseStep;
        d->offset.y = t;
        if (t < kZero) {
            d->offset.y = 0;
            c->moving = 0;
        }
    }
    if (c->reset) {
        d->offset.y = 0;
        c->reset = 0;
        c->moving = 0;
    }
    x = m[2] * d->offset.z + m[1] * d->offset.y + m[0] * d->offset.x;
    y = m[5] * d->offset.z + m[4] * d->offset.y + m[3] * d->offset.x;
    z = m[8] * d->offset.z + m[7] * d->offset.y + m[6] * d->offset.x;
    p.x = x + d->base.x;
    p.y = y + d->base.y;
    p.z = z + d->base.z;
    scale = sobj_scale(&d->o);
    DynCube_Move(Game_GetWorld(), d, &p, VirModel_GetRadius(d->o.model) * scale);
}
HOOK(0x444260, Drawer_Run);

void Cabinet_Create(Cabinet *c)
{
    Vec3d zero = {0, 0, 0};
    float scale;

    QTaskList_Add(Game_GetTaskList0(), c);
    RigidDynCubeObj_Init(gCurrentWorld, c, NULL);
    scale = sobj_scale(&c->o);
    DynCube_Add(gCurrentWorld, c, &zero, VirModel_GetRadius(c->o.model) * scale, 1);
    Sector_AddObject(c->o.world, c, DynCubeObj_GetPosition(c));
    c->place.x = 0;
    c->place.y = 0;
    c->place.z = 0;
    c->yaw = 0;
    c->model[0] = 0;
    c->opening = 0;
    c->closing = 0;
    c->inUse = 0;
    c->lastInUse = 0;
    c->useTick = 0;
    c->searched = 0;
    c->registered = 0;
    c->searchTime = 4.0f;
    c->moving = 0;
}
HOOK(0x444420, Cabinet_Create);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533898) float kDrawerCloseStep INIT(102.4f);
DATA(533898) float kDrawerOpen INIT(1843.2f);
DATA(533898) float kDrawerOpenStep INIT(153.6f);
BSS(57C1A4) uint16_t gDrawerType;
PAD(igb, 57C1A4, 57C1A6, 0x2);
BSS(57C1A4) void *gCabinetParams;
BSS(57C1A4) uint16_t gCabinetType;
PAD(igb, 57C1A4, 57C1AE, 0x2);
