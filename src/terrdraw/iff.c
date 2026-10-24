/* IFF files: a tree of big endian "FORM" groups and data chunks. */
#include <stddef.h>

#include "hook.h"
#include "igi.h"
#include "terrdraw/terrdrawtypes.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));

#define IFF_FORM 0x464F524D

void Iff_Free(IffNode *n)
{
    IffNode *next;

    while (n) {
        if (n->child)
            Iff_Free(n->child);
        if (n->owned) {
            Mem_Free(n->data);
            n->data = NULL;
        }
        next = n->next;
        Mem_Free(n);
        n = next;
    }
}
HOOK(0x517840, Iff_Free);

IffNode *Iff_FindForm(IffNode *n, int type)
{
    IffNode *r;

    for (; n; n = n->next) {
        if (n->id == IFF_FORM) {
            if (n->type == type)
                return n;
            if (n->child && (r = Iff_FindForm(n->child, type)))
                return r;
        } else if (n->type == type) {
            return n;
        }
    }
    return NULL;
}
HOOK(0x517890, Iff_FindForm);

IffNode *Iff_FindNextForm(IffNode *n, int type)
{
    IffNode *r;

    if (n->type == type)
        n = n->next;
    for (; n; n = n->next) {
        if (n->id == IFF_FORM) {
            if (n->type == type)
                return n;
            if (n->child && (r = Iff_FindForm(n->child, type)))
                return r;
        } else if (n->type == type) {
            return n;
        }
    }
    return NULL;
}
HOOK(0x5178E0, Iff_FindNextForm);

IffNode *Iff_FindChunk(IffNode *form, int id)
{
    IffNode *n, *r;

    for (n = form->child; n; n = n->next) {
        if (n->id == IFF_FORM) {
            if ((r = Iff_FindChunk(n, id)))
                return r;
        } else if (n->id == id) {
            return n;
        }
    }
    return NULL;
}
HOOK(0x517940, Iff_FindChunk);

/* The next chunk with this id after n, stepping up to the parent's first
   child when the list ends. */
IffNode *Iff_FindNextChunk(IffNode *n, int id)
{
    IffNode *next;

    if (!n)
        return NULL;
    do {
        next = n->next;
        if (!next) {
            if (!n->parent || !n->parent->next)
                return NULL;
            next = n->parent->next->child;
        }
        if (!next)
            return NULL;
        n = next;
    } while (next->id != id);
    return next;
}
HOOK(0x517980, Iff_FindNextChunk);

void *Iff_GetData(IffNode *n)
{
    return n->data;
}
HOOK(0x5179C0, Iff_GetData);

unsigned int Iff_ReadLong(const unsigned char *p)
{
    return (unsigned int)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}
HOOK(0x5179D0, Iff_ReadLong);

IffNode *Iff_ParseList(unsigned char *data, int size, IffNode *parent);

IffNode *Iff_Parse(unsigned char *data)
{
    if (Iff_ReadLong(data) != IFF_FORM)
        return NULL;
    return Iff_ParseList(data, 1, NULL);
}
HOOK(0x517A00, Iff_Parse);

/* size 1 at the top level reads just the one FORM there. */
IffNode *Iff_ParseList(unsigned char *data, int size, IffNode *parent)
{
    IffNode *first = NULL, *prev = NULL, *n;
    int pos = 0, id;

    while (pos < size) {
        id = Iff_ReadLong(data + pos);
        n = Mem_Alloc(sizeof(IffNode), 4);
        if (!first)
            first = n;
        else
            prev->next = n;
        n->next = NULL;
        n->child = NULL;
        n->id = id;
        n->type = 0;
        n->size = Iff_ReadLong(data + pos + 4);
        n->data = data + pos + 8;
        n->parent = parent;
        n->owned = 0;
        if (id == IFF_FORM) {
            n->type = Iff_ReadLong(data + pos + 8);
            n->child = Iff_ParseList(data + pos + 12, n->size - 4, n);
        }
        pos += n->size + 8;
        if (pos & 1)
            pos++;
        prev = n;
    }
    return first;
}
HOOK(0x517A30, Iff_ParseList);
