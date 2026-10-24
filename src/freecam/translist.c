#include "freecam/freecam.h"

/* TransparencyList collects transparent triangle batches during the frame
   and draws them back to front at the end of the scene. */

typedef struct TransBatch {
    int unk0;
    int count;          /* triangles */
    char unk8[0x10];
    int stride;         /* bytes per vertex */
    uint32_t fvf;
    char verts[];
} TransBatch;

typedef struct TransEntry {
    float depth;
    TransBatch *batch;
} TransEntry;

typedef struct TransparencyList {
    Task task;
    char unk20[0x48];
    void *world;
    int count;
    int used;
    char *buffer;
    TransEntry *entries;
    int unk7C;
} TransparencyList;

typedef struct RenderNode {
    void *next;
    int primitive;
    int unk8;
    TransparencyList *list;
} RenderNode;

typedef struct D3DDevice D3DDevice;
typedef struct D3DDeviceVtbl {
    void *unk0[25];
    long(__stdcall *DrawPrimitive)(D3DDevice *dev, int type, uint32_t fvf, void *verts, uint32_t count,
                                   uint32_t flags);
} D3DDeviceVtbl;
struct D3DDevice {
    const D3DDeviceVtbl *vtbl;
};

_Static_assert(sizeof(TransparencyList) == 0x80, "TransparencyList");
_Static_assert(offsetof(TransBatch, verts) == 0x20, "TransBatch");
_Static_assert(offsetof(D3DDeviceVtbl, DrawPrimitive) == 0x64, "DrawPrimitive");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int id, void *fn));
ORIG(0x497800, void *, Frame_Alloc, (int size));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));

void crt_qsort(void *base, unsigned num, unsigned width, int (*comp)(const void *, const void *));

extern uint16_t gTransListType;
extern int gTransPrimitive;
extern int gTransStartPrimitive;
extern char *gTransBufferEnd;
extern TransEntry *gTransEntryEnd;
extern int *gTransUsed;
extern int *gTransCount;
extern int gRenderQueueCount;
extern D3DDevice *gD3DDevice;

void TransparencyList_Delete(TransparencyList *l)
{
    DynCube_ListRemove(l->world, l, -1);
    Mem_Free(l->buffer);
    l->buffer = NULL;
    Mem_Free(l->entries);
    l->entries = NULL;
}
HOOK(0x531F60, TransparencyList_Delete);

void TransparencyList_Render(TransparencyList *l);
void TransparencyList_Start(RenderNode *n);
void TransparencyList_Draw(RenderNode *n);

void TransparencyList_Init(void)
{
    gTransListType = TaskType_New("TransparencyList", sizeof(TransparencyList), gDynCubeObjType, 0, 0);
    TaskType_SetHandler(gTransListType, 4, TransparencyList_Render);
    TaskType_SetHandler2(gTransListType, 2, TransparencyList_Delete);
    gTransPrimitive = Primitive_Register("TransparencyListPrimitive");
    gTransStartPrimitive = Primitive_Register("TransparencyListStartPrimitive");
    Primitive_SetHandler(gTransStartPrimitive, TransparencyList_Start);
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gTransPrimitive, TransparencyList_Draw);
}
HOOK(0x531FA0, TransparencyList_Init);

/* back to front */
int TransparencyList_Compare(const TransEntry *a, const TransEntry *b)
{
    if (a->depth > b->depth)
        return -1;
    if (!(a->depth >= b->depth))
        return 1;
    return 0;
}
HOOK(0x5320D0, TransparencyList_Compare);

void TransparencyList_Draw(RenderNode *n)
{
    int count = n->list->count, i, k;
    TransEntry *e = n->list->entries;

    crt_qsort(e, count, sizeof(TransEntry), (int (*)(const void *, const void *))TransparencyList_Compare);
    for (i = 0; i < count; i++) {
        TransBatch *b = e[i].batch;
        int stride = b->stride;
        uint32_t fvf = b->fvf;
        char *v = b->verts;

        for (k = b->count; k > 0; k--) {
            gD3DDevice->vtbl->DrawPrimitive(gD3DDevice, 4, fvf, v, 3, 0);
            v += stride * 3;
        }
    }
    gTransBufferEnd = NULL;
}
HOOK(0x532030, TransparencyList_Draw);

void TransparencyList_Start(RenderNode *n)
{
    TransparencyList *l = n->list;

    gTransBufferEnd = l->buffer + l->used;
    gTransUsed = &l->used;
    gTransCount = &l->count;
    gTransEntryEnd = l->entries + l->count;
}
HOOK(0x532100, TransparencyList_Start);

void TransparencyList_Render(TransparencyList *l)
{
    RenderNode *n = Frame_Alloc(sizeof(RenderNode));

    n->primitive = gTransPrimitive;
    n->list = l;
    RenderQueue_Add(0, n);
    n = Frame_Alloc(sizeof(RenderNode));
    n->list = l;
    n->primitive = gTransStartPrimitive;
    RenderQueue_Add(gRenderQueueCount - 1, n);
    gTransBufferEnd = l->buffer;
    gTransEntryEnd = l->entries;
    gTransUsed = &l->used;
    gTransCount = &l->count;
    *gTransUsed = 0;
    *gTransCount = 0;
}
HOOK(0x532140, TransparencyList_Render);

void TransparencyList_Shutdown(void)
{
    TaskType_Free(gTransListType);
    Primitive_Unregister(gTransPrimitive);
    Primitive_Unregister(gTransStartPrimitive);
}
HOOK(0x5321C0, TransparencyList_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54F884) uint16_t gTransListType INIT(385);
PAD(igd, 54F884, 54F886, 0x2);
BSS(A84620) char *gTransBufferEnd;
BSS(A84620) TransEntry *gTransEntryEnd;
PAD(igb, A84620, A84628, 0x4);
BSS(A84620) int *gTransUsed;
BSS(A84620) int *gTransCount;
BSS(C28F90) int gTransPrimitive;
BSS(C28F90) int gTransStartPrimitive;
