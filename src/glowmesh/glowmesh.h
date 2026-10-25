#ifndef GLOWMESH_GLOWMESH_H
#define GLOWMESH_GLOWMESH_H

#include <stdint.h>

#include "igi.h"
#include "transctx/transctxtypes.h"
#include "core/coretypes.h"

/* exe constants, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }
#define F64(name, bits) static const union { uint64_t u; double d; } name = { bits }

/* the active TransContext (0xBCAAE0) and scene (0xBCABA0), only the fields used here */
extern Scene gScene;

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

#endif
