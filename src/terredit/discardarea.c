#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "terredit/terredit.h"

FN_EXTERN(Task_Nothing);

/* "DiscardTerrainArea": an editor box that fills itself with DiscardTerrain
   cubes. The box is cut into octants down to maxDepth levels; a cube is
   placed where every 4096 unit column below its top is solid ground with a
   material. */

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401780, void, Task_DeleteChildren, (void *task));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C9750, void, Coll_GetPointMaterials, (Terrain *t, void **results, const Vec3d *points, int count, char unk,
                                              void *unk2, unsigned int flags));
ORIG(0x4CB9F0, void, Coll_DropPointsMat, (Terrain *t, void *hits, const Vec3d *points, int count, const double *dz,
                                          int unk, int unk2, unsigned int flags));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

#define Task_Nothing (FN(0x4D3710, Task_Nothing))

void *DiscardTerrain_New(void *parent, const Vec3d *pos, int shift);

typedef struct DiscardTerrainArea {
    Task task;
    Vec3d pos;
    Vec3d min;          /* relative to the terrain offset */
    Vec3d max;
    int shift;          /* cell size is 2^31 >> shift */
    int maxDepth;
    char regenerate;
    char unk71[7];
} DiscardTerrainArea;

/* the hit record of Coll_DropPointsMat */
typedef struct DropHit {
    void *task;
    int unk4;
    Vec3d pos;
    char unk20[0x38];
    int found;
    int unk5C;
    double dist;
    char unk68[0x8];
} DropHit;

/* what Coll_GetPointMaterials passes on to the collision handlers */
typedef struct MatQuery {
    int unk0;
    void *self;
} MatQuery;

_Static_assert(sizeof(DiscardTerrainArea) == 0x78, "DiscardTerrainArea");
_Static_assert(offsetof(DropHit, dist) == 0x60, "DropHit");

extern uint16_t gDiscardAreaType;
extern void *gDiscardAreaParams;

static const double kHalf = 0.5;
static const double kStep = 4096.0;
static const double kAboveGround = 40.96;

void DiscardTerrainArea_Create(DiscardTerrainArea *a);
void DiscardTerrainArea_GetInfo(DiscardTerrainArea *a, ObjInfo *info);
void DiscardTerrainArea_GetParams(void *task, void **out);
void DiscardTerrainArea_Changed(DiscardTerrainArea *a);

void DiscardTerrainArea_Init(void)
{
    DiscardTerrainArea t;
    uint16_t type;

    type = TaskType_New("DiscardTerrainArea", sizeof(DiscardTerrainArea), RigidDynCubeObj_GetType(), 0, 0);
    gDiscardAreaType = type;
    TaskType_SetHandler(type, 0, DiscardTerrainArea_Create);
    TaskType_SetHandler(gDiscardAreaType, 7, DiscardTerrainArea_GetInfo);
    TaskType_SetHandler2(gDiscardAreaType, 2, Task_Nothing);
    TaskType_SetHandler(gDiscardAreaType, Param_GetTableEvent(), DiscardTerrainArea_GetParams);
    TaskType_SetHandler(gDiscardAreaType, Param_GetChangedEvent(), DiscardTerrainArea_Changed);
    gDiscardAreaParams = ParamTable_New(4);
    ParamTable_Add(gDiscardAreaParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gDiscardAreaParams, &t, &t.shift, Param_Int32(), 7, NULL);
    ParamTable_Add(gDiscardAreaParams, &t, &t.maxDepth, Param_Int32(), 7, NULL);
    ParamTable_Add(gDiscardAreaParams, &t, &t.regenerate, Param_Bool8(), 7, NULL);
}
HOOK(0x52B6C0, DiscardTerrainArea_Init);

void DiscardTerrainArea_Create(DiscardTerrainArea *a)
{
    a->pos.x = a->pos.y = a->pos.z = 0;
    a->min.x = a->min.y = a->min.z = 0;
    a->max.x = a->max.y = a->max.z = 0;
    a->shift = 12;
    a->maxDepth = 16;
    a->regenerate = 0;
}
HOOK(0x52B7F0, DiscardTerrainArea_Create);

void DiscardTerrainArea_Fill(DiscardTerrainArea *a, int depth, int size, const Vec3d *center);

void DiscardTerrainArea_Changed(DiscardTerrainArea *a)
{
    int size = (0x40000000 >> (a->shift & 31)) * 2;
    int mask = -size;
    Terrain *w = gCurrentWorld;
    Vec3d p = a->pos;
    double fx, fy, fz, s, ox, oy, oz;
    long double h, cz;
    Vec3d min, max;

    p.x = w->offset[0] + p.x;
    p.y = w->offset[1] + p.y;
    p.z = w->offset[2] + p.z;
    fx = x87_fistp(p.x) & mask;
    fy = x87_fistp(p.y) & mask;
    fz = x87_fistp(p.z) & mask;
    s = size;
    max.x = s + fx;
    max.y = s + fy;
    max.z = s + fz;
    h = s * kHalf;
    p.x = h + fx;
    p.y = h + fy;
    cz = fz + h;
    p.x = p.x - w->offset[0];
    p.y = p.y - w->offset[1];
    p.z = cz - w->offset[2];
    ox = w->offset[0];
    min.x = fx - ox;
    oy = w->offset[1];
    min.y = fy - oy;
    oz = w->offset[2];
    min.z = fz - oz;
    a->min = min;
    max.x = max.x - ox;
    max.y = max.y - oy;
    max.z = max.z - oz;
    a->max = max;
    if (a->regenerate) {
        Task_DeleteChildren(a);
        DiscardTerrainArea_Fill(a, a->shift, size, &p);
        a->regenerate = 0;
    }
}
HOOK(0x52B840, DiscardTerrainArea_Changed);

int DiscardTerrainArea_IsSolid(DiscardTerrainArea *a, int size, const Vec3d *center);

void DiscardTerrainArea_Fill(DiscardTerrainArea *a, int depth, int size, const Vec3d *center)
{
    static const double ox[8] = { 0.25, 0.75, 0.25, 0.75, 0.25, 0.75, 0.25, 0.75 };
    static const double oy[8] = { 0.25, 0.25, 0.75, 0.75, 0.25, 0.25, 0.75, 0.75 };
    static const double oz[8] = { 0.25, 0.25, 0.25, 0.25, 0.75, 0.75, 0.75, 0.75 };
    double s, h;
    int i;

    if (depth > a->maxDepth)
        return;
    if (DiscardTerrainArea_IsSolid(a, size, center) == 1) {
        DiscardTerrain_New(a, center, depth);
        return;
    }
    s = size;
    h = s * kHalf;
    for (i = 0; i < 8; i++) {
        Vec3d p = *center;

        p.x = (s * ox[i] - h) + p.x;
        p.y = (s * oy[i] - h) + p.y;
        p.z = (s * oz[i] - h) + p.z;
        DiscardTerrainArea_Fill(a, depth + 1, size / 2, &p);
    }
}
HOOK(0x52B9F0, DiscardTerrainArea_Fill);

/* 1 when every 4096 unit column of the cube's top face hits ground with a
   material within size below it, else 2 */
int DiscardTerrainArea_IsSolid(DiscardTerrainArea *a, int size, const Vec3d *center)
{
    double dz = -(double)size;
    double h = size * kHalf;
    Vec3d p = *center;
    int n = size / 4096;
    int i, j;
    DropHit hit;
    MatQuery q;
    Vec3d g;
    float *mat;

    p.x = p.x - h;
    p.y = p.y - h;
    p.z = h + p.z;
    q.self = a;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            Coll_DropPointsMat(gCurrentWorld, &hit, &p, 1, &dz, 0xFF, 0, 0x80000000);
            if (!hit.found || !(-dz >= hit.dist))
                return 2;
            g = hit.pos;
            g.z = hit.pos.z + kAboveGround;
            Coll_GetPointMaterials(gCurrentWorld, (void **)&mat, &g, 1, 0xC, &q, 0xFFFFFFFF);
            if (!(mat[3] < 0.0f || mat[3] > 0.0f))
                return 2;
            p.y = p.y + kStep;
        }
        p.y = center->y - h;
        p.x = p.x + kStep;
    }
    return 1;
}
HOOK(0x52BCC0, DiscardTerrainArea_IsSolid);

void DiscardTerrainArea_GetParams(void *task, void **out)
{
    *out = gDiscardAreaParams;
}
HOOK(0x52BE60, DiscardTerrainArea_GetParams);

void DiscardTerrainArea_GetInfo(DiscardTerrainArea *a, ObjInfo *info)
{
    info->pos = a->pos;
    obj_info_identity(info);
    info->radius = 0x40000000 >> (a->shift & 31);
    info->name = NULL;
}
HOOK(0x52BE70, DiscardTerrainArea_GetInfo);

void DiscardTerrainArea_Shutdown(void)
{
    QTaskList_Free(gDiscardAreaParams);
    TaskType_Free(gDiscardAreaType);
}
HOOK(0x52BEE0, DiscardTerrainArea_Shutdown);
