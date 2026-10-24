/* Water: a square of water surface snapped to a power of two grid. While
   a water object draws, its level is pushed for the terrain; points and
   lines are tested against its surface for the collision code. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "nodereg/nodereg.h"

/* the parts of the world used here */
typedef struct WaterWorld {
    char unk0[0xD3C];
    int unkD3C;
    char unkD40[0xD64 - 0xD40];
    int unkD64;
    char unkD68[0xDE0 - 0xD68];
    int offset[3];
    char unkDEC[0xDFC - 0xDEC];
    double *waterLevel;
} WaterWorld;

/* a kind of water, set up by script */
typedef struct WaterType {
    int id;
    int unk4;
    int unk8[4];
    float height;
    float unk1C;
    int unk20;
    int unk24;
    int unk28;
    int unk2C;
} WaterType;

/* "Water" (0x4C48), a RigidDynCubeObj */
typedef struct Water {
    Task task;
    Vec3d pos;
    char unk38[0x60];
    float scale[3];
    char unkA4[0xF0 - 0xA4];
    WaterWorld *world;
    int unkF4;
    void *neighbors[4];     /* &neighbor->neighbors of the waters with neighborIds */
    char unk108[0x4938 - 0x108];
    Vec3d drawMin;
    Vec3d drawMax;
    char unk4968[0x18];
    int typeData[4];
    char unk4990[0x4B4C - 0x4990];
    float ambient[3];
    float tint[3];
    char unk4B64[0x14];
    float height;
    float unk4B7C;
    char unk4B80[8];
    float level;
    float top;
    int unk4B90;
    int size;
    int unk4B98;
    int unk4B9C;
    int frames;
    int unk4BA4;
    int unk4BA8;
    char unk4BAC[0x24];
    Vec3d center;
    Vec3d min;
    Vec3d max;
    float levelParam;       /* 0 to 1 of the size above the object */
    float drawLevel;
    int id;
    int shift;              /* size is 2^31 >> shift */
    int type;
    int unk4C2C;
    int unk4C30;
    int resolution;
    int neighborIds[4];
} Water;

_Static_assert(sizeof(Water) == 0x4C48, "Water");
_Static_assert(offsetof(Water, ambient) == 0x4B4C, "Water.ambient");
_Static_assert(offsetof(Water, height) == 0x4B78, "Water.height");
_Static_assert(offsetof(Water, center) == 0x4BD0, "Water.center");
_Static_assert(offsetof(WaterWorld, waterLevel) == 0xDFC, "WaterWorld");

typedef struct WaterInfo {
    Vec3d pos;
    int unk18[3];
    Matrix matrix;
    float radius;
    char *name;
} WaterInfo;

/* arguments of the CellMask draw event */
typedef struct WaterCellMask {
    void *mask;
    int size;
    Vec3d *origin;
    float half;
} WaterCellMask;

/* a hit of a moving point with the surface, 0x70 bytes */
typedef struct WaterHit {
    int unk0;
    int unk4;
    Vec3d pos;
    char unk20[0x18];
    Vec3f normal;
    char unk44[8];
    int unk4C;
    int unk50;
    int unk54;
    int hit;
    int unk5C;
    double dist;
    char unk68[8];
} WaterHit;

typedef struct WaterLineQuery {
    Vec3d *to;
    Vec3d *from;
    int count;
    char unkC[0x14];
    WaterHit *hits;
} WaterLineQuery;

typedef struct WaterPointQuery {
    Vec3d *points;
    int count;
    int *out;
} WaterPointQuery;

_Static_assert(sizeof(WaterHit) == 0x70, "WaterHit");

#define WATER_TYPES 64
#define WATER_GLOBAL_FLAG 0x40000000

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401F60, void, Task_SetGlobalFlags, (uint32_t flags));
ORIG(0x401F80, void, Task_ClearGlobalFlags, (uint32_t flags));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x499FF0, int, Float_Log2, (float f));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6E70, void, DynCube_MoveBox, (void *world, void *obj, Vec3d *center, Vec3d *min, Vec3d *max));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4E55B0, uint8_t, CellMask_GetDrawEvent, (void));
ORIG(0x4E5610, void, CellMask_FillTriangle, (void *mask, float x0, float y0, float x1, float y1, float x2, float y2));
ORIG(0x4F0F50, int, Param_Real32, (void));
ORIG(0x4F0FA0, int, Param_Int32, (void));
ORIG(0x4F1010, int, Param_ObjectPos, (void));
ORIG(0x4F1160, char, Level_IsRunning, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F19C0, int, ParamType_New, (const char *name, int parent));
ORIG(0x4F1A40, void, ParamType_Free, (int type));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, int type, int flags, void *extra));
ORIG(0x52D6C0, void, WaterMesh_Init, (void **neighbors));

extern Water *gWaters[];
extern int gWaterLevelType;
extern void *gWaterParams;
extern Water *gWaterStack[];
extern WaterType gWaterTypes[];
extern uint16_t gWaterType;
extern int gWaterCount;
extern int gWaterDepth;
extern float gAmbient[];
extern float gWaterTint[];
extern double gWaterLevel;
extern WaterWorld *gWorld;

static const double kZero = 0.0;
static const double kHalf = 0.5;
static const float kHalfF = 0.5f;
static const float kRadius = 4096.0f;
static const float kScale = 0x1.e79e7cp-14f;
static const float kTopScale = 0x1.19999ap+0f;     /* 1.1 */

void Water_DrawCellMask(Water *w, WaterCellMask *c)
{
    long double k = (long double)c->size * kHalfF / c->half;
    float x0 = k * (w->min.x - c->origin->x + c->half);
    float y0 = k * (w->min.y - c->origin->y + c->half);
    float x1 = k * (w->max.x - c->origin->x + c->half);
    float y1 = k * (w->max.y - c->origin->y + c->half);

    CellMask_FillTriangle(c->mask, x0, y0, x1, y0, x0, y1);
    CellMask_FillTriangle(c->mask, x1, y0, x0, y1, x1, y1);
}
HOOK(0x518930, Water_DrawCellMask);

static void push_level(Water *w)
{
    gWaterLevel = w->drawLevel;
    gWorld->waterLevel = &gWaterLevel;
    gWaterStack[++gWaterDepth] = w;
}

static void pop_level(void)
{
    if (--gWaterDepth) {
        gWaterLevel = gWaterStack[gWaterDepth]->drawLevel;
        gWorld->waterLevel = &gWaterLevel;
        return;
    }
    gWorld->waterLevel = NULL;
}

void Water_PreDraw(Water *w)
{
    Level_IsRunning();
    w->ambient[0] = gAmbient[0];
    w->ambient[1] = gAmbient[1];
    w->ambient[2] = gAmbient[2];
    w->tint[0] = gWaterTint[0];
    w->tint[1] = gWaterTint[1];
    w->tint[2] = gWaterTint[2];
    w->ambient[0] = w->ambient[0] * kHalfF;
    w->ambient[1] = w->ambient[1] * kHalfF;
    w->ambient[2] = w->ambient[2] * kHalfF;
    push_level(w);
}
HOOK(0x518A00, Water_PreDraw);

void Water_PostDraw(Water *w)
{
    pop_level();
}
HOOK(0x518AB0, Water_PostDraw);

void Water_PushLevel(Water *w)
{
    push_level(w);
}
HOOK(0x518B00, Water_PushLevel);

void Water_PopLevel(Water *w)
{
    pop_level();
}
HOOK(0x518B40, Water_PopLevel);

void Water_Create(Water *w)
{
    int i;

    w->world = gWorld;
    w->min.x = w->min.y = w->min.z = 0;
    w->max.x = w->max.y = w->max.z = 0;
    w->shift = 10;
    w->resolution = 0x200;
    w->drawLevel = 0;
    w->levelParam = 0;
    w->type = 0;
    w->unk4C2C = 0;
    w->unk4C30 = 0;
    w->id = 0;
    for (i = 0; i < 4; i++)
        w->neighborIds[i] = -1;
    RigidDynCubeObj_Init(w->world, w, VirModel_Find("johan"));
    DynCube_Add(w->world, w, &w->center, kRadius, 2);
    gWaterDepth = 0;
    gWaters[gWaterCount++] = w;
}
HOOK(0x518B90, Water_Create);

void Water_Run(Water *w)
{
    w->frames++;
}
HOOK(0x518C90, Water_Run);

/* records where each line from -> to inside the square crosses the
   surface, keeping the closest hit */
void Water_TestLines(Water *w, WaterLineQuery *q)
{
    WaterHit *out = q->hits;
    Vec3d *p = q->to;
    Vec3d *from = q->from;
    WaterHit rec;
    int i;

    for (i = 0; i < q->count; i++, p++, from++, out++) {
        long double lvl, d;

        if (!(p->x >= w->min.x) || p->x > w->max.x || !(p->y >= w->min.y) || p->y > w->max.y)
            continue;
        lvl = w->height * kHalfF + w->top;
        rec.normal.x = 0;
        rec.normal.y = 0;
        rec.normal.z = 1;
        rec.unk50 = gWorld->unkD3C;
        rec.unk4C = gWorld->unkD64;
        rec.pos.x = p->x;
        rec.pos.y = p->y;
        rec.pos.z = lvl;
        d = p->z - lvl;
        if (!(d >= kZero))
            d = -d;
        if (!(lvl >= from->z) == !(lvl >= p->z))
            continue;
        if (out->hit && d >= out->dist)
            continue;
        d = lvl - p->z;
        rec.hit = 1;
        rec.unk0 = 0;
        if (!(d >= kZero))
            d = -d;
        rec.dist = d;
        *out = rec;
    }
}
HOOK(0x518CA0, Water_TestLines);

/* the world's water value for each point inside the square and below
   the surface */
void Water_TestPoints(Water *w, WaterPointQuery *q)
{
    Vec3d *p = q->points;
    int *out = q->out;
    int i;

    for (i = 0; i < q->count; i++, p++, out++) {
        if (!(p->x >= w->min.x) || p->x > w->max.x || !(p->y >= w->min.y) || p->y > w->max.y)
            continue;
        if (w->height * kHalfF + w->top > p->z)
            *out = gWorld->unkD64;
    }
}
HOOK(0x518E50, Water_TestPoints);

void Water_Delete(Water *w);
void Water_GetParamTable(Water *w, ParamTable *out);
void Water_GetInfo(Water *w, WaterInfo *info);
void Water_ParamsChanged(Water *w);

void Water_Open(void)
{
    Water p;
    int i;

    gWaterDepth = 0;
    for (i = 0; i < WATER_TYPES; i++) {
        WaterType *t = &gWaterTypes[i];

        t->id = -1;
        t->unk28 = 0;
        t->height = 0;
        t->unk1C = 0;
        t->unk8[0] = 0;
        t->unk8[1] = 0;
        t->unk8[2] = 0;
        t->unk8[3] = 0;
    }
    gWaterType = TaskType_New("Water", sizeof(Water), RigidDynCubeObj_GetType(), 0, 0);
    TaskType_SetHandler(gWaterType, 15, Water_PushLevel);
    TaskType_SetHandler(gWaterType, 16, Water_PopLevel);
    TaskType_SetHandler(gWaterType, 0, Water_Create);
    TaskType_SetHandler2(gWaterType, 1, Water_Run);
    TaskType_SetHandler(gWaterType, 4, Water_PreDraw);
    TaskType_SetHandler(gWaterType, 5, Water_PostDraw);
    TaskType_SetHandler(gWaterType, 7, Water_GetInfo);
    TaskType_SetHandler2(gWaterType, 2, Water_Delete);
    TaskType_SetHandler(gWaterType, Param_GetTableEvent(), Water_GetParamTable);
    TaskType_SetHandler(gWaterType, Param_GetChangedEvent(), Water_ParamsChanged);
    TaskType_SetHandler(gWaterType, 10, Water_TestLines);
    TaskType_SetHandler(gWaterType, 11, Water_TestPoints);
    TaskType_SetHandler(gWaterType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gWaterType, 9, NULL);
    TaskType_SetHandler(gWaterType, 12, NULL);
    TaskType_SetHandler(gWaterType, CellMask_GetDrawEvent(), Water_DrawCellMask);
    Task_SetGlobalFlags(WATER_GLOBAL_FLAG);
    TaskType_SetFlags(gWaterType, WATER_GLOBAL_FLAG);
    gWaterParams = ParamTable_New(16);
    gWaterLevelType = ParamType_New("WaterLevel", Param_Real32());
    ParamTable_Add(gWaterParams, &p, &p.center, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gWaterParams, &p, &p.shift, Param_Int32(), 7, NULL);
    ParamTable_Add(gWaterParams, &p, &p.levelParam, gWaterLevelType, 7, NULL);
    ParamTable_Add(gWaterParams, &p, &p.resolution, Param_Int32(), 7, NULL);
    ParamTable_Add(gWaterParams, &p, &p.type, Param_Int32(), 7, NULL);
    ParamTable_Add(gWaterParams, &p, &p.unk4C2C, Param_Int32(), 7, NULL);
    ParamTable_Add(gWaterParams, &p, &p.unk4C30, Param_Int32(), 7, NULL);
}
HOOK(0x518EF0, Water_Open);

void Water_Delete(Water *w)
{
    DynCube_Remove(w->world, w);
    gWaters[--gWaterCount] = NULL;
}
HOOK(0x5191E0, Water_Delete);

void Water_GetParamTable(Water *w, ParamTable *out)
{
    *out = gWaterParams;
}
HOOK(0x519210, Water_GetParamTable);

void Water_GetInfo(Water *w, WaterInfo *info)
{
    long double h;

    info->pos = w->pos;
    info->unk18[0] = 0;
    info->unk18[1] = 0;
    info->unk18[2] = 0;
    info->matrix.m[0] = 1;
    info->matrix.m[1] = 0;
    info->matrix.m[2] = 0;
    info->matrix.m[3] = 0;
    info->matrix.m[4] = 1;
    info->matrix.m[5] = 0;
    info->matrix.m[6] = 0;
    info->matrix.m[7] = 0;
    info->matrix.m[8] = 1;
    info->matrix.count = 0;
    info->name = NULL;
    h = 0x40000000 >> w->shift;
    info->radius = h + h;
}
HOOK(0x519220, Water_GetInfo);

void *Water_FindById(int id);

/* snaps the square to the grid, sets the level from the parameters and
   the water type, and links the neighbors */
void Water_ParamsChanged(Water *w)
{
    int size = (0x40000000 >> w->shift) * 2;
    int mask = -size;
    WaterWorld *world = w->world;
    WaterType *t;
    long double d, h, z;
    Vec3d c = w->center;
    int res, step, half, lvl, i;

    c.x = world->offset[0] + c.x;
    c.y = world->offset[1] + c.y;
    c.z = world->offset[2] + c.z;
    w->min.x = x87_fistp(c.x) & mask;
    w->min.y = x87_fistp(c.y) & mask;
    w->min.z = x87_fistp(c.z) & mask;
    d = size;
    w->max.x = d + w->min.x;
    w->max.y = d + w->min.y;
    w->max.z = d + w->min.z;
    c = w->min;
    h = d * kHalf;
    c.x = h + c.x;
    c.y = h + c.y;
    z = h + c.z;
    c.z = z;
    world = w->world;
    c.x = c.x - world->offset[0];
    c.y = c.y - world->offset[1];
    c.z = z - world->offset[2];
    w->min.x = w->min.x - w->world->offset[0];
    w->min.y = w->min.y - w->world->offset[1];
    w->min.z = w->min.z - w->world->offset[2];
    w->max.x = w->max.x - w->world->offset[0];
    w->max.y = w->max.y - w->world->offset[1];
    w->max.z = w->max.z - w->world->offset[2];
    w->scale[0] = w->scale[1] = w->scale[2] = (long double)size * kScale;
    DynCube_MoveBox(w->world, w, &c, &w->min, &w->max);
    res = w->resolution;
    if (res < 0x1000)
        res = 0x1000;
    w->resolution = res;
    step = 1 << (Float_Log2(res) >> 3);
    w->size = size;
    half = size / 2;
    lvl = x87_fistp(w->levelParam * (long double)half + w->pos.z);
    lvl = lvl / step * step;
    w->drawMin = w->min;
    w->drawLevel = lvl;
    w->level = lvl;
    w->drawMax = w->max;
    for (i = 0; i <= WATER_TYPES; i++)
        if (gWaterTypes[i].id == w->type)
            break;
    t = &gWaterTypes[i];
    w->unk4B9C = w->unk4C30;
    w->unk4B98 = w->unk4C2C;
    *(int *)&w->height = *(int *)&t->height;
    *(int *)&w->unk4B7C = *(int *)&t->unk1C;
    w->unk4BA4 = t->unk20;
    w->unk4BA8 = t->unk24;
    w->top = w->height * kTopScale + (long double)lvl;
    w->typeData[0] = t->unk8[0];
    w->typeData[1] = t->unk8[1];
    w->typeData[2] = t->unk8[2];
    w->typeData[3] = t->unk8[3];
    w->ambient[0] = gAmbient[0];
    w->ambient[1] = gAmbient[1];
    w->ambient[2] = gAmbient[2];
    w->neighbors[0] = Water_FindById(w->neighborIds[0]);
    w->neighbors[1] = Water_FindById(w->neighborIds[1]);
    w->neighbors[2] = Water_FindById(w->neighborIds[2]);
    w->neighbors[3] = Water_FindById(w->neighborIds[3]);
    WaterMesh_Init(w->neighbors);
}
HOOK(0x519290, Water_ParamsChanged);

void Water_Close(void)
{
    ParamType_Free(gWaterLevelType);
    QTaskList_Free(gWaterParams);
    Task_ClearGlobalFlags(WATER_GLOBAL_FLAG);
    TaskType_Free(gWaterType);
}
HOOK(0x519650, Water_Close);

/* the neighbor list of the last water with this id */
void *Water_FindById(int id)
{
    void *r = NULL;
    int i;

    for (i = 0; i < gWaterCount; i++)
        if (gWaters[i]->id == id)
            r = gWaters[i]->neighbors;
    return r;
}
HOOK(0x519690, Water_FindById);

void Water_SetTint(const float *c)
{
    gWaterTint[0] = c[0];
    gWaterTint[1] = c[1];
    gWaterTint[2] = c[2];
}
HOOK(0x5196D0, Water_SetTint);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A44344) WaterWorld *gWorld;
ALIAS(gCurrentWorld, gWorld, 0);
ALIAS(gRoom, gWorld, 0);
ALIAS(gTerrain, gWorld, 0);
ALIAS(gWalkWorld, gWorld, 0);
PAD(igb, A80200, A80200, 0x100);
ALIAS(gWaters, pad_A80200, 0);
BSS(A80200) int gWaterLevelType;
BSS(A80200) void *gWaterParams;
PAD(igb, A80200, A80308, 0x100);
ALIAS(gWaterStack, pad_A80308, 0);
PAD(igb, A80200, A80408, 0xC0C);
ALIAS(gWaterTypes, pad_A80408, 0);
BSS(A80200) uint16_t gWaterType;
PAD(igb, A80200, A81016, 0x6);
BSS(A80200) int gWaterCount;
BSS(A80200) int gWaterDepth;
PAD(igb, B818A0, B818A0, 0xC);
ALIAS(gAmbient, pad_B818A0, 0);
ALIAS(gAmbientColors, pad_B818A0, 0);
BSS(BA2040) double gWaterLevel;
