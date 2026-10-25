#pragma GCC optimize("excess-precision=standard")

#include <string.h>
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "hook.h"
#include "igi.h"
#include "sysmisc/sysmisctypes.h"
#include "camfeed/camfeedvars.h"
#include "file/filevars.h"
#include "res/resvars.h"
#include "d3d/d3dvars.h"
#include "sysmisc/sysmiscvars.h"

/* Texture functions of the Direct3D render mode: textures are DirectDraw
   surfaces, created and filled when the engine downloads them. */

/* argument of the GetTextureSize render function */
typedef struct TexSize {
    unsigned width;
    unsigned height;
    unsigned outWidth;
    unsigned outHeight;
} TexSize;

/* a texture pixel format the device offered, with its shifts and widths */
typedef struct TexFormat {
    DDPIXELFORMAT pf;
    int aShift;
    int rShift;
    int gShift;
    int bShift;
    int aBits;
    int rBits;
    int gBits;
    int bBits;
    int rgbBits;
} TexFormat;

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, unsigned align));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B1000, void, RenderMode_Call, (int mode, int method, void *arg));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x495170, void, Direct3D_AddSetModeCallback, (void (*fn)(void)));
ORIG(0x492BE0, const char *, DDraw_GetErrorString, (HRESULT hr));
ORIG(0x4B2700, int, Texture_GetSizeId, (void));
ORIG(0x4B26E0, int, Texture_GetRefreshId, (void));
ORIG(0x4B26F0, int, Texture_GetReloadId, (void));
ORIG(0x4B2710, int, Texture_GetSetLODId, (void));
ORIG(0x4B2640, int, Texture_GetActiveTMU, (void));
ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x517FD0, void, Pixel_8To32, (uint32_t *dst, int w, int h, int unk, const void *src, const uint32_t *colors,
                                  int pitch, int rows, int unk2));
ORIG(0x517C90, void, Pixel_1555To32, (uint32_t *dst, int w, int h, const void *src, int pitch, int rows, int unk));
ORIG(0x517F00, void, Pixel_Copy32, (uint32_t *dst, int w, int h, const void *src, int pitch, int rows, int unk));
ORIG(0x517DD0, void, Pixel_565To32, (uint32_t *dst, int w, int h, const void *src, int pitch, int rows, int unk));
ORIG(0x517B00, void, Pixel_Halve32, (uint32_t *dst, const uint32_t *src, int w, int h));

extern Pool gTexPool;
extern TexFormat gTexFormats[];
extern int gActiveTMU;
extern int gTexLOD;
extern int gTexCount;
extern int gTexBytes;
extern int gTexFormatCount;
extern LONGLONG gTexReleaseTime;

#define DX_ERROR "%s\n\nDirectX error message: %s"
#define FMT_NO_ALPHA 0x20000000
#define FMT_MIPMAPS 0x40
#define IS_POW2(v) ((~((v) - 1) & (v)) == (v))

#define DX_FAIL(text, hr) do { \
        crt_sprintf(msg, text); \
        Error_Show(DX_ERROR, msg, DDraw_GetErrorString(hr)); \
        for (;;) \
            ; \
    } while (0)

static const float kAlphaMany = 0.2f;
static const float kAlphaNone = 0.0f;

void D3DTex_GetTextureSize(TexSize *s);
void D3DTex_AllocTexture(TextureDesc *d);
void D3DTex_DeAllocTexture(Texture *t);
void D3DTex_DownloadTexture(TextureDesc *d);
void D3DTex_AllocPalette(PaletteDesc *d);
void D3DTex_DeAllocPalette(Palette *p);
void D3DTex_GetTMUs(int *out);
void D3DTex_SetActiveTMU(int *in);
void D3DTex_GetActiveTMU(int *out);
void D3DTex_SetLOD(int *in);
void D3DTex_EnumFormats(void);
void D3DTex_FindFormat(DDPIXELFORMAT *out, int format, unsigned flags, float alpha);
int D3DTex_SurfaceBytes(IDirectDrawSurface7 *s);
int D3DTex_MipLevels(int width, int height);
void D3DTex_Upload(IDirectDrawSurface7 *dst, int width, int height, const uint32_t *src, int srcW, int srcH);

static int highest_bit(unsigned v)
{
    int i = 31;

    while (!(v >> i))
        i--;
    return i;
}

static int lowest_bit(unsigned v)
{
    int i = 0;

    while (!((v >> i) & 1))
        i++;
    return i;
}

/* 1.0 minus the average alpha of 32 bit pixels */
long double D3DTex_AlphaCoverage(const uint32_t *pixels, int width, int height)
{
    int n = width * height;
    unsigned total = n * 255;
    unsigned left = total;

    for (; n > 0; n--)
        left -= *pixels++ >> 24;
    return (long double)left / (int)total;
}
HOOK(0x50A620, D3DTex_AlphaCoverage);

void D3DTex_AllocPalette(PaletteDesc *d)
{
    Palette *p = d->palette;

    p->colors = Mem_Alloc(0x400, 4);
    if (d->colors)
        memcpy(p->colors, d->colors, 0x400);
}
HOOK(0x50A670, D3DTex_AllocPalette);

void D3DTex_DeAllocPalette(Palette *p)
{
    Mem_Free(p->colors);
    p->colors = NULL;
}
HOOK(0x50A6A0, D3DTex_DeAllocPalette);

#define SET_METHOD(id, fn) do { \
        int method_ = (id); \
        RenderMode_SetMethod(Direct3D_GetRenderMode(), method_, fn); \
    } while (0)

void D3DTex_Init(void)
{
    SET_METHOD(Texture_GetSizeId(), D3DTex_GetTextureSize);
    SET_METHOD(gAllocTextureId, D3DTex_AllocTexture);
    SET_METHOD(gDeAllocTextureId, D3DTex_DeAllocTexture);
    SET_METHOD(gDownloadTextureId, D3DTex_DownloadTexture);
    SET_METHOD(gAllocPalette, D3DTex_AllocPalette);
    SET_METHOD(gDeAllocPalette, D3DTex_DeAllocPalette);
    SET_METHOD(gDownloadPalette, Debug_Log);
    SET_METHOD(Texture_GetRefreshId(), Task_Nothing);
    SET_METHOD(Texture_GetReloadId(), Task_Nothing);
    SET_METHOD(gGetActiveTMUId, D3DTex_GetActiveTMU);
    SET_METHOD(gSetActiveTMUId, D3DTex_SetActiveTMU);
    SET_METHOD(gGetTMUsId, D3DTex_GetTMUs);
    SET_METHOD(Texture_GetSetLODId(), D3DTex_SetLOD);
    Pool_Init(&gTexPool, 4, 1000, 0);
    Direct3D_AddSetModeCallback(D3DTex_EnumFormats);
}
HOOK(0x50A6C0, D3DTex_Init);

/* rounds up to powers of two within the device limits and aspect ratio */
void D3DTex_GetTextureSize(TexSize *s)
{
    D3DDEVICEDESC7 *dd = &gD3DDeviceDesc;
    unsigned aspect = dd->dwMaxTextureAspectRatio;
    unsigned w = s->width, h = s->height;
    unsigned minW, minH, r;

    if (aspect >= 0x100)
        aspect = 0x100;
    if (!IS_POW2(w))
        w = 1u << ((highest_bit(w) + 1) & 31);
    if (!IS_POW2(h))
        h = 1u << ((highest_bit(h) + 1) & 31);
    minW = dd->dwMinTextureWidth;
    if (minW <= 1)
        minW = 1;
    minH = dd->dwMinTextureHeight;
    if (minH <= 1)
        minH = 1;
    if (w < minW)
        w = minW;
    else if (w > dd->dwMaxTextureWidth)
        w = dd->dwMaxTextureWidth;
    if (h < minH)
        h = minH;
    else if (h > dd->dwMaxTextureHeight)
        h = dd->dwMaxTextureHeight;
    if (w > h) {
        r = w / h;
        if (r > aspect && aspect)
            h = r * h / aspect;
    } else {
        r = h / w;
        if (r > aspect && aspect)
            w = r * w / aspect;
    }
    s->outHeight = h;
    s->outWidth = w;
}
HOOK(0x50A830, D3DTex_GetTextureSize);

void D3DTex_AllocTexture(TextureDesc *d)
{
    int height = d->height;
    int width = d->width;
    int mipmap = d->mipmap;
    int pitch = d->pitch;
    int format = d->format;
    DDPIXELFORMAT pf;
    Texture *t;
    TexSize s;

    D3DTex_FindFormat(&pf, format, d->flags, 0.0f);
    t = d->tex;
    memset(t, 0, sizeof(*t));
    t->device = Direct3D_GetRenderMode();
    t->unk1C = 0;
    t->surface = NULL;
    if (!mipmap && (gD3DDeviceDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL)) {
        t->uScale = 1.0f;
        t->vScale = 1.0f;
    } else {
        int method;

        s.width = width;
        s.height = height;
        method = Texture_GetSizeId();
        RenderMode_Call(Direct3D_GetRenderMode(), method, &s);
        width = s.outWidth;
        height = s.outHeight;
        pitch = (pf.dwRGBBitCount >> 3) * width;
        t->uScale = (long double)(int)s.width / (int)s.outWidth;
        t->vScale = (long double)(int)s.height / (int)s.outHeight;
    }
    t->width = width;
    t->height = height;
    t->pitch = pitch;
    t->format = format;
    t->alpha = 0.0f;
    if (d->format == 1) {
        Pool *p = &gTexPool;
        char *slot = p->data + p->free[p->used] * p->stride;

        *(int *)(slot + p->size) = p->used;
        p->used++;
        *(Texture **)slot = t;
    }
}
HOOK(0x50A940, D3DTex_AllocTexture);

/* picks the format with the most alpha bits for see through textures, the
   most color bits with some alpha for slightly see through ones, else the
   most color bits */
void D3DTex_FindFormat(DDPIXELFORMAT *out, int format, unsigned flags, float alpha)
{
    TexFormat *best = NULL;
    int most = -1, i;

    if (alpha > kAlphaMany && !(flags & FMT_NO_ALPHA)) {
        for (i = 0; i < gTexFormatCount; i++) {
            if (gTexFormats[i].aBits > most) {
                best = &gTexFormats[i];
                most = gTexFormats[i].aBits;
            }
        }
    } else if (alpha > kAlphaNone && !(flags & FMT_NO_ALPHA)) {
        for (i = 0; i < gTexFormatCount; i++) {
            if (gTexFormats[i].rgbBits > most && gTexFormats[i].aBits) {
                best = &gTexFormats[i];
                most = gTexFormats[i].rgbBits;
            }
        }
    } else {
        for (i = 0; i < gTexFormatCount; i++) {
            if (gTexFormats[i].rgbBits > most) {
                best = &gTexFormats[i];
                most = gTexFormats[i].rgbBits;
            }
        }
    }
    *out = best->pf;
}
HOOK(0x50AA70, D3DTex_FindFormat);

void D3DTex_DeAllocTexture(Texture *t)
{
    Pool *p = &gTexPool;
    LARGE_INTEGER t0, t1;
    int i;

    gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, NULL);
    gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 1, NULL);
    for (i = 0; i < p->used; i++) {
        char *slot = p->data + p->free[i] * p->stride;
        int j, k, idx;
        char *last;

        if (*(Texture **)slot != t)
            continue;
        p->used--;
        j = *(int *)(slot + p->size);
        if (j != p->used) {
            last = p->data + p->free[p->used] * p->stride;
            k = *(int *)(last + p->size);
            idx = p->free[j];
            p->free[j] = p->free[k];
            p->free[k] = idx;
            *(int *)(last + p->size) = j;
        }
        break;
    }
    if (t->surface) {
        int bytes;

        gTexCount--;
        bytes = D3DTex_SurfaceBytes(t->surface);
        gTexBytes -= bytes;
        gTexturesCreated = gTexCount;
        gTextureBytes = gTexBytes;
        QueryPerformanceCounter(&t0);
        t->surface->lpVtbl->Release(t->surface);
        QueryPerformanceCounter(&t1);
        gTexReleaseTime += t1.QuadPart - t0.QuadPart;
        t->surface = NULL;
    }
}
HOOK(0x50AB50, D3DTex_DeAllocTexture);

/* video memory of a surface and its mipmaps */
int D3DTex_SurfaceBytes(IDirectDrawSurface7 *s)
{
    char msg[1024];
    DDSURFACEDESC2 sd;
    DDSCAPS2 caps;
    HRESULT hr;
    int total = 0;

    while (s) {
        sd.dwSize = sizeof(sd);
        hr = s->lpVtbl->GetSurfaceDesc(s, &sd);
        if (hr)
            DX_FAIL("Couldn't get surface description", hr);
        total += (sd.ddpfPixelFormat.dwRGBBitCount >> 3) * sd.dwHeight * sd.dwWidth;
        caps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
        caps.dwCaps2 = 0;
        caps.dwCaps3 = 0;
        caps.dwCaps4 = 0;
        hr = s->lpVtbl->GetAttachedSurface(s, &caps, &s);
        if (hr == DDERR_NOTFOUND)
            break;
        if (hr)
            DX_FAIL("Couldn't get attached surface", hr);
    }
    return total;
}
HOOK(0x50AC90, D3DTex_SurfaceBytes);

/* Converts the texture (and its mipmaps) to 32 bit, then creates the
   surface and uploads every level from the LOD on. */
void D3DTex_DownloadTexture(TextureDesc *d)
{
    char msg[1024];
    int lod = gTexLOD;
    uint32_t *buf, *cur;
    const uint8_t *data;
    int w, h, pitch, level, bytes, method;
    DDSURFACEDESC2 sd, sd2, sd3;
    DDSCAPS2 caps;
    IDirectDrawSurface7 *surf;
    TexSize s;
    HRESULT hr;

    if (d->mipmap && (!IS_POW2(d->width) || !IS_POW2(d->height))) {
        Error_Show("Trying to download mipmap texture that is not power of 2 in width or height");
        for (;;)
            ;
    }
    gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, NULL);
    gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 1, NULL);
    buf = Mem_Alloc(d->height * d->width * 8, 4);
    w = d->width;
    pitch = d->pitch;
    h = d->height;
    data = d->data;
    cur = buf;
    do {
        switch (d->format) {
        case 1:
            Pixel_8To32(cur, w, h, 0, data, d->palette->colors, pitch, h, 0);
            break;
        case 2:
            Pixel_1555To32(cur, w, h, data, pitch >> 1, h, 0);
            break;
        case 3:
        case 4:
        case 0x43:
            Pixel_Copy32(cur, w, h, data, pitch >> 2, h, 0);
            break;
        case 5:
            Pixel_565To32(cur, w, h, data, pitch >> 1, h, 0);
            break;
        default:
            Error_Show("Unsupported texture format");
            for (;;)
                ;
        }
        data += h * pitch;
        cur += h * w;
        w >>= 1;
        h >>= 1;
        pitch >>= 1;
    } while ((d->format & FMT_MIPMAPS) && w > 0 && h > 0);
    d->tex->alpha = D3DTex_AlphaCoverage(buf, d->width, d->height);
    if (d->tex->surface) {
        gTexCount--;
        bytes = D3DTex_SurfaceBytes(d->tex->surface);
        gTexBytes -= bytes;
        gTexturesCreated = gTexCount;
        gTextureBytes = gTexBytes;
        d->tex->surface->lpVtbl->Release(d->tex->surface);
        d->tex->surface = NULL;
    }
    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    sd.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
    sd.ddsCaps.dwCaps2 = DDSCAPS2_HINTSTATIC;
    if (gD3DDeviceDesc.dwDevCaps & D3DDEVCAPS_HWRASTERIZATION)
        sd.ddsCaps.dwCaps2 = DDSCAPS2_HINTSTATIC | DDSCAPS2_TEXTUREMANAGE;
    if (!d->mipmap && (gD3DDeviceDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL)) {
        sd.dwWidth = d->width;
        sd.dwHeight = d->height;
        d->tex->uScale = 1.0f;
        d->tex->vScale = 1.0f;
    } else {
        int sw = d->width, sh = d->height;

        s.width = sw;
        s.height = sh;
        if (d->mipmap) {
            sw >>= lod & 31;
            sh >>= lod & 31;
            s.width = sw;
            s.height = sh;
            if (sw <= 1)
                s.width = 1;
            if (sh <= 1)
                s.height = 1;
        }
        method = Texture_GetSizeId();
        RenderMode_Call(Direct3D_GetRenderMode(), method, &s);
        sd.dwWidth = s.outWidth;
        sd.dwHeight = s.outHeight;
        d->tex->uScale = (long double)(int)s.width / (int)s.outWidth;
        d->tex->vScale = (long double)(int)s.height / (int)s.outHeight;
    }
    if (gActiveTMU != -1) {
        sd.dwFlags |= DDSD_TEXTURESTAGE;
        sd.dwTextureStage = Texture_GetActiveTMU();
    }
    if (d->mipmap) {
        sd.ddsCaps.dwCaps |= DDSCAPS_MIPMAP | DDSCAPS_COMPLEX;
        sd.dwFlags |= DDSD_MIPMAPCOUNT;
        sd.dwMipMapCount = D3DTex_MipLevels(sd.dwWidth, sd.dwHeight);
        if (lod >= (int)sd.dwMipMapCount - 1)
            lod = sd.dwMipMapCount - 1;
    }
    D3DTex_FindFormat(&sd.ddpfPixelFormat, d->format, d->flags, d->tex->alpha);
    hr = gDDraw->lpVtbl->CreateSurface(gDDraw, &sd, &d->tex->surface, NULL);
    if (hr)
        DX_FAIL("Couldn't create texture surface", hr);
    sd2.dwSize = sizeof(sd2);
    hr = d->tex->surface->lpVtbl->GetSurfaceDesc(d->tex->surface, &sd2);
    if (hr)
        DX_FAIL("Couldn't get surface description", hr);
    w = d->width;
    h = d->height;
    if (!d->mipmap) {
        D3DTex_Upload(d->tex->surface, sd.dwWidth, sd.dwHeight, buf, w, h);
    } else if (d->format & FMT_MIPMAPS) {
        /* the source has its own mipmaps, one after the other in buf */
        surf = d->tex->surface;
        caps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
        caps.dwCaps2 = 0;
        caps.dwCaps3 = 0;
        caps.dwCaps4 = 0;
        cur = buf;
        for (level = 0; w && h && level < lod + (int)sd.dwMipMapCount; level++) {
            if (level >= lod) {
                D3DTex_Upload(surf, w, h, cur, w, h);
                if (level != (int)sd.dwMipMapCount + lod - 1) {
                    hr = surf->lpVtbl->GetAttachedSurface(surf, &caps, &surf);
                    if (hr)
                        DX_FAIL("Couldn't get mipmap", hr);
                }
            }
            cur += w * h;
            w >>= 1;
            h >>= 1;
        }
    } else {
        /* each level is made from the one before */
        surf = d->tex->surface;
        caps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
        caps.dwCaps2 = 0;
        caps.dwCaps3 = 0;
        caps.dwCaps4 = 0;
        sd3.dwSize = sizeof(sd3);
        hr = surf->lpVtbl->GetSurfaceDesc(surf, &sd3);
        if (hr)
            DX_FAIL("Couldn't get surface description", hr);
        cur = buf;
        for (level = 0; level < (int)sd3.dwMipMapCount + lod; level++) {
            uint32_t *next;

            if (level >= lod) {
                D3DTex_Upload(surf, w, h, cur, w, h);
                if (level != (int)sd.dwMipMapCount + lod - 1) {
                    hr = surf->lpVtbl->GetAttachedSurface(surf, &caps, &surf);
                    if (hr)
                        DX_FAIL("Couldn't get mipmap", hr);
                }
            }
            next = cur + w * h;
            Pixel_Halve32(next, cur, w, h);
            cur = next;
            w >>= 1;
            h >>= 1;
        }
    }
    Mem_Free(buf);
    gTexCount++;
    bytes = D3DTex_SurfaceBytes(d->tex->surface);
    gTexBytes += bytes;
    gTexturesCreated = gTexCount;
    gTextureBytes = gTexBytes;
}
HOOK(0x50AD90, D3DTex_DownloadTexture);

int D3DTex_MipLevels(int width, int height)
{
    int n = width < height ? width : height;
    int i = n ? highest_bit(n) : 0;

    return (i > 0 ? i : 0) + 1;
}
HOOK(0x50B4C0, D3DTex_MipLevels);

/* bsf/bsr of a mask; for a zero mask they keep the given value, like the
   instructions do */
static int bsf_or(unsigned v, int keep)
{
    return v ? lowest_bit(v) : keep;
}

static int bsr_or(unsigned v, int keep)
{
    return v ? highest_bit(v) : keep;
}

typedef struct PixelShifts {
    int bRight, bLeft;
    int rRight, rLeft;
    int gRight, gLeft;
    int aRight, aLeft;
} PixelShifts;

static uint32_t convert_pixel(uint32_t p, const PixelShifts *k)
{
    return ((p & 0xFF) >> k->bRight) << k->bLeft | ((p & 0xFF0000) >> k->rRight) << k->rLeft
           | ((p & 0xFF00) >> k->gRight) << k->gLeft | ((p & 0xFF000000) >> k->aRight) << k->aLeft;
}

/* Converts 32 bit pixels to the surface format through a system memory
   surface. Missing columns and rows repeat the last pixel and row. */
void D3DTex_Upload(IDirectDrawSurface7 *dst, int width, int height, const uint32_t *src, int srcW, int srcH)
{
    char msg[1024];
    DDSURFACEDESC2 sd, tmp;
    DDPIXELFORMAT *pf = &sd.ddpfPixelFormat;
    IDirectDrawSurface7 *surf;
    PixelShifts k;
    int aShift, aBits, rShift, gShift, bShift, rHigh, gHigh, bHigh;
    const uint32_t *row;
    char *pixels;
    int pitch, x, y;
    HRESULT hr;

    sd.dwSize = sizeof(sd);
    hr = dst->lpVtbl->GetSurfaceDesc(dst, &sd);
    if (hr)
        DX_FAIL("Couldn't get surfacedesc", hr);
    memset(&tmp, 0, sizeof(tmp));
    tmp.dwSize = sizeof(tmp);
    tmp.dwHeight = height;
    tmp.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    tmp.dwWidth = width;
    tmp.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
    tmp.ddpfPixelFormat = *pf;
    hr = gDDraw->lpVtbl->CreateSurface(gDDraw, &tmp, &surf, NULL);
    if (hr)
        DX_FAIL("Couldn't create surface", hr);
    hr = surf->lpVtbl->Lock(surf, NULL, &tmp, DDLOCK_WAIT, NULL);
    if (hr)
        DX_FAIL("Couldn't lock surface", hr);
    pixels = tmp.lpSurface;
    pitch = tmp.lPitch;
    if (pf->dwRGBAlphaBitMask) {
        aShift = lowest_bit(pf->dwRGBAlphaBitMask);
        rShift = highest_bit(pf->dwRGBAlphaBitMask);
        aBits = rShift - aShift + 1;
    } else {
        aShift = 0;
        aBits = 0;
        rShift = 0;
    }
    rShift = bsf_or(pf->dwRBitMask, rShift);
    gShift = bsf_or(pf->dwGBitMask, 0);
    bShift = bsf_or(pf->dwBBitMask, gShift);
    rHigh = bsr_or(pf->dwRBitMask, 0);
    gHigh = bsr_or(pf->dwGBitMask, rHigh);
    bHigh = bsr_or(pf->dwBBitMask, 0);
    k.aRight = (32 - aBits) & 31;
    k.gRight = (gShift - gHigh + 15) & 31;
    k.bRight = (bShift - bHigh + 7) & 31;
    k.rRight = (rShift - rHigh + 23) & 31;
    k.aLeft = aShift & 31;
    k.rLeft = rShift & 31;
    k.gLeft = gShift & 31;
    k.bLeft = bShift & 31;
    if (!(pf->dwFlags & DDPF_RGB)) {
        Error_Show("Invalid texture format");
        for (;;)
            ;
    }
    if (pf->dwRGBBitCount == 32) {
        if (pf->dwRBitMask == 0xFF0000 && pf->dwGBitMask == 0xFF00 && pf->dwBBitMask == 0xFF && srcW == width
            && srcH == height && pitch == width * 4) {
            memcpy(pixels, src, (unsigned)(width * height) * 4);
        } else {
            row = src;
            for (y = 0; y < srcH; y++) {
                uint32_t *d = (uint32_t *)(pixels + y * pitch);
                uint32_t last;

                for (x = 0; x < srcW; x++)
                    *d++ = convert_pixel(row[x], &k);
                last = d[-1];
                for (x = srcW; x < width; x++)
                    *d++ = last;
                row += srcW;
            }
            row = src + (srcH - 1) * srcW;
            for (y = srcH; y < height; y++) {
                uint32_t *d = (uint32_t *)(pixels + y * pitch);
                uint32_t last;

                for (x = 0; x < srcW; x++)
                    *d++ = convert_pixel(row[x], &k);
                last = d[-1];
                for (x = srcW; x < width; x++)
                    *d++ = last;
            }
        }
    } else if (pf->dwRGBBitCount == 16) {
        row = src;
        for (y = 0; y < srcH; y++) {
            uint16_t *d = (uint16_t *)(pixels + y * pitch);
            uint16_t last;

            for (x = 0; x < srcW; x++)
                *d++ = convert_pixel(row[x], &k);
            last = d[-1];
            for (x = srcW; x < width; x++)
                *d++ = last;
            row += srcW;
        }
        row = src + (srcH - 1) * srcW;
        for (y = srcH; y < height; y++) {
            uint16_t *d = (uint16_t *)(pixels + y * pitch);
            uint16_t last;

            for (x = 0; x < srcW; x++)
                *d++ = convert_pixel(row[x], &k);
            last = d[-1];
            for (x = srcW; x < width; x++)
                *d++ = last;
        }
    } else {
        Error_Show("Invalid texture format");
        for (;;)
            ;
    }
    hr = surf->lpVtbl->Unlock(surf, NULL);
    if (hr)
        DX_FAIL("Couldn't unlock surface", hr);
    hr = dst->lpVtbl->Blt(dst, NULL, surf, NULL, DDBLT_WAIT, NULL);
    if (hr)
        DX_FAIL("Couldn't blit surface", hr);
    surf->lpVtbl->Release(surf);
}
HOOK(0x50B4F0, D3DTex_Upload);

void D3DTex_GetTMUs(int *out)
{
    WORD stages = gD3DDeviceDesc.wMaxTextureBlendStages;
    WORD textures = gD3DDeviceDesc.wMaxSimultaneousTextures;

    *out = textures < stages ? textures : stages;
}
HOOK(0x50BB80, D3DTex_GetTMUs);

void D3DTex_SetActiveTMU(int *in)
{
    gActiveTMU = *in;
}
HOOK(0x50BBB0, D3DTex_SetActiveTMU);

void D3DTex_GetActiveTMU(int *out)
{
    *out = gActiveTMU;
}
HOOK(0x50BBC0, D3DTex_GetActiveTMU);

void D3DTex_SetLOD(int *in)
{
    gTexLOD = *in;
}
HOOK(0x50BBD0, D3DTex_SetLOD);

HRESULT WINAPI D3DTex_AddFormat(DDPIXELFORMAT *pf, void *ctx);

void D3DTex_EnumFormats(void)
{
    gTexFormatCount = 0;
    gD3DDevice->lpVtbl->EnumTextureFormats(gD3DDevice, D3DTex_AddFormat, NULL);
}
HOOK(0x50BBE0, D3DTex_EnumFormats);

/* keeps RGB formats of up to 16 bits, with or without alpha */
HRESULT WINAPI D3DTex_AddFormat(DDPIXELFORMAT *pf, void *ctx)
{
    TexFormat *f;

    if (pf->dwRGBBitCount > 16 || !(pf->dwFlags & DDPF_RGB) || !pf->dwRBitMask || !pf->dwGBitMask
        || !pf->dwBBitMask || (pf->dwFlags & ~(DDPF_RGB | DDPF_ALPHAPIXELS)))
        return D3DENUMRET_OK;
    f = &gTexFormats[gTexFormatCount++];
    f->pf = *pf;
    if (pf->dwRGBAlphaBitMask) {
        f->aShift = lowest_bit(pf->dwRGBAlphaBitMask);
        f->aBits = highest_bit(pf->dwRGBAlphaBitMask) - f->aShift + 1;
    } else {
        f->aShift = 0;
        f->aBits = 0;
    }
    f->rShift = lowest_bit(pf->dwRBitMask);
    f->gShift = lowest_bit(pf->dwGBitMask);
    f->bShift = lowest_bit(pf->dwBBitMask);
    f->rBits = highest_bit(pf->dwRBitMask) + 1 - f->rShift;
    f->gBits = highest_bit(pf->dwGBitMask) + 1 - f->gShift;
    f->bBits = highest_bit(pf->dwBBitMask) + 1 - f->bShift;
    f->rgbBits = f->bBits + f->gBits + f->rBits;
    return D3DENUMRET_OK;
}
HOOK(0x50BC00, D3DTex_AddFormat);

void D3DTex_Shutdown(void)
{
    Pool_Free(&gTexPool);
}
HOOK(0x50BD70, D3DTex_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5CA124) int gTexturesCreated;
BSS(5CA124) int gTextureBytes;
BSS(A7C128) Pool gTexPool;
PAD(igb, A7C128, A7C140, 0xD6C);
ALIAS(gTexFormats, pad_A7C140, 0);
BSS(A7C128) int gActiveTMU;
BSS(A7C128) int gTexLOD;
BSS(A7C128) int gTexCount;
BSS(A7C128) int gTexBytes;
BSS(A7C128) int gTexFormatCount;
BSS(A7C128) LONGLONG gTexReleaseTime;
BSS(BCAC00) D3DDEVICEDESC7 gD3DDeviceDesc;
