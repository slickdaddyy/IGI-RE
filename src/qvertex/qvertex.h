#ifndef QVERTEX_QVERTEX_H
#define QVERTEX_QVERTEX_H

#include <stdint.h>

#include "igi.h"
#include "qvertex/qvertextypes.h"

/* A texture handle as the drivers fill it in. */
typedef struct QTexture {
    int device;
    short width, height;   /* offscreen driver */
    short pitch, format;
    void *pixels;
    float uScale, vScale;  /* Direct3D driver */
    int unk18, unk1C;
    void *surface;
} QTexture;

typedef struct QPalette {
    int device;
    int slot;
    uint16_t *colors;
} QPalette;

extern QTexture *gQVertexTexture;
extern int gQVertexStrip;
extern QPalette *gQVertexPalette;
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
