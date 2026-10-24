#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "dxinit/dxinit.h"

/* a part of a mesh: vertices are x, y, z, unused */
typedef struct MeshPart {
    int unk0;
    int count;
    int unk8[3];
    float (*verts)[4];
    int unk18[2];
} MeshPart;

/* An object whose mesh is bent along a chain of segments. Its x axis runs from start to end
   and is split into equal segments, each with a position and a rotation. */
typedef struct SplineObj {
    char unk0[0x6C];
    char **models;
    float rot[9];
    char unk94[0x68];
    float start;
    float end;
    float pos[21][3];
    float segRot[23][10];
    int unk598;
    int segments;
    char unk5A0[0x14];
    char rigid;
    char reversed;
} SplineObj;

_Static_assert(offsetof(SplineObj, start) == 0xFC, "start");
_Static_assert(offsetof(SplineObj, segRot) == 0x200, "segRot");
_Static_assert(offsetof(SplineObj, rigid) == 0x5B4, "rigid");

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
float *SplineMesh_GetVerts(SplineObj *o, int model, int part, int unused3, int unused4, int *count)
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

        for (i = 0; i < p->count; i++, v++, out += 3) {
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
        for (i = 0; i < p->count; i++, v++, out += 3) {
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
            pa = o->pos[k];
            pb = o->pos[k + 1];
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
        *count = p->count;
    return out - p->count * 3;
}
HOOK(0x49A050, SplineMesh_GetVerts);

void SplineCache_Reset(void)
{
    gSplineCache[0] = NULL;
}
HOOK(0x49A340, SplineCache_Reset);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, 720558, 4) __typeof__(gSplineCache[0]) store_720558[2];
ALIAS(gSplineCache, store_720558, 0);
STORE(igb, 720558, 4) __typeof__(gSplineCacheData[0]) store_720560[4];
ALIAS(gSplineCacheData, store_720560, 0);
STORE(igb, B46D40, 4) __typeof__(gSplineVerts[0]) store_B46D40[60000];
ALIAS(gSplineVerts, store_B46D40, 0);
