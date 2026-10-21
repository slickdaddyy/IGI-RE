#ifndef POOL_H
#define POOL_H

/* count fixed size slots plus a table of free slot indices after them */
typedef struct Pool {
    char *data;
    int *free;
    int used;
    int count;
    int stride;
    int size;
} Pool;

void Pool_Init(Pool *pool, int size, int count, unsigned int align);
void Pool_Free(Pool *pool);

#endif
