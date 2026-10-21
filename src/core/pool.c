#include "hook.h"
#include "core/mem.h"
#include "core/pool.h"

void Pool_Init(Pool *pool, int size, int count, unsigned int align)
{
    int stride, i;

    if (align < 4)
        align = 4;
    pool->size = size;
    stride = (size + align + 3) & ~(align - 1);
    pool->data = Mem_Alloc((stride + 4) * count, align);
    pool->free = (int *)(pool->data + stride * count);
    pool->count = count;
    pool->stride = stride;
    pool->used = 0;
    for (i = 0; i < count; i++)
        pool->free[i] = i;
}
HOOK(0x4B0B00, Pool_Init);

void Pool_Free(Pool *pool)
{
    Mem_Free(pool->data);
    pool->data = NULL;
}
HOOK(0x4B0B70, Pool_Free);
