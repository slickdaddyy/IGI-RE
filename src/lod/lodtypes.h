#ifndef LOD_LODTYPES_H
#define LOD_LODTYPES_H

#include <stdint.h>

/* Cached data of a visited cell, 2048 of them */
typedef struct QCacheEntry {
    int cell[4];
    int frame; /* -1 = free */
    int kind;  /* which pool data comes from */
    char *data;
} QCacheEntry;

/* One collected object: the handler slot to call and the task */
typedef struct QEntry {
    uint8_t slot;
    struct DynCubeObj *task;
} QEntry;

#endif
