#ifndef TERRDRAW_H
#define TERRDRAW_H

#include <stdint.h>

typedef struct TerrNode {
    struct TerrNode *next;
    struct TerrNode *prev;
} TerrNode;

typedef struct TerrList {
    TerrNode *head;
    TerrNode *tail;
    TerrNode *tailpred;
} TerrList;

typedef struct TerrPool {
    char *data;
    int *free;
    int used;
    int count;
    int stride;
    int size;
} TerrPool;

/* A render chunk: a block of the terrain at (x, y), kept in a hash table. */
typedef struct TerrChunk {
    TerrNode node;
    char unk8[0x2EE0];
    int unk2EE8;
    int x;
    int y;
} TerrChunk;

_Static_assert(sizeof(TerrChunk) == 0x2EF4, "TerrChunk");

#endif
