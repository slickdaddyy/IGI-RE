#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "terredit/terredit.h"
#include "coll/colltypes.h"

FN_EXTERN(VehiclePart_Delete);

/* "DiscardTerrain": a cube of the dynamic cube tree that tells the qcube
   walk to drop the terrain inside it, and takes collision points that fall
   inside it away from the terrain. */

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (Terrain *world, void *obj, void **model));
ORIG(0x4C6E70, void, DynCube_MoveBox, (Terrain *world, void *obj, Vec3d *center, Vec3d *min, Vec3d *max));
ORIG(0x4C7140, void, DynCube_Add, (Terrain *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (Terrain *world, void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4E5360, uint8_t, DynCubeObj_GetMaskVisibleEvent, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1160, int, Level_IsRunning, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

#define VehiclePart_Delete (FN(0x426860, VehiclePart_Delete))

typedef struct DiscardTerrain {
    Task task;
    char unk20[0x48];
    Terrain *world;
    char unk6C[0x2C];
    float scale[3];
    char unkA4[0x4C];
    Vec3d pos;
    Vec3d min;          /* relative to the terrain offset */
    Vec3d max;
    Vec3d center;
    int shift;          /* cell size is 2^31 >> shift */
    int unk154;
} DiscardTerrain;

/* the argument of world event 0 */
typedef struct WorldEventArg {
    int unk0;
    Terrain *world;
} WorldEventArg;

_Static_assert(sizeof(DiscardTerrain) == 0x158, "DiscardTerrain");
_Static_assert(offsetof(DiscardTerrain, scale) == 0x98, "DiscardTerrain.scale");

extern uint16_t gDiscardTerrainType;
extern void *gDiscardTerrainParams;
extern int gQCubeSkipCells;

static const float kRadius = 4096.0f;
static const double kHalf = 0.5;
static const float kScale = 0x1.e79e7cp-14f;

/* clears active[] for every point inside the box */
void DiscardTerrain_CollQuery(DiscardTerrain *dt, CollQuery *q)
{
    int i;

    for (i = 0; i < q->count; i++) {
        const Vec3d *p = &q->points[i];

        if ((p->x > dt->min.x) != (p->x > dt->max.x) && (p->y > dt->min.y) != (p->y > dt->max.y) &&
            (p->z > dt->min.z) != (p->z > dt->max.z))
            q->active[i] = 0;
    }
}
HOOK(0x52BF00, DiscardTerrain_CollQuery);

void DiscardTerrain_MaskVisible(void *task, int *out)
{
    *out = 0;
}
HOOK(0x52BFE0, DiscardTerrain_MaskVisible);

int DiscardTerrain_Draw(void)
{
    return Level_IsRunning();
}
HOOK(0x52BFF0, DiscardTerrain_Draw);

void DiscardTerrain_Create(DiscardTerrain *dt)
{
    dt->pos.x = dt->pos.y = dt->pos.z = 0;
    dt->min.x = dt->min.y = dt->min.z = 0;
    dt->max.x = dt->max.y = dt->max.z = 0;
    dt->shift = 10;
    RigidDynCubeObj_Init(gCurrentWorld, dt, VirModel_Find("johan"));
    DynCube_Add(dt->world, dt, &dt->pos, kRadius, 4);
}
HOOK(0x52C000, DiscardTerrain_Create);

/* slot 15, while the qcube walk is inside the box */
void DiscardTerrain_Visit(void)
{
    gQCubeSkipCells = 1;
}
HOOK(0x52C0B0, DiscardTerrain_Visit);

void DiscardTerrain_Changed(DiscardTerrain *dt, const char *arg);
void DiscardTerrain_GetParams(void *task, void **out);
void DiscardTerrain_GetInfo(DiscardTerrain *dt, ObjInfo *info);
void DiscardTerrain_WorldRemove(DiscardTerrain *dt, Terrain **arg);
void DiscardTerrain_WorldAdd(DiscardTerrain *dt, WorldEventArg *arg);

void DiscardTerrain_Init(void)
{
    DiscardTerrain t;
    uint16_t type;

    type = TaskType_New("DiscardTerrain", sizeof(DiscardTerrain), EditorRigidObj_GetType(), 0, 0);
    gDiscardTerrainType = type;
    TaskType_SetHandler(type, 0, DiscardTerrain_Create);
    TaskType_SetHandler(gDiscardTerrainType, 4, DiscardTerrain_Draw);
    TaskType_SetHandler(gDiscardTerrainType, 15, DiscardTerrain_Visit);
    TaskType_SetHandler(gDiscardTerrainType, 7, DiscardTerrain_GetInfo);
    TaskType_SetHandler2(gDiscardTerrainType, 2, VehiclePart_Delete);
    TaskType_SetHandler(gDiscardTerrainType, Param_GetTableEvent(), DiscardTerrain_GetParams);
    TaskType_SetHandler(gDiscardTerrainType, Param_GetChangedEvent(), DiscardTerrain_Changed);
    TaskType_SetHandler(gDiscardTerrainType, DynCubeObj_GetMaskVisibleEvent(), DiscardTerrain_MaskVisible);
    TaskType_SetHandler(gDiscardTerrainType, 11, DiscardTerrain_CollQuery);
    TaskType_SetHandler(gDiscardTerrainType, World_GetEvent1(), DiscardTerrain_WorldRemove);
    TaskType_SetHandler(gDiscardTerrainType, World_GetEvent0(), DiscardTerrain_WorldAdd);
    TaskType_SetHandler(gDiscardTerrainType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gDiscardTerrainType, 9, NULL);
    TaskType_SetHandler(gDiscardTerrainType, 8, NULL);
    TaskType_SetHandler(gDiscardTerrainType, 10, NULL);
    gDiscardTerrainParams = ParamTable_New(2);
    ParamTable_Add(gDiscardTerrainParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gDiscardTerrainParams, &t, &t.shift, Param_Int32(), 7, NULL);
}
HOOK(0x52C0C0, DiscardTerrain_Init);

/* snaps the box to its cell, moves it in the dyncube tree and, unless arg
   names a parameter, passes the event on to the children */
void DiscardTerrain_Changed(DiscardTerrain *dt, const char *arg)
{
    Terrain *w = dt->world;
    int size = (0x40000000 >> (dt->shift & 31)) * 2;
    int mask = -size;
    double s, h, cx, cy;
    long double cz;
    float scale;

    dt->center = dt->pos;
    dt->center.x = w->offset[0] + dt->center.x;
    dt->center.y = w->offset[1] + dt->center.y;
    dt->center.z = w->offset[2] + dt->center.z;
    dt->min.x = x87_fistp(dt->center.x) & mask;
    dt->min.y = x87_fistp(dt->center.y) & mask;
    dt->min.z = x87_fistp(dt->center.z) & mask;
    s = size;
    dt->center = dt->min;
    dt->max.x = dt->min.x + s;
    dt->max.y = dt->min.y + s;
    dt->max.z = dt->min.z + s;
    h = s * kHalf;
    cx = h + dt->center.x;
    dt->center.x = cx;
    cy = h + dt->center.y;
    dt->center.y = cy;
    cz = h + dt->center.z;
    dt->center.z = cz;
    dt->center.x = cx - w->offset[0];
    dt->center.y = cy - w->offset[1];
    dt->center.z = cz - w->offset[2];
    dt->min.x = dt->min.x - w->offset[0];
    dt->min.y = dt->min.y - w->offset[1];
    dt->min.z = dt->min.z - w->offset[2];
    dt->max.x = dt->max.x - w->offset[0];
    dt->max.y = dt->max.y - w->offset[1];
    dt->max.z = dt->max.z - w->offset[2];
    scale = size * kScale;
    dt->scale[0] = scale;
    dt->scale[1] = scale;
    dt->scale[2] = scale;
    DynCube_MoveBox(w, dt, &dt->center, &dt->min, &dt->max);
    if (!arg || !*arg)
        CALL_CHILDREN(dt, Param_GetChangedEvent(), (void *)arg);
}
HOOK(0x52C290, DiscardTerrain_Changed);

void DiscardTerrain_GetParams(void *task, void **out)
{
    *out = gDiscardTerrainParams;
}
HOOK(0x52C500, DiscardTerrain_GetParams);

void DiscardTerrain_GetInfo(DiscardTerrain *dt, ObjInfo *info)
{
    info->pos = dt->pos;
    obj_info_identity(info);
    info->radius = 0x40000000 >> (dt->shift & 31);
    info->name = NULL;
}
HOOK(0x52C510, DiscardTerrain_GetInfo);

void DiscardTerrain_WorldRemove(DiscardTerrain *dt, Terrain **arg)
{
    DynCube_Remove(*arg, dt);
}
HOOK(0x52C580, DiscardTerrain_WorldRemove);

void DiscardTerrain_WorldAdd(DiscardTerrain *dt, WorldEventArg *arg)
{
    DynCube_Add(dt->world, dt, &dt->pos, kRadius, 4);
    DynCube_MoveBox(arg->world, dt, &dt->center, &dt->min, &dt->max);
}
HOOK(0x52C5A0, DiscardTerrain_WorldAdd);

void DiscardTerrain_Shutdown(void)
{
    QTaskList_Free(gDiscardTerrainParams);
    TaskType_Free(gDiscardTerrainType);
}
HOOK(0x52C5F0, DiscardTerrain_Shutdown);

DiscardTerrain *DiscardTerrain_New(void *parent, const Vec3d *pos, int shift)
{
    DiscardTerrain *dt;
    TaskFn fn;

    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        dt = (DiscardTerrain *)Task_New(parent, gDiscardTerrainType, NULL);
        Mem_ClearMode();
    } else {
        dt = (DiscardTerrain *)Task_New(parent, gDiscardTerrainType, NULL);
    }
    dt->shift = shift;
    dt->pos = *pos;
    fn = HANDLER(Param_GetChangedEvent(), dt->task.type);
    if (fn)
        fn(dt, NULL);
    return dt;
}
HOOK(0x52C610, DiscardTerrain_New);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A8454C) uint16_t gDiscardTerrainType;
PAD(igb, A8454C, A8454E, 0x2);
BSS(A8454C) void *gDiscardTerrainParams;
