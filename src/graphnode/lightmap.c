#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "graphnode/graphnode.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x516B20, void, crt_splitpath, (const char *path, char *drive, char *dir, char *fname, char *ext));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1160, char, Level_IsRunning, (void));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B47E0, void, Random_Seed, (void *r, unsigned int seed));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x48F250, int, AppContext_GetTerrainLightmapsUsed, (void));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B23B0, void, Texture_Create, (void *tex, void *palette, int w, int h, int pitch, int unk1, int unk2,
                                      int flags, void *data));
ORIG(0x4B2610, void, Texture_SetActiveTMU, (int tmu));
ORIG(0x4B2670, int, Texture_GetTMUs, (void));
ORIG(0x4B5B60, int, Resource_Exists, (const char *path));
ORIG(0x4B5F00, void *, Resource_Load, (const char *path, int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *path));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4C7350, void, DynCube_AddBox, (void *world, void *obj, Vec3d *center, const Vec3d *min, const Vec3d *max,
                                  int tree));
ORIG(0x4C6E70, void, DynCube_MoveBox, (void *world, void *obj, Vec3d *center, const Vec3d *min, const Vec3d *max));
ORIG(0x4CB9F0, void, Coll_DropPointsMat, (void *world, void *results, Vec3d *points, int count, const double *dz,
                                          int unk, int unk2, unsigned int flags));

typedef struct Terrain {
    char unk0[0xDE0];
    int offset[3];
    int lightMapCount;
    int unkDF0;
    void *palette;
    void *lmpData;
} Terrain;

/* a square of terrain lightmap; tall ones get a child per cell height */
typedef struct TerrainLightMap {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    Terrain *world;
    int unk6C;
    Vec3d place;
    Vec3d min;
    Vec3d max;
    int shift;          /* cell size is 2^31 >> shift */
    int texShift;
    char unkC0, unkC1, unkC2, unkC3, unkC4, unkC5, unkC6;
    char placed;
    char unkC8, unkC9;
    char isChild;
    char unkCB, unkCC;
    char active;
    char unkCE[2];
    int unkD0;
    int size;           /* texture width */
    int slot;           /* in gLightMapTasks and gLightMapDraw */
    int lightmap;       /* number of the .lmp file, -1 none */
    int visible;
    int unkE4[9];
    void *texture;
    Task *children;
} TerrainLightMap;

_Static_assert(sizeof(TerrainLightMap) == 0x110, "TerrainLightMap");

typedef struct Lightmap {
    int unk0;
    void *data;
    char texture[0x24];
} Lightmap;

typedef struct LightMapDraw {
    int unk0;
    int shift;
    int unk8, unkC;
    float scale;
    float texScale;
    double x, y;
    void *texture;
    int unk2C;
} LightMapDraw;

typedef struct CollHit {
    Task *task;
    int unk4;
    Vec3d pos;
    char unk20[0x38];
    int found;
    char unk5C[0x14];
} CollHit;

#define LIGHTMAP_MAX 0x800

extern char gLightMapRandom[];
extern uint16_t gTerrainLightMapType;
extern void *gTerrainLightMapParams;
extern uint16_t gDynCubeObjType;
extern Terrain *gCurrentWorld;
extern int gVisibleLightMaps[];
extern int gVisibleLightMapCount;
extern TerrainLightMap *gLightMapTasks[];
extern Lightmap gLightmaps[];
extern LightMapDraw gLightMapDraw[];
extern const char gWorldName[];

static const double kLiftZ = 268435456.0;
static const double kHalf = 0.5;
static const double kDropZ = -536870912.0;
static const float kOne = 1.0f;
static const float kTexSize = 256.0f;
static const float kHalfF = 0.5f;

/* fistp with the current rounding mode, low 32 bits */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

void TerrainLightMap_Create(TerrainLightMap *lm)
{
    int i;

    lm->world = gCurrentWorld;
    lm->place.x = lm->place.y = lm->place.z = 0;
    lm->min.x = lm->min.y = lm->min.z = 0;
    lm->max.x = lm->max.y = lm->max.z = 0;
    for (i = 0; i < 9; i++)
        lm->unkE4[i] = 0;
    lm->unkC0 = lm->unkC1 = lm->unkC2 = lm->unkC3 = 0;
    lm->unkC4 = lm->unkC5 = lm->unkC6 = 0;
    lm->placed = lm->unkC8 = lm->unkC9 = 0;
    lm->isChild = lm->unkCB = lm->unkCC = lm->active = 0;
    lm->slot = 0;
    lm->lightmap = -1;
    lm->shift = 10;
    lm->unkD0 = 0x40;
    lm->size = 0x40;
    lm->children = NULL;
    lm->texture = NULL;
    Random_Seed(gLightMapRandom, 0x19111911);
}
HOOK(0x4F8190, TerrainLightMap_Create);

/* both branches of the original do the same */
void TerrainLightMap_Show(TerrainLightMap *lm)
{
    Level_IsRunning();
    if (lm->texture) {
        lm->visible = 1;
        gVisibleLightMaps[gVisibleLightMapCount++] = lm->slot;
    } else {
        lm->visible = 0;
    }
}
HOOK(0x4F82B0, TerrainLightMap_Show);

void TerrainLightMap_Hide(TerrainLightMap *lm)
{
    if (lm->visible)
        gVisibleLightMapCount--;
}
HOOK(0x4F8320, TerrainLightMap_Hide);

void TerrainLightMap_Delete(TerrainLightMap *lm);
void TerrainLightMap_GetParams(TerrainLightMap *lm, void **out);
void TerrainLightMap_Changed(TerrainLightMap *lm);

void TerrainLightMap_Init(void)
{
    TerrainLightMap t;
    uint16_t type;

    Random_Init(gLightMapRandom);
    Random_Seed(gLightMapRandom, 0x19111911);
    type = TaskType_New("TerrainLightMap", sizeof(TerrainLightMap), gDynCubeObjType, 0, 0);
    gTerrainLightMapType = type;
    TaskType_SetHandler(type, 0, TerrainLightMap_Create);
    TaskType_SetHandler(gTerrainLightMapType, 0x10, TerrainLightMap_Hide);
    TaskType_SetHandler2(gTerrainLightMapType, 2, TerrainLightMap_Delete);
    TaskType_SetHandler(gTerrainLightMapType, Param_GetTableEvent(), TerrainLightMap_GetParams);
    TaskType_SetHandler(gTerrainLightMapType, Param_GetChangedEvent(), TerrainLightMap_Changed);
    TaskType_SetHandler(gTerrainLightMapType, 10, NULL);
    TaskType_SetHandler(gTerrainLightMapType, 11, NULL);
    TaskType_SetHandler(gTerrainLightMapType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gTerrainLightMapType, 9, NULL);
    TaskType_SetHandler(gTerrainLightMapType, 12, NULL);
    TaskType_SetHandler(gTerrainLightMapType, 8, NULL);
    TaskType_SetHandler(gTerrainLightMapType, 4, NULL);
    TaskType_SetHandler(gTerrainLightMapType, 5, NULL);
    TaskType_SetHandler(gTerrainLightMapType, 15, TerrainLightMap_Show);
    gTerrainLightMapParams = ParamTable_New(15);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.place, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.shift, Param_Int32(), 7, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.size, Param_Int32(), 7, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC5, Param_PushButton(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC4, Param_Bool8(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC6, Param_Bool8(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC3, Param_Bool8(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC0, Param_PushButton(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC2, Param_PushButton(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC1, Param_PushButton(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.lightmap, Param_Int32(), 1, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC8, Param_Bool8(), 1, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkC9, Param_PushButton(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkCB, Param_Bool8(), 6, NULL);
    ParamTable_Add(gTerrainLightMapParams, &t, &t.unkCC, Param_Bool8(), 6, NULL);
    gVisibleLightMapCount = 0;
}
HOOK(0x4F8340, TerrainLightMap_Init);

void TerrainLightMap_Delete(TerrainLightMap *lm)
{
    if (!lm->active)
        return;
    if (!lm->isChild) {
        int last = lm->world->lightMapCount - 1;
        int slot = lm->slot;

        if (slot != last) {
            TerrainLightMap *m = gLightMapTasks[last];

            gLightMapTasks[slot] = m;
            m->slot = slot;
        }
        lm->world->lightMapCount = last;
        if (lm->texture && !gLightmaps[lm->lightmap].data)
            Texture_Free(lm->texture);
    }
    DynCube_Remove(lm->world, lm);
}
HOOK(0x4F86E0, TerrainLightMap_Delete);

void TerrainLightMap_GetParams(TerrainLightMap *lm, void **out)
{
    *out = gTerrainLightMapParams;
}
HOOK(0x4F8770, TerrainLightMap_GetParams);

void TerrainLightMap_SetDrawInfo(TerrainLightMap *lm);
TerrainLightMap *TerrainLightMap_NewChild(Task *parent, TerrainLightMap *lm, const Vec3d *place);

static void load_texture(TerrainLightMap *lm)
{
    char path[256], drive[128], dir[128], fname[128], ext[128];

    crt_splitpath(gWorldName, drive, dir, fname, ext);
    crt_sprintf(path, "%s%slightmap/%s%d.lmp", drive, dir, fname, lm->lightmap);
    if (Resource_Exists(path)) {
        void *data = Resource_Load(path, NULL);
        int size = lm->size;

        if (Texture_GetTMUs() > 1)
            Texture_SetActiveTMU(1);
        Texture_Create(lm->texture, lm->world->palette, size, size, size, 1, 0, 0, data);
        if (Texture_GetTMUs() > 1)
            Texture_SetActiveTMU(-1);
        Resource_Unload(path);
    }
}

/* takes the next free slot and the lightmap texture */
static void place(TerrainLightMap *lm)
{
    int n = lm->world->lightMapCount;
    int i = 0;

    lm->placed = 1;
    while (i < n && gLightMapTasks[i])
        i++;
    lm->slot = i;
    gLightMapTasks[i] = lm;
    if (i == n)
        n++;
    if (n >= LIGHTMAP_MAX) {
        Error_Show("Too many lightmaps whene generated");
        for (;;)
            ;
    }
    lm->world->lightMapCount = n;
    if (lm->world->lmpData) {
        if (lm->lightmap != -1 && gLightmaps[lm->lightmap].data)
            lm->texture = gLightmaps[lm->lightmap].texture;
    } else if (lm->lightmap != -1) {
        load_texture(lm);
    }
}

/* Snaps the cube to the cell grid at the ground under the placed point and
   adds children above and below for terrain that is higher than a cell. */
void TerrainLightMap_Changed(TerrainLightMap *lm)
{
    int half = (0x40000000 >> lm->shift) * 2;
    double dz = kDropZ;
    char was_placed;
    char mode = 0;
    Terrain *w;
    Vec3d p;
    CollHit hit;
    int mask, ix, iy, iz;
    long double h;

    if (!AppContext_GetTerrainLightmapsUsed())
        return;
    was_placed = lm->placed;
    lm->active = 1;
    if (!lm->isChild) {
        mode = Mem_GetMode();
        if (!mode)
            Mem_SetDefaultMode();
        if (lm->children) {
            Task_Delete(lm->children);
            lm->children = NULL;
        }
        if (!lm->placed)
            place(lm);
    }
    p = lm->place;
    if (!lm->isChild) {
        Vec3d q = lm->place;
        int i;

        q.z = q.z + kLiftZ;
        hit.found = 0;
        for (i = 0; i < 5; i++) {
            Coll_DropPointsMat(lm->world, &hit, &q, 1, &dz, 0xFF, 0, 0x80000000);
            if (hit.found)
                break;
        }
        if (hit.found)
            p.z = hit.pos.z;
    }
    w = lm->world;
    mask = -half;
    p.x = w->offset[0] + p.x;
    p.y = w->offset[1] + p.y;
    p.z = w->offset[2] + p.z;
    ix = x87_fistp(p.x) & mask;
    lm->min.x = ix;
    iy = x87_fistp(p.y) & mask;
    lm->min.y = iy;
    iz = x87_fistp(p.z) & mask;
    lm->min.z = iz;
    p = lm->min;
    lm->max.x = (long double)half + ix;
    lm->max.y = (long double)half + iy;
    lm->max.z = (long double)half + iz;
    h = half * kHalf;
    p.x = h + p.x;
    p.y = h + p.y;
    p.x = p.x - w->offset[0];
    p.y = p.y - w->offset[1];
    p.z = h + p.z - w->offset[2];
    lm->min.x = lm->min.x - w->offset[0];
    lm->min.y = lm->min.y - w->offset[1];
    lm->min.z = lm->min.z - w->offset[2];
    lm->max.x = lm->max.x - w->offset[0];
    lm->max.y = lm->max.y - w->offset[1];
    lm->placed = 1;
    lm->max.z = lm->max.z - w->offset[2];
    if (!was_placed)
        DynCube_AddBox(w, lm, &p, &lm->min, &lm->max, 4);
    DynCube_MoveBox(lm->world, lm, &p, &lm->min, &lm->max);
    if (!lm->isChild) {
        int s, n, spawned = 0;
        Vec3d c, q;
        double lo, hi, cell;

        switch (lm->size) {
        case 0x100: s = 7; break;
        case 0x80: s = 6; break;
        case 0x40: s = 5; break;
        case 0x20: s = 4; break;
        case 0x10: s = 3; break;
        case 8: s = 2; break;
        default:
            Error_Show("Illegal shift size");
            for (;;)
                ;
        }
        lm->texShift = 30 - lm->shift - s;
        c = lm->pos;
        lo = hi = c.z;
        cell = 0x40000000 >> lm->shift;
        for (n = 32; n; n--) {
            q = c;
            q.x = (Random_Float(gLightMapRandom) * 2 - kOne) * cell + q.x;
            q.y = (Random_Float(gLightMapRandom) * 2 - kOne) * cell + q.y;
            q.z = q.z + kLiftZ;
            Coll_DropPointsMat(lm->world, &hit, &q, 1, &dz, 0xFF, 0, 0x80000000);
            if (hit.found) {
                double v = hit.pos.z;

                if (!(v >= lo))
                    lo = v;
                if (v > hi)
                    hi = v;
            }
        }
        q = c;
        q.z = c.z - cell;
        while (!(lo >= q.z)) {
            if (!spawned) {
                lm->children = Task_New(lm, Task_GetRootType(), NULL);
                spawned = 1;
            }
            q.z = q.z - cell;
            TerrainLightMap_NewChild(lm->children, lm, &q);
            q.z = q.z - cell;
        }
        q = c;
        q.z = cell + c.z;
        while (hi > q.z) {
            if (!spawned) {
                lm->children = Task_New(lm, Task_GetRootType(), NULL);
                spawned = 1;
            }
            q.z = q.z + cell;
            TerrainLightMap_NewChild(lm->children, lm, &q);
            q.z = q.z + cell;
        }
        if (!mode)
            Mem_ClearMode();
    }
    TerrainLightMap_SetDrawInfo(lm);
}
HOOK(0x4F8780, TerrainLightMap_Changed);

void TerrainLightMap_SetDrawInfo(TerrainLightMap *lm)
{
    LightMapDraw *d = &gLightMapDraw[lm->slot];
    int s;

    d->texture = lm->texture;
    d->scale = kOne;
    s = 0x17 - lm->shift;
    d->shift = s;
    d->scale = kOne / (1 << s) * d->scale;
    d->texScale = kTexSize / lm->size * kHalfF;
    d->x = lm->pos.x - (0x40000000 >> lm->shift);
    d->y = lm->pos.y - (0x40000000 >> lm->shift);
}
HOOK(0x4F8EF0, TerrainLightMap_SetDrawInfo);

void TerrainLightMap_Shutdown(void)
{
    QTaskList_Free(gTerrainLightMapParams);
    TaskType_Free(gTerrainLightMapType);
}
HOOK(0x4F8FB0, TerrainLightMap_Shutdown);

uint16_t TerrainLightMap_GetType(void)
{
    return gTerrainLightMapType;
}
HOOK(0x4F8FD0, TerrainLightMap_GetType);

/* a child covering the next cell up or down, sharing the parent's texture */
TerrainLightMap *TerrainLightMap_NewChild(Task *parent, TerrainLightMap *lm, const Vec3d *place)
{
    TerrainLightMap *t;
    TaskHandler fn;

    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        t = (TerrainLightMap *)Task_New(parent, gTerrainLightMapType, NULL);
        Mem_ClearMode();
    } else {
        t = (TerrainLightMap *)Task_New(parent, gTerrainLightMapType, NULL);
    }
    t->isChild = 1;
    t->shift = lm->shift;
    t->slot = lm->slot;
    t->size = lm->size;
    t->texShift = lm->texShift;
    t->texture = lm->texture;
    t->place = *place;
    fn = TASK_HANDLER(Param_GetChangedEvent(), t->task.type);
    if (fn)
        fn(t, NULL);
    return t;
}
HOOK(0x4F8FE0, TerrainLightMap_NewChild);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, A76CD0, A76CD0, 0xC);
ALIAS(gLightMapRandom, pad_A76CD0, 0);
BSS(A76CD0) uint16_t gTerrainLightMapType;
PAD(igb, A76CD0, A76CDE, 0x26);
BSS(A76CD0) void *gTerrainLightMapParams;
BSS(B97D24) int gVisibleLightMapCount;
ALIAS(gCounterB, gVisibleLightMapCount, 0);
PAD(igb, BA80A0, BA80A0, 0x10);
ALIAS(gLightMapDraw, pad_BA80A0, 0);
PAD(igb, BC00A0, BC00A0, 0x2000);
ALIAS(gLightMapTasks, pad_BC00A0, 0);
