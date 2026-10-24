#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "shadow/shadow.h"
#include "sound/soundtypes.h"

typedef struct VertList {
    float *verts;       /* 6 floats each, position first */
    int count;
} VertList;

/* fistp with the current rounding mode */
static inline long long x87_fistp(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* Bends model vertices along the spline: x picks the segment and the
   position inside it, y and z are placed with the two joints' frames. */
void SplineObj_BendVerts(SplineObjSegment *s, VertList *list)
{
    float *v = list->verts;
    float inv = kOne / (s->end - s->start);
    int i;

    if (s->rigid) {
        float mid = (s->start + s->end) * kHalf;
        const float *m = s->rot;

        for (i = 0; i < list->count; i++, v += 6) {
            long double dx = v[0] - (long double)mid;
            float y = v[1], z = v[2];
            float x2 = dx * m[0] + z * m[2] + y * m[1];
            float y2 = z * m[5] + y * m[4] + dx * m[3];
            float z2 = dx * m[6] + z * m[8] + y * m[7];

            v[0] = x2;
            v[1] = y2;
            v[2] = z2;
        }
        return;
    }
    {
        long double base = -(long double)(s->reversed ? s->end : s->start);
        long double scale = s->segments * (long double)inv;

        if (s->reversed)
            scale = -scale;
        for (i = 0; i < list->count; i++, v += 6) {
            long double f = (base + v[0]) * scale;
            int k = (int)x87_fistp(f);
            long double frac, a0, c0;
            float y, z, b1, b2, c1, c2;
            const float *p, *q, *t, *u;

            if (k == s->segments)
                k--;
            frac = f - k;
            if (frac < kZero)
                frac = kZero;
            else if (!(kOne >= frac))
                frac = kOne;
            p = s->segRot[k];
            q = s->segRot[k + 1];
            t = s->segPos[k];
            u = s->segPos[k + 1];
            y = v[1];
            z = v[2];
            a0 = p[2] * (long double)z + p[1] * (long double)y;
            b1 = p[5] * z + p[4] * y;
            b2 = p[8] * z + p[7] * y;
            a0 = a0 + t[0];
            b1 = b1 + t[1];
            b2 = b2 + t[2];
            c0 = q[2] * (long double)z + q[1] * (long double)y;
            c1 = q[5] * z + q[4] * y;
            c2 = q[8] * z + q[7] * y;
            c0 = c0 + u[0];
            c1 = c1 + u[1];
            c2 = c2 + u[2];
            v[0] = (c0 - a0) * frac + a0;
            v[1] = (c1 - (long double)b1) * frac + b1;
            v[2] = (c2 - (long double)b2) * frac + b2;
        }
    }
}
HOOK(0x4E1440, SplineObj_BendVerts);
