/* Terrain triangle clipping: against the near plane in view space, then
   against the four scene edges in screen space, splitting a triangle in two
   when one corner is cut off. The pieces go to a TL vertex and index list. */
#pragma GCC optimize("excess-precision=standard")

#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "transctx/transctxtypes.h"

#define CLIP_LEFT 1
#define CLIP_TOP 2
#define CLIP_RIGHT 4
#define CLIP_BOTTOM 8
#define CLIP_NEAR 0x10

/* A corner as the terrain renderer passes it. Corners behind the near plane
   hold their view space position in sx, sy, q. c[] holds colour, uv and
   fog, multiplied by q once the near plane clipping is done. */
typedef struct ClipVtx {
    float sx, sy, q, rhw;
    uint32_t color, specular;
    float u, v;
    uint8_t code;
    uint8_t pad21[3];
    float *pos;
    float c[6];
} ClipVtx;

typedef struct TLVertexSpec {
    float sx, sy, sz, rhw;
    uint32_t color, specular;
    float tu, tv;
} TLVertexSpec;

typedef struct ClipOut {
    TLVertexSpec *verts;
    int *vertCount;
    int16_t *indices;
    int *triCount;
    int base;
} ClipOut;

_Static_assert(sizeof(ClipVtx) == 0x40, "ClipVtx");

extern int gClipCount;
extern float gClipZBias;
extern float gClipZScale;
extern Scene gScene;
extern float gClipRhwScale;
extern float gNearPlane;
extern float gTerrainScaleY;
extern float gTerrainScaleX;
extern float gTerrainCenterX;
extern float gTerrainCenterY;

static const union {
    uint32_t u;
    float f;
} kOne = { 0x3F800000 };

static inline int64_t fistp64(long double x)
{
    int64_t r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return r;
}

/* NaN counts as outside on the left and top edges only. */
#define OUT_L(x) (!((x) >= gScene.minX))
#define OUT_T(y) (!((y) >= gScene.minY))
#define OUT_R(x) ((x) > gScene.maxX)
#define OUT_B(y) ((y) > gScene.maxY)

static uint8_t outcode(long double xl, long double yt, long double xr, long double yb)
{
    return OUT_B(yb) << 3 | OUT_R(xr) << 2 | OUT_T(yt) << 1 | OUT_L(xl);
}

static void lerp_attrs(ClipVtx *o, const ClipVtx *in, long double t)
{
    int k;

    for (k = 0; k < 6; k++)
        o->c[k] = (o->c[k] - in->c[k]) * t + in->c[k];
}

/* o.q, o.c[] towards in with sx and sy done by the caller */
static void lerp_rest(ClipVtx *o, const ClipVtx *in, long double t)
{
    o->q = (o->q - in->q) * t + in->q;
    lerp_attrs(o, in, t);
}

static void lerp_all(ClipVtx *o, const ClipVtx *in, long double t)
{
    o->sx = (o->sx - in->sx) * t + in->sx;
    o->sy = (o->sy - in->sy) * t + in->sy;
    lerp_rest(o, in, t);
}

static void lerp_pos(float *o, const float *in, long double t)
{
    o[0] = (o[0] - in[0]) * t + in[0];
    o[1] = (o[1] - in[1]) * t + in[1];
    o[2] = (o[2] - in[2]) * t + in[2];
}

static long double near_t(const float *in, const float *o)
{
    return (gNearPlane - in[2]) / (o[2] - in[2]);
}

static void view_pos(ClipVtx *v, float *p)
{
    v->pos = p;
    if (!(v->code & CLIP_NEAR)) {
        p[2] = kOne.f / v->q;
        p[0] = (v->sx - gTerrainCenterX) / (gTerrainScaleX * v->q);
        p[1] = (v->sy - gTerrainCenterY) / (gTerrainScaleY * v->q);
    } else {
        p[0] = v->sx;
        p[1] = v->sy;
        p[2] = v->q;
    }
    v->c[0] = (int)(v->color >> 16 & 0xFF);
    v->c[1] = (int)(v->color >> 8 & 0xFF);
    v->c[2] = (int)(v->color & 0xFF);
    v->c[3] = v->u;
    v->c[4] = v->v;
    v->c[5] = (int)(v->specular >> 24);
}

static void premultiply(ClipVtx *v)
{
    int k;

    for (k = 0; k < 6; k++)
        v->c[k] = v->q * v->c[k];
}

static void emit(ClipVtx **v, ClipOut *out)
{
    int16_t *idx;
    int k, n;

    n = *out->triCount;
    *out->triCount = n + 1;
    idx = out->indices + n * 3;
    for (k = 0; k < 3; k++) {
        ClipVtx *p = v[k];
        long double q = p->q;
        float inv = kOne.f / q;
        TLVertexSpec *d;

        p->rhw = q * gClipRhwScale;
        p->q = q * gClipZScale + gClipZBias;
        idx[k] = (int16_t)(*out->vertCount + out->base);
        n = *out->vertCount;
        d = out->verts + n;
        *out->vertCount = n + 1;
        memcpy(d, p, sizeof(*d));
        d->tu = inv * p->c[3];
        d->tv = inv * p->c[4];
        d->specular = (uint32_t)fistp64(inv * p->c[5]) << 24;
        d->color = ((uint32_t)fistp64(inv * p->c[0]) << 8 | (uint32_t)fistp64(inv * p->c[1])) << 8 |
                   (uint32_t)fistp64(inv * p->c[2]);
    }
}

/* Picks the corners for a clip by bit: with two corners outside, in is the
   one inside; with one outside, o is that one. The other two follow in
   triangle order. */
static void pick(ClipVtx **v, int bit, int inside, ClipVtx **first, ClipVtx **second, ClipVtx **third)
{
    int k;

    if ((!(v[0]->code & bit)) == inside)
        k = 0;
    else if ((!(v[1]->code & bit)) == inside)
        k = 1;
    else
        k = 2;
    *first = v[k];
    *second = v[(k + 1) % 3];
    *third = v[(k + 2) % 3];
}

void TerrDraw_ClipTriangle(ClipVtx a, ClipVtx b, ClipVtx c, ClipOut *out)
{
    ClipVtx *v[3] = { &a, &b, &c };
    ClipVtx *in, *o, *o2, *i1, *i2, t;
    float pos[3][3], tpos[3], tf;
    long double d, s, sx, sy;
    unsigned int any, all;
    int k, n;

    gClipCount++;
    any = a.code | b.code | c.code;
    if (a.code & b.code & c.code) {
        gClipCount--;
        return;
    }

    if (any & CLIP_NEAR) {
        n = ((a.code & CLIP_NEAR) + (b.code & CLIP_NEAR) + (c.code & CLIP_NEAR)) >> 4;
        for (k = 0; k < 3; k++)
            view_pos(v[k], pos[k]);
        if (n == 2) {
            pick(v, CLIP_NEAR, 1, &in, &o, &o2);
            s = near_t(in->pos, o->pos);
            lerp_pos(o->pos, in->pos, s);
            lerp_attrs(o, in, s);
            s = kOne.f / o->pos[2];
            o->q = s;
            o->sx = s * gTerrainScaleX * o->pos[0] + gTerrainCenterX;
            o->sy = gTerrainScaleY * o->pos[1] * o->q + gTerrainCenterY;
            o->code = outcode(o->sx, o->sy, o->sx, o->sy);

            s = near_t(in->pos, o2->pos);
            lerp_pos(o2->pos, in->pos, s);
            lerp_attrs(o2, in, s);
            s = kOne.f / o2->pos[2];
            o2->q = s;
            o2->sx = s * o2->pos[0] * gTerrainScaleX + gTerrainCenterX;
            o2->sy = o2->pos[1] * o2->q * gTerrainScaleY + gTerrainCenterY;
            o2->code = outcode(o2->sx, o2->sy, o2->sx, o2->sy);
            for (k = 0; k < 3; k++)
                premultiply(v[k]);
        } else {
            long double fog;

            pick(v, CLIP_NEAR, 0, &o, &i1, &i2);
            t = *o;
            t.pos = tpos;
            memcpy(tpos, o->pos, sizeof(tpos));
            s = near_t(i1->pos, o->pos);
            lerp_pos(o->pos, i1->pos, s);
            lerp_attrs(o, i1, s);
            s = kOne.f / o->pos[2];
            o->q = s;
            o->sx = s * o->pos[0] * gTerrainScaleX + gTerrainCenterX;
            o->sy = o->pos[1] * o->q * gTerrainScaleY + gTerrainCenterY;
            o->code = outcode(o->sx, o->sy, o->sx, o->sy);

            s = near_t(i2->pos, tpos);
            lerp_pos(tpos, i2->pos, s);
            for (k = 0; k < 5; k++)
                t.c[k] = (t.c[k] - i2->c[k]) * s + i2->c[k];
            fog = (t.c[5] - i2->c[5]) * s + i2->c[5];
            s = kOne.f / tpos[2];
            t.q = s;
            sx = s * tpos[0] * gTerrainScaleX + gTerrainCenterX;
            t.sx = sx;
            sy = t.q * tpos[1] * gTerrainScaleY + gTerrainCenterY;
            t.sy = sy;
            t.code = outcode(sx, sy, sx, t.sy);
            for (k = 0; k < 3; k++)
                premultiply(v[k]);
            for (k = 0; k < 5; k++)
                t.c[k] = t.q * t.c[k];
            t.c[5] = t.q * fog;
            TerrDraw_ClipTriangle(*i2, t, *o, out);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
        if (all) {
            gClipCount--;
            return;
        }
    }

    if (any & CLIP_LEFT) {
        n = (a.code & CLIP_LEFT) + (b.code & CLIP_LEFT) + (c.code & CLIP_LEFT);
        if (n == 2) {
            pick(v, CLIP_LEFT, 1, &in, &o, &o2);
            for (k = 0; k < 2; k++, o = o2) {
                d = o->sx - in->sx;
                tf = (gScene.minX - in->sx) / d;
                o->sx = d * tf + in->sx;
                o->sy = (o->sy - in->sy) * tf + in->sy;
                lerp_rest(o, in, tf);
                o->code = OUT_B(o->sy) << 3 | OUT_R(o->sx) << 2 | OUT_T(o->sy) << 1;
            }
        } else {
            pick(v, CLIP_LEFT, 0, &o, &i1, &i2);
            d = o->sx - i1->sx;
            tf = (gScene.minX - i1->sx) / d;
            t = *o;
            o->sx = d * tf + i1->sx;
            o->sy = (o->sy - i1->sy) * tf + i1->sy;
            lerp_rest(o, i1, tf);
            o->code = OUT_B(o->sy) << 3 | OUT_R(o->sx) << 2 | OUT_T(o->sy) << 1;

            d = t.sx - i2->sx;
            tf = (gScene.minX - i2->sx) / d;
            sx = d * tf + i2->sx;
            t.sx = sx;
            sy = (t.sy - i2->sy) * tf + i2->sy;
            t.sy = sy;
            lerp_rest(&t, i2, tf);
            t.code = OUT_B(t.sy) << 3 | OUT_R(sx) << 2 | OUT_T(sy) << 1;
            TerrDraw_ClipTriangle(*i2, t, *o, out);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
        if (all) {
            gClipCount--;
            return;
        }
    }

    if (any & CLIP_TOP) {
        n = ((a.code & CLIP_TOP) + (b.code & CLIP_TOP) + (c.code & CLIP_TOP)) >> 1;
        if (n == 2) {
            pick(v, CLIP_TOP, 1, &in, &o, &o2);
            for (k = 0; k < 2; k++, o = o2) {
                s = (gScene.minY - in->sy) / (o->sy - in->sy);
                lerp_all(o, in, s);
                o->code = OUT_B(o->sy) << 3 | OUT_R(o->sx) << 2;
            }
        } else {
            pick(v, CLIP_TOP, 0, &o, &i1, &i2);
            s = (gScene.minY - i1->sy) / (o->sy - i1->sy);
            t = *o;
            lerp_all(o, i1, s);
            o->code = OUT_B(o->sy) << 3 | OUT_R(o->sx) << 2;

            d = t.sy - i2->sy;
            tf = (gScene.minY - i2->sy) / d;
            t.sx = (t.sx - i2->sx) * tf + i2->sx;
            sy = d * tf + i2->sy;
            t.sy = sy;
            lerp_rest(&t, i2, tf);
            t.code = OUT_B(sy) << 3 | OUT_R(t.sx) << 2;
            TerrDraw_ClipTriangle(*i2, t, *o, out);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
        if (all) {
            gClipCount--;
            return;
        }
    }

    if (any & CLIP_RIGHT) {
        n = ((a.code & CLIP_RIGHT) + (b.code & CLIP_RIGHT) + (c.code & CLIP_RIGHT)) >> 2;
        if (n == 2) {
            pick(v, CLIP_RIGHT, 1, &in, &o, &o2);
            for (k = 0; k < 2; k++, o = o2) {
                d = o->sx - in->sx;
                tf = (gScene.maxX - in->sx) / d;
                o->sx = d * tf + in->sx;
                o->sy = (o->sy - in->sy) * tf + in->sy;
                lerp_rest(o, in, tf);
                o->code = OUT_B(o->sy) << 3;
            }
        } else {
            pick(v, CLIP_RIGHT, 0, &o, &i1, &i2);
            d = o->sx - i1->sx;
            tf = (gScene.maxX - i1->sx) / d;
            t = *o;
            o->sx = d * tf + i1->sx;
            o->sy = (o->sy - i1->sy) * tf + i1->sy;
            lerp_rest(o, i1, tf);
            o->code = OUT_B(o->sy) << 3;

            d = t.sx - i2->sx;
            tf = (gScene.maxX - i2->sx) / d;
            t.sx = d * tf + i2->sx;
            sy = (t.sy - i2->sy) * tf + i2->sy;
            t.sy = sy;
            lerp_rest(&t, i2, tf);
            t.code = OUT_B(sy) << 3;
            TerrDraw_ClipTriangle(*i2, t, *o, out);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
        if (all) {
            gClipCount--;
            return;
        }
    }

    if (any & CLIP_BOTTOM) {
        n = ((a.code & CLIP_BOTTOM) + (b.code & CLIP_BOTTOM) + (c.code & CLIP_BOTTOM)) >> 3;
        if (n == 2) {
            pick(v, CLIP_BOTTOM, 1, &in, &o, &o2);
            for (k = 0; k < 2; k++, o = o2) {
                s = (gScene.maxY - in->sy) / (o->sy - in->sy);
                lerp_all(o, in, s);
                o->code = 0;
            }
        } else {
            pick(v, CLIP_BOTTOM, 0, &o, &i1, &i2);
            s = (gScene.maxY - i1->sy) / (o->sy - i1->sy);
            t = *o;
            lerp_all(o, i1, s);
            o->code = 0;

            d = t.sy - i2->sy;
            tf = (gScene.maxY - i2->sy) / d;
            t.sx = (t.sx - i2->sx) * tf + i2->sx;
            t.sy = d * tf + i2->sy;
            lerp_rest(&t, i2, tf);
            t.code = 0;
            TerrDraw_ClipTriangle(*i2, t, *o, out);
        }
        if (a.code & b.code & c.code) {
            gClipCount--;
            return;
        }
    }

    emit(v, out);
}
HOOK(0x514760, TerrDraw_ClipTriangle);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A7E070) int gClipCount;
BSS(C311A0) float gTerrainScaleY;
BSS(C311A0) float gTerrainScaleX;
BSS(C311B0) float gTerrainCenterX;
BSS(C311B0) float gTerrainCenterY;
