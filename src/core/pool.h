#ifndef POOL_H
#define POOL_H

#include "types.h"

void Pool_Init(Pool *pool, int size, int count, unsigned int align);
void Pool_Free(Pool *pool);

#endif
