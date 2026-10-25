#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "qvertex/qvertex.h"
#include "sysmisc/sysmisctypes.h"
#include "transctx/transctxtypes.h"
#include "file/filevars.h"
#include "math/mathvars.h"
#include "res/resvars.h"
#include "sysmisc/sysmiscvars.h"
#include "weapon/weaponvars.h"
#include "transctx/transctxvars.h"

FN_EXTERN(Task_Nothing);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x497800, void *, Frame_Alloc, (int size));
ORIG(0x497860, void, Primitive_Draw, (void *prim));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));
ORIG(0x498370, int, Offscreen_GetMode, (void));

/* the shared do-nothing function */
#define NOTHING (FN(0x4D3710, Task_Nothing))

/* One palette slot only: slot n is byte n - 1. */
extern char gPaletteSlots[];
extern uint8_t gPaletteColors[];
extern uint16_t gPalette565[];

QV_F32(kZeroLocal, 0x00000000);
QV_F32(kOneLocal, 0x3F800000);
QV_F32(k255Local, 0x437F0000);
QV_F32(kInv255, 0x3B808081);

/* A sprite primitive of the offscreen driver: one textured or plain
   rectangle. */
typedef struct OffscreenSprite {
    void *next;
    int method;
    int unk08;
    Texture tex;
    Palette pal;
    unsigned int flags;
    short hasPalette;
    short hasTexture;
    float u0, v0;
    float x, y;
    float alpha;
    float r, g, b;
    float u1, v1;
    float w, h;
    int unk74[4];
} OffscreenSprite;

_Static_assert(sizeof(OffscreenSprite) == 0x84, "OffscreenSprite");

void OffscreenBuffer_AllocTexture(TextureDesc *d);
void OffscreenBuffer_DeAllocTexture(Texture *t);
void OffscreenBuffer_AllocPalette(PaletteDesc *d);
void OffscreenBuffer_DeAllocPalette(Palette *p);
void OffscreenBuffer_QVertexStrip(QVertexStrip *s);
void OffscreenBufferSprite_Draw(OffscreenSprite *s);
void OffscreenBufferSprite_RenderRect(OffscreenSprite *s);
void OffscreenBufferSprite_RenderSprite(OffscreenSprite *s);

void OffscreenBuffer_InitTextures(void)
{
    int mode = Offscreen_GetMode();

    RenderMode_SetMethod(mode, gAllocTextureId, OffscreenBuffer_AllocTexture);
    RenderMode_SetMethod(mode, gDeAllocTextureId, OffscreenBuffer_DeAllocTexture);
    RenderMode_SetMethod(mode, gDownloadTextureId, NOTHING);
    RenderMode_SetMethod(mode, gAllocPalette, OffscreenBuffer_AllocPalette);
    RenderMode_SetMethod(mode, gDeAllocPalette, OffscreenBuffer_DeAllocPalette);
    RenderMode_SetMethod(mode, gDownloadPalette, NOTHING);
    memset(gPaletteSlots, 0, 8);
}
HOOK(0x50BD80, OffscreenBuffer_InitTextures);

void OffscreenBuffer_AllocTexture(TextureDesc *d)
{
    Texture *t = (Texture *)d->tex;

    t->pixels = Mem_Alloc(d->pitch * d->height, 4);
    memcpy(t->pixels, d->data, (unsigned)(d->pitch * d->height));
    t->width = d->width;
    t->height = d->height;
    t->pitch = d->pitch;
    t->format = d->format;
}
HOOK(0x50BE10, OffscreenBuffer_AllocTexture);

void OffscreenBuffer_DeAllocTexture(Texture *t)
{
    Mem_Free(t->pixels);
    t->pixels = NULL;
}
HOOK(0x50BE70, OffscreenBuffer_DeAllocTexture);

/* Converts the 256 colours to RGB565. Hangs if the slot is taken. */
void OffscreenBuffer_AllocPalette(PaletteDesc *d)
{
    Palette *p = (Palette *)d->palette;
    const uint8_t *c;
    int i;

    if (gPaletteSlots[0])
        for (;;);
    gPaletteSlots[0] = 1;
    p->slot = 1;
    p->colors = gPalette565;
    memcpy(gPaletteColors, d->colors, 0x400);
    for (i = 0; i < 256; i++) {
        c = gPaletteColors + i * 4;
        gPalette565[i] = (((c[2] & 0xF8) << 5 | (c[1] & 0xFC)) << 3) | c[0] >> 3;
    }
}
HOOK(0x50BE90, OffscreenBuffer_AllocPalette);

void OffscreenBuffer_DeAllocPalette(Palette *p)
{
    gPaletteSlots[p->slot - 1] = 0;
}
HOOK(0x50BF10, OffscreenBuffer_DeAllocPalette);

void OffscreenBuffer_InitQVertex(void)
{
    RenderMode_SetMethod(Offscreen_GetMode(), gQVertexStrip, OffscreenBuffer_QVertexStrip);
    RenderMode_SetMethod(Offscreen_GetMode(), gQVertexDrawStrip, OffscreenBufferSprite_Draw);
}
HOOK(0x50EC00, OffscreenBuffer_InitQVertex);

/* Only two vertex strips (sprite corners) are supported. */
void OffscreenBuffer_QVertexStrip(QVertexStrip *s)
{
    OffscreenSprite local, *n;
    QVertex *v;

    if (s->count != 2) {
        Error_Show("Strip length not supported.");
        for (;;);
    }
    n = s->queue < 0 ? &local : Frame_Alloc(sizeof(OffscreenSprite));
    if (!n)
        return;
    n->method = gQVertexDrawStrip;
    n->flags = s->flags;
    if (gQVertexTexture) {
        n->tex = *gQVertexTexture;
        n->hasTexture = 1;
    } else {
        n->hasTexture = 0;
    }
    if (gQVertexPalette) {
        n->pal = *gQVertexPalette;
        n->hasPalette = 1;
    } else {
        n->hasPalette = 0;
    }
    v = s->verts;
    n->u0 = v[0].u;
    n->v0 = v[0].v;
    n->u1 = v[1].u;
    n->v1 = v[1].v;
    n->x = v[0].x;
    n->y = v[0].y;
    n->r = v[0].r;
    n->g = v[0].g;
    n->b = v[0].b;
    /* the size is the first corner again, so plain rectangles are as
       wide as their x */
    n->w = v[0].x;
    n->h = v[0].y;
    if (n->flags & 0x80000)
        n->alpha = v[0].a;
    else
        n->alpha = kOneLocal.f;
    if (s->queue < 0)
        Primitive_Draw(n);
    else
        RenderQueue_Add(s->queue, n);
}
HOOK(0x50EC40, OffscreenBuffer_QVertexStrip);

void OffscreenBufferSprite_Draw(OffscreenSprite *s)
{
    if (s->hasTexture)
        OffscreenBufferSprite_RenderSprite(s);
    else
        OffscreenBufferSprite_RenderRect(s);
}
HOOK(0x50EDA0, OffscreenBufferSprite_Draw);

/* Blends a plain colour over the rectangle: c * (1 - a) + colour * a. */
void OffscreenBufferSprite_RenderRect(OffscreenSprite *s)
{
    int x, y, cx0, cy0, cx1, cy1, h, i, j, xx, yy;
    uint32_t *row, *p, c, nr, ng, nb;
    int R, G, B;
    long double a;
    float inv;

    if (!gOffscreenTarget.pixels)
        return;
    y = QV_Fist(s->y);
    x = QV_Fist(s->x);
    row = gOffscreenTarget.pixels + (gOffscreenTarget.width * y + x);
    cx0 = QV_Fist(gOffscreenClip[0]);
    cy0 = QV_Fist(gOffscreenClip[1]);
    cx1 = QV_Fist(gOffscreenClip[2]) + cx0;
    cy1 = QV_Fist(gOffscreenClip[3]) + cy0;
    if (x >= cx1 || y >= cy1)
        return;
    if (QV_Fist(s->w) + x < cx0)
        return;
    h = QV_Fist(s->h);
    if (h + y < cy0)
        return;
    a = s->alpha;
    inv = kOneLocal.f - a;
    R = QV_Fist(a * s->r * k255Local.f);
    G = QV_Fist(a * s->g * k255Local.f);
    B = QV_Fist(a * s->b * k255Local.f);
    if (h <= 0)
        return;
    j = 0;
    yy = y;
    do {
        if (yy >= cy0 && yy < cy1 && kZeroLocal.f < s->w) {
            i = 0;
            xx = x;
            p = row;
            do {
                if (xx >= cx0 && xx < cx1) {
                    c = *p;
                    nr = R + QV_Fist((int)(c >> 16 & 0xFF) * (long double)inv);
                    ng = QV_Fist((int)(c >> 8 & 0xFF) * (long double)inv) + G;
                    nb = QV_Fist((int)(c & 0xFF) * (long double)inv) + B;
                    if (nr > 0xFF)
                        nr = 0xFF;
                    if (ng > 0xFF)
                        ng = 0xFF;
                    if (nb > 0xFF)
                        nb = 0xFF;
                    *p = ((nr & 0xFF) << 8 | (ng & 0xFFFF)) << 8 | (nb & 0xFFFFFF);
                }
                i++;
                xx++;
                p++;
            } while (!((long double)i >= s->w));
        }
        row += gOffscreenTarget.width;
        j++;
        yy++;
    } while (j < QV_Fist(s->h));
}
HOOK(0x50EDC0, OffscreenBufferSprite_RenderRect);

static int Clamp255(long double v)
{
    if (kZeroLocal.f > v)
        v = kZeroLocal.f;
    else if (!(kOneLocal.f >= v))
        v = kOneLocal.f;
    return QV_Fist(v * k255Local.f);
}

void OffscreenBufferSprite_RenderSprite(OffscreenSprite *s)
{
    int tw, th, u0, v0, du, dv, x, y, cx0, cy0, cx1, cy1, white, i, j, k, xx, yy;
    long double ftw;
    float fth;
    uint32_t *row, *p, c, d, out;

    if (!gOffscreenTarget.pixels)
        return;
    tw = s->tex.width;
    th = s->tex.height;
    ftw = tw;
    fth = th;
    u0 = QV_Fist(ftw * s->u0);
    v0 = QV_Fist(fth * s->v0);
    du = QV_Fist(ftw * s->u1) - u0;
    dv = QV_Fist(fth * s->v1) - v0;
    y = QV_Fist(s->y);
    x = QV_Fist(s->x);
    row = gOffscreenTarget.pixels + (gOffscreenTarget.width * y + x);
    cx0 = QV_Fist(gOffscreenClip[0]);
    cy0 = QV_Fist(gOffscreenClip[1]);
    cx1 = QV_Fist(gOffscreenClip[2]) + cx0;
    cy1 = QV_Fist(gOffscreenClip[3]) + cy0;
    if (x >= cx1 || y >= cy1 || x + du < cx0 || y + dv < cy0)
        return;
    white = QV_Bits(&s->r) == 0x3F800000 && QV_Bits(&s->g) == 0x3F800000 && QV_Bits(&s->b) == 0x3F800000;

    if (s->hasPalette) {
        /* 8 bit: always the whole texture, the uv range is ignored */
        const uint16_t *pal = s->pal.colors;
        const uint8_t *src = s->tex.pixels, *sp;

        if (th <= 0)
            return;
        j = 0;
        yy = y;
        do {
            if (yy >= cy0 && yy < cy1 && tw > 0) {
                i = 0;
                sp = src;
                p = row;
                do {
                    xx = x + i;
                    if (xx >= cx0 && xx < cx1) {
                        if (white) {
                            out = pal[*sp];
                        } else {
                            c = pal[*sp];
                            out = QV_Fist((int)(c >> 5 & 0x3F) * s->g);
                            out |= (uint32_t)QV_Fist((int)(c >> 11) * s->r) << 9;
                            out = (out << 7 | QV_Fist((int)(c & 0x1F) * s->b)) << 3;
                        }
                        *p = out;
                    }
                    i++;
                    sp++;
                    p++;
                } while (i < s->tex.width);
            }
            row += gOffscreenTarget.width;
            src += s->tex.pitch;
            j++;
            yy++;
        } while (j < s->tex.height);
        return;
    }

    if (s->tex.format == 2) {
        /* 1555, pixels without the top bit are transparent */
        const uint8_t *src = (const uint8_t *)s->tex.pixels + ((uint32_t)(s->tex.pitch * v0) >> 1) * 2;
        const uint16_t *sp;

        if (dv <= 0)
            return;
        yy = y;
        j = dv;
        do {
            if (yy >= cy0 && yy < cy1 && du > 0) {
                sp = (const uint16_t *)src + u0;
                p = row;
                xx = x;
                k = du;
                do {
                    if (xx >= cx0 && xx < cx1 && (*sp & 0x8000)) {
                        c = *sp;
                        if (white) {
                            out = ((((c & 0x7C00) << 3 | (c & 0x3E0)) << 3) | (c & 0x1F)) << 3;
                        } else {
                            out = QV_Fist((int)(c >> 5 & 0x1F) * s->g);
                            out = (out | (uint32_t)QV_Fist((int)(c >> 10 & 0x1F) * s->r) << 8) << 8;
                            out = (out | QV_Fist((int)(c & 0x1F) * s->b)) << 3;
                        }
                        *p = out;
                    }
                    sp++;
                    xx++;
                    p++;
                } while (--k);
            }
            row += gOffscreenTarget.width;
            src += ((uint32_t)(int)s->tex.pitch >> 1) * 2;
            yy++;
        } while (--j);
    } else if (s->tex.format == 3) {
        /* 8888, alpha blended */
        const uint8_t *src = (const uint8_t *)s->tex.pixels + ((uint32_t)(s->tex.pitch * v0) >> 2) * 4;
        const uint32_t *sp;
        long double a, inv, db;
        float tr, tg, tb, dr, dg;

        if (dv <= 0)
            return;
        yy = y;
        j = dv;
        do {
            if (yy >= cy0 && yy < cy1 && du > 0) {
                sp = (const uint32_t *)src + u0;
                p = row;
                xx = x;
                k = du;
                do {
                    if (xx >= cx0 && xx < cx1) {
                        c = *sp;
                        a = (int)(c >> 24) * (long double)kInv255.f;
                        if (a != kZeroLocal.f) {
                            inv = kOneLocal.f - a;
                            d = *p;
                            tr = (int)(c >> 16 & 0xFF) * (long double)s->r * a * kInv255.f;
                            tg = (int)(c >> 8 & 0xFF) * (long double)s->g * a * kInv255.f;
                            tb = (int)(c & 0xFF) * (long double)s->b * a * kInv255.f;
                            dr = (int)(d >> 16 & 0xFF) * inv * kInv255.f;
                            dg = (int)(d >> 8 & 0xFF) * inv * kInv255.f;
                            db = (int)(d & 0xFF) * inv * kInv255.f;
                            out = (uint32_t)Clamp255((long double)dr + tr) << 8;
                            out = (out | Clamp255((long double)dg + tg)) << 8;
                            *p = out | Clamp255(db + tb);
                        }
                    }
                    xx++;
                    sp++;
                    p++;
                } while (--k);
            }
            row += gOffscreenTarget.width;
            src += ((uint32_t)(int)s->tex.pitch >> 2) * 4;
            yy++;
        } while (--j);
    } else {
        Error_Show("OffscreenBufferSprite_RenderSprite() : Texture type not supported.");
        for (;;);
    }
}
HOOK(0x50F070, OffscreenBufferSprite_RenderSprite);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, A7CEC8, 1) char store_A7CEC8[16];
ALIAS(gPaletteSlots, store_A7CEC8, 0);
STORE(igb, BC7A00, 2) uint16_t store_BC7A00[2048];
ALIAS(gPalette565, store_BC7A00, 0);
STORE(igb, BC7A00, 1) uint8_t store_BC8A00[8192];
ALIAS(gPaletteColors, store_BC8A00, 0);
