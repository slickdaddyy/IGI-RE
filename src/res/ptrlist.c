#include "hook.h"
#include "igi.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));

typedef struct PtrList {
    unsigned int magic;
    int unk4;
    int count;
    int capacity;
    unsigned int magic2;
    void *items[];
} PtrList;

PtrList *PtrList_New(int capacity)
{
    PtrList *l = Mem_Alloc(capacity * 4 + sizeof(PtrList), 4);

    l->capacity = capacity;
    l->unk4 = -1;
    l->count = 0;
    l->magic = 0xABCDABCD;
    l->magic2 = 0xDCBADCBA;
    return l;
}
HOOK(0x4B73F0, PtrList_New);

void PtrList_Add(PtrList *l, void *p)
{
    l->items[l->count++] = p;
}
HOOK(0x4B7430, PtrList_Add);

void PtrList_Clear(PtrList *l, int i)
{
    l->items[i] = NULL;
}
HOOK(0x4B7450, PtrList_Clear);
