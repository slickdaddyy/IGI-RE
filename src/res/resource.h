#ifndef RES_RESOURCE_H
#define RES_RESOURCE_H

#include "igi.h"

typedef struct Node {
    struct Node *next;
    struct Node *prev;
} Node;

typedef struct List {
    Node *head;
    Node *tail;
    Node *tailpred;
} List;

enum {
    RES_PACK,   /* an "IRES" ILFF form or a "RESOURCE" pack, owns its data */
    RES_MEMBER, /* a resource inside a pack */
    RES_FILE,   /* any other file, owns its data */
    RES_USER,   /* memory registered by the game */
    RES_PATH    /* a ';' separated list of resource names */
};

typedef struct Resource {
    Node node;               /* on gResourceList */
    Node child;              /* on the pack's children list */
    struct Resource *self;   /* owner of the child node */
    List children;
    unsigned int hash;
    int type;
    int size;
    int used;
    int unk30;
    void *data;
    char name[0x200];        /* upper case, '/' separators */
    const char *info;        /* text of a DBUG/CSTR chunk */
    int time[2];
} Resource;

/* Fixed size object pool: slot i of the order table holds the index of
   the i-th allocated object, each object stores its order position just
   past its own size. */
typedef struct Pool {
    char *base;
    int *order;
    int used;
    int capacity;
    int stride;
    int size;
} Pool;

/* Chunk header of an ILFF file */
typedef struct Chunk {
    unsigned int id;
    unsigned int size;
    unsigned int align;
    unsigned int next;
    unsigned int form;
} Chunk;

#define CHUNK_ID(a, b, c, d) ((unsigned int)(a) | (unsigned int)(b) << 8 | (unsigned int)(c) << 16 | (unsigned int)(d) << 24)

void String_ToUpper(char *dst, const char *src);
unsigned int Resource_Hash(const char *name);
int Resource_IsLoaded(const char *name, Resource **out);
void *Resource_Load(const char *name, int *size);
void Resource_Unload(const char *name);

Chunk *Chunk_Next(Chunk *c);
Chunk *Chunk_FirstChild(Chunk *c);
void *Chunk_Data(Chunk *c, Chunk *parent);
void Chunk_GetInfo(unsigned int *size, unsigned int *align, Chunk *c);
unsigned int Chunk_FormType(Chunk *c);
unsigned int Chunk_Id(Chunk *c);
Chunk *Chunk_Find(Chunk *c, unsigned int id);

#endif
