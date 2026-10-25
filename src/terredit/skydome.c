#pragma GCC optimize("excess-precision=standard")
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "hook.h"
#include "terredit/terredit.h"
#include "res/restypes.h"
#include "camfeed/camfeedvars.h"
#include "render/rendervars.h"
#include "terredit/terreditvars.h"
#include "d3d/d3dvars.h"

/* The sky dome of FlatSky: a vertex buffer of 321 coloured vertices (the
   top, 15 rings of 20 from 90 degrees down by 6, and a ring below the
   horizon) drawn around the camera. Colours blend between four corners of
   one of two colour sets, chosen by whether the ring is above the horizon
   angle. */

ORIG(0x495200, void, Direct3D_AddRestoreCallback, (void (*fn)(void)));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4CEBA0, void *, VirModel_Add, (const char *name));

typedef struct SkyVertex {
    float x, y, z;
    uint32_t color;
} SkyVertex;

#define SKY_VERTS 321
#define SKY_INDICES 1860

typedef struct SkyDome {
    float color[6][3];
    float horizon;
    int unk4C;
    IDirect3DVertexBuffer7 *vb;
    uint16_t index[SKY_INDICES];
} SkyDome;

_Static_assert(sizeof(SkyDome) == 0xEDC, "SkyDome");

extern SkyDome *gSkyDome;
extern void *gSkyModel;
extern uint32_t gSkyUnusedTint[];

static const float kHalfPi = 0x1.921fb6p+0f;
static const float kRingStep = 0x1.aceeap-4f;
static const float kRadiusScale = 0x1.99999ap-1f;
static const float kHalf = 0.5f;
static const float kOne = 1.0f;
static const float kBlendStep = 0x1.99999ap-4f;
static const float kAzimuthStep = 0x1.41b2f8p-2f;
static const float kBelow = -0.5f;
static const float k255 = 255.0f;
static const float kDefaultHorizon = 0x1.921fb6p-2f;

void SkyDome_SetColors(SkyDome *d, const float *top, const float *c2, const float *c3, const float *c4,
                       const float *c5);
void SkyDome_SetAngle(SkyDome *d, float angle);
void SkyDome_InitIndices(SkyDome *d);
void SkyDome_Fill(SkyDome *d);

static void new_vb(SkyDome *d)
{
    D3DVERTEXBUFFERDESC desc;

    desc.dwSize = sizeof(desc);
    desc.dwCaps = (gD3DTnL ? 0 : D3DVBCAPS_SYSTEMMEMORY) | D3DVBCAPS_WRITEONLY;
    desc.dwFVF = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    desc.dwNumVertices = SKY_VERTS;
    IDirect3D7_CreateVertexBuffer(gD3D, &desc, &d->vb, 0);
}

SkyDome *SkyDome_New(void)
{
    SkyDome *d = Mem_Alloc(sizeof(SkyDome), 4);
    float red[3] = { 1.0f, 0, 0 };
    float white[3] = { 1.0f, 1.0f, 1.0f };
    float blue[3] = { 0, 0, 1.0f };

    new_vb(d);
    SkyDome_SetColors(d, blue, white, white, red, red);
    SkyDome_SetAngle(d, kDefaultHorizon);
    SkyDome_InitIndices(d);
    SkyDome_Fill(d);
    gSkyDome = d;
    return d;
}
HOOK(0x52CB30, SkyDome_New);

uint32_t SkyDome_Color(const float *c0, const float *c1, const float *c2, const float *c3, float u, float v);

void SkyDome_Fill(SkyDome *d)
{
    SkyVertex *p = NULL;
    float a, t, b, cosA, sinA, w, f1;
    int row, i;

    IDirect3DVertexBuffer7_Lock(d->vb, DDLOCK_WRITEONLY, (void **)&p, NULL);
    p->x = 0;
    p->y = 0;
    p->z = gSkyRadius * kRadiusScale;
    p->color = SkyDome_Color(d->color[0], d->color[0], d->color[0], d->color[0], 0, 0);
    p++;
    a = kHalfPi;
    for (row = 15; row; row--) {
        float (*c)[3];
        long double ra = a - kRingStep;
        long double f;

        a = ra;
        t = 0;
        b = 0;
        if (ra >= d->horizon) {
            f = (a - d->horizon) / (kHalfPi - d->horizon);
            c = d->color;
        } else {
            long double h = d->horizon * kHalf;

            c = d->color + 2;
            if (!(a >= h))
                f = 0;
            else
                f = a / h - kOne;
        }
        cosA = x87_cos(a);
        sinA = x87_sin(a);
        f1 = kOne - f;
        for (i = 20; i; i--) {
            uint32_t color;

            p->x = x87_cos(b) * (gSkyRadius * kRadiusScale) * cosA;
            p->y = x87_sin(b) * (gSkyRadius * kRadiusScale) * cosA;
            p->z = gSkyRadius * kRadiusScale * sinA;
            if (!(t >= kOne))
                w = kOne - t;
            else
                w = t - kOne;
            color = SkyDome_Color(c[0], c[1], c[2], c[3], w, f1);
            t = t + kBlendStep;
            p->color = color;
            p++;
            b = b + kAzimuthStep;
        }
    }
    for (i = 20; i; i--) {
        *(uint32_t *)&p->x = *(uint32_t *)&p[-20].x;
        *(uint32_t *)&p->y = *(uint32_t *)&p[-20].y;
        p->z = gSkyRadius * kRadiusScale * kBelow;
        p->color = p[-20].color;
        p++;
    }
    IDirect3DVertexBuffer7_Unlock(d->vb);
}
HOOK(0x52CC40, SkyDome_Fill);

/* bilinear blend of four colours, as 0xRRGGBB */
uint32_t SkyDome_Color(const float *c0, const float *c1, const float *c2, const float *c3, float u, float v)
{
    long double ar = (c1[0] - c0[0]) * u + c0[0];
    float ag = (c1[1] - c0[1]) * u + c0[1];
    float ab = (c1[2] - c0[2]) * u + c0[2];
    long double br = (c3[0] - c2[0]) * u + c2[0];
    float bg = (c3[1] - c2[1]) * u + c2[1];
    float bb = (c3[2] - c2[2]) * u + c2[2];
    long double r = (br - ar) * v + ar;
    float g = (bg - ag) * v + ag;
    float bl = (bb - ab) * v + ab;
    int ir, ig, ib;

    ir = x87_fistp(r * k255);
    if (ir >= 0xFF)
        ir = 0xFF;
    ig = x87_fistp(g * k255);
    if (ig >= 0xFF)
        ig = 0xFF;
    ib = x87_fistp(bl * k255);
    if (ib >= 0xFF)
        ib = 0xFF;
    return (ir << 8 | ig) << 8 | ib;
}
HOOK(0x52CE90, SkyDome_Color);

/* a fan around the top, then two triangles per quad of the rings */
void SkyDome_InitIndices(SkyDome *d)
{
    uint16_t *p = d->index;
    int prev = 19;
    int row, i;

    for (i = 0; i < 20; i++) {
        *p++ = 0;
        *p++ = prev + 1;
        *p++ = i + 1;
        prev = i;
    }
    for (row = 0; row < 15; row++) {
        int base = row * 20;

        prev = 19;
        for (i = 0; i < 20; i++) {
            *p++ = i + base + 1;
            *p++ = prev + base + 1;
            *p++ = i + base + 21;
            *p++ = prev + base + 1;
            *p++ = prev + base + 21;
            *p++ = i + base + 21;
            prev = i;
        }
    }
}
HOOK(0x52CFA0, SkyDome_InitIndices);

void SkyDome_Free(SkyDome *d)
{
    gSkyDome = NULL;
    IDirect3DVertexBuffer7_Release(d->vb);
    Mem_Free(d);
}
HOOK(0x52D030, SkyDome_Free);

void SkyDome_Render(SkyDome *d)
{
    IDirect3DDevice7 *dev;
    float angle = 0;
    float m[16];
    float c;
    long double s;

    if (gLightPool.used) {
        float *l = (float *)(gLightPool.data + *gLightPool.free * gLightPool.stride);

        angle = x87_atan2(l[0], l[1]) + kHalfPi;
    }
    gD3DTextureStates->current = -1;
    gD3DBlendStates->current = -1;
    gD3DRenderStates->current = -1;
    gD3DClipStates->current = -1;
    dev = gD3DDevice;
    IDirect3DDevice7_SetRenderState(dev, D3DRENDERSTATE_ALPHABLENDENABLE, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_FOGENABLE, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZENABLE, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZFUNC, 2);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ZWRITEENABLE, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_CULLMODE, 3);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_SPECULARENABLE, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_LIGHTING, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_DITHERENABLE, 1);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_ALPHATESTENABLE, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, D3DRENDERSTATE_CLIPPING, 1);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, D3DTSS_COLOROP, 3);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, D3DTSS_COLORARG1, 2);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, D3DTSS_COLORARG2, 0);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, D3DTSS_ALPHAOP, 1);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 1, D3DTSS_COLOROP, 1);
    c = x87_cos(angle) * gWorldScale;
    s = x87_sin(angle) * gWorldScale;
    m[0] = c;
    m[1] = x87_neg(s);
    m[2] = 0;
    m[3] = 0;
    m[4] = s;
    m[5] = c;
    m[6] = 0;
    m[7] = 0;
    m[8] = 0;
    m[9] = 0;
    *(uint32_t *)&m[10] = *(uint32_t *)&gWorldScale;
    m[11] = 0;
    m[12] = 0;
    m[13] = 0;
    m[14] = 0;
    m[15] = 1.0f;
    IDirect3DDevice7_SetTransform(gD3DDevice, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX *)m);
    IDirect3DDevice7_DrawIndexedPrimitiveVB(gD3DDevice, D3DPT_TRIANGLELIST, d->vb, 0, SKY_VERTS, d->index,
                                            SKY_INDICES, 0);
}
HOOK(0x52D060, SkyDome_Render);

void SkyDome_SetColors(SkyDome *d, const float *top, const float *c2, const float *c3, const float *c4,
                       const float *c5)
{
    const float *src[6] = { top, top, c2, c3, c4, c5 };
    int i, j;

    for (i = 0; i < 6; i++)
        for (j = 0; j < 3; j++)
            ((uint32_t *)d->color[i])[j] = ((const uint32_t *)src[i])[j];
    SkyDome_Fill(d);
}
HOOK(0x52D2B0, SkyDome_SetColors);

void SkyDome_SetAngle(SkyDome *d, float angle)
{
    d->horizon = angle;
}
HOOK(0x52D350, SkyDome_SetAngle);

void SkyDome_Restore(void);

void SkyDome_Init(void)
{
    Direct3D_AddRestoreCallback(SkyDome_Restore);
}
HOOK(0x52D360, SkyDome_Init);

/* recreates the vertex buffer after the device was lost */
void SkyDome_Restore(void)
{
    SkyDome *d = gSkyDome;

    if (d) {
        IDirect3DVertexBuffer7_Release(d->vb);
        new_vb(d);
        SkyDome_Fill(d);
    }
}
HOOK(0x52D370, SkyDome_Restore);

void Sky_SetTint(const uint32_t *v)
{
    gSkyUnusedTint[0] = v[0];
    gSkyUnusedTint[1] = v[1];
    gSkyUnusedTint[2] = v[2];
}
HOOK(0x52D400, Sky_SetTint);

void Sky_Init(void)
{
    gSkyModel = VirModel_Add("Sky");
}
HOOK(0x52D420, Sky_Init);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 54F720, 4) uint32_t store_54F720[15] INIT({1065353216, 1065353216, 1065353216, 7957331, 45, 1681141797, 842016045, 807742820, 25650, 1681010725, 842016045, 807742820, 25650, 1681141797});
ALIAS(gSkyUnusedTint, store_54F720, 0);
BSS(A84568) SkyDome *gSkyDome;
PAD(igb, A84568, A8456C, 0x4);
BSS(A84568) void *gSkyModel;
BSS(BCADBC) float gSkyRadius;
