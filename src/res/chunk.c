#include "hook.h"
#include "igi.h"
#include "res/resource.h"

#define gChunkError VAR(int, 0xA43E5C)

#define IS_POW2(x) ((~((x) - 1) & (x)) == (x))

Chunk *Chunk_Next(Chunk *c)
{
    if (c && c->next)
        return (Chunk *)((char *)c + c->next);
    return NULL;
}
HOOK(0x4B7470, Chunk_Next);

Chunk *Chunk_FirstChild(Chunk *c)
{
    unsigned int off;

    if (!c || !c->size)
        return NULL;
    if (!IS_POW2(c->align)) {
        gChunkError = 1;
        return NULL;
    }
    off = (c->align + 0x13) & ~(c->align - 1);
    if (off < c->size)
        return (Chunk *)((char *)c + off);
    return NULL;
}
HOOK(0x4B7490, Chunk_FirstChild);

void *Chunk_Data(Chunk *c, Chunk *parent)
{
    unsigned int off;

    if (!parent || !c)
        return NULL;
    off = (char *)c - (char *)parent;
    if (!IS_POW2(parent->align) || off >= parent->size || !IS_POW2(c->align) || c->align > parent->align) {
        gChunkError = 1;
        return NULL;
    }
    if (!c->size)
        return NULL;
    return (char *)parent + ((c->align + off + 0xF) & ~(c->align - 1));
}
HOOK(0x4B74D0, Chunk_Data);

void Chunk_GetInfo(unsigned int *size, unsigned int *align, Chunk *c)
{
    if (!c)
        return;
    if (size)
        *size = c->size;
    if (align)
        *align = c->align;
}
HOOK(0x4B7540, Chunk_GetInfo);

unsigned int Chunk_FormType(Chunk *c)
{
    return c ? c->form : 0;
}
HOOK(0x4B7570, Chunk_FormType);

unsigned int Chunk_Id(Chunk *c)
{
    return c ? c->id : 0;
}
HOOK(0x4B7580, Chunk_Id);

Chunk *Chunk_Find(Chunk *c, unsigned int id)
{
    for (; c; c = Chunk_Next(c))
        if (c->id == id)
            return c;
    return NULL;
}
HOOK(0x4B7590, Chunk_Find);
