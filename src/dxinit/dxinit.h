#ifndef DXINIT_DXINIT_H
#define DXINIT_DXINIT_H

#include <stdint.h>

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "igi.h"

typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

/* the view the frame is drawn with (0xBCAAE0) */
typedef struct View {
    float m[9];
    int frame;
    Vec3d pos;
} View;

/* PtrList (0x4B73F0): current index at +4, functions at +0x14 */
typedef struct PtrList {
    int magic;
    int current;
    int count;
    int capacity;
    int magic2;
    void (*items[1])(void);
} PtrList;

#define gView VAR(View, 0xBCAAE0)
#define gViewPos VAR(Vec3d, 0xBCAB08)
#define gNearPlaneScale VAR(float, 0xBCABEC)
#define gD3DTnL VAR(int, 0xBCAD74)
#define gD3DZEnable VAR(int, 0xBCAD78)
#define gD3DListA VAR(PtrList *, 0xBCADE8)
#define gD3DListB VAR(PtrList *, 0xBCAD7C)
#define gD3DListC VAR(PtrList *, 0xBCADAC)
#define gD3DStateList VAR(PtrList *, 0xBCADC4)
#define gD3D VAR(IDirect3D7 *, 0x5CA110)
#define gD3DDevice VAR(IDirect3DDevice7 *, 0x5CA114)
#define gDynCubeObjType VAR(uint16_t, 0x54875C)
#define gTaskHandlers ((void (*(*)[0x180])(void *, void *))0xA96AE0)
#define gRenderFuncs ((void (**)(void *))0xA94E84)

#define SET_RS(state, value) gD3DDevice->lpVtbl->SetRenderState(gD3DDevice, (D3DRENDERSTATETYPE)(state), (value))
#define SET_TSS(stage, state, value) \
    gD3DDevice->lpVtbl->SetTextureStageState(gD3DDevice, (stage), (D3DTEXTURESTAGESTATETYPE)(state), (value))

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* x87 fcom equality: unordered counts as equal */
static inline int x87_eq(long double a, long double b)
{
    return !(a < b) && !(a > b);
}

static inline uint32_t f2u(float f)
{
    union { float f; uint32_t u; } v = { f };

    return v.u;
}

#endif
