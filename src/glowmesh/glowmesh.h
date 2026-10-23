#ifndef GLOWMESH_GLOWMESH_H
#define GLOWMESH_GLOWMESH_H

#include <stdint.h>

#include "igi.h"

/* exe constants, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }
#define F64(name, bits) static const union { uint64_t u; double d; } name = { bits }

/* the active TransContext (0xBCAAE0) and scene (0xBCABA0), only the fields used here */
#define gViewMatrix ((float *)0xBCAAE0)
#define gViewPos ((double *)0xBCAB08)
#define gViewScaleX VAR(float, 0xBCAB28)
#define gViewScaleY VAR(float, 0xBCAB2C)
#define gViewCenterX VAR(float, 0xBCAB30)
#define gViewCenterY VAR(float, 0xBCAB34)
#define gSceneX VAR(float, 0xBCABB8)
#define gSceneY VAR(float, 0xBCABBC)
#define gSceneW VAR(float, 0xBCABC0)
#define gSceneH VAR(float, 0xBCABC4)
#define gSceneHalfW VAR(float, 0xBCABC8)
#define gSceneHalfH VAR(float, 0xBCABCC)
#define gSceneMinX VAR(float, 0xBCABD0)
#define gSceneMinY VAR(float, 0xBCABD4)
#define gSceneMaxX VAR(float, 0xBCABD8)
#define gSceneMaxY VAR(float, 0xBCABDC)
#define gNearPlane VAR(float, 0xBCADC0)
#define gQCubeActive VAR(int, 0xBCAB38)
#define gQCubeMinSize VAR(int, 0xB16C6C)
#define gRenderFuncs ((void (**)(void *))0xA94E84)

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

#endif
