#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "shadow/shadow.h"

ORIG(0x49A050, float *, SplineMesh_GetVerts, (void *obj, int model, int part, int unk3, int unk4, int *count));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4BD070, int, Ray_SkipTriangle, (void *model, uint16_t *tri, float v, float u));

/* a node of a bounding sphere tree (0x18 bytes) */
typedef struct RayNode {
    float x, y, z;
    float radius;
    uint16_t firstTri;
    uint16_t triCount;
    uint16_t child;
    uint16_t next;
} RayNode;

/* a collision mesh part (0x20 bytes) */
typedef struct RayPart {
    int triCount;
    int unk04[2];
    int nodeCount;          /* 0x0C */
    uint16_t (*tris)[4];    /* 0x10, three vertices and an info index */
    float (*verts)[4];      /* 0x14 */
    char *triInfo;          /* 0x18, 12 bytes each, material at +8 */
    RayNode *nodes;         /* 0x1C */
} RayPart;

typedef struct RayBuffer {
    RayNode *nodes;
    int count;
} RayBuffer;

/* "SplineObjSegment", the fields used here */
typedef struct RayObj {
    Task task;
    Vec3d pos;              /* 0x20 */
    char unk38[0x30];
    char *world;            /* 0x68, materials at +0xD3C */
    char **models;          /* 0x6C */
    float m[9];             /* 0x70 */
    int unk94;
    float scale;            /* 0x98 */
    char unk9C[8];
    int lod;                /* 0xA4 */
    char unkA8[0x4C];
    float radius;           /* 0xF4 */
    float length;
    float minX;             /* 0xFC */
    float maxX;             /* 0x100 */
    char unk104[0x41C];
    float inv[9];           /* 0x520 */
    char unk544[0x5C];
    RayBuffer buffers[2];   /* 0x5A0 */
    void *lastModel;
    char rigid;             /* 0x5B4 */
    char reversed;          /* 0x5B5 */
} RayObj;

_Static_assert(offsetof(RayObj, models) == 0x6C, "models");
_Static_assert(offsetof(RayObj, lod) == 0xA4, "lod");
_Static_assert(offsetof(RayObj, radius) == 0xF4, "radius");
_Static_assert(offsetof(RayObj, inv) == 0x520, "inv");
_Static_assert(offsetof(RayObj, buffers) == 0x5A0, "buffers");
_Static_assert(offsetof(RayObj, rigid) == 0x5B4, "rigid");

/* one result of the line test (0x70 bytes) */
typedef struct RayHit {
    void *obj;
    int unk04;
    Vec3d pos;              /* 0x08 */
    float local[3];         /* 0x20 */
    float normal2[3];       /* 0x2C */
    float normal[3];        /* 0x38 */
    int unk44;
    int tri;                /* 0x48 */
    int backMaterial;       /* 0x4C */
    int frontMaterial;      /* 0x50 */
    int unk54;
    int valid;              /* 0x58 */
    int unk5C;
    double dist;            /* 0x60 */
    char unk68[8];
} RayHit;

/* the argument of the line test events */
typedef struct RayQuery {
    Vec3d *from;
    Vec3d *to;
    int count;
    uint8_t event;          /* 0x0C, asked first unless 0xFF */
    char *allow;            /* 0x10 */
    int part;               /* 0x14 */
    int unk18[2];
    RayHit *hits;           /* 0x20 */
} RayQuery;

_Static_assert(offsetof(RayHit, tri) == 0x48, "RayHit");
_Static_assert(offsetof(RayHit, dist) == 0x60, "RayHit");
_Static_assert(sizeof(RayHit) == 0x70, "RayHit");
_Static_assert(offsetof(RayQuery, hits) == 0x20, "RayQuery");

typedef struct Ray {
    RayObj *o;
    RayQuery *q;
    RayPart *part;
    void *lodModel;
    char fetched;
    float *verts;           /* bent vertices, fetched once */
    RayHit *hit;
    char prevBack;
    char hasHit;
    char found;
    float a[3];             /* start, object space */
    float b[3];             /* end, object space */
    float end[3];           /* end point of the best hit */
    float bmin[3];
    float bmax[3];
    float d[3];
    double invLen2;
    double best;
    float dist;
    uint16_t *bestTri;
} Ray;

static const float kBox = 0x1.99999ap-4f;      /* 0.1 */
static const double kEps = 0.1;
static const double kZeroD = 0.0;

/* x87 "equal" also holds for unordered values */
#define FEQ(a, b) (!((a) < (b) || (a) > (b)))

static int ray_outside(const RayObj *o, const Vec3d *from, const Vec3d *to)
{
    long double lo, hi;

    lo = o->pos.x - (long double)o->radius;
    if (lo > from->x && lo > to->x)
        return 1;
    hi = (long double)o->radius + o->pos.x;
    if (!(hi >= from->x) && !(hi >= to->x))
        return 1;
    lo = o->pos.y - (long double)o->radius;
    if (lo > from->y && lo > to->y)
        return 1;
    hi = (long double)o->radius + o->pos.y;
    if (!(hi >= from->y) && !(hi >= to->y))
        return 1;
    lo = o->pos.z - (long double)o->radius;
    if (lo > from->z && lo > to->z)
        return 1;
    hi = (long double)o->radius + o->pos.z;
    if (!(hi >= from->z) && !(hi >= to->z))
        return 1;
    return 0;
}

static float box_min(float p, long double q)
{
    return (!(p >= q) ? p : q) - kBox;
}

static float box_max(float p, long double q)
{
    return (p > q ? p : q) + kBox;
}

static void ray_box(Ray *r, const float *p)
{
    int k;

    for (k = 0; k < 3; k++)
        r->bmin[k] = box_min(r->a[k], p[k]);
    for (k = 0; k < 3; k++)
        r->bmax[k] = box_max(r->a[k], p[k]);
}

/* the previous hit moved into object space, relative to the start */
static void prev_local(const Ray *r, float *lx, float *ly, float *lz)
{
    const RayObj *o = r->o;
    const RayHit *h = r->hit;
    long double A = h->pos.x - o->pos.x;
    long double B = h->pos.y - o->pos.y;
    long double C = h->pos.z - o->pos.z;

    *lx = (C * o->m[6] + B * o->m[3]) + A * o->m[0];
    *ly = (C * o->m[7] + B * o->m[4]) + A * o->m[1];
    *lz = (C * o->m[8] + B * o->m[5]) + A * o->m[2];
}

/* 1 when the node's sphere reaches the line and its box meets the ray's */
static int node_hit(const Ray *r, const RayNode *n, int order)
{
    long double X = (long double)n->x - r->a[0];
    long double Y = (long double)n->y - r->a[1];
    long double Z = (long double)n->z - r->a[2];
    double Xd = X, Yd = Y, Zd = Z;
    long double dot, len2;
    float rad = n->radius;

    if (order)
        dot = (Y * r->d[1] + X * r->d[0]) + Z * r->d[2];
    else
        dot = (Z * r->d[2] + Y * r->d[1]) + X * r->d[0];
    len2 = ((long double)Xd * Xd + (long double)Yd * Yd) + (long double)Zd * Zd;
    len2 = len2 - dot * dot * r->invLen2;
    if (!((long double)rad * rad >= len2))
        return 0;
    if (!((long double)n->radius + n->x >= r->bmin[0]))
        return 0;
    if (!((long double)n->y + n->radius >= r->bmin[1]))
        return 0;
    if (!((long double)n->z + n->radius >= r->bmin[2]))
        return 0;
    if ((long double)n->x - n->radius > r->bmax[0])
        return 0;
    if ((long double)n->y - n->radius > r->bmax[1])
        return 0;
    if ((long double)n->z - n->radius > r->bmax[2])
        return 0;
    return 1;
}

static int tri_outside(const Ray *r, const float *v0, const float *v1, const float *v2)
{
    int k;

    for (k = 0; k < 3; k++)
        if (r->bmin[k] > v0[k] && r->bmin[k] > v1[k] && r->bmin[k] > v2[k])
            return 1;
    for (k = 0; k < 3; k++)
        if (!(r->bmax[k] >= v0[k]) && !(r->bmax[k] >= v1[k]) && !(r->bmax[k] >= v2[k]))
            return 1;
    return 0;
}

/* The hit point inside the triangle: solves p - v0 = u e2 + v e1 on the two
   axes the normal leans on least. Returns 0 when outside. */
static int tri_inside(const float *v0, const float *P, const float *e2, float e1x, float e1y, float e1z,
                      float nx, float ny, float nz, float n0, float n1, float n2, double *u, long double *v)
{
    float ax = !(n0 >= kZero) ? -n0 : n0;
    float ay = !(n1 >= kZero) ? -n1 : n1;
    float az = !(n2 >= kZero) ? -n2 : n2;
    float s1, s2, s3, e0, e1;

    if (ax > ay && ax > az) {
        if (FEQ(nx, kZeroD))
            return 0;
        s1 = P[2] - v0[2];
        s2 = P[1] - v0[1];
        *u = ((long double)e2[1] * s1 - (long double)e2[2] * s2) / nx;
        if (!(*u >= kZeroD) || *u > kOneD)
            return 0;
        e0 = !(e2[1] >= kZero) ? -e2[1] : e2[1];
        e1 = !(e2[2] >= kZero) ? -e2[2] : e2[2];
        if (e0 > e1) {
            *v = ((long double)s2 - e1y * *u) / e2[1];
            return 1;
        }
    } else if (ay > az) {
        if (FEQ(ny, kZeroD))
            return 0;
        s1 = P[2] - v0[2];
        s3 = P[0] - v0[0];
        *u = ((long double)s1 * e2[0] - (long double)e2[2] * s3) / ny;
        if (!(*u >= kZeroD) || *u > kOneD)
            return 0;
        e0 = !(e2[0] >= kZero) ? -e2[0] : e2[0];
        e1 = !(e2[2] >= kZero) ? -e2[2] : e2[2];
        if (e0 > e1) {
            *v = ((long double)s3 - e1x * *u) / e2[0];
            return 1;
        }
    } else {
        if (FEQ(nz, kZeroD))
            return 0;
        s2 = P[1] - v0[1];
        s3 = P[0] - v0[0];
        *u = ((long double)s2 * e2[0] - (long double)e2[1] * s3) / nz;
        if (!(*u >= kZeroD) || *u > kOneD)
            return 0;
        e0 = !(e2[0] >= kZero) ? -e2[0] : e2[0];
        e1 = !(e2[1] >= kZero) ? -e2[1] : e2[1];
        if (e0 > e1) {
            *v = ((long double)s3 - e1x * *u) / e2[0];
            return 1;
        }
        if (FEQ(e2[1], kZero))
            return 0;
        *v = ((long double)s2 - e1y * *u) / e2[1];
        return 1;
    }
    if (FEQ(e2[2], kZero))
        return 0;
    *v = ((long double)s1 - e1z * *u) / e2[2];
    return 1;
}

/* keeps the hit when it is the nearest so far; within 0.1 of the best one a
   back face wins over a front face */
static void tri_accept(Ray *r, uint16_t *tri, float t, double denom, const float *P)
{
    double dist = (long double)t * r->dist;
    char back = !(denom >= kZeroD);

    if (r->hasHit && dist + kEps >= r->best) {
        if (!(dist >= r->best)) {
            if (!back && r->prevBack)
                return;
        } else {
            if (dist - kEps >= r->best)
                return;
            if (!back || r->prevBack)
                return;
        }
    }
    r->best = dist;
    r->end[0] = P[0];
    r->end[1] = P[1];
    r->end[2] = P[2];
    r->prevBack = back;
    r->found = 1;
    r->hasHit = 1;
    r->bestTri = tri;
    ray_box(r, P);
}

static void tri_test(Ray *r, uint16_t *tri, int rigid)
{
    const float *v0, *v1, *v2;
    float e2[3], e1x, e1y, e1z, nx, nyp, nz, N[3], t, P[3];
    long double e1zl, den, tt, v;
    double denom, u;

    if (rigid) {
        v0 = r->part->verts[tri[0]];
        v1 = r->part->verts[tri[1]];
        v2 = r->part->verts[tri[2]];
    } else {
        v0 = r->verts + tri[0] * 3;
        v1 = r->verts + tri[1] * 3;
        v2 = r->verts + tri[2] * 3;
    }
    if (tri_outside(r, v0, v1, v2))
        return;
    e2[0] = v2[0] - v0[0];
    e2[1] = v2[1] - v0[1];
    e2[2] = v2[2] - v0[2];
    e1x = v1[0] - v0[0];
    e1y = v1[1] - v0[1];
    e1zl = (long double)v1[2] - v0[2];
    e1z = e1zl;
    nx = e1zl * e2[1] - (long double)e1y * e2[2];
    nyp = (long double)e1z * e2[0] - (long double)e1x * e2[2];
    nz = (long double)e1y * e2[0] - (long double)e1x * e2[1];
    N[0] = nx;
    N[1] = -(long double)nyp;
    N[2] = nz;
    if (!rigid && r->o->reversed) {
        N[0] = -N[0];
        N[1] = -N[1];
        N[2] = -N[2];
    }
    den = ((long double)N[2] * r->d[2] + (long double)N[1] * r->d[1]) + (long double)N[0] * r->d[0];
    denom = den;
    if (FEQ(den, kZeroD))
        return;
    if (rigid)
        tt = ((((long double)v0[0] - r->a[0]) * N[0] + ((long double)v0[2] - r->a[2]) * N[2]) +
              ((long double)v0[1] - r->a[1]) * N[1]) / denom;
    else
        tt = ((((long double)v0[2] - r->a[2]) * N[2] + ((long double)v0[1] - r->a[1]) * N[1]) +
              ((long double)v0[0] - r->a[0]) * N[0]) / denom;
    t = tt;
    if (!(tt >= kZero) || t > kOne)
        return;
    P[0] = (long double)t * r->d[0] + r->a[0];
    P[1] = (long double)t * r->d[1] + r->a[1];
    P[2] = (long double)t * r->d[2] + r->a[2];
    if (!tri_inside(v0, P, e2, e1x, e1y, e1z, nx, nyp, nz, N[0], N[1], N[2], &u, &v))
        return;
    if (!(v >= kZeroD) || v + u > kOneD)
        return;
    if (Ray_SkipTriangle(r->lodModel, tri, v, u))
        return;
    tri_accept(r, tri, t, denom, P);
}

static void node_tris(Ray *r, const RayNode *n, int rigid)
{
    uint16_t *tri = r->part->tris[n->firstTri];
    int i;

    for (i = n->firstTri; i < n->firstTri + n->triCount; i++, tri += 4)
        tri_test(r, tri, rigid);
}

/* walks the sphere tree depth first; the triangles of a node are tested
   when its sphere is hit, or when it is the child that missed */
static void ray_walk(Ray *r, RayNode *nodes, int rigid)
{
    RayNode *stack[101];
    RayNode *n = nodes;
    int depth = 0, start = 1;

    stack[0] = nodes;
    for (;;) {
        if (!start) {
            if (!n->next) {
                if (--depth < 0)
                    return;
                n = stack[depth];
                continue;
            }
            n = &nodes[n->next];
            stack[depth] = n;
            if (!node_hit(r, n, rigid))
                continue;
        }
        start = 0;
        while (n->child) {
            n = &nodes[n->child];
            stack[++depth] = n;
            if (!node_hit(r, n, 0))
                break;
        }
        if (n->triCount)
            node_tris(r, n, rigid);
    }
}

static int ray_material(const Ray *r)
{
    int index = *(uint16_t *)(r->part->triInfo + r->bestTri[3] * 12 + 8);

    if (index == 0xFFFF)
        return *(int *)(r->o->world + 0xD40);
    return *(int *)(r->o->world + 0xD3C + index * 4);
}

static void hit_finish(Ray *r, RayHit *h, int material)
{
    if (r->o->reversed) {
        h->normal[0] = -h->normal[0];
        h->normal[1] = -h->normal[1];
        h->normal[2] = -h->normal[2];
    }
    h->normal2[0] = h->normal[0];
    h->normal2[1] = h->normal[1];
    h->normal2[2] = h->normal[2];
    h->obj = r->o;
    h->backMaterial = r->prevBack ? material : 0;
    h->frontMaterial = r->prevBack ? 0 : material;
    h->unk54 = 1;
    h->valid = 1;
    h->dist = r->best;
    h->tri = ((char *)r->bestTri - (char *)r->part->tris) >> 3;
}

static void bent_ray(Ray *r, Vec3d *from, Vec3d *to)
{
    RayObj *o = r->o;
    RayHit *h = r->hit;
    long double dx, dy, dz;

    r->a[0] = from->x - o->pos.x;
    r->a[1] = from->y - o->pos.y;
    r->a[2] = from->z - o->pos.z;
    r->b[0] = to->x - o->pos.x;
    r->b[1] = to->y - o->pos.y;
    r->b[2] = to->z - o->pos.z;
    r->dist = Vec3d_Distance(from, to);
    if (r->hasHit) {
        float lx, ly, lz;
        long double xs, ys, ex, ez;
        double ey;

        prev_local(r, &lx, &ly, &lz);
        xs = lx / (long double)o->scale;
        ys = ly / (long double)o->scale;
        lz = lz / o->scale;
        ey = ys - r->a[1];
        ex = xs - r->a[0];
        ez = (long double)lz - r->a[2];
        r->best = __builtin_sqrtl((ex * ex + (long double)ey * ey) + ez * ez);
    }
    ray_box(r, r->b);
    r->end[0] = r->b[0];
    r->end[1] = r->b[1];
    r->end[2] = r->b[2];
    dy = (long double)r->b[1] - r->a[1];
    dx = (long double)r->b[0] - r->a[0];
    dz = (long double)r->b[2] - r->a[2];
    r->invLen2 = kOneD / ((dz * dz + dx * dx) + dy * dy);
    if (!r->fetched) {
        r->verts = SplineMesh_GetVerts(o, o->lod, r->q->part, 0, 0, NULL);
        r->fetched = 1;
    }
    r->d[0] = to->x - from->x;
    r->d[1] = to->y - from->y;
    r->d[2] = to->z - from->z;
    if (!r->part->nodeCount)
        return;
    ray_walk(r, o->buffers[r->q->part].nodes, 0);
    if (!r->found)
        return;
    if (h->valid && !(r->best < h->dist))
        return;
    {
        const float *v0 = r->verts + r->bestTri[0] * 3;
        const float *v1 = r->verts + r->bestTri[1] * 3;
        const float *v2 = r->verts + r->bestTri[2] * 3;
        int material = ray_material(r);
        long double E2x = (long double)v2[0] - v0[0];
        long double E2y = (long double)v2[1] - v0[1];
        long double E2z = (long double)v2[2] - v0[2];
        float E1x = v1[0] - v0[0], E1y = v1[1] - v0[1];
        long double E1z = (long double)v1[2] - v0[2];

        h->normal[0] = E1z * E2y - (long double)E1y * E2z;
        h->normal[1] = -(E1z * E2x - (long double)E1x * E2z);
        h->normal[2] = (long double)E1y * E2x - (long double)E1x * E2y;
        h->pos.x = (long double)r->end[0] + o->pos.x;
        h->pos.y = (long double)r->end[1] + o->pos.y;
        h->pos.z = (long double)r->end[2] + o->pos.z;
        h->local[0] = r->end[0];
        h->local[1] = r->end[1];
        h->local[2] = r->end[2];
        hit_finish(r, h, material);
    }
}

static void rigid_ray(Ray *r, Vec3d *from, Vec3d *to)
{
    RayObj *o = r->o;
    RayHit *h = r->hit;
    const float *i = o->inv;
    long double X, Y, Z, mid, ez, dx, dz;
    double dyd;
    float ax, bx;

    X = from->x - o->pos.x;
    Y = from->y - o->pos.y;
    Z = from->z - o->pos.z;
    ax = (X * i[0] + Z * i[2]) + Y * i[1];
    r->a[1] = (X * i[3] + Z * i[5]) + Y * i[4];
    r->a[2] = (Y * i[7] + X * i[6]) + Z * i[8];
    mid = ((long double)o->minX + o->maxX) * kHalf;
    r->a[0] = mid + ax;
    X = to->x - o->pos.x;
    Y = to->y - o->pos.y;
    Z = to->z - o->pos.z;
    bx = (X * i[0] + Z * i[2]) + Y * i[1];
    r->b[1] = (X * i[3] + Z * i[5]) + Y * i[4];
    r->b[2] = (X * i[6] + Z * i[8]) + Y * i[7];
    r->b[0] = mid + bx;
    r->dist = Vec3d_Distance(from, to);
    r->end[0] = r->b[0];
    r->end[1] = r->b[1];
    r->end[2] = r->b[2];
    ez = r->b[2];
    if (r->hasHit) {
        float lx, ly, lz;
        long double ex, ey, ex2;

        prev_local(r, &lx, &ly, &lz);
        r->end[0] = lx / o->scale;
        r->end[1] = ly / o->scale;
        ez = lz / (long double)o->scale;
        ey = (long double)r->end[1] - r->a[1];
        ex = (long double)r->end[0] - r->a[0];
        ex2 = ez - r->a[2];
        r->best = __builtin_sqrtl((ex * ex + ey * ey) + ex2 * ex2);
    }
    r->bmin[0] = box_min(r->a[0], r->end[0]);
    r->bmin[1] = box_min(r->a[1], r->end[1]);
    r->bmin[2] = box_min(r->a[2], ez);
    r->bmax[0] = box_max(r->a[0], r->end[0]);
    r->bmax[1] = box_max(r->a[1], r->end[1]);
    r->bmax[2] = box_max(r->a[2], ez);
    r->end[0] = r->b[0];
    r->end[1] = r->b[1];
    r->end[2] = r->b[2];
    {
        long double dy = (long double)r->b[1] - r->a[1];

        dyd = dy;
        dx = (long double)r->b[0] - r->a[0];
        dz = (long double)r->b[2] - r->a[2];
        r->invLen2 = kOneD / ((dz * dz + dx * dx) + (long double)dyd * dyd);
        r->d[0] = dx;
        r->d[1] = dy;
        r->d[2] = dz;
    }
    if (!r->part->nodeCount)
        return;
    ray_walk(r, r->part->nodes, 1);
    if (!r->found)
        return;
    {
        const float *m = o->m;
        long double px = r->end[0] - ((long double)o->minX + o->maxX) * kHalf;
        float wx = ((long double)r->end[2] * m[2] + (long double)r->end[1] * m[1]) + px * m[0];
        float wy = ((long double)r->end[2] * m[5] + (long double)r->end[1] * m[4]) + px * m[3];
        long double wz = ((long double)r->end[2] * m[8] + (long double)r->end[1] * m[7]) + px * m[6];
        Vec3d w;

        w.x = wx + o->pos.x;
        w.y = wy + o->pos.y;
        w.z = wz + o->pos.z;
        r->best = Vec3d_Distance(from, &w);
        if (h->valid && !(r->best < h->dist))
            return;
        {
            const float *v0 = r->part->verts[r->bestTri[0]];
            const float *v1 = r->part->verts[r->bestTri[1]];
            const float *v2 = r->part->verts[r->bestTri[2]];
            int material = ray_material(r);
            long double E2x = (long double)v2[0] - v0[0];
            long double E2y = (long double)v2[1] - v0[1];
            long double E2z = (long double)v2[2] - v0[2];
            float E1x = v1[0] - v0[0], E1y = v1[1] - v0[1], E1z = v1[2] - v0[2];
            float W2x = (E2z * m[2] + E2x * m[0]) + E2y * m[1];
            float W2y = (E2z * m[5] + E2x * m[3]) + E2y * m[4];
            long double W2z = (E2z * m[8] + E2x * m[6]) + E2y * m[7];
            float W1x = ((long double)E1z * m[2] + (long double)E1x * m[0]) + (long double)E1y * m[1];
            float W1y = ((long double)E1z * m[5] + (long double)E1x * m[3]) + (long double)E1y * m[4];
            long double W1z = ((long double)E1z * m[8] + (long double)E1x * m[6]) + (long double)E1y * m[7];

            h->normal[0] = W1z * W2y - W1y * W2z;
            h->normal[1] = -(W1z * W2x - W1x * W2z);
            h->normal[2] = (long double)W1y * W2x - (long double)W1x * W2y;
            h->local[0] = w.x - o->pos.x;
            h->local[1] = w.y - o->pos.y;
            h->local[2] = w.z - o->pos.z;
            h->pos = w;
            hit_finish(r, h, material);
        }
    }
}

/* Line test of a batch of rays against one mesh part of the bent (or
   rigid) segment; each ray keeps the nearest hit in its RayHit. Stops at
   the first ray that misses the bounding cube. */
void SplineObjSegment_Hit(RayObj *o, RayQuery *q)
{
    Ray r;
    RayHit *hit = q->hits;
    Vec3d *from = q->from, *to = q->to;
    char *info;
    int i;

    r.o = o;
    r.q = q;
    r.fetched = 0;
    r.verts = NULL;
    if (!o->models || o->lod == -1)
        return;
    r.lodModel = o->models[o->lod];
    info = *(char **)(*(char **)((char *)r.lodModel + 0xC) + 0xC);
    if (*(int *)(info + 0x28))
        return;
    r.part = (RayPart *)(*(char **)(info + 0x2C) + q->part * 0x20);
    if (!r.part || !r.part->triCount)
        return;
    if (q->event != 0xFF) {
        TaskFn h = HANDLER(q->event, o->task.type);

        if (h) {
            h(o, q->allow);
            if (!*q->allow)
                return;
        }
    }
    for (i = 0; i < q->count; i++, from++, to++, hit++) {
        r.hit = hit;
        r.prevBack = hit->valid ? hit->backMaterial != 0 : 0;
        r.hasHit = *(char *)&hit->valid;
        r.found = 0;
        if (ray_outside(o, from, to))
            return;
        if (o->rigid)
            rigid_ray(&r, from, to);
        else
            bent_ray(&r, from, to);
    }
}
HOOK(0x4E16E0, SplineObjSegment_Hit);
