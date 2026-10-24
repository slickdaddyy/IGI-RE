#ifndef DXINIT_DXINIT_H
#define DXINIT_DXINIT_H

#include <stdint.h>

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "res/restypes.h"
#include "transctx/transctxtypes.h"

extern TransCtx gView;
extern Vec3d gViewPos;
extern float gNearPlaneScale;
extern int gD3DTnL;
extern int gD3DZEnable;
extern PtrList *gD3DListA;
extern PtrList *gD3DListB;
extern PtrList *gD3DListC;
extern PtrList *gD3DStateList;
extern IDirect3D7 *gD3D;
extern IDirect3DDevice7 *gD3DDevice;
extern uint16_t gDynCubeObjType;
extern void (* gTaskHandlers[][0x180])(void *, void *);
extern void (*gRenderFuncs[])(void *);

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
