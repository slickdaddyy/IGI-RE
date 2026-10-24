#include <string.h>

#include "hook.h"
#include "igi.h"
#include "magicobj/magicobj.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4017C0, char, TaskEvent_Alloc, (uint16_t type));
ORIG(0x401860, void, TaskEvent_Set, (uint16_t type, uint8_t index));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401D30, void, TaskEvent_MapSlot, (uint8_t slot, int bit));
ORIG(0x401D50, void, TaskEvent_UnmapSlot, (uint8_t slot));
ORIG(0x48F250, int, AppContext_GetTerrainLightmapsUsed, (void));
ORIG(0x4B0B00, void, Pool_Init, (MagicPool *pool, int stride, int count, int index_offset));
ORIG(0x4B0B70, void, Pool_Free, (MagicPool *pool));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B22F0, void, Texture_Free, (void *tex));
ORIG(0x4B23B0, void, Texture_Create, (void *tex, void *palette, int w, int h, int pitch, int unk1, int unk2,
                                      int flags, void *data));
ORIG(0x4B2610, void, Texture_SetActiveTMU, (int tmu));
ORIG(0x4B2670, int, Texture_GetTMUs, (void));
ORIG(0x4B5B60, int, Resource_Exists, (const char *path));
ORIG(0x4B5F00, void *, Resource_Load, (const char *path, int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *path));
ORIG(0x4B63D0, void, Resource_Flush, (void *res));
ORIG(0x4B7CF0, void, Palette_Create, (void *palette, int *colors, int unk));
ORIG(0x4B7D30, void, Palette_Free, (void *palette));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, MagicTask *t));
ORIG(0x4CE620, void, DynCube_UpdateStart, (World *w, int room));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D1220, void, QCube_Clear, (void));
ORIG(0x4E54E0, void, CellMask_Free, (void *p));
ORIG(0x5175A0, void *, TexSet_Load, (const char *path));
ORIG(0x5177F0, void, TexSet_Free, (void *p));

#define gTaskHandlers ((MagicHandler (*)[0x180])0xA96AE0)
extern MagicNode *gTaskStack[];
extern int gTaskStackTop;
extern int gTaskHandlerDepth;
extern uint16_t gDynCubeObjType;
extern char gWorldEvents[];
extern int gWorldUnkA44348[];
extern char gWorldName[];
extern Lightmap gLightmaps[];
#define gLightmapCount 0x800
extern char gUnkB81920[];
extern char gUnkB81A20[];
extern int gUnkB97D20;
extern int gCounterA;
extern int gCounterB;
extern int gCounterC;
extern int gCounterD;
extern int gCounterE;
extern int gSavedCounters[];

void Coll_ResetMods(void)
{
    gCounterA = 0;
    gCounterB = 0;
    gCounterC = 0;
    gCounterD = 0;
    gCounterE = 0;
}
HOOK(0x4C5810, Coll_ResetMods);

void Coll_SaveMods(void)
{
    gSavedCounters[0] = gCounterA;
    gSavedCounters[1] = gCounterB;
    gSavedCounters[2] = gCounterC;
    gSavedCounters[3] = gCounterD;
    gSavedCounters[4] = gCounterE;
}
HOOK(0x4C5830, Coll_SaveMods);

void Coll_RestoreMods(void)
{
    gCounterA = gSavedCounters[0];
    gCounterB = gSavedCounters[1];
    gCounterC = gSavedCounters[2];
    gCounterD = gSavedCounters[3];
    gCounterE = gSavedCounters[4];
}
HOOK(0x4C5870, Coll_RestoreMods);

int DynCubeTree_GetChildCount(int kind)
{
    switch (kind) {
    case 0:
        return 1;
    case 1:
    case 2:
    case 3:
        return 2;
    case 4:
        return 4;
    case 5:
        return 8;
    }
    Error_Show("Invalid dyncubetree");
    for (;;)
        ;
}
HOOK(0x4C58B0, DynCubeTree_GetChildCount);

void World_Init(void)
{
    int i;

    for (i = 8; i <= 13; i++)
        TaskEvent_Set(0, i);
    TaskEvent_MapSlot(4, 0);
    TaskEvent_MapSlot(5, 1);
    TaskEvent_MapSlot(8, 3);
    TaskEvent_MapSlot(9, 4);
    TaskEvent_MapSlot(10, 5);
    TaskEvent_MapSlot(11, 6);
    TaskEvent_MapSlot(13, 2);
    gWorldEvents[0] = TaskEvent_Alloc(0x181);
    gWorldEvents[1] = TaskEvent_Alloc(0x181);
    TaskType_SetHandler(gDynCubeObjType, gWorldEvents[1], DynCubeObj_DeactivateHandler);
    memset(gWorldUnkA44348, 0, 17 * sizeof(int));
}
HOOK(0x4C5900, World_Init);

void DynCubeObj_DeactivateHandler(MagicTask *t, void **arg)
{
    MagicNode *n = t->head;
    MagicHandler h;

    if (n->next) {
        while (n) {
            gTaskStack[gTaskStackTop++] = n->next && n->next->next ? n->next : NULL;
            h = gTaskHandlers[World_GetEvent1()][((MagicTask *)n)->type];
            if (h)
                h(n, arg);
            n = gTaskStack[--gTaskStackTop];
        }
    }
    DynCube_Remove(*arg, t);
}
HOOK(0x4C5A10, DynCubeObj_DeactivateHandler);

void World_Shutdown(void)
{
    TaskEvent_UnmapSlot(4);
    TaskEvent_UnmapSlot(5);
    TaskEvent_UnmapSlot(8);
    TaskEvent_UnmapSlot(9);
    TaskEvent_UnmapSlot(10);
    TaskEvent_UnmapSlot(11);
    TaskEvent_UnmapSlot(13);
    TaskEvent_Free(8);
    TaskEvent_Free(9);
    TaskEvent_Free(10);
    TaskEvent_Free(11);
    TaskEvent_Free(12);
    TaskEvent_Free(13);
    TaskEvent_Free(gWorldEvents[0]);
    TaskEvent_Free(gWorldEvents[1]);
}
HOOK(0x4C5AA0, World_Shutdown);

World *World_New(int rooms)
{
    World *w = Mem_Alloc(sizeof(World), 4);
    WorldRoom *r;
    int i;

    w->unkDFC = 0;
    w->unk0C = NULL;
    w->lightmapPath[0] = 0;
    memset(w->unkD1C, 0, sizeof(w->unkD1C));
    w->unkD18 = 0;
    w->unkDEC = 0;
    w->lightmaps = NULL;
    for (i = 0; i < gLightmapCount; i++) {
        gLightmaps[i].size = 0;
        gLightmaps[i].data = NULL;
    }
    w->unkDC4 = NULL;
    w->unkDC0 = NULL;
    w->unkDE0 = 0;
    w->unkDE4 = 0;
    w->unkDE8 = 0;
    w->texbank = NULL;
    w->palette = NULL;
    w->unk08 = NULL;
    w->unk04 = NULL;
    w->unkDCC = NULL;
    w->unkDD0 = NULL;
    Pool_Init(&w->pool, 0x40, rooms, 4);
    for (i = 0; i < 6; i++) {
        r = (WorldRoom *)(w->pool.data + w->pool.slots[w->pool.used] * w->pool.stride);
        *(int *)((char *)r + w->pool.index_offset) = w->pool.used;
        w->pool.used++;
        w->rooms[i] = r;
        DynCubeNode_Init(w, r, 0, 0x40000000, i);
        w->unkE8[i] = 0;
        DynCube_UpdateStart(w, i);
        memset(w->unk118[i], 0, sizeof(w->unk118[i]));
        w->unk100[i] = gTaskHandlerDepth;
    }
    w->unkDC8 = 0;
    w->unkDF0 = 0;
    gUnkB97D20 = 0;
    Coll_ResetMods();
    return w;
}
HOOK(0x4C5B20, World_New);

void DynCubeNode_Init(World *w, WorldRoom *r, int unk04, int unk00, int index)
{
    (void)w;
    r->unk38 = 0x8000;
    r->unk3A = 0x8000;
    r->unk30 = 0;
    r->unk04 = unk04;
    r->unk00 = unk00;
    memset(r->unk08, 0, sizeof(r->unk08));
    r->index = index;
    r->unk3C = NULL;
}
HOOK(0x4C5C90, DynCubeNode_Init);

static void free_unshared(void **list, const char *shared)
{
    int i;

    for (i = 0; i < 0x100; i++) {
        if (list[i * 3] && !shared[i]) {
            Mem_Free(list[i * 3]);
            list[i * 3] = NULL;
        }
    }
}

void World_Free(World *w)
{
    int i, slot, last, other, tmp;
    char *lastItem;
    int *slots;

    if (w->unkDC0) {
        Mem_Free(w->unkDC0);
        w->unkDC0 = NULL;
    }
    if (w->unkDC4) {
        Mem_Free(w->unkDC4);
        w->unkDC4 = NULL;
    }
    if (w->texbank) {
        TexSet_Free(w->texbank);
        w->texbank = NULL;
    }
    if (w->palette) {
        Palette_Free(w->palette);
        Mem_Free(w->palette);
        w->palette = NULL;
    }
    if (w->unkDCC)
        free_unshared(w->unkDCC, gUnkB81A20);
    if (w->unkDD0)
        free_unshared(w->unkDD0, gUnkB81920);
    if (w->unk08)
        Resource_Flush(w->unk08);
    if (w->unk0C) {
        Resource_Flush(w->unk0C);
        QCube_Clear();
    }
    if (w->unk04)
        Resource_Flush(w->unk04);
    if (w->unkDCC)
        Resource_Flush(w->unkDCC);
    if (w->unkDD0)
        Resource_Flush(w->unkDD0);
    for (i = 0; i < gLightmapCount; i++) {
        if (gLightmaps[i].data)
            Texture_Free(gLightmaps[i].texture);
    }
    for (i = 0; i < 6; i++) {
        if (w->rooms[i]->unk3C)
            CellMask_Free(w->rooms[i]->unk3C);
        /* inlined pool release: swap the room's slot with the last used one */
        slot = *(int *)((char *)w->rooms[i] + w->pool.index_offset);
        last = --w->pool.used;
        if (slot != last) {
            slots = w->pool.slots;
            lastItem = w->pool.data + slots[last] * w->pool.stride;
            other = *(int *)(lastItem + w->pool.index_offset);
            tmp = slots[slot];
            slots[slot] = slots[other];
            w->pool.slots[other] = tmp;
            *(int *)(lastItem + w->pool.index_offset) = slot;
        }
    }
    Pool_Free(&w->pool);
    Mem_Free(w);
}
HOOK(0x4C5CE0, World_Free);

/* loads <name>.tex and, when enabled, the <name>.lmp lightmaps */
void World_LoadTextures(World *w, const char *name)
{
    char path[128];
    int colors[256];
    int size, n, off, i;
    int *p;
    Lightmap *lm;

    strcpy(gWorldName, name);
    strcpy(path, name);
    strcat(path, ".tex");
    if (Texture_GetTMUs() > 1)
        Texture_SetActiveTMU(0);
    w->texbank = TexSet_Load(path);
    if (Texture_GetTMUs() > 1)
        Texture_SetActiveTMU(-1);
    for (i = 0; i < 256; i++)
        colors[i] = i << 16;
    w->palette = Mem_Alloc(0xC, 4);
    Palette_Create(w->palette, colors, 1);
    if (!AppContext_GetTerrainLightmapsUsed())
        return;
    strcpy(path, name);
    strcat(path, ".lmp");
    strcpy(w->lightmapPath, path);
    if (!Resource_Exists(path))
        return;
    p = Resource_Load(path, &size);
    w->lightmaps = p;
    if (p) {
        lm = gLightmaps;
        for (off = 0; off < size; lm++) {
            n = *p++;
            off += 4;
            if (n) {
                if (Texture_GetTMUs() > 1)
                    Texture_SetActiveTMU(1);
                Texture_Create(lm->texture, w->palette, n, n, n, 1, 0, 0x20000000, p);
                if (Texture_GetTMUs() > 1)
                    Texture_SetActiveTMU(-1);
                lm->size = n;
                lm->data = p;
            }
            p = (int *)((char *)p + n * n);
            off += n * n;
        }
    }
    Resource_Unload(path);
}
HOOK(0x4C5F00, World_LoadTextures);
