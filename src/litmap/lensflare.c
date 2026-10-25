#include "hook.h"
#include "litmap/litmap.h"
#include "dirlight/dirlighttypes.h"
#include "nodereg/noderegvars.h"

ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F60, void *, Param_Real32x3, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F19C0, int, ParamType_New, (const char *name, void *parent));
ORIG(0x4F1A40, void, ParamType_Free, (int type));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x5274C0, uint16_t, Dirlight_GetType, (void));

/* a flare drawn along the direction of its parent Dirlight */
typedef struct Lensflare {
    Task task;
    char unk20[0x48];
    void *world;
    float dir[3];
    float color[3];
    int lightId;
    int unk88[4];
} Lensflare;

_Static_assert(offsetof(Lensflare, world) == 0x68, "world");
_Static_assert(sizeof(Lensflare) == 0x98, "Lensflare");

/* one sprite of a flare */
typedef struct LensflareItem {
    Task task;
    void *world;
    float size;
    float range;
    float color[3];
    void *texture;
    int textureIndex;
} LensflareItem;

_Static_assert(sizeof(LensflareItem) == 0x40, "LensflareItem");

typedef struct LensflareSlot {
    int id;
    int unk4;
    int unk8;
    int unkC;
    int unk10;
} LensflareSlot;

#define LENSFLARE_TEXTURES 10

extern uint16_t gLensflareType;
extern uint16_t gLensflareItemType;
extern void *gLensflareParams;
extern void *gLensflareItemParams;
extern int gRangeReal32Type;
extern char gLensflareRandom[];
extern int gLensflareTextureUsed[];
extern char gLensflareTextures[];
extern LensflareSlot gLensflareSlots[];
extern int gLensflareUnusedInts[];

static const float kItemSize = 64.0f;

void Lensflare_Create(Lensflare *l);
void Lensflare_Delete(Lensflare *l);
void Lensflare_GetParams(Lensflare *l, void **out);
void Lensflare_Changed(Lensflare *l);
void LensflareItem_Create(LensflareItem *it);
void LensflareItem_GetParams(LensflareItem *it, void **out);
void LensflareItem_Changed(LensflareItem *it);

void Lensflare_Init(void)
{
    Lensflare t;
    LensflareItem it;
    int i;

    gLensflareType = TaskType_New("Lensflare", sizeof(Lensflare), Task_GetRootType(), 0, 0);
    gLensflareItemType = TaskType_New("LensflareItem", sizeof(LensflareItem), Task_GetRootType(), 0, 0);
    TaskType_SetHandler(gLensflareType, 0, Lensflare_Create);
    TaskType_SetHandler2(gLensflareType, 2, Lensflare_Delete);
    TaskType_SetHandler(gLensflareType, Param_GetTableEvent(), Lensflare_GetParams);
    TaskType_SetHandler(gLensflareType, Param_GetChangedEvent(), Lensflare_Changed);
    TaskType_SetHandler(gLensflareType, 4, Task_Nothing);
    TaskType_SetHandler(gLensflareItemType, 0, LensflareItem_Create);
    TaskType_SetHandler2(gLensflareItemType, 2, Task_Nothing);
    TaskType_SetHandler(gLensflareItemType, Param_GetTableEvent(), LensflareItem_GetParams);
    TaskType_SetHandler(gLensflareItemType, Param_GetChangedEvent(), LensflareItem_Changed);
    gLensflareParams = ParamTable_New(3);
    gLensflareItemParams = ParamTable_New(4);
    gRangeReal32Type = ParamType_New("RangeReal32", Param_Real32());
    ParamTable_Add(gLensflareParams, &t, &t.dir, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gLensflareParams, &t, &t.color, Param_RGB(), 7, NULL);
    ParamTable_Add(gLensflareParams, &t, &t.lightId, Param_Int32(), 7, NULL);
    ParamTable_Add(gLensflareItemParams, &it, &it.color, Param_RGB(), 7, NULL);
    ParamTable_Add(gLensflareItemParams, &it, &it.range, (void *)gRangeReal32Type, 7, NULL);
    ParamTable_Add(gLensflareItemParams, &it, &it.size, Param_Real32(), 7, NULL);
    ParamTable_Add(gLensflareItemParams, &it, &it.textureIndex, Param_Int32(), 7, NULL);
    Random_Init(gLensflareRandom);
    for (i = 0; i < LENSFLARE_TEXTURES; i++)
        gLensflareTextureUsed[i] = 0;
    for (i = 0; i < LENSFLARE_TEXTURES; i++) {
        gLensflareSlots[i].id = -1;
        gLensflareSlots[i].unk8 = 0;
    }
    for (i = 0; i < LENSFLARE_TEXTURES; i++)
        gLensflareUnusedInts[i] = 0;
}
HOOK(0x520BB0, Lensflare_Init);

void LensflareItem_Create(LensflareItem *it)
{
    it->world = gCurrentWorld;
    it->texture = NULL;
    it->textureIndex = 0;
    it->color[0] = it->color[1] = it->color[2] = 1.0f;
    it->range = 0;
    it->size = kItemSize;
}
HOOK(0x520E40, LensflareItem_Create);

void LensflareItem_GetParams(LensflareItem *it, void **out)
{
    *out = gLensflareItemParams;
}
HOOK(0x520E70, LensflareItem_GetParams);

void LensflareItem_Changed(LensflareItem *it)
{
    if (it->textureIndex >= LENSFLARE_TEXTURES)
        it->textureIndex = LENSFLARE_TEXTURES - 1;
    if (it->textureIndex < 0)
        it->textureIndex = 0;
    it->texture = gLensflareTextureUsed[it->textureIndex] ? gLensflareTextures + it->textureIndex * 0x24 : NULL;
}
HOOK(0x520E80, LensflareItem_Changed);

void Lensflare_Create(Lensflare *l)
{
    l->world = gCurrentWorld;
    l->dir[2] = 1.0f;
    l->color[0] = l->color[1] = l->color[2] = 1.0f;
    l->lightId = -1;
    l->dir[0] = l->dir[1] = 0;
    l->unk88[0] = l->unk88[1] = l->unk88[2] = l->unk88[3] = 0;
    DynCube_ListAdd(l->world, l, 0);
}
HOOK(0x520EC0, Lensflare_Create);

void Lensflare_Delete(Lensflare *l)
{
    DynCube_ListRemove(l->world, l, 0);
}
HOOK(0x520F20, Lensflare_Delete);

void Lensflare_GetParams(Lensflare *l, void **out)
{
    *out = gLensflareParams;
}
HOOK(0x520F40, Lensflare_GetParams);

/* takes direction and colour from the light it is attached to or names */
void Lensflare_Changed(Lensflare *l)
{
    Dirlight *d;

    if (l->lightId == -1)
        d = (Dirlight *)l->task.parent;
    else
        d = (Dirlight *)Task_FindById(l->lightId);
    if (!d || d->task.type != Dirlight_GetType())
        return;
    l->dir[0] = d->dir.x;
    l->dir[1] = d->dir.y;
    l->dir[2] = d->dir.z;
    l->color[0] = d->color.x;
    l->color[1] = d->color.y;
    l->color[2] = d->color.z;
}
HOOK(0x520F50, Lensflare_Changed);

void Lensflare_Shutdown(void)
{
    ParamType_Free(gRangeReal32Type);
    QTaskList_Free(gLensflareItemParams);
    QTaskList_Free(gLensflareParams);
    TaskType_Free(gLensflareItemType);
    TaskType_Free(gLensflareType);
}
HOOK(0x520FF0, Lensflare_Shutdown);

uint16_t Lensflare_GetType(void)
{
    return gLensflareType;
}
HOOK(0x521030, Lensflare_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, A83860, 4) int store_A83860[10];
ALIAS(gLensflareUnusedInts, store_A83860, 0);
STORE(igb, A83860, 1) char store_A83888[12];
ALIAS(gLensflareRandom, store_A83888, 0);
BSS(A83860) uint16_t gLensflareType;
PAD(igb, A83860, A83896, 0x2);
BSS(A83860) int gRangeReal32Type;
BSS(A83860) void *gLensflareParams;
PAD(igb, A83860, A838A0, 0x8);
PAD(igb, A83860, A838A8, 0xC0);
ALIAS(gLensflareSlots, pad_A838A8, 0);
BSS(A83860) uint16_t gLensflareItemType;
PAD(igb, A83860, A8396A, 0x6);
BSS(A83860) void *gLensflareItemParams;
STORE(igb, BA1E20, 4) int store_BA1E20[16];
ALIAS(gLensflareTextureUsed, store_BA1E20, 0);
STORE(igb, BA1E20, 1) char store_BA1E60[360];
ALIAS(gLensflareTextures, store_BA1E60, 0);
