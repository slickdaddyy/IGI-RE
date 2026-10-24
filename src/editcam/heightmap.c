#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "editcam/editcam.h"

FN_EXTERN(Task_ReturnZero);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
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
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4E5360, uint8_t, DynCubeObj_GetMaskVisibleEvent, (void));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B47E0, void, Random_Seed, (void *r, unsigned int seed));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4C7350, void, DynCube_AddBox, (void *world, void *obj, Vec3d *center, Vec3d *min, Vec3d *max, int tree));
ORIG(0x4C6E70, void, DynCube_MoveBox, (void *world, void *obj, Vec3d *center, Vec3d *min, Vec3d *max));
ORIG(0x4CB9F0, void, Coll_DropPointsMat, (void *world, CollHit *hit, Vec3d *points, int count, const double *dz,
                                          int unk, int unk2, unsigned int flags));
ORIG(0x506C60, int, TextureModifier_RoundSize, (int n));

#define DynCubeObj_MaskVisible (FN(0x4F64E0, Task_ReturnZero))

/* a box of terrain whose .hmp map entry raises or lowers the ground */
typedef struct HeightMap {
    DynCubeObj obj;
    Terrain *world;
    int unk6C;
    Vec3d place;
    Vec3d min;
    Vec3d max;
    int shift;          /* cell size is 2^31 >> shift */
    int hmpIndex;       /* .hmp table entry, -1 none */
    int lastHmp;
    int unkC4;
    int texShift;
    int sizeShift;
    int unkD0, unkD4, unkD8;
    char exclusive;     /* the only entry with its flag set */
    char unkDD;
    char shared;
    char moved;
    char isChild;
    char dropToGround;
    char unkE2;
    char unkE3;
    int size;           /* map width in cells */
    int shown;
    int unkEC, unkF0, unkF4;
    int slot;           /* in gHeightMaps and gCollBumps */
    Task *children;
} HeightMap;

_Static_assert(sizeof(HeightMap) == 0x100, "HeightMap");
_Static_assert(offsetof(HeightMap, slot) == 0xF8, "HeightMap.slot");

extern char gHeightMapRandom[];
extern HeightMap *gHeightMaps[];
extern void *gHeightMapParams;
extern uint16_t gHeightMapType;
extern int gHeightMapCount;
extern char gHmpResident[];
extern int gCollWalkTasks;
extern int gCollBumpList[];
extern int gCollBumpDepth;
extern int gCollBumpCount;
extern CollBump gCollBumps[];
#define gSlotBumps (gCollBumps + 1) /* slots count from 1 */

static const double kLiftZ = 268435456.0;
static const double kHalf = 0.5;
static const double kDropZ = -536870912.0;
static const float kOne = 1.0f;

void HeightMap_Create(HeightMap *hm)
{
    hm->lastHmp = -1;
    hm->slot = -1;
    hm->world = gCurrentWorld;
    hm->moved = 1;
    hm->dropToGround = 1;
    hm->place.x = hm->place.y = hm->place.z = 0;
    hm->min.x = hm->min.y = hm->min.z = 0;
    hm->max.x = hm->max.y = hm->max.z = 0;
    hm->shift = 12;
    hm->hmpIndex = 0xFF;
    hm->unkC4 = 0;
    hm->exclusive = 0;
    hm->size = 0x80;
    hm->unkDD = 0;
    hm->isChild = 0;
    hm->unkEC = 0;
    hm->shared = 0;
    hm->children = NULL;
    hm->unkF0 = 0;
    hm->unkF4 = 0;
    hm->unkD0 = 8;
    hm->unkD4 = 7;
    hm->unkD8 = 7;
    hm->unkE2 = 0;
    Random_Seed(gHeightMapRandom, 0x19111911);
}
HOOK(0x506DE0, HeightMap_Create);

void HeightMap_Enter(HeightMap *hm)
{
    if (hm->hmpIndex >= 0 && hm->world->hmp[hm->hmpIndex].data) {
        hm->shown = 1;
        gCollBumpList[gCollBumpDepth] = hm->slot + 1;
        gCollBumpCount = hm->slot + 1;
        gCollBumpDepth++;
        return;
    }
    hm->shown = 0;
}
HOOK(0x506F00, HeightMap_Enter);

void HeightMap_Leave(HeightMap *hm)
{
    int n;

    if (!hm->shown)
        return;
    gCollBumpCount = 0;
    n = --gCollBumpDepth;
    if (n - 1 >= 0)
        gCollBumpCount = gCollBumpList[n - 1];
}
HOOK(0x506F60, HeightMap_Leave);

void HeightMap_Delete(HeightMap *hm);
void HeightMap_GetParams(HeightMap *hm, void **out);
void HeightMap_Changed(HeightMap *hm);
void HeightMap_Collect(HeightMap *hm);

void HeightMap_Init(void)
{
    HeightMap t;
    uint16_t type;

    Random_Init(gHeightMapRandom);
    Random_Seed(gHeightMapRandom, 0x19111911);
    type = TaskType_New("HeightMap", sizeof(HeightMap), gDynCubeObjType, 0, 0);
    gHeightMapType = type;
    TaskType_SetHandler(type, 0, HeightMap_Create);
    TaskType_SetHandler(gHeightMapType, 13, HeightMap_Collect);
    TaskType_SetHandler(gHeightMapType, 16, HeightMap_Leave);
    TaskType_SetHandler2(gHeightMapType, 2, HeightMap_Delete);
    TaskType_SetHandler(gHeightMapType, Param_GetTableEvent(), HeightMap_GetParams);
    TaskType_SetHandler(gHeightMapType, Param_GetChangedEvent(), HeightMap_Changed);
    TaskType_SetHandler(gHeightMapType, 10, NULL);
    TaskType_SetHandler(gHeightMapType, 11, NULL);
    TaskType_SetHandler(gHeightMapType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gHeightMapType, 9, NULL);
    TaskType_SetHandler(gHeightMapType, 12, NULL);
    TaskType_SetHandler(gHeightMapType, 15, HeightMap_Enter);
    TaskType_SetHandler(gHeightMapType, DynCubeObj_GetMaskVisibleEvent(), DynCubeObj_MaskVisible);
    TaskType_SetHandler(gHeightMapType, 8, NULL);
    TaskType_SetHandler(gHeightMapType, 4, NULL);
    TaskType_SetHandler(gHeightMapType, 5, NULL);
    gHeightMapParams = ParamTable_New(14);
    ParamTable_Add(gHeightMapParams, &t, &t.place, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.shift, Param_Int32(), 7, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.size, Param_Int32(), 7, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.exclusive, Param_Bool8(), 7, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkDD, Param_Bool8(), 6, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkEC, Param_PushButton(), 6, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkF0, Param_PushButton(), 6, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.dropToGround, Param_Bool8(), 6, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.hmpIndex, Param_Int32(), 3, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkD0, Param_Int32(), 7, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkD4, Param_Int32(), 7, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkD8, Param_Int32(), 6, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkF4, Param_PushButton(), 6, NULL);
    ParamTable_Add(gHeightMapParams, &t, &t.unkE2, Param_Bool8(), 6, NULL);
    gCollBumpDepth = 0;
}
HOOK(0x506FA0, HeightMap_Init);

void HeightMap_Delete(HeightMap *hm)
{
    if (!hm->isChild) {
        if (hm->lastHmp != -1 && !gHmpResident[hm->lastHmp]) {
            Mem_Free(hm->world->hmp[hm->lastHmp].data);
            hm->world->hmp[hm->lastHmp].data = NULL;
            hm->world->hmp[hm->lastHmp].data = NULL;
        }
        if (hm->slot != -1) {
            int last = gHeightMapCount - 1;

            if (hm->slot != last) {
                HeightMap *m = gHeightMaps[last];

                gHeightMaps[hm->slot] = m;
                m->slot = hm->slot;
            }
            gHeightMapCount--;
        }
    }
    DynCube_Remove(hm->world, hm);
}
HOOK(0x507340, HeightMap_Delete);

void HeightMap_GetParams(HeightMap *hm, void **out)
{
    *out = gHeightMapParams;
}
HOOK(0x507400, HeightMap_GetParams);

HeightMap *HeightMap_NewChild(Task *parent, HeightMap *hm, Vec3d *place);

/* (size + 1)^2 heights, all at the middle level 0x40 */
static void *new_heights(int size)
{
    int n = (size + 1) * (size + 1);
    void *p = Mem_Alloc(n, 4);

    memset(p, 0x40, n);
    return p;
}

/* Snaps the box to the cell grid, takes a .hmp entry and adds a child box
   for every cell height between the lowest and highest ground under it. */
void HeightMap_Changed(HeightMap *hm)
{
    double dz = kDropZ;
    int cell2 = (0x40000000 >> hm->shift) * 2;
    int mode = 0;
    TerrainMap *maps = NULL;
    Terrain *w;
    Vec3d p;
    CollHit hit;
    int ix, iy, iz;
    double min_y, min_z;
    long double h;
    CollBump *bump;

    gCollWalkTasks = 0;
    if (!hm->isChild) {
        mode = Mem_GetMode();
        if (!mode)
            Mem_SetDefaultMode();
        if (hm->children) {
            Task_Delete(hm->children);
            hm->children = NULL;
        }
        maps = hm->world->hmp;
        if (hm->slot == -1) {
            gHeightMaps[gHeightMapCount] = hm;
            hm->slot = gHeightMapCount;
            if (++gHeightMapCount >= 0x100) {
                Error_Show("Too many texturemodfiers");
                for (;;)
                    ;
            }
        }
    }
    gSlotBumps[hm->slot].heights = NULL;
    p = hm->place;
    if (!hm->isChild) {
        Vec3d q = hm->place;
        int i;

        hit.found = 0;
        for (i = 0; i < 5; i++) {
            Coll_DropPointsMat(hm->world, &hit, &q, 1, &dz, 0xFF, 0, 0x80000000);
            if (hit.found)
                break;
        }
        if (hit.found)
            p.z = hit.pos.z;
    }
    w = hm->world;
    p.x = w->offset[0] + p.x;
    p.y = w->offset[1] + p.y;
    p.z = w->offset[2] + p.z;
    ix = x87_fistp(p.x) & -cell2;
    hm->min.x = ix;
    iy = x87_fistp(p.y) & -cell2;
    min_y = iy;
    hm->min.y = min_y;
    iz = x87_fistp(p.z) & -cell2;
    min_z = iz;
    hm->min.z = min_z;
    p = hm->min;
    hm->max.x = (long double)cell2 + ix;
    hm->max.y = (long double)cell2 + min_y;
    hm->max.z = (long double)cell2 + min_z;
    h = cell2 * kHalf;
    p.x = h + p.x;
    p.y = h + p.y;
    p.z = h + p.z;
    p.x = p.x - w->offset[0];
    p.y = p.y - w->offset[1];
    p.z = p.z - w->offset[2];
    hm->min.x = hm->min.x - w->offset[0];
    hm->min.y = hm->min.y - w->offset[1];
    hm->min.z = hm->min.z - w->offset[2];
    hm->max.x = hm->max.x - w->offset[0];
    hm->max.y = hm->max.y - w->offset[1];
    hm->max.z = hm->max.z - w->offset[2];
    if (hm->moved)
        DynCube_AddBox(w, hm, &p, &hm->min, &hm->max, 4);
    DynCube_MoveBox(hm->world, hm, &p, &hm->min, &hm->max);
    if (!hm->isChild) {
        if (hm->hmpIndex != -1) {
            int size = TextureModifier_RoundSize(hm->size);
            int idx = hm->hmpIndex;
            int s;

            hm->size = size;
            if (!maps[idx].data) {
                for (idx = 0; idx < 0x100 && maps[idx].data; idx++)
                    ;
                if (idx == 0x100) {
                    Warning_Show("limit reached");
                    idx = 0xFF;
                }
                maps[idx].size = size;
                maps[idx].data = new_heights(size);
                gHmpResident[idx] = 0;
                hm->hmpIndex = idx;
            } else {
                if (hm->moved)
                    hm->shared = 1;
                if (idx == hm->lastHmp) {
                    TerrainMap *e = &maps[idx];

                    if (e->size != size) {
                        if (!gHmpResident[idx]) {
                            Mem_Free(e->data);
                            e->data = NULL;
                        } else {
                            hm->shared = 0;
                        }
                        e->size = size;
                        e->data = new_heights(size);
                        gHmpResident[idx] = 0;
                    }
                } else {
                    hm->size = maps[idx].size;
                    hm->exclusive = maps[idx].flag;
                }
            }
            switch (size) {
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
            hm->texShift = 30 - hm->shift - s;
            hm->sizeShift = s - 2;
        }
        if (hm->hmpIndex == -1 && hm->lastHmp != -1) {
            if (!gHmpResident[hm->lastHmp]) {
                Mem_Free(maps[hm->lastHmp].data);
                maps[hm->lastHmp].data = NULL;
            }
            maps[hm->lastHmp].data = NULL;
        }
        hm->lastHmp = hm->hmpIndex;
        if (hm->exclusive) {
            int i;

            for (i = 0; i < 0x100; i++)
                maps[i].flag = 0;
            if (hm->hmpIndex >= 0)
                maps[hm->hmpIndex].flag = 1;
        } else if (hm->hmpIndex >= 0) {
            maps[hm->hmpIndex].flag = 0;
        }
        hm->moved = 0;
        if (hm->dropToGround) {
            Vec3d c = hm->obj.pos, q;
            double lo, hi, cell;
            int spawned = 0;
            int n;

            lo = hi = c.z;
            cell = 0x40000000 >> hm->shift;
            for (n = 200; n; n--) {
                long double r;

                q = c;
                r = Random_Float(gHeightMapRandom);
                q.x = (r + r - kOne) * cell + q.x;
                r = Random_Float(gHeightMapRandom);
                q.y = (r + r - kOne) * cell + q.y;
                q.z = q.z + kLiftZ;
                Coll_DropPointsMat(hm->world, &hit, &q, 1, &dz, 0xFF, 0, 0x80000000);
                if (hit.found) {
                    long double v = hit.pos.z;

                    if (v < lo)
                        lo = v;
                    if (v > hi)
                        hi = v;
                }
            }
            q = c;
            q.z = c.z - cell;
            while (!(lo >= q.z)) {
                if (!spawned) {
                    hm->children = Task_New(hm, Task_GetRootType(), NULL);
                    spawned = 1;
                }
                q.z = q.z - cell;
                HeightMap_NewChild(hm->children, hm, &q);
                q.z = q.z - cell;
            }
            q = c;
            q.z = cell + c.z;
            while (hi > q.z) {
                if (!spawned) {
                    hm->children = Task_New(hm, Task_GetRootType(), NULL);
                    spawned = 1;
                }
                q.z = q.z + cell;
                HeightMap_NewChild(hm->children, hm, &q);
                q.z = q.z + cell;
            }
        }
        if (!mode)
            Mem_ClearMode();
    }
    bump = &gSlotBumps[hm->slot];
    maps = hm->world->hmp;
    bump->sizeShift = hm->sizeShift;
    bump->scale = kOne;
    bump->unk0 = hm->unkC4;
    bump->texShift = hm->texShift;
    bump->heights = (const signed char *)maps[hm->hmpIndex].data;
    bump->scale = kOne / (1 << bump->texShift) * bump->scale;
    bump->x = hm->obj.pos.x - (0x40000000 >> hm->shift);
    bump->y = hm->obj.pos.y - (0x40000000 >> hm->shift);
    hm->moved = 0;
    hm->exclusive = maps[hm->hmpIndex].flag;
    gCollWalkTasks = 1;
}
HOOK(0x507410, HeightMap_Changed);

void HeightMap_Collect(HeightMap *hm)
{
    if (hm->hmpIndex >= 0 && gSlotBumps[hm->slot].heights)
        gCollBumpList[gCollBumpDepth++] = hm->slot;
}
HOOK(0x507D00, HeightMap_Collect);

void HeightMap_Shutdown(void)
{
    QTaskList_Free(gHeightMapParams);
    TaskType_Free(gHeightMapType);
}
HOOK(0x507D40, HeightMap_Shutdown);

/* a box one cell above or below, sharing the parent's map */
HeightMap *HeightMap_NewChild(Task *parent, HeightMap *hm, Vec3d *place)
{
    HeightMap *t;
    TaskFn fn;

    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        t = (HeightMap *)Task_New(parent, gHeightMapType, NULL);
        Mem_ClearMode();
    } else {
        t = (HeightMap *)Task_New(parent, gHeightMapType, NULL);
    }
    t->isChild = 1;
    t->shift = hm->shift;
    t->size = hm->size;
    t->texShift = hm->texShift;
    t->sizeShift = hm->sizeShift;
    t->unkC4 = hm->unkC4;
    t->slot = hm->slot;
    t->hmpIndex = hm->hmpIndex;
    t->max = hm->max;
    t->min = hm->min;
    t->place = *place;
    fn = HANDLER(Param_GetChangedEvent(), t->obj.task.type);
    if (fn)
        fn(t, NULL);
    return t;
}
HOOK(0x507D60, HeightMap_NewChild);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, A7AD10, A7AD10, 0x428);
ALIAS(gHeightMaps, pad_A7AD10, 0);
PAD(igb, A7AD10, A7B138, 0x10C);
ALIAS(gHeightMapRandom, pad_A7B138, 0);
BSS(A7AD10) void *gHeightMapParams;
BSS(A7AD10) uint16_t gHeightMapType;
PAD(igb, A7AD10, A7B24A, 0x6);
BSS(A7AD10) int gHeightMapCount;
PAD(igb, B81920, B81920, 0x100);
ALIAS(gHmpResident, pad_B81920, 0);
ALIAS(gUnkB81920, pad_B81920, 0);
PAD(igb, B97D40, B97D40, 0x40);
ALIAS(gCollBumpList, pad_B97D40, 0);
