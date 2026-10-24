#include "hook.h"
#include "igi.h"
#include "res/sprite.h"
#include "qvertex/qvertextypes.h"

ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int id));
ORIG(0x509930, void, QVertex_InitDriver, (void));
ORIG(0x509960, void, QVertex_ShutdownDriver, (void));

extern void (*gRenderFuncs[])(QVertexStrip *);
extern Texture *gQVertexTexture;
extern int gQVertexStrip;
extern Palette *gQVertexPalette;
extern const float gColorScale;
extern const float gPixelCenter;

static void QVertex_Set(QVertex *v, float x, float y, float z, float u, float w, float a, float r, float g, float b)
{
    v->x = x;
    v->y = y;
    v->z = z;
    v->u = u;
    v->v = w;
    v->a = a;
    v->r = r;
    v->g = g;
    v->b = b;
}

static void QVertex_Draw(Sprite *s, Texture *t, int untextured, unsigned int flags, int unk8, int count, QVertex *verts)
{
    QVertexStrip strip;

    strip.lines = untextured;
    strip.flags = flags;
    strip.queue = unk8;
    strip.count = count;
    strip.verts = verts;
    gQVertexTexture = t;
    gQVertexPalette = s ? s->palette : NULL;
    gRenderFuncs[gQVertexStrip](&strip);
}

void QVertex_DrawFrame(Sprite *s, float x, float y, float r, float g, float b, float a, float z, int frame,
                       unsigned int flags, int unk20)
{
    SpriteFrame *f = &s->frames[frame];
    QVertex v[2];
    float cr = r * gColorScale, cg = g * gColorScale, cb = b * gColorScale;

    QVertex_Set(&v[0], (long double)f->x + x, (long double)f->y + y, z, f->uv[0], f->uv[1], a, cr, cg, cb);
    QVertex_Set(&v[1], (long double)(f->w + f->x) + x, (long double)(f->h + f->y) + y, z, f->uv[2], f->uv[3], a, cr, cg,
                cb);
    QVertex_Draw(s, f->texture, 0, flags, unk20, 2, v);
}
HOOK(0x4B50B0, QVertex_DrawFrame);

void QVertex_DrawQuad(Sprite *s, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                      float r, float g, float b, int frame, unsigned int flags, int unk20)
{
    SpriteFrame *f = &s->frames[frame];
    QVertex v[4];
    float cr = r * gColorScale, cg = g * gColorScale, cb = b * gColorScale;

    QVertex_Set(&v[0], x0, y0, 1.0f, f->uv[0], f->uv[1], 1.0f, cr, cg, cb);
    QVertex_Set(&v[1], x1, y1, 1.0f, f->uv[2], f->uv[1], 1.0f, cr, cg, cb);
    QVertex_Set(&v[2], x2, y2, 1.0f, f->uv[0], f->uv[3], 1.0f, cr, cg, cb);
    QVertex_Set(&v[3], x3, y3, 1.0f, f->uv[2], f->uv[3], 1.0f, cr, cg, cb);
    QVertex_Draw(s, f->texture, 0, flags, unk20, 4, v);
}
HOOK(0x4B51F0, QVertex_DrawQuad);

void QVertex_DrawQuadEx(Sprite *s, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                        float r, float g, float b, float z, float a, int frame, unsigned int flags, int unk20)
{
    SpriteFrame *f = &s->frames[frame];
    QVertex v[4];
    float cr = r * gColorScale, cg = g * gColorScale, cb = b * gColorScale;

    QVertex_Set(&v[0], x0, y0, z, f->uv[0], f->uv[1], a, cr, cg, cb);
    QVertex_Set(&v[1], x1, y1, z, f->uv[2], f->uv[1], a, cr, cg, cb);
    QVertex_Set(&v[2], x2, y2, z, f->uv[0], f->uv[3], a, cr, cg, cb);
    QVertex_Set(&v[3], x3, y3, z, f->uv[2], f->uv[3], a, cr, cg, cb);
    QVertex_Draw(s, f->texture, 0, flags, unk20, 4, v);
}
HOOK(0x4B53B0, QVertex_DrawQuadEx);

void QVertex_DrawQuadUV(Sprite *s, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                        float u0, float v0, float u1, float v1, float u2, float v2, float u3, float v3,
                        float r, float g, float b, float z, float a, int frame, unsigned int flags, int unk20)
{
    QVertex v[4];
    float cr = r * gColorScale, cg = g * gColorScale, cb = b * gColorScale;

    QVertex_Set(&v[0], x0, y0, z, u0, v0, a, cr, cg, cb);
    QVertex_Set(&v[1], x1, y1, z, u1, v1, a, cr, cg, cb);
    QVertex_Set(&v[2], x2, y2, z, u2, v2, a, cr, cg, cb);
    QVertex_Set(&v[3], x3, y3, z, u3, v3, a, cr, cg, cb);
    QVertex_Draw(s, s->frames[frame].texture, 0, flags, unk20, 4, v);
}
HOOK(0x4B5560, QVertex_DrawQuadUV);

void QVertex_DrawLine(float x0, float y0, float z0, float x1, float y1, float z1, float r0, float g0, float b0,
                      float r1, float g1, float b1, float a, unsigned int flags, int unk20)
{
    QVertex v[2];

    QVertex_Set(&v[0], x0 - gPixelCenter, y0 - gPixelCenter, z0, 0, 0, a, r0 * gColorScale, g0 * gColorScale,
                b0 * gColorScale);
    QVertex_Set(&v[1], x1 - gPixelCenter, y1 - gPixelCenter, z1, 0, 0, a, r1 * gColorScale, g1 * gColorScale,
                b1 * gColorScale);
    QVertex_Draw(NULL, NULL, 1, flags, unk20, 2, v);
}
HOOK(0x4B5730, QVertex_DrawLine);

static float Fcos(float x)
{
    float r;

    __asm__("fcos" : "=t"(r) : "0"(x));
    return r;
}

static float Fsin(float x)
{
    float r;

    __asm__("fsin" : "=t"(r) : "0"(x));
    return r;
}

/* The intermediate values stay on the FPU stack in the original, so this
   uses long double where it keeps full precision and float where it
   stores to memory. */
/* fchs, so gcc cannot turn -(a - b) into b - a (they round apart in round-down) */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

void QVertex_DrawPolyline(const float *p, int n, float ox, float oy, float z, float r, float g, float b, float a,
                          float angle, float sx, float sy, unsigned int flags, int unk20)
{
    volatile float c = Fcos(angle), s = Fsin(angle);
    int i;

    for (i = n - 1; i > 0; i--, p += 2) {
        long double x0 = (long double)sx * p[0];
        long double y0 = (long double)sy * p[1];
        volatile float x1 = (long double)sx * p[2];
        volatile float y1f = (long double)sy * p[3];
        long double y1 = (long double)sy * p[3];

        if (angle < 0.0 || angle > 0.0) {
            volatile float ry0 = x0 * s + y0 * c;
            long double rx0 = x87_neg(y0 * s - x0 * c);
            long double ry1 = (long double)x1 * s + (long double)y1f * c;

            x1 = x87_neg((long double)y1f * s - (long double)x1 * c);
            x0 = rx0;
            y0 = ry0;
            y1 = ry1;
        }
        QVertex_DrawLine((float)(x0 + ox), (float)(y0 + oy), z, (float)((long double)x1 + ox), (float)(y1 + oy), z, r,
                         g, b, r, g, b, a, flags, unk20);
    }
}
HOOK(0x4B5860, QVertex_DrawPolyline);

void QVertex_Init(void)
{
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    gQVertexStrip = RenderMode_CreateMethod("QVertex:RegisterStrip");
    QVertex_InitDriver();
}
HOOK(0x4B59A0, QVertex_Init);

void QVertex_Shutdown(void)
{
    QVertex_ShutdownDriver();
    RenderMode_DeleteMethod(gQVertexStrip);
}
HOOK(0x4B59D0, QVertex_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(534AB0) const float gColorScale INIT(0.0078125f);
BSS(A84A50) Texture *gQVertexTexture;
ALIAS(gDrawUnkA84A50, gQVertexTexture, 0);
BSS(A84A58) Palette *gQVertexPalette;
ALIAS(gDrawUnkA84A58, gQVertexPalette, 0);
