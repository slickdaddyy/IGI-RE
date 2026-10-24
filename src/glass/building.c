#pragma GCC optimize("excess-precision=standard")

#include "glass/glass.h"
#include "hook.h"

ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4DEE80, uint16_t, SectorObject_GetType, (void));
ORIG(0x4DEE90, void, SectorObject_SetModel, (void *o, const char *name));
ORIG(0x4DEEC0, void, SectorObject_Update, (void *o));
ORIG(0x4E0810, void, SectorObject_SetMatrix, (void *o, Matrix *m));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));

/* "Building" (0x1F0), a SectorObject (0x1A0 bytes) placed by the level */
typedef struct Building {
    Obj o;
    char unkF0[0xB0];
    Matrix matrix;      /* 0x1A0 */
    char model[16];     /* 0x1C8 */
    Vec3d place;        /* 0x1D8 */
} Building;

_Static_assert(offsetof(Building, model) == 0x1C8, "Building");
_Static_assert(sizeof(Building) == 0x1F0, "Building");

#define gBuildingType VAR(uint16_t, 0x57C200)
#define gBuildingParams VAR(void *, 0x57C204)

#define sub_416AB0 ((void *)0x416AB0)
#define Terminal_MapHandler ((void *)0x443DC0)

void Building_Create(Building *b, void *arg);
void Building_Delete(Building *b);
void Building_GetParamsHandler(Task *t, void **out);
void Building_ParamsChanged(Building *b);
void Building_WorldRemove(Building *b, int *arg);
void Building_WorldAdd(Building *b, int *arg);

#define FIELD(name) ((void *)offsetof(Building, name))

void Building_Open(void)
{
    gBuildingType = TaskType_New("Building", sizeof(Building), SectorObject_GetType(), 0);
    TaskType_SetHandler(gBuildingType, 0, Building_Create);
    TaskType_SetHandler2(gBuildingType, 2, Building_Delete);
    TaskType_SetHandler(gBuildingType, Param_GetTableEvent(), Building_GetParamsHandler);
    TaskType_SetHandler(gBuildingType, Param_GetChangedEvent(), Building_ParamsChanged);
    TaskType_SetHandler(gBuildingType, Game_GetEvent13(), sub_416AB0);
    TaskType_SetHandler(gBuildingType, ComputerObject_GetEvent0(), Terminal_MapHandler);
    TaskType_SetHandler(gBuildingType, World_GetEvent1(), Building_WorldRemove);
    TaskType_SetHandler(gBuildingType, World_GetEvent0(), Building_WorldAdd);
    gBuildingParams = ParamTable_New(3);
    ParamTable_Add(gBuildingParams, NULL, FIELD(place), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gBuildingParams, NULL, FIELD(matrix), Param_Real32x9(), 7, NULL);
    ParamTable_Add(gBuildingParams, NULL, FIELD(model), Param_String16(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_BUILDING", gBuildingType);
}
HOOK(0x4485E0, Building_Open);

void Building_Create(Building *b, void *arg)
{
    Vec3d zero = {0, 0, 0};

    HANDLER(0, SectorObject_GetType())(b, arg);
    DynCube_Add(Game_GetWorld(), b, &zero, 1.0f, 8);
    b->place.x = 0;
    b->place.y = 0;
    b->place.z = 0;
    b->model[0] = 0;
    b->matrix.m[0] = 1.0f;
    b->matrix.m[1] = 0;
    b->matrix.m[2] = 0;
    b->matrix.m[3] = 0;
    b->matrix.m[4] = 1.0f;
    b->matrix.m[5] = 0;
    b->matrix.m[6] = 0;
    b->matrix.m[7] = 0;
    b->matrix.m[8] = 1.0f;
    b->matrix.unk24 = 0;
}
HOOK(0x448760, Building_Create);

void Building_Delete(Building *b)
{
    DynCube_Remove(b->o.world, b);
    HANDLER(2, SectorObject_GetType())(b);
}
HOOK(0x448850, Building_Delete);

void Building_GetParamsHandler(Task *t, void **out)
{
    *out = gBuildingParams;
}
HOOK(0x448880, Building_GetParamsHandler);

void Building_ParamsChanged(Building *b)
{
    float scale, radius;

    SectorObject_SetModel(b, b->model);
    scale = obj_scale(&b->o);
    radius = VirModel_GetRadius(b->o.model) * scale;
    DynCube_Move(Game_GetWorld(), b, &b->place, radius);
    SectorObject_SetMatrix(b, &b->matrix);
    SectorObject_Update(b);
}
HOOK(0x448890, Building_ParamsChanged);

/* arg[1] is the world */
void Building_WorldAdd(Building *b, int *arg)
{
    float scale = obj_scale(&b->o);
    uint8_t slot;

    DynCube_Add(arg[1], b, &b->place, VirModel_GetRadius(b->o.model) * scale, 8);
    slot = World_GetEvent0();
    HANDLER(slot, SectorObject_GetType())(b, arg);
    EACH_CHILD(&b->o.task, World_GetEvent0(), arg);
}
HOOK(0x448950, Building_WorldAdd);

/* arg[0] is the world */
void Building_WorldRemove(Building *b, int *arg)
{
    uint8_t slot;

    EACH_CHILD(&b->o.task, World_GetEvent1(), arg);
    slot = World_GetEvent1();
    HANDLER(slot, SectorObject_GetType())(b, arg);
    DynCube_Remove(arg[0], b);
}
HOOK(0x448A90, Building_WorldRemove);

void Building_Close(void)
{
    QTaskList_Free(gBuildingParams);
    TaskType_Free(gBuildingType);
}
HOOK(0x448B50, Building_Close);
