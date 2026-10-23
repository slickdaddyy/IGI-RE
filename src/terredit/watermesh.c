/* WaterMesh: the wave table, per vertex height and normal, the light on the
   water and the fade with distance. The quad tree itself is in src/freecam. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "terredit/terredit.h"

typedef struct WaterMesh {
    char unk0[0x4870];
    Vec3d cam;              /* 0x4870 */
    double dirX, dirY;      /* 0x4888 wave direction */
    char unk4898[0x10];
    double projX, projY;    /* 0x48A8 */
    double waves[25][2];    /* 0x48B8 */
    char unk4A48[0xC];
    float ambient[3];       /* 0x4A54 */
    char unk4A60[0x20];
    float amp;              /* 0x4A80 */
    float wavelength;       /* 0x4A84 */
    float cell;             /* 0x4A88: wavelength / 10 */
    float phase;            /* 0x4A8C */
    float unk4A90;
    float level;            /* 0x4A94 */
    float slope;            /* 0x4A98: amp / wavelength */
    char unk4A9C[0xC];
    uint32_t time;          /* 0x4AA8 */
    uint32_t period1;       /* 0x4AAC */
    uint32_t period2;       /* 0x4AB0 */
} WaterMesh;

typedef struct WaterVertex {
    char unk0[8];
    Vec3d pos;
    char unk20[0x18];
    float normal[3];        /* 0x38 */
    char unk44[8];
    uint32_t color;         /* 0x4C */
    uint32_t color2;        /* 0x50 */
    int unk54;
    int lit;                /* 0x58 */
    int unk5C;
    double depth;           /* 0x60 */
} WaterVertex;

_Static_assert(offsetof(WaterMesh, waves) == 0x48B8, "waves");
_Static_assert(offsetof(WaterMesh, ambient) == 0x4A54, "ambient");
_Static_assert(offsetof(WaterMesh, amp) == 0x4A80, "amp");
_Static_assert(offsetof(WaterMesh, time) == 0x4AA8, "time");
_Static_assert(offsetof(WaterVertex, normal) == 0x38, "normal");
_Static_assert(offsetof(WaterVertex, depth) == 0x60, "depth");

ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));

#define gWaterPrimitive VAR(int, 0xA84578)
#define gLightData VAR(char *, 0xB041E0)
#define gLightIndex VAR(int *, 0xB041E4)
#define gLightCount VAR(int, 0xB041E8)
#define gLightStride VAR(int, 0xB041F0)
#define gWaterName ((const char *)0x54F75C)

#define kZero VAR(float, 0x5333EC)
#define kOne VAR(float, 0x5333E0)
#define kOneD VAR(double, 0x5333F8)
#define kHalf VAR(float, 0x533504)
#define kOneHalf VAR(float, 0x53353C)
#define kTenth VAR(float, 0x5333C8)
#define kTwoPiF VAR(float, 0x5334A8)
#define kTwoPi VAR(double, 0x534CC0)
#define kPeriodScale VAR(float, 0x534CC8)

/* x87 "not equal" (C3 clear): false for NaN */
static inline int fne(long double a)
{
    return a < kZero || a > kZero;
}

void WaterMesh_Draw(void *node);

/* fills the 5 x 5 wave table from the time; returns time / period2 */
uint32_t WaterMesh_UpdateWaves(WaterMesh *w)
{
    uint32_t period = w->period2, p2, t = 0;
    long double fp2, fperiod;
    int i, j, k;

    p2 = (uint32_t)x87_fistp((long double)(uint64_t)period * kPeriodScale);
    fp2 = (int)p2;
    fperiod = (int)period;
    for (i = 1; i <= 5; i++) {
        k = i;
        for (j = 0; j < 5; j++) {
            long double half = (long double)k * kHalf;
            long double q1, q2;

            t = w->time;
            q1 = (long double)(uint64_t)((t + 0xBD) % p2) / fp2;
            q2 = (long double)(uint64_t)(t % period) / fperiod;
            w->waves[(i - 1) * 5 + j][0] = x87_sin(q2 * kTwoPi + half) * w->cell;
            w->waves[(i - 1) * 5 + j][1] = x87_sin(q1 * kTwoPi + half) * w->cell;
            k += i;
        }
    }
    return t / period;
}
HOOK(0x52D9F0, WaterMesh_UpdateWaves);

/* the water surface at pos: height, depth below it, normal and colours */
void WaterMesh_GetVertex(WaterMesh *w, WaterVertex *out, const Vec3d *pos)
{
    int ix = x87_fistp(x87_abs(pos->x) / w->cell) % 5;
    int iy = x87_fistp(x87_abs(pos->y) / w->cell) % 5;
    const double *wave = w->waves[ix * 5 + iy];
    long double th, s, h, k, nz, inv;
    float as;

    th = (((long double)pos->y + wave[1]) * w->projY + ((long double)pos->x + wave[0]) * w->projX - w->phase) /
         w->wavelength * kTwoPiF;
    s = x87_sin(th);
    h = (s + kOne) * w->amp + w->level;
    memcpy(&out->pos, pos, 16);
    out->pos.z = h;
    out->depth = pos->z - h;
    as = x87_abs(s);
    k = (long double)kOne - as;
    if (x87_cos(th) > kZero) {
        out->normal[0] = x87_neg(w->dirX * k * w->slope);
        out->normal[1] = x87_neg(w->dirY * k * w->slope);
    } else {
        out->normal[0] = w->dirX * k * w->slope;
        out->normal[1] = w->dirY * k * w->slope;
    }
    nz = ((long double)as + kOne) * kHalf;
    out->normal[2] = nz;
    if (fne(out->normal[0]) || fne(out->normal[1]) || fne(nz)) {
        inv = kOneD / x87_sqrt(nz * nz + (long double)out->normal[0] * out->normal[0] +
                               (long double)out->normal[1] * out->normal[1]);
        out->normal[0] = out->normal[0] * inv;
        out->normal[1] = out->normal[1] * inv;
        out->normal[2] = nz * inv;
    }
    out->color = *(uint32_t *)((char *)gCurrentWorld + 0xD64);
    out->color2 = *(uint32_t *)((char *)gCurrentWorld + 0xD3C);
    out->lit = 1;
}
HOOK(0x52DAE0, WaterMesh_GetVertex);

/* 1 near the camera, falling to 0 between 1 and 1.5 times size away */
long double WaterMesh_GetFade(WaterMesh *w, const Vec3d *p, int size)
{
    long double dx = x87_abs((long double)w->cam.x - p->x), m, f, r;
    float dy = x87_abs((long double)w->cam.y - p->y);
    float dz = x87_abs((long double)w->cam.z - w->level);

    m = dy > dz ? dy : dz;
    if (dx > m)
        m = dx;
    f = size;
    r = (f * kOneHalf - m) / (f * kHalf);
    if (r < kZero)
        return kZero;
    if (!(r <= kOne))
        return kOne;
    return r;
}
HOOK(0x52DCB0, WaterMesh_GetFade);

void WaterMesh_Open(void)
{
    gWaterPrimitive = Primitive_Register(gWaterName);
    Primitive_SetHandler(gWaterPrimitive, (void *)0x52DD90);
}
HOOK(0x52DD70, WaterMesh_Open);

/* ambient plus every light, weighted by the up part of the normal */
void WaterMesh_Light(WaterMesh *w, float *color, const float *normal)
{
    float v[3], len, g = 0, b = 0;
    long double sr = kZero, sg = kZero, sb = kZero;
    int i, n;

    memcpy(v, normal, sizeof(v));
    v[2] = v[2] * kTenth;
    if (fne(v[0]) || fne(v[1]) || fne(v[2])) {
        len = x87_sqrt((long double)v[2] * v[2] + (long double)v[0] * v[0] + (long double)v[1] * v[1]);
        v[2] = kOneD / len * v[2];
    }
    n = gLightCount;
    if (n > 0) {
        for (i = 0; i < n; i++) {
            const float *l = (const float *)(gLightData + gLightStride * gLightIndex[i]);

            if (v[2] > kZero) {
                sr = (long double)v[2] * l[3] + sr;
                sg = (long double)v[2] * l[4] + sg;
                sb = (long double)v[2] * l[5] + sb;
            }
        }
        b = sb;
        g = sg;
    }
    color[0] = sr + w->ambient[0];
    color[1] = (long double)g + w->ambient[1];
    color[2] = (long double)b + w->ambient[2];
}
HOOK(0x52E180, WaterMesh_Light);
