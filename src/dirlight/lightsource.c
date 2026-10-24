/* The list of light sources the lightmap builder asks for light. */
#include "hook.h"
#include "dirlight/dirlight.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));

void LightSource_Add(void *light)
{
    Pool *pool = &gLightSourcePool;
    void **item = (void **)(pool->data + pool->free[pool->used] * pool->stride);

    *(int *)((char *)item + pool->size) = pool->used;
    pool->used++;
    *item = light;
}
HOOK(0x5268E0, LightSource_Add);

void LightSource_Remove(void *light)
{
    Pool *pool = &gLightSourcePool;
    int used = pool->used;
    int i;

    for (i = 0; i < used; i++) {
        char *item = pool->data + pool->free[i] * pool->stride;
        int idx, last, tmp;
        char *end;

        if (*(void **)item != light)
            continue;
        idx = *(int *)(item + pool->size);
        pool->used = --used;
        if (idx != used) {
            end = pool->data + pool->free[used] * pool->stride;
            tmp = pool->free[idx];
            last = *(int *)(end + pool->size);
            pool->free[idx] = pool->free[last];
            pool->free[last] = tmp;
            *(int *)(end + pool->size) = idx;
        }
        return;
    }
    Error_Show("Lightsource not found");
    for (;;)
        ;
}
HOOK(0x526920, LightSource_Remove);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A83CF8) Pool gLightSourcePool;
