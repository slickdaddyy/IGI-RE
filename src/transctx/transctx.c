#include <string.h>

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "hook.h"
#include "transctx/transctx.h"

ORIG(0x491CF0, int *, Display_GetMode, (void));
ORIG(0x491D40, long double, Display_GetAspect, (void));
ORIG(0x492BE0, const char *, DDraw_GetErrorString, (HRESULT hr));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x4950F0, void, Direct3D_MakeMatrix, (D3DMATRIX *out, const void *rot, const double *pos));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int method));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B0EE0, void, RenderMode_ForEach, (void *fn, void *user));

int Primitive_Register(const char *name);

#define gTransContextPrim VAR(int, 0x543A68)
#define gUnk543A64 VAR(int, 0x543A64)
#define gSetActiveTransContextMethod VAR(int, 0x6E5BA4)
#define gSetNearClippingPlaneMethod VAR(int, 0x6E5BA8)
#define gTransContextValid VAR(int, 0x6E5BB0)
#define gTransContextSelects VAR(int, 0x6E5BB4)
#define gViewPos ((double *)0xBCAB08)
#define gNearPlaneScale VAR(float, 0xBCABEC)
#define gLayer0MinZ VAR(float, 0xBCABF0)
#define gLayer0MaxZ VAR(float, 0xBCACF0)
#define gLayer1MaxZ VAR(float, 0xBCAD8C)
#define gLayer2MaxZ VAR(float, 0xBCAD9C)
#define gD3DDevice VAR(IDirect3DDevice7 *, 0x5CA114)
#define gD3DMinZ VAR(float, 0xBCADA0)
#define gD3DZ24 VAR(int, 0xBCADA4)
#define gLayer2Far VAR(float, 0xBCADA8)
#define gLayer2Near VAR(float, 0xBCADB0)
#define gLayer1MinZ VAR(float, 0xBCADB4)
#define gLayer0Far VAR(float, 0xBCADB8)
#define gNearPlane VAR(float, 0xBCADC0)
#define gD3DMaxZ VAR(float, 0xBCADC8)
#define gLayer0Near VAR(float, 0xBCADD8)
#define gLayer2MinZ VAR(float, 0xBCADE4)

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(kMinusOne, 0xBF800000);
static const double kOneD = 1.0;

void sub_497940(int v)
{
    gUnk543A64 = v;
}
HOOK(0x497940, sub_497940);

void TransContext_DrawPrim(RenderNode *n);

void TransContext_RegisterModeCB(const int *mode)
{
    RenderMode_SetMethod(*mode, gTransContextPrim, TransContext_DrawPrim);
}
HOOK(0x497950, TransContext_RegisterModeCB);

void TransContext_Select(TransContext *ctx);

/* A queued TransContext: the context follows the 0x10 byte node. */
void TransContext_DrawPrim(RenderNode *n)
{
    TransContext_Select((TransContext *)((char *)n + 0x10));
}
HOOK(0x497970, TransContext_DrawPrim);

void Direct3D_SetActiveTransContext(TransContext **arg);
void Direct3D_SetNearClippingPlane(float *arg);

void TransContext_Init(void)
{
    TransContext_Invalidate();
    gTransContextPrim = Primitive_Register("TransContextPrimitive");
    RenderMode_ForEach(TransContext_RegisterModeCB, NULL);
    gSetActiveTransContextMethod = RenderMode_CreateMethod("SetActiveTransContext");
    gSetNearClippingPlaneMethod = RenderMode_CreateMethod("SetNearClippingPlane");
    sub_497940(1);
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gSetActiveTransContextMethod, Direct3D_SetActiveTransContext);
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gSetNearClippingPlaneMethod, Direct3D_SetNearClippingPlane);
}
HOOK(0x497980, TransContext_Init);

#define DX_ERROR "%s\n\nDirectX error message: %s"

static void __attribute__((noreturn)) DXError(const char *what, HRESULT hr)
{
    char buf[1024];

    crt_sprintf(buf, what);
    Error_Show(DX_ERROR, buf, DDraw_GetErrorString(hr));
    for (;;)
        ;
}

/* Direct3D's SetActiveTransContext: viewport depth range of the layer, projection and view matrices. */
void Direct3D_SetActiveTransContext(TransContext **arg)
{
    TransContext *ctx = *arg;
    D3DVIEWPORT7 vp;
    D3DMATRIX m, view;
    float sx = kOne.f / ctx->fov;
    float sy = kMinusOne.f / ctx->fovY;
    long double q;
    float zn;
    HRESULT hr;

    vp.dwX = 0;
    vp.dwY = 0;
    vp.dwWidth = Display_GetMode()[1];
    vp.dwHeight = Display_GetMode()[2];
    switch (ctx->layer) {
    case 0:
        vp.dvMinZ = gLayer0MinZ;
        vp.dvMaxZ = gLayer0MaxZ;
        hr = IDirect3DDevice7_SetViewport(gD3DDevice, &vp);
        if (hr)
            DXError("Couldn't set viewport", hr);
        zn = gLayer0Near;
        q = gLayer0Far / ((long double)gLayer0Far - zn);
        break;
    case 1:
        vp.dvMinZ = gLayer1MinZ;
        vp.dvMaxZ = gLayer1MaxZ;
        hr = IDirect3DDevice7_SetViewport(gD3DDevice, &vp);
        if (hr)
            DXError("Couldn't set viewport", hr);
        zn = gD3DMinZ;
        q = gD3DMaxZ / ((long double)gD3DMaxZ - zn);
        break;
    case 2:
        vp.dvMinZ = gLayer2MinZ;
        vp.dvMaxZ = gLayer2MaxZ;
        hr = IDirect3DDevice7_SetViewport(gD3DDevice, &vp);
        if (hr)
            DXError("Couldn't set viewport", hr);
        zn = gLayer2Near;
        q = gLayer2Far / ((long double)gLayer2Far - zn);
        break;
    default:
        Error_Show("Invalid layer");
        for (;;)
            ;
    }
    memset(&m, 0, sizeof(m));
    m._33 = q;
    m._43 = -(q * zn);
    m._11 = sx;
    m._22 = sy;
    m._34 = kOne.f;
    memcpy(ctx->proj, &m, sizeof(m));
    hr = IDirect3DDevice7_SetTransform(gD3DDevice, D3DTRANSFORMSTATE_PROJECTION, &m);
    if (hr)
        DXError("Couldn't set projection matrix", hr);
    Direct3D_MakeMatrix(&view, ctx, NULL);
    hr = IDirect3DDevice7_SetTransform(gD3DDevice, D3DTRANSFORMSTATE_VIEW, &view);
    if (hr)
        DXError("Couldn't set view matrix", hr);
}
HOOK(0x497A00, Direct3D_SetActiveTransContext);

/* arg[0] is the new near plane, arg[1] gets whether the z-buffer has 24 bits. */
void Direct3D_SetNearClippingPlane(float *arg)
{
    TransContext *ctx = &gTransContext;

    *(int *)&gNearPlane = *(int *)arg;
    gD3DMinZ = gNearPlaneScale * gNearPlane;
    Direct3D_SetActiveTransContext(&ctx);
    *(int *)&arg[1] = gD3DZ24;
}
HOOK(0x497D00, Direct3D_SetNearClippingPlane);

void TransContext_Shutdown(void)
{
    RenderMode_DeleteMethod(gSetNearClippingPlaneMethod);
    RenderMode_DeleteMethod(gSetActiveTransContextMethod);
    gTransContextPrim = -1;
    TransContext_Invalidate();
}
HOOK(0x497D40, TransContext_Shutdown);

/* Sets up a camera for the current scene from a 3x3 rotation (plus one word), a position and fov. */
void TransContext_Create(TransContext *ctx, const float *rot, const double *pos, float fov)
{
    float r[9];
    volatile float t;
    int i;

    memcpy(ctx->m, rot, 40);
    memcpy(r, ctx->m, sizeof(r));
    for (i = 0; i < 3; i++) {
        ctx->m[i * 3 + 0] = r[i];
        ctx->m[i * 3 + 1] = r[i + 3];
        ctx->m[i * 3 + 2] = r[i + 6];
    }
    memcpy(ctx->pos, pos, sizeof(ctx->pos));
    ctx->fov = fov;
    t = gScene.h * fov;
    ctx->fovY = t / (Display_GetAspect() * gScene.w);
    ctx->scaleX = gScene.halfW / ctx->fov;
    ctx->scaleY = Display_GetAspect() * ctx->scaleX;
    ctx->centerX = gScene.x + gScene.halfW;
    ctx->centerY = gScene.y + gScene.halfH;
    ctx->unk58 = 1;
    ctx->layer = 1;
}
HOOK(0x497D70, TransContext_Create);

void sub_497E60(TransContext *ctx, float v)
{
    ctx->unk5C = v;
}
HOOK(0x497E60, sub_497E60);

/* Makes ctx the active camera and tells the render mode. */
void TransContext_Select(TransContext *ctx)
{
    void (*fn)(void *) = gRenderFuncs[gSetActiveTransContextMethod];
    TransContext *p = &gTransContext;

    gTransContext = *ctx;
    if (fn)
        fn(&p);
    gTransContextValid = 1;
    gTransContextSelects++;
}
HOOK(0x497E70, TransContext_Select);

int TransContext_GetSelectCount(void)
{
    return gTransContextSelects;
}
HOOK(0x497EC0, TransContext_GetSelectCount);

void TransContext_Invalidate(void)
{
    gTransContextValid = 0;
}
HOOK(0x497ED0, TransContext_Invalidate);

/* Selects ctx when the scene's last queue is drawn. */
void TransContext_Queue(const TransContext *ctx)
{
    RenderNode *n = Frame_Alloc(0x10 + sizeof(TransContext));

    n->type = gTransContextPrim;
    memcpy((char *)n + 0x10, ctx, sizeof(TransContext));
    RenderQueue_Add(gScene.queueCount - 1, n);
}
HOOK(0x497EE0, TransContext_Queue);

/* Screen position and depth of a world point with the active camera. */
void TransContext_Project(float *out, float *depth, const double *p)
{
    TransContext *t = &gTransContext;
    long double dx = p[0] - gViewPos[0];
    long double dy = p[1] - gViewPos[1];
    long double dz = p[2] - gViewPos[2];
    volatile float a = t->m[2] * dz + t->m[1] * dy + t->m[0] * dx;
    volatile float b = t->m[5] * dz + t->m[4] * dy + t->m[3] * dx;
    long double c = t->m[8] * dz + t->m[7] * dy + t->m[6] * dx;
    long double k;

    *depth = c;
    if (!(c > kZero.f))
        return;
    k = kOneD / c;
    out[0] = t->scaleX * a * k + t->centerX;
    out[1] = t->scaleY * b * k + t->centerY;
}
HOOK(0x497F20, TransContext_Project);

/* Same for a point already in view space. */
void TransContext_ProjectView(float *out, const float *v)
{
    long double k = kOneD / v[2];

    out[0] = v[0] * gTransContext.scaleX * k + gTransContext.centerX;
    out[1] = v[1] * gTransContext.scaleY * k + gTransContext.centerY;
}
HOOK(0x498000, TransContext_ProjectView);

/* Screen size of something of the given size at a depth. */
void TransContext_ScaleSize(float *out, const float *size, float depth)
{
    long double k = kOneD / depth;

    out[0] = size[0] * gTransContext.scaleX * k;
    out[1] = size[1] * gTransContext.scaleY * k;
}
HOOK(0x498040, TransContext_ScaleSize);

int TransContext_GetNearClipMethod(void)
{
    return gSetNearClippingPlaneMethod;
}
HOOK(0x498070, TransContext_GetNearClipMethod);
