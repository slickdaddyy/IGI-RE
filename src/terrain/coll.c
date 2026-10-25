/* Point tests against the triangles of a terrain leaf cell. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "terrain/terrain.h"
#include "coll/colltypes.h"

static const double kOneOver24 = 1.0 / 24.0;

ORIG(0x4B2C70, int, Vec3_RayPlane, (Vec3f *out, const Vec3f *p, const Vec3f *d, const Vec3f *n));
ORIG(0x4B2D90, char, Vec3_TriangleCoords, (float *u, float *v, const Vec3f *p, const Vec3f *e1, const Vec3f *e2,
                                           const Vec3f *n));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3190, void, Vec3d_Cross, (Vec3d *out, const Vec3d *a, const Vec3d *b));

extern CollBump gCollBumps[];
extern int gCollBumpTop;

static inline int fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* x == 0 for the x87, where NaN compares equal */
static inline int is_zero(double x)
{
    return !(x < 0.0 || x > 0.0);
}

/* Sets *inside when p is inside the closed mesh of a leaf cell placed at
   base: a ray from p to the center of the first triangle is crossed with
   every triangle, and the nearest one hit tells which side p is on. A
   leaf is short tris, short verts offset, short counts[2], then the
   triangles (three byte indices) and the packed 6:6:6 vertices. */
int Coll_LeafPointInside(const short *leaf, const Vec3d *base, const Vec3d *p, uint8_t shift, int *inside,
                         unsigned int flags)
{
    Vec3d v[64], center, hit = {0, 0, 0};
    const unsigned int *tris = (const unsigned int *)(leaf + 4);
    const unsigned int *verts = (const unsigned int *)((const char *)tris + leaf[1]);
    const signed char *heights = NULL;
    float bump_x = 0, bump_y = 0, bump_scale = 0, sign = 1.0f;
    int bump_shift = 0, nverts = leaf[3] + leaf[2], found = 0, i, n;
    double scale, best;
    Vec3f normal = {0, 0, 0};

    if (gCollBumpTop) {
        const CollBump *b = &gCollBumps[gCollBumpTop - 1];

        bump_x = (float)b->x;
        bump_scale = b->scale;
        heights = b->heights;
        bump_y = (float)b->y;
        bump_shift = b->sizeShift + 3;
    }
    if (flags & ~3u)
        sign = -1.0f;
    scale = (long double)(0x40000000 >> (shift & 31)) * kOneOver24;
    for (i = 0; i < nverts; i++) {
        unsigned int q = verts[i];
        long double x = (long double)(int)((q >> 26) - 24) * scale;
        double y = (long double)(int)(((q >> 20) & 0x3F) - 24) * scale;
        double z = (long double)(int)(((q >> 14) & 0x3F) - 24) * scale;
        double X, Y, Z;

        if (flags & 4)
            x = -x;
        if (flags & 1) {
            double t = y;

            if (flags & 2) {
                y = -x;
                x = t;
            } else {
                y = x;
                x = -(long double)t;
            }
        } else if (flags & 2) {
            x = -x;
            y = -y;
        }
        X = x + base->x;
        Y = (long double)y + base->y;
        Z = (long double)z + base->z;
        if (gCollBumpTop) {
            long double fx = ((long double)X - bump_x) * bump_scale;
            float fy = ((long double)Y - bump_y) * bump_scale;
            int ix = fistp(fx), iy = fistp(fy);
            int r0 = (iy << (bump_shift & 31)) + iy;
            int r1 = ((iy + 1) << (bump_shift & 31)) + iy + 1 + ix + 1;
            long double tx = fx - ix;
            long double ty = fy - (long double)iy;
            long double h00 = heights[r0 + ix], h01 = heights[r0 + ix + 1], h11 = heights[r1];
            float h10 = heights[r1 - 1];
            long double top = (h01 - h00) * tx + h00;
            long double bottom = (h11 - h10) * tx + h10;

            Z = (((bottom - top) * ty + top) - 64.0) * 256.0f + Z;
        }
        v[i].x = X;
        v[i].y = Y;
        v[i].z = Z;
    }
    n = leaf[0];
    if (n <= 0) {
        *inside = 0;
        return 1;
    }
    {
        unsigned int t = tris[0];
        const Vec3d *a = &v[t & 0xFF], *b = &v[(t >> 8) & 0xFF], *c = &v[(t >> 16) & 0xFF];
        long double sx = (long double)a->x + b->x + c->x;
        double sy = (long double)a->y + b->y + c->y;
        double sz = (long double)a->z + b->z + c->z;

        center.x = sx * (1.0 / 3.0);
        center.y = sy * (1.0 / 3.0);
        center.z = sz * (1.0 / 3.0);
    }
    best = 4611686018427387904.0;
    do {
        unsigned int t = *tris++;
        const Vec3d *a = &v[t & 0xFF], *b = &v[(t >> 8) & 0xFF], *c = &v[(t >> 16) & 0xFF];
        Vec3d e1, e2, nd, H;
        Vec3f e1f, e2f, nf, rel, dir, out;
        long double hz, d2;

        e1.x = b->x - a->x;
        e1.y = b->y - a->y;
        e1.z = b->z - a->z;
        e2.x = c->x - a->x;
        e2.y = c->y - a->y;
        e2.z = c->z - a->z;
        Vec3d_Cross(&nd, &e1, &e2);
        if (is_zero(nd.x) && is_zero(nd.y) && is_zero(nd.z))
            continue;
        Vec3d_Normalize(&nd);
        e1f.x = e1.x;
        e1f.y = e1.y;
        e1f.z = e1.z;
        e2f.x = e2.x;
        e2f.y = e2.y;
        e2f.z = e2.z;
        nf.x = nd.x;
        nf.y = nd.y;
        nf.z = nd.z;
        rel.x = (long double)p->x - a->x;
        rel.y = (long double)p->y - a->y;
        rel.z = (long double)p->z - a->z;
        dir.x = (long double)center.x - p->x;
        dir.y = (long double)center.y - p->y;
        dir.z = (long double)center.z - p->z;
        if (!Vec3_RayPlane(&out, &rel, &dir, &nf))
            continue;
        if (!Vec3_TriangleCoords(NULL, NULL, &out, &e1f, &e2f, &nf))
            continue;
        H.x = ((long double)a->x - p->x) + out.x;
        H.y = ((long double)a->y - p->y) + out.y;
        hz = ((long double)a->z - p->z) + out.z;
        H.z = hz;
        d2 = hz * H.z + (long double)H.y * H.y + (long double)H.x * H.x;
        if (best > d2) {
            best = d2;
            found = 1;
            normal = nf;
            hit = H;
        }
    } while (--n);
    *inside = found && (normal.z * (long double)hit.z + normal.y * (long double)hit.y
                        + normal.x * (long double)hit.x) * sign > 0.0;
    return 1;
}
HOOK(0x4C9020, Coll_LeafPointInside);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(BA2058) int gCollBumpTop;
