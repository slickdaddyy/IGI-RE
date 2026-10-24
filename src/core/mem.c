#include "hook.h"
#include "igi.h"
#include "core/error.h"
#include "core/mem.h"

ORIG(0x4AA670, void *, crt_malloc, (size_t size));
ORIG(0x4A6A50, void, crt_free, (void *p));
ORIG(0x4C1780, void, Debug_Log, (void));

extern int gMemCount;
extern char gMemReady;
extern char gMemMode;
extern char gMemModeStack[];
extern int gMemModeTop;
extern List gMemList;

void Mem_SetDefaultMode(void)
{
    gMemMode = 1;
}
HOOK(0x4B0D40, Mem_SetDefaultMode);

void Mem_ClearMode(void)
{
    gMemMode = 0;
}
HOOK(0x4B0D50, Mem_ClearMode);

void Mem_Init(void)
{
    gMemList.head = (Node *)&gMemList.tail;
    gMemList.tail = NULL;
    gMemList.tailpred = (Node *)&gMemList;
    gMemReady = 1;
    Mem_SetDefaultMode();
    gMemModeTop = 16;
}
HOOK(0x4B0BA0, Mem_Init);

void Mem_Shutdown(void)
{
    int count = 0;
    Node *n, *next;

    Debug_Log();
    n = gMemList.head;
    for (next = n->next; next; next = next->next) {
        Mem_Free(((MemBlock *)n)->user);
        n = next;
        count++;
    }
    gMemReady = 0;
    Mem_ClearMode();
    if (count)
        Warning_Show("%d block%s of memory have not been deallocated.", count, count == 1 ? "" : "s");
    if (gMemCount)
        Warning_Show("Internal memory allocator inconsistency: Totalalloc: %d", gMemCount);
}
HOOK(0x4B0BE0, Mem_Shutdown);

void Mem_PushMode(char mode)
{
    gMemModeStack[--gMemModeTop] = gMemMode;
    gMemMode = mode;
}
HOOK(0x4B0D60, Mem_PushMode);

void Mem_PopMode(void)
{
    gMemMode = gMemModeStack[gMemModeTop++];
}
HOOK(0x4B0D90, Mem_PopMode);

void *Mem_Alloc(size_t size, size_t align)
{
    size_t total = size + sizeof(MemBlock) + align;
    MemBlock *b;
    uintptr_t user;

    gMemCount++;
    b = crt_malloc(total);
    if (!b) {
        size_t sum_size = 0, sum_total = 0;
        Node *n;

        for (n = gMemList.head; n->next; n = n->next) {
            sum_total += ((MemBlock *)n)->total;
            sum_size += ((MemBlock *)n)->size;
        }
        Error_Show("Mem_Alloc() failed to allocate %d bytes.\r\nTotal allocated by user: %d.\r\nTotal by Mem_Alloc():%d.",
                   total, sum_size, sum_total);
        for (;;)
            ;
    }
    user = ((uintptr_t)b + sizeof(MemBlock) - 1 + align) & ~(uintptr_t)(align - 1);
    *(MemBlock **)((user - 4) & ~(uintptr_t)3) = b;
    b->node.prev = NULL;
    b->node.next = NULL;
    if (!gMemReady)
        Mem_Init();
    List_AddTail(&gMemList, &b->node);
    b->user = (void *)user;
    b->total = total;
    b->size = size;
    return (void *)user;
}
HOOK(0x4B0C60, Mem_Alloc);

void Mem_Free(void *p)
{
    MemBlock *b;

    gMemCount--;
    b = *(MemBlock **)(((uintptr_t)p - 4) & ~(uintptr_t)3);
    List_Remove(&b->node);
    crt_free(b);
}
HOOK(0x4B0D10, Mem_Free);

char Mem_GetMode(void)
{
    return gMemMode;
}
HOOK(0x4B0DB0, Mem_GetMode);
