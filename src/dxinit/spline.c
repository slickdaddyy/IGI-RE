#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "dxinit/dxinit.h"
#include "sound/soundtypes.h"
#include "magicobj/magicobjtypes.h"

extern float gSplineVerts[];
extern void *gSplineCache[];
extern int gSplineCacheData[];

extern const float kZero;
extern const float kOne;
extern const float kHalfF;

/* exponent and top three mantissa bits of a float >= 1 (8 * log2 x, roughly), else 0 */
unsigned Float_Log2(int bits)
{
    if (bits > 0x3F800000)
        return (unsigned)(bits - 0x3F800000) >> 20;
    return 0;
}
HOOK(0x499FF0, Float_Log2);

void SplineCache_Remove(void *obj)
{
    int i;

    for (i = 0; i < 1; i++) {
        if (obj == gSplineCache[i]) {
            gSplineCache[i] = NULL;
            gSplineCacheData[i] = 0;
            return;
        }
    }
}
HOOK(0x49A010, SplineCache_Remove);

/* Vertices of one mesh part bent along the segments (or turned by rot when rigid), in a
   static buffer. */
float *SplineMesh_GetVerts(SplineObjSegment *o, int model, int part, int unused3, int unused4, int *count)
{
    float inv = kOne / (o->end - o->start);
    char *m = *(char **)(*(char **)(o->models[model] + 0xC) + 0xC);
    MeshPart *p = (MeshPart *)(*(char **)(m + 0x2C) + part * sizeof(MeshPart));
    float (*v)[4] = p->verts;
    float *out = gSplineVerts;
    int i;

    (void)unused3;
    (void)unused4;
    if (o->rigid) {
        float mid = (o->start + o->end) * kHalfF;

        for (i = 0; i < p->vertCount; i++, v++, out += 3) {
            long double px = v[0][0] - mid;
            float py = v[0][1], pz = v[0][2];

            out[0] = px * o->rot[0] + pz * o->rot[2] + py * o->rot[1];
            out[1] = px * o->rot[3] + pz * o->rot[5] + py * o->rot[4];
            out[2] = px * o->rot[6] + pz * o->rot[8] + py * o->rot[7];
        }
    } else {
        long double base = -(long double)(o->reversed ? o->end : o->start);
        long double scale = o->segments * (long double)inv;

        if (o->reversed)
            scale = -scale;
        for (i = 0; i < p->vertCount; i++, v++, out += 3) {
            long double t = (base + v[0][0]) * scale;
            long double f, a0, b0;
            float a1, a2, b1, b2;
            int k = x87_fistp(t);
            const float *ra, *rb, *pa, *pb;

            if (k == o->segments)
                k--;
            f = t - k;
            if (kZero > f)
                f = kZero;
            else if (!(kOne >= f))
                f = kOne;
            ra = o->segRot[k];
            rb = o->segRot[k + 1];
            pa = o->segPos[k];
            pb = o->segPos[k + 1];
            a0 = ra[2] * v[0][2] + ra[1] * v[0][1];
            a1 = ra[5] * v[0][2] + ra[4] * v[0][1];
            a2 = ra[8] * v[0][2] + ra[7] * v[0][1];
            a0 += pa[0];
            a1 = a1 + pa[1];
            a2 = a2 + pa[2];
            b0 = rb[2] * v[0][2] + rb[1] * v[0][1];
            b1 = rb[5] * v[0][2] + rb[4] * v[0][1];
            b2 = rb[8] * v[0][2] + rb[7] * v[0][1];
            b0 += pb[0];
            b1 = b1 + pb[1];
            b2 = b2 + pb[2];
            out[0] = (b0 - a0) * f + a0;
            out[1] = (b1 - a1) * f + a1;
            out[2] = (b2 - a2) * f + a2;
        }
    }
    if (count)
        *count = p->vertCount;
    return out - p->vertCount * 3;
}
HOOK(0x49A050, SplineMesh_GetVerts);

void SplineCache_Reset(void)
{
    gSplineCache[0] = NULL;
}
HOOK(0x49A340, SplineCache_Reset);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, 720558, 720558, 0x8);
ALIAS(gSplineCache, pad_720558, 0);
PAD(igb, 720558, 720560, 0x10);
ALIAS(gSplineCacheData, pad_720560, 0);
PAD(igb, B46D40, B46D40, 0x3A980);
ALIAS(gSplineVerts, pad_B46D40, 0);
