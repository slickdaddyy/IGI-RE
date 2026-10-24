#ifndef LOD_LODTYPES_H
#define LOD_LODTYPES_H

/* Cached data of a visited cell, 2048 of them */
typedef struct QCacheEntry {
    int cell[4];
    int frame; /* -1 = free */
    int kind;  /* which pool data comes from */
    char *data;
} QCacheEntry;

#endif
