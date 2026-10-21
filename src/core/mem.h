#ifndef MEM_H
#define MEM_H

#include <stddef.h>

#include "core/list.h"

typedef struct MemBlock {
    Node node;
    void *user;
    size_t total;
    size_t size;
    int unused[2];
} MemBlock;

void Mem_Init(void);
void Mem_SetDefaultMode(void);
void *Mem_Alloc(size_t size, size_t align);
void Mem_Free(void *p);
void Mem_PushMode(char mode);
void Mem_PopMode(void);

#endif
