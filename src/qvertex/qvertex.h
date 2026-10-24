#ifndef QVERTEX_QVERTEX_H
#define QVERTEX_QVERTEX_H

#include <stdint.h>

#include "igi.h"

/* What the QVertex drawing functions hand to the render driver. */
typedef struct QVertex {
    float x, y, z;
    float u, v;
    float a, r, g, b;
} QVertex;

typedef struct QVertexStrip {
    int lines;          /* a line strip instead of a triangle strip */
    unsigned int flags;
    int queue;          /* render queue, < 0 draws at once */
    int count;
    QVertex *verts;
} QVertexStrip;

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
