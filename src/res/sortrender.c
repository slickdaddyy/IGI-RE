#include "hook.h"
#include "igi.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x497800, void *, Frame_Alloc, (int size));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));

typedef struct SortItem {
    void (*fn)(void *data);
    void *data;
    unsigned int key;
} SortItem;

typedef struct SortList {
    int count;
    int capacity;
    SortItem items[];
} SortList;

typedef struct SortTask {
    void *next;
    int task;
    int unk8;
    SortList *list;
} SortTask;

extern int gSortTask;
extern char gSortData[];
extern SortList *gSortList;
extern int gSortDataUsed;
extern int gSortActive;

void SortRender_Sort(SortItem *items, int count);

void SortRender_Flush(SortTask *t)
{
    SortList *l = t->list;
    int i;

    if (l->count) {
        SortRender_Sort(l->items, l->count);
        for (i = 0; i < l->count; i++)
            l->items[i].fn(l->items[i].data);
        l->count = 0;
    }
    gSortDataUsed = 0;
}
HOOK(0x4B7100, SortRender_Flush);

/* Radix sort, positive keys come out largest first */
void SortRender_Sort(SortItem *items, int count)
{
    int hist[4][256] = {{0}};
    SortItem tmp[count > 0 ? count : 1];
    SortItem *src = items, *dst = tmp, *t;
    int i, pass, sum;

    for (i = 0; i < count; i++) {
        unsigned int k = items[i].key;

        if (!(k & 0x80000000))
            k ^= 0x7FFFFFFF;
        items[i].key = k;
        hist[0][k & 0xFF]++;
        hist[1][k >> 8 & 0xFF]++;
        hist[2][k >> 16 & 0xFF]++;
        hist[3][k >> 24]++;
    }
    for (pass = 0; pass < 4; pass++) {
        sum = 0;
        for (i = 0; i < 256; i++) {
            int n = hist[pass][i];

            hist[pass][i] = sum;
            sum += n;
        }
    }
    for (pass = 0; pass < 4; pass++) {
        for (i = 0; i < count; i++)
            dst[hist[pass][src[i].key >> pass * 8 & 0xFF]++] = src[i];
        t = src;
        src = dst;
        dst = t;
    }
}
HOOK(0x4B7160, SortRender_Sort);

void *SortRender_Add(void (*fn)(void *), int size, unsigned int key)
{
    SortItem *it = &gSortList->items[gSortList->count++];

    it->data = gSortData + gSortDataUsed;
    gSortDataUsed += size;
    it->fn = fn;
    it->key = key;
    return it->data;
}
HOOK(0x4B72E0, SortRender_Add);

SortList *SortRender_NewList(int capacity)
{
    SortList *l = Mem_Alloc(capacity * sizeof(SortItem) + 8, 4);

    l->capacity = capacity;
    l->count = 0;
    return l;
}
HOOK(0x4B7330, SortRender_NewList);

void SortRender_Submit(SortList *l)
{
    SortTask *t = Frame_Alloc(sizeof(SortTask));

    t->task = gSortTask;
    t->list = l;
    RenderQueue_Add(0, t);
    gSortList = NULL;
    gSortActive = 0;
}
HOOK(0x4B7360, SortRender_Submit);

void SortRender_Begin(SortList *l)
{
    gSortActive = 1;
    gSortList = l;
}
HOOK(0x4B73A0, SortRender_Begin);

void SortRender_Init(void)
{
    gSortTask = Primitive_Register("SortRender");
    Primitive_SetHandler(gSortTask, SortRender_Flush);
}
HOOK(0x4B73C0, SortRender_Init);

void SortRender_Shutdown(void)
{
    Primitive_Unregister(gSortTask);
}
HOOK(0x4B73E0, SortRender_Shutdown);
