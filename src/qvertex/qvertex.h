#ifndef QVERTEX_QVERTEX_H
#define QVERTEX_QVERTEX_H

#include <stdint.h>

#include "igi.h"
#include "qvertex/qvertextypes.h"
#include "res/restypes.h"

extern int gQVertexStrip;
extern int gQVertexDrawStrip;

/* a float constant from the exe, bit exact */
#define QV_F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

/* fistp with the current rounding mode, low 32 bits */
static inline int QV_Fist(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

static inline uint32_t QV_Bits(const float *f)
{
    return *(const uint32_t *)f;
}

#endif
