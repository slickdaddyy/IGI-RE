#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "litmap/litmap.h"
#include "math/mathvars.h"

extern float kHemiNear;
extern float kHemiAlmostOne;
extern double kAlmostOneD;

/* where a triangle is drawn: a buffer of ids with a depth buffer next to
   it, and the texture of ids to draw from */
typedef struct Raster {
    int *color;
    float *depth;
    int pitch;
    int *ids;
    int wmask;
    int hmask;
    int shift;
    int minX;
    int maxX;
    int affine;
} Raster;

/* the edges being walked; xl, xr and zl stay in x87 registers in the
   original, the rest is stored as doubles */
typedef struct Edges {
    long double xl, xr, zl;
    double ul, vl;
    double dxl, dxr, dzl, dul, dvl;
    double dzdx, dudx, dvdx;
} Edges;

static void draw_rows(const Raster *r, Edges *e, int row, int count)
{
    int base = row * r->pitch;

    do {
        int x0, x1;

        x0 = e->xl >= kZeroD ? round_int(e->xl + kAlmostOneD) : round_int(e->xl);
        x1 = (e->xr >= kZeroD ? round_int(e->xr) : round_int(e->xr - kAlmostOneD)) + 1;
        if (x0 < r->minX)
            x0 = r->minX;
        if (x1 > r->maxX)
            x1 = r->maxX;
        if (x0 < x1) {
            long double dx = (long double)x0 - e->xl;
            double dxd = dx;
            long double z = dx * e->dzdx + e->zl;
            long double u = dxd * e->dudx + e->ul;
            long double v = dxd * e->dvdx + e->vl;
            int i = base + x0;
            int n = x1 - x0;

            do {
                float *p = &r->depth[i];

                if (r->affine) {
                    if (!(z >= *p)) {
                        int iv = round_int(v);
                        int iu = round_int(u);

                        r->color[i] = r->ids[((iv & r->hmask) << r->shift) + (iu & r->wmask)];
                        *p = z;
                    }
                } else if (z > *p) {
                    long double w = kOneD / z;
                    int iv = round_int(w * v);
                    int iu = round_int(w * u);

                    r->color[i] = r->ids[((iv & r->hmask) << r->shift) + (iu & r->wmask)];
                    *p = z;
                }
                z = z + e->dzdx;
                u = u + e->dudx;
                v = v + e->dvdx;
                i++;
            } while (--n);
        }
        e->xl = e->xl + e->dxl;
        e->xr = e->xr + e->dxr;
        e->zl = e->zl + e->dzl;
        e->ul = e->ul + e->dul;
        e->vl = e->vl + e->dvl;
        base += r->pitch;
    } while (--count);
}

/* Draws a screen space triangle (x, y, depth) into the id buffer with a
   depth test, taking ids from a texture. Without `affine` z is 1/z and the
   texture coordinates are divided by z. */
void Hemicube_DrawTriangle(int *color, float *depth, int pitch, int *ids, int idW, int idH, int minX, int minY,
                           int maxX, int maxY, float *a, float *b, float *c, float *ta, float *tb, float *tc,
                           int affine)
{
    float *top, *bot, *mid, *ttop, *tbot, *tmid, *lv, *rv, *lt, *rt;
    float s[3], st[2];
    long double r, w, dy, sub;
    double subd, savedXr = 0;
    int y0, y1, drawn = 0;
    Raster ras;
    Edges e;

    if (a[1] > b[1] && a[1] > c[1])
        bot = a, tbot = ta;
    else if (b[1] > c[1])
        bot = b, tbot = tb;
    else
        bot = c, tbot = tc;
    if (!(a[1] >= b[1]) && !(a[1] >= c[1]))
        top = a, ttop = ta;
    else if (!(b[1] >= c[1]))
        top = b, ttop = tb;
    else
        top = c, ttop = tc;
    if (!fne(bot[1], top[1]))
        return;
    if (a != top && a != bot)
        mid = a, tmid = ta;
    else if (b != top && b != bot)
        mid = b, tmid = tb;
    else
        mid = c, tmid = tc;

    /* the point on the long edge level with the middle vertex */
    r = ((long double)mid[1] - top[1]) / ((long double)bot[1] - top[1]);
    s[1] = mid[1];
    s[0] = ((long double)bot[0] - top[0]) * r + top[0];
    s[2] = ((long double)bot[2] - top[2]) * r + top[2];
    st[0] = ((long double)tbot[0] - ttop[0]) * r + ttop[0];
    st[1] = ((long double)tbot[1] - ttop[1]) * r + ttop[1];
    if (!(mid[0] >= s[0])) {
        lv = mid, lt = tmid;
        rv = s, rt = st;
    } else if (mid[0] > s[0]) {
        lv = s, lt = st;
        rv = mid, rt = tmid;
    } else {
        return;
    }

    ras.color = color;
    ras.depth = depth;
    ras.pitch = pitch;
    ras.ids = ids;
    ras.wmask = idW - 1;
    ras.hmask = idH - 1;
    ras.shift = 31 - __builtin_clz(idW);
    ras.minX = minX;
    ras.maxX = maxX;
    ras.affine = affine;

    y0 = top[1] >= kZero ? round_int((long double)top[1] + kHemiAlmostOne) : round_int(top[1]);
    y1 = (mid[1] >= kZero ? round_int(mid[1]) : round_int((long double)mid[1] - kHemiAlmostOne)) + 1;
    if (y0 < minY)
        y0 = minY;
    if (y1 > maxY)
        y1 = maxY;
    if (y0 < y1) {
        drawn = 1;
        w = (long double)rv[0] - lv[0];
        dy = (long double)mid[1] - top[1];
        if (lv == mid)
            e.dxl = ((long double)lv[0] - top[0]) / dy;
        else
            e.dxl = ((long double)bot[0] - top[0]) / ((long double)bot[1] - top[1]);
        if (rv == mid)
            e.dxr = ((long double)rv[0] - top[0]) / dy;
        else
            e.dxr = ((long double)bot[0] - top[0]) / ((long double)bot[1] - top[1]);
        e.dzl = ((long double)lv[2] - top[2]) / dy;
        e.dul = ((long double)lt[0] - ttop[0]) / dy;
        e.dvl = ((long double)lt[1] - ttop[1]) / dy;
        e.dzdx = ((long double)rv[2] - lv[2]) / w;
        e.dudx = ((long double)rt[0] - lt[0]) / w;
        e.dvdx = ((long double)rt[1] - lt[1]) / w;
        sub = (long double)y0 - top[1];
        subd = sub;
        e.xl = sub * e.dxl + top[0];
        e.xr = subd * e.dxr + top[0];
        e.zl = subd * e.dzl + top[2];
        e.ul = subd * e.dul + ttop[0];
        e.vl = subd * e.dvl + ttop[1];
        draw_rows(&ras, &e, y0, y1 - y0);
        savedXr = e.xr;
    }

    y0 = mid[1] >= kZero ? round_int((long double)mid[1] + kHemiAlmostOne) : round_int(mid[1]);
    y1 = (bot[1] >= kZero ? round_int(bot[1]) : round_int((long double)bot[1] - kHemiAlmostOne)) + 1;
    if (y0 < minY)
        y0 = minY;
    if (y1 > maxY)
        y1 = maxY;
    if (y0 >= y1)
        return;
    w = (long double)rv[0] - lv[0];
    dy = (long double)bot[1] - mid[1];
    if (lv == mid)
        e.dxl = ((long double)bot[0] - lv[0]) / dy;
    else if (!drawn)
        e.dxl = ((long double)bot[0] - top[0]) / ((long double)bot[1] - top[1]);
    if (rv == mid)
        e.dxr = ((long double)bot[0] - rv[0]) / dy;
    else if (!drawn)
        e.dxr = ((long double)bot[0] - top[0]) / ((long double)bot[1] - top[1]);
    e.dzl = ((long double)bot[2] - lv[2]) / dy;
    e.dul = ((long double)tbot[0] - lt[0]) / dy;
    e.dvl = ((long double)tbot[1] - lt[1]) / dy;
    e.dzdx = ((long double)rv[2] - lv[2]) / w;
    e.dudx = ((long double)rt[0] - lt[0]) / w;
    e.dvdx = ((long double)rt[1] - lt[1]) / w;
    subd = (long double)y0 - mid[1];
    /* the long edge carries on from the top half */
    if (lv == mid || !drawn) {
        e.xl = subd * e.dxl + lv[0];
        e.xr = savedXr;
    }
    if (rv == mid || !drawn)
        e.xr = subd * e.dxr + rv[0];
    e.zl = subd * e.dzl + lv[2];
    e.ul = subd * e.dul + lt[0];
    e.vl = subd * e.dvl + lt[1];
    draw_rows(&ras, &e, y0, y1 - y0);
}
HOOK(0x521110, Hemicube_DrawTriangle);

/* moves v to the near plane on the edge towards `to`; the original keeps
   v.z + near unrounded unless `round` */
static void clip_vertex(float *v, float *uv, const float *to, const float *touv, int round)
{
    long double s, t;

    if (round) {
        float z = v[2] + kHemiNear;

        s = z;
    } else {
        s = (long double)v[2] + kHemiNear;
    }
    t = ((long double)kHemiNear - to[2]) / (s - to[2]);
    v[2] = kHemiNear;
    v[0] = ((long double)v[0] - to[0]) * t + to[0];
    v[1] = ((long double)v[1] - to[1]) * t + to[1];
    uv[0] = ((long double)uv[0] - touv[0]) * t + touv[0];
    uv[1] = ((long double)uv[1] - touv[1]) * t + touv[1];
}

/* camera space to screen: z becomes 1/z and the texture coordinates are
   divided by z for perspective correct drawing */
static void project(float (*v)[3], float (*uv)[2], int n, float offX, float offY, float scaleX, float scaleY)
{
    int i;

    for (i = 0; i < n; i++) {
        float w = kOne / v[i][2];

        v[i][2] = w;
        v[i][0] = w * v[i][0] * scaleX + offX;
        v[i][1] = w * v[i][1] * scaleY + offY;
        uv[i][0] = w * uv[i][0];
        uv[i][1] = w * uv[i][1];
    }
}

/* Clips a camera space triangle against the near plane (z = 4.096),
   projects it and draws the one or two triangles that are left. */
void Hemicube_ClipTriangle(int *color, float *depth, int pitch, int *ids, int idW, int idH, int minX, int minY,
                           int maxX, int maxY, float offX, float offY, float scaleX, float scaleY, float *p0,
                           float *p1, float *p2, float *t0, float *t1, float *t2, int affine)
{
    float v[4][3], uv[4][2];
    int behind[3], n, i, k, a, b;

    if (p0[2] >= kZero && p1[2] >= kZero && p2[2] >= kZero) {
        Hemicube_DrawTriangle(color, depth, pitch, ids, idW, idH, minX, minY, maxX, maxY, p0, p1, p2, t0, t1, t2,
                              affine);
        return;
    }
    for (i = 0; i < 3; i++) {
        v[0][i] = p0[i];
        v[1][i] = p1[i];
        v[2][i] = p2[i];
    }
    for (i = 0; i < 2; i++) {
        uv[0][i] = t0[i];
        uv[1][i] = t1[i];
        uv[2][i] = t2[i];
    }
    for (i = 0; i < 3; i++)
        behind[i] = !(v[i][2] > kZero);
    n = behind[0] + behind[1] + behind[2];
    if (n == 3)
        return;
    if (n == 2) {
        for (k = 0; k < 2 && behind[k]; k++)
            ;
        for (i = 0; i < 3; i++) {
            if (i != k)
                clip_vertex(v[i], uv[i], v[k], uv[k], 0);
        }
        project(v, uv, 3, offX, offY, scaleX, scaleY);
        Hemicube_DrawTriangle(color, depth, pitch, ids, idW, idH, minX, minY, maxX, maxY, v[0], v[1], v[2], uv[0],
                              uv[1], uv[2], affine);
        return;
    }
    for (k = 0; k < 2 && !behind[k]; k++)
        ;
    a = (k + 1) % 3;
    b = (k + 2) % 3;
    v[3][0] = v[k][0];
    v[3][1] = v[k][1];
    v[3][2] = v[k][2];
    uv[3][0] = uv[k][0];
    uv[3][1] = uv[k][1];
    clip_vertex(v[k], uv[k], v[a], uv[a], 0);
    clip_vertex(v[3], uv[3], v[b], uv[b], 1);
    project(v, uv, 4, offX, offY, scaleX, scaleY);
    Hemicube_DrawTriangle(color, depth, pitch, ids, idW, idH, minX, minY, maxX, maxY, v[0], v[1], v[2], uv[0], uv[1],
                          uv[2], affine);
    Hemicube_DrawTriangle(color, depth, pitch, ids, idW, idH, minX, minY, maxX, maxY, v[b], v[k], v[3], uv[b],
                          uv[k], uv[3], affine);
}
HOOK(0x521960, Hemicube_ClipTriangle);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(534BB8) float kHemiNear INIT(4.096f);
DATA(534C90) double kAlmostOneD INIT(0.9999899864196777);
DATA(534C90) float kHemiAlmostOne INIT(0.99999f);
PAD(igd, 534C90, 534C9C, 0x4) INIT({0xCD, 0x20, 0x80, 0x3F});
