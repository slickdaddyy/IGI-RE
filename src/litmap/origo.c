#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "litmap/litmap.h"

ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4E5360, uint8_t, DynCubeObj_GetMaskVisibleEvent, (void));
ORIG(0x52BFE0, void, DiscardTerrain_MaskVisible, (void));

typedef struct Terrain {
    char unk0[0xDE0];
    int offset[3];
} Terrain;

/* The world origin marker: the integer part of its position becomes the
   terrain offset, so far away maps keep their precision. */
typedef struct Origo {
    Task task;
    Vec3d pos;
    char unk38[0x34];
    void **model;
    char unk70[0x28];
    float scale[3];
    char unkA4[0x4C];
    Terrain *world;
    int unkF4;
    Vec3d place;
    int grid[3];
    int start[3];
    int placed;
    int unk12C;
} Origo;

_Static_assert(offsetof(Origo, scale) == 0x98, "scale");
_Static_assert(offsetof(Origo, world) == 0xF0, "world");
_Static_assert(offsetof(Origo, placed) == 0x128, "placed");
_Static_assert(sizeof(Origo) == 0x130, "Origo");

/* filled by the slot 7 handler of a RigidDynCubeObj */
typedef struct BoundSphere {
    Vec3d pos;
    float unk18[3];
    Matrix matrix;
    float radius;
    int unk50;
} BoundSphere;

_Static_assert(offsetof(BoundSphere, radius) == 0x4C, "radius");

extern uint16_t gOrigoType;
extern void *gOrigoParams;
extern Vec3d *gOrigoPlace;
extern int gOrigoDelta[];

static const float kOrigoRadius = 4096.0f;

void Origo_Create(Origo *o)
{
    o->world = gCurrentWorld;
    o->place.x = o->place.y = o->place.z = 0;
    o->placed = 0;
    RigidDynCubeObj_Init(o->world, o, VirModel_Find("johan"));
    DynCube_Add(o->world, o, &o->place, kOrigoRadius, 1);
}
HOOK(0x520730, Origo_Create);

void Origo_GetBounds(Origo *o, BoundSphere *b);
void Origo_Delete(Origo *o);
void Origo_GetParams(Origo *o, void **out);
void Origo_Changed(Origo *o);

void Origo_Init(void)
{
    Origo t;
    uint16_t type;

    type = TaskType_New("Origo", sizeof(Origo), RigidDynCubeObj_GetType(), 0, 0);
    gOrigoType = type;
    TaskType_SetHandler(type, 0, Origo_Create);
    TaskType_SetHandler(gOrigoType, 4, Debug_Log);
    TaskType_SetHandler(gOrigoType, 7, Origo_GetBounds);
    TaskType_SetHandler2(gOrigoType, 2, Origo_Delete);
    TaskType_SetHandler(gOrigoType, Param_GetTableEvent(), Origo_GetParams);
    TaskType_SetHandler(gOrigoType, Param_GetChangedEvent(), Origo_Changed);
    TaskType_SetHandler(gOrigoType, 10, NULL);
    TaskType_SetHandler(gOrigoType, 11, NULL);
    TaskType_SetHandler(gOrigoType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gOrigoType, 9, NULL);
    TaskType_SetHandler(gOrigoType, 8, NULL);
    TaskType_SetHandler(gOrigoType, 12, NULL);
    TaskType_SetHandler(gOrigoType, DynCubeObj_GetMaskVisibleEvent(), DiscardTerrain_MaskVisible);
    gOrigoParams = ParamTable_New(1);
    ParamTable_Add(gOrigoParams, &t, &t.place, Param_ObjectPos(), 7, NULL);
}
HOOK(0x5207A0, Origo_Init);

void Origo_GetBounds(Origo *o, BoundSphere *b)
{
    float s;
    int i;

    b->pos = o->pos;
    b->unk18[0] = b->unk18[1] = b->unk18[2] = 0;
    for (i = 0; i < 9; i++)
        b->matrix.m[i] = 0;
    b->matrix.m[0] = b->matrix.m[4] = b->matrix.m[8] = 1.0f;
    b->matrix.unk24 = 0;
    s = !(o->scale[2] >= o->scale[1]) ? o->scale[1] : o->scale[2];
    if (o->scale[0] > s)
        s = o->scale[0];
    b->unk50 = 0;
    b->radius = VirModel_GetRadius(o->model) * s;
}
HOOK(0x520910, Origo_GetBounds);

void Origo_Delete(Origo *o)
{
    DynCube_Remove(o->world, o);
}
HOOK(0x5209E0, Origo_Delete);

void Origo_GetParams(Origo *o, void **out)
{
    *out = gOrigoParams;
}
HOOK(0x520A00, Origo_GetParams);

void Origo_Changed(Origo *o)
{
    Vec3d p;

    if (!o->placed) {
        o->grid[0] = round_int(o->place.x) & ~1;
        o->placed = 1;
        o->grid[1] = round_int(o->place.y) & ~1;
        o->grid[2] = round_int(o->place.z) & ~1;
        o->start[0] = o->grid[0];
        o->start[1] = o->grid[1];
        o->start[2] = o->grid[2];
        o->world->offset[0] = o->grid[0];
        o->world->offset[1] = o->grid[1];
        o->world->offset[2] = o->grid[2];
    }
    gOrigoPlace = &o->place;
    o->grid[0] = round_int(o->place.x) & ~1;
    o->grid[1] = round_int(o->place.y) & ~1;
    o->grid[2] = round_int(o->place.z) & ~1;
    gOrigoDelta[0] = o->start[0] - o->grid[0];
    gOrigoDelta[1] = o->start[1] - o->grid[1];
    gOrigoDelta[2] = o->start[2] - o->grid[2];
    p = o->place;
    p.x = p.x - o->start[0];
    p.y = p.y - o->start[1];
    p.z = p.z - o->start[2];
    DynCube_Move(o->world, o, &p, kOrigoRadius);
}
HOOK(0x520A10, Origo_Changed);

void Origo_Shutdown(void)
{
    QTaskList_Free(gOrigoParams);
    TaskType_Free(gOrigoType);
}
HOOK(0x520B90, Origo_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A83850) void *gOrigoParams;
BSS(A83850) uint16_t gOrigoType;
PAD(igb, A83850, A83856, 0x2);
BSS(BA1FC8) Vec3d *gOrigoPlace;
PAD(igb, BA1FC8, BA1FCC, 0x4);
PAD(igb, BA1FC8, BA1FD0, 0xC);
ALIAS(gOrigoDelta, pad_BA1FD0, 0);
