#include "hook.h"
#include "igi.h"
#include "res/restypes.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));

PtrList *PtrList_New(int capacity)
{
    PtrList *l = Mem_Alloc(capacity * 4 + sizeof(PtrList), 4);

    l->capacity = capacity;
    l->current = -1;
    l->count = 0;
    l->magic = 0xABCDABCD;
    l->magic2 = 0xDCBADCBA;
    return l;
}
HOOK(0x4B73F0, PtrList_New);

int PtrList_Add(PtrList *l, void *p)
{
    int i = l->count++;
    l->items[i] = p;
    return i;
}
HOOK(0x4B7430, PtrList_Add);

void PtrList_Clear(PtrList *l, int i)
{
    l->items[i] = NULL;
}
HOOK(0x4B7450, PtrList_Clear);
