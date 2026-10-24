#pragma GCC optimize("excess-precision=standard")

#include "glass/glass.h"
#include "hook.h"

ORIG(0x4012A0, void *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x49A450, void *, PolyList_New, (int world, int a, Vec3d *pos, Matrix *m, void *polys, int count, int b, int c,
                                    int d, int e, int f));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));

/* one flying piece of glass */
typedef struct Shard {
    Vec3f color0;
    Vec3f color;        /* 0x0C, gets color0 each frame */
    Vec3f center;       /* 0x18 */
    Vec3f v[3];         /* 0x24 */
    int spin;           /* 0x48, index into gShardSpin */
    Vec3f vel;          /* 0x4C */
    float unk58;
    int active;         /* 0x5C */
} Shard;

/* the drawn polygon of a shard */
typedef struct ShardPoly {
    float *v[3];
    float *color[3];    /* 0x0C */
    char unk18[0x18];
    float fade[3];      /* 0x30 */
    Vec3f normal;       /* 0x3C */
    int unk48;
    int unk4C;
} ShardPoly;

/* what PolyList_New makes from the polygons */
typedef struct ShardMesh {
    char unk0[0x6C];
    ShardPoly *polys;   /* 0x6C */
    char unk70[0x2C];
    int count;          /* 0x9C */
} ShardMesh;

/* "GlassShatter" (0x2B048), the pieces of a broken Glass for 30 frames */
typedef struct GlassShatter {
    Task task;
    char unk20[0x48];
    void **model;       /* 0x68 */
    Vec3f dir;          /* 0x6C, 10 units long, in model space */
    Vec3d pos;          /* 0x78 */
    Matrix matrix;      /* 0x90 */
    int count;          /* 0xB8 */
    int frames;         /* 0xBC */
    Shard shards[1000]; /* 0xC0 */
    ShardPoly polys[1000]; /* 0x177C0 */
    ShardMesh *mesh;    /* 0x2B040 */
    char unk2B044[4];
} GlassShatter;

/* the collision mesh of a model LOD */
typedef struct TriMesh {
    int count;
    char unk4[0xC];
    uint16_t (*tris)[4]; /* 0x10 */
    float (*verts)[4];  /* 0x14 */
} TriMesh;

/* where a bullet hit */
typedef struct HitInfo {
    char unk0[8];
    Vec3d pos;          /* 0x08 */
    char unk20[0x18];
    Vec3f normal;       /* 0x38 */
} HitInfo;

_Static_assert(sizeof(Shard) == 0x60, "Shard");
_Static_assert(sizeof(ShardPoly) == 0x50, "ShardPoly");
_Static_assert(offsetof(GlassShatter, polys) == 0x177C0, "GlassShatter");
_Static_assert(sizeof(GlassShatter) == 0x2B048, "GlassShatter");
_Static_assert(offsetof(HitInfo, normal) == 0x38, "HitInfo");

#define gShatterColor VAR(const float *, 0x57C228)
#define gShatterRandom ((void *)0x57C230)
#define gShatterTen VAR(float, 0x57C23C)
#define gShatterType VAR(uint16_t, 0x57C240)
#define gShatter244 VAR(int, 0x57C244)
#define gShatterAlpha VAR(float, 0x57C248)
#define gShatterHitA VAR(Vec3f, 0x57C250)
#define gShatterTask VAR(GlassShatter *, 0x57C25C)
#define gShatterNormal VAR(Vec3f, 0x57C260)
#define gShatterPolys VAR(ShardPoly *, 0x57C26C)
#define gShatterHitB VAR(Vec3f, 0x57C270)
#define gShatterShards VAR(Shard *, 0x57C27C)
#define gShardSpin ((Matrix *)0x57C280)

#define kTen VAR(float, 0x533480)
#define kTwenty VAR(float, 0x5335B0)
#define kForty VAR(float, 0x5338C0)
#define kSpin VAR(float, 0x5338BC)      /* 1/900 */
#define kFall VAR(float, 0x5338C4)      /* -44.6 */

#define gShardColor ((const float *)0x53C8E0) /* 0.7, 0.7, 0.7 */
#define gCrushSounds ((const char (*)[0x20])0x53C880)

void GlassShatter_Run(GlassShatter *g);
void GlassShatter_Delete(GlassShatter *g);

/* 30 random spins, the later ones faster */
void GlassShatter_Open(void)
{
    int i;

    Random_Init(gShatterRandom);
    for (i = 0; i < 30; i++) {
        float a, b, c, fi;

        a = Random_Float(gShatterRandom) * kForty - kTwenty;
        b = Random_Float(gShatterRandom) * kForty - kTwenty;
        fi = i;
        c = (Random_Float(gShatterRandom) * kForty - kTwenty) * fi * kSpin;
        Matrix_SetEuler(&gShardSpin[i], fi * a * kSpin, fi * b * kSpin, c);
    }
    gShatterType = TaskType_New("GlassShatter", sizeof(GlassShatter), Task_GetRootType(), 0);
    TaskType_SetHandler2(gShatterType, 1, GlassShatter_Run);
    TaskType_SetHandler2(gShatterType, 2, GlassShatter_Delete);
}
HOOK(0x449D30, GlassShatter_Open);

/* spins the shards, lets them fall and fades them out */
void GlassShatter_Run(GlassShatter *g)
{
    float gx = g->matrix.m[6] * kFall;
    float gy = g->matrix.m[7] * kFall;
    float gz = g->matrix.m[8] * kFall;
    int i;

    gShatterAlpha = g->frames * kFrame;
    for (i = 0; i < g->mesh->count; i++) {
        ShardPoly *p = &g->mesh->polys[i];

        p->fade[0] = gShatterAlpha;
        p->fade[1] = gShatterAlpha;
        p->fade[2] = gShatterAlpha;
    }
    for (i = 0; i < g->count; i++) {
        Shard *s = &g->shards[i];
        Vec3f *n = &g->polys[i].normal;
        const float *r;
        long double a0, a1, a2, na2;
        float b0, b1, b2, c0, c1, c2, na0, na1, nb0, nb1, nb2, nc0, nc1, nc2, n0, n1, n2, cx, cy, cz;

        if (!s->active)
            continue;
        a0 = s->v[0].x - s->center.x;
        a1 = s->v[0].y - s->center.y;
        a2 = s->v[0].z - s->center.z;
        b0 = s->v[1].x - s->center.x;
        b1 = s->v[1].y - s->center.y;
        b2 = s->v[1].z - s->center.z;
        c0 = s->v[2].x - s->center.x;
        c1 = s->v[2].y - s->center.y;
        c2 = s->v[2].z - s->center.z;
        r = gShardSpin[s->spin].m;
        na0 = (a2 * r[2] + a1 * r[1]) + a0 * r[0];
        na1 = (a0 * r[3] + a2 * r[5]) + a1 * r[4];
        na2 = (a0 * r[6] + a2 * r[8]) + a1 * r[7];
        nb0 = (b2 * r[2] + b1 * r[1]) + b0 * r[0];
        nb1 = (b0 * r[3] + b2 * r[5]) + b1 * r[4];
        nb2 = (b0 * r[6] + b2 * r[8]) + b1 * r[7];
        nc0 = (c2 * r[2] + c1 * r[1]) + c0 * r[0];
        nc1 = (c0 * r[3] + c2 * r[5]) + c1 * r[4];
        nc2 = (c0 * r[6] + c2 * r[8]) + c1 * r[7];
        n0 = (r[2] * n->z + n->x * r[0]) + n->y * r[1];
        n1 = (n->y * r[4] + n->z * r[5]) + n->x * r[3];
        n2 = (n->y * r[7] + r[6] * n->x) + n->z * r[8];
        n->x = n0;
        n->y = n1;
        n->z = n2;
        s->color = s->color0;
        cx = s->vel.x + s->center.x;
        s->center.x = cx;
        cy = s->vel.y + s->center.y;
        s->center.y = cy;
        cz = s->vel.z + s->center.z;
        s->center.z = cz;
        s->v[0].x = cx + na0;
        s->v[0].y = cy + na1;
        s->v[0].z = cz + na2;
        s->v[1].x = cx + nb0;
        s->v[1].y = cy + nb1;
        s->v[1].z = cz + nb2;
        s->v[2].x = cx + nc0;
        s->v[2].y = cy + nc1;
        s->v[2].z = cz + nc2;
        s->vel.x = gx + s->vel.x;
        s->vel.y = gy + s->vel.y;
        s->vel.z = gz + s->vel.z;
    }
    if (--g->frames == 0)
        Task_Delete(g);
}
HOOK(0x449E50, GlassShatter_Run);

void GlassShatter_Delete(GlassShatter *g)
{
    Task_Delete(g->mesh);
}
HOOK(0x44A1A0, GlassShatter_Delete);

void GlassShatter_Close(void)
{
    TaskType_Free(gShatterType);
}
HOOK(0x44A1C0, GlassShatter_Close);

/* the hit point in model space */
static void to_local(Vec3f *out, const HitInfo *hit, const Vec3d *pos, const float *m)
{
    long double z;
    float a0, a1, a2;

    out->x = hit->pos.x - pos->x;
    out->y = hit->pos.y - pos->y;
    z = hit->pos.z - pos->z;
    out->z = z;
    a0 = (z * m[6] + out->y * m[3]) + out->x * m[0];
    a1 = (out->z * m[7] + out->y * m[4]) + out->x * m[1];
    a2 = (out->x * m[2] + out->z * m[8]) + out->y * m[5];
    out->x = a0;
    out->y = a1;
    out->z = a2;
}

void GlassShatter_PlaySound(Vec3d *pos);
void GlassShatter_AddTri(float *a, float *b, float *c);

/* breaks the triangles of the model that face away from dir into shards */
void GlassShatter_Spawn(void **model, Vec3f *dir, Vec3d *pos, Matrix *matrix, void *arg)
{
    HitInfo *hit = arg;
    char *info = *(char **)(*(char **)((char *)model[0] + 0xC) + 0xC);
    GlassShatter *g = Task_New(Level_GetDynamic(), gShatterType, NULL);
    const float *m = matrix->m;
    TriMesh *mesh;
    long double s, zs;
    float xs, ys, n0, n1, n2;
    int i;

    gShatter244 = 0;
    mesh = *(TriMesh **)(info + 0x2C);
    if (!mesh->count)
        return;
    g->dir = *dir;
    s = kTen / x87_sqrt(((long double)g->dir.x * g->dir.x + (long double)g->dir.y * g->dir.y) +
                        (long double)g->dir.z * g->dir.z);
    xs = g->dir.x * s;
    g->dir.x = xs;
    ys = g->dir.y * s;
    g->dir.y = ys;
    zs = s * g->dir.z;
    g->dir.z = zs;
    n0 = (zs * m[6] + ys * m[3]) + xs * m[0];
    n1 = (zs * m[7] + ys * m[4]) + xs * m[1];
    n2 = (xs * m[2] + zs * m[8]) + ys * m[5];
    g->dir.x = n0;
    g->dir.y = n1;
    g->dir.z = n2;
    g->pos = *pos;
    g->matrix = *matrix;
    g->model = model;
    g->frames = 30;
    to_local(&gShatterHitB, hit, pos, m);
    to_local(&gShatterHitA, hit, pos, m);
    n0 = (hit->normal.z * m[6] + hit->normal.y * m[3]) + m[0] * hit->normal.x;
    n1 = (hit->normal.z * m[7] + hit->normal.y * m[4]) + hit->normal.x * m[1];
    n2 = (m[2] * hit->normal.x + hit->normal.z * m[8]) + hit->normal.y * m[5];
    gShatterNormal.y = n1;
    gShatterNormal.x = n0;
    gShatterShards = g->shards;
    gShatterTen = 10.0f;
    gShatterColor = gShardColor;
    gShatterNormal.z = n2;
    gShatterTask = g;
    gShatterPolys = g->polys;
    g->count = 0;
    for (i = 0; i < mesh->count; i++) {
        uint16_t *t = mesh->tris[i];
        float *p0 = mesh->verts[t[0]], *p1 = mesh->verts[t[1]], *p2 = mesh->verts[t[2]];
        long double u0 = p2[0] - p0[0], u1 = p2[1] - p0[1], u2 = p2[2] - p0[2];
        float w0 = p1[0] - p0[0], w1 = p1[1] - p0[1];
        long double w2 = p1[2] - p0[2], nz;
        float nx, ny;

        nx = w2 * u1 - w1 * u2;
        ny = -(w2 * u0 - w0 * u2);
        nz = u0 * w1 - w0 * u1;
        if (!((nx * dir->x + nz * dir->z) + ny * dir->y >= kZero))
            GlassShatter_AddTri(p0, p1, p2);
    }
    g->mesh = PolyList_New(Game_GetWorld(), 0, pos, matrix, g->polys, g->count, 0, 1, 0, 0, 0);
    GlassShatter_PlaySound(pos);
}
HOOK(0x44A1D0, GlassShatter_Spawn);

void GlassShatter_PlaySound(Vec3d *pos)
{
    int i = Random_Int(Game_GetRandom(), 3);
    Task *s = Sound_Play(Level_GetDynamic(), gCrushSounds[i]);

    if (s)
        Sound_SetPos(s, pos);
}
HOOK(0x44A630, GlassShatter_PlaySound);

#define kThird VAR(float, 0x5338CC)
#define kHalf VAR(float, 0x533504)
#define kThirty VAR(float, 0x5333BC)
#define kTenth VAR(float, 0x5333C8)
#define kSpeed VAR(float, 0x5333B8)     /* 4096 */
#define kHalfSpeed VAR(float, 0x533688) /* 2048 */
#define kPerFrame VAR(float, 0x5338C8)  /* 1/30 */
#define kNear VAR(float, 0x5338D8)      /* 1/5120 */
#define kSizeScale VAR(float, 0x5338D4) /* 4505.6 */
#define kSizeMin VAR(float, 0x5338D0)   /* 409.6 */
#define kOneD VAR(double, 0x5333F8)

static long double sq3(long double x, long double y, long double z)
{
    return (x * x + y * y) + z * z;
}

/* d is the hit relative to a corner, e an edge from it. Normalizes e and
   returns how far along it the hit lies; *len gets the edge length. */
static long double edge_proj(const float *d, float *e, float *len)
{
    float dlen = x87_sqrt(sq3(d[0], d[1], d[2]));
    float inv = kOne / dlen;
    long double nx = d[0] * inv, ny = d[1] * inv, inv2;
    float nz = d[2] * inv;

    *len = x87_sqrt(sq3(e[0], e[1], e[2]));
    inv2 = kOne / *len;
    e[0] = inv2 * e[0];
    e[1] = inv2 * e[1];
    e[2] = inv2 * e[2];
    return ((nz * e[2] + ny * e[1]) + nx * e[0]) * dlen;
}

/* 0 at the hit, 1 from 5120 units away */
static float nearness(float d2)
{
    float m = x87_sqrt(d2) * kNear;

    if (kZero > m)
        m = 0;
    else if (!(kOne >= m))
        m = 1.0f;
    return m;
}

/* where to cut an edge of squared length len: near the middle, shifted by
   the squared lengths of the edges before and after it, plus noise */
static long double split_at(float prev, float next, float len)
{
    long double f;
    float x;

    if (!(prev >= next)) {
        f = x87_sqrt(prev) / x87_sqrt(len);
        x = !(f >= kHalf) ? f : 0.5f;
        return Random_Float(gShatterRandom) * kTenth + x;
    }
    f = kOne - x87_sqrt(next) / x87_sqrt(len);
    x = f > kHalf ? f : 0.5f;
    return x - Random_Float(gShatterRandom) * kTenth;
}

/* cuts edge s-e at t and recurses on the two halves */
static void split(float *s, float *e, float *o, long double t)
{
    float p[3];

    p[0] = (e[0] - s[0]) * t + s[0];
    p[1] = (e[1] - s[1]) * t + s[1];
    p[2] = (e[2] - s[2]) * t + s[2];
    GlassShatter_AddTri(e, o, p);
    GlassShatter_AddTri(p, o, s);
}

static float absf(float v)
{
    return v < kZero ? -v : v;
}

/* Cuts a triangle of the glass into shards, smaller ones near the hit, and
   adds each shard to the GlassShatter being built. */
void GlassShatter_AddTri(float *a, float *b, float *c)
{
    const Vec3f *h = &gShatterHitA;
    const Vec3f *n = &gShatterNormal;
    long double da, ex, ey, ez, det, u, v, ra;
    float d[3], db[3], ab[3], bc[3], ac[3], dbs, dcs, lab, lbc, lca, rb, rc, r;
    float m3c = 0, m20 = 0, m10 = 0;
    char inside = 1;
    Shard *s;
    ShardPoly *p;

    ex = h->x - a[0];
    ey = h->y - a[1];
    ez = h->z - a[2];
    d[2] = ez;
    d[1] = ey;
    d[0] = ex;
    da = sq3(ex, ey, ez);
    ez = h->z - b[2];
    db[2] = ez;
    ey = h->y - b[1];
    db[1] = ey;
    ex = h->x - b[0];
    db[0] = ex;
    dbs = sq3(ex, ey, ez);
    dcs = sq3(h->x - c[0], h->y - c[1], h->z - c[2]);
    ez = b[2] - a[2];
    ey = b[1] - a[1];
    ab[1] = ey;
    ex = b[0] - a[0];
    ab[0] = ex;
    lab = sq3(ex, ey, ez);
    ab[2] = ez;
    {
        long double x = c[0] - b[0], y = c[1] - b[1], z = c[2] - b[2];

        bc[2] = z;
        bc[1] = y;
        bc[0] = x;
        lbc = (y * y + z * z) + x * x;
    }
    lca = sq3(a[0] - c[0], a[1] - c[1], a[2] - c[2]);
    ac[0] = c[0] - a[0];
    ac[1] = c[1] - a[1];
    ac[2] = c[2] - a[2];

    /* is the hit inside the triangle? solved in the plane the normal is
       most perpendicular to */
    if (absf(n->y) > absf(n->x) && absf(n->y) > absf(n->z)) {
        det = ab[2] * ac[0] - ab[0] * ac[2];
        if (!fne(det, kZero))
            inside = 0;
        u = (d[2] * ac[0] - d[0] * ac[2]) / det;
        if (!(u >= kZero) || u > kOne)
            inside = 0;
        if (absf(ac[0]) > absf(ac[2])) {
            v = (d[0] - u * ab[0]) / ac[0];
        } else {
            if (!fne(ac[2], kZero))
                inside = 0;
            v = (d[2] - u * ab[2]) / ac[2];
        }
    } else if (absf(n->z) > absf(n->x)) {
        det = ab[1] * ac[0] - ab[0] * ac[1];
        if (!fne(det, kZero))
            inside = 0;
        u = (d[1] * ac[0] - d[0] * ac[1]) / det;
        if (!(u >= kZero) || u > kOne)
            inside = 0;
        if (absf(ac[0]) > absf(ac[1])) {
            v = (d[0] - u * ab[0]) / ac[0];
        } else {
            if (!fne(ac[1], kZero))
                inside = 0;
            v = (d[1] - u * ab[1]) / ac[1];
        }
    } else {
        det = ab[2] * ac[1] - ab[1] * ac[2];
        if (!fne(det, kZero))
            inside = 0;
        u = (d[2] * ac[1] - d[1] * ac[2]) / det;
        if (!(u >= kZero) || u > kOne)
            inside = 0;
        if (absf(ac[1]) > absf(ac[2])) {
            v = (d[1] - u * ab[1]) / ac[1];
        } else {
            if (!fne(ac[2], kZero))
                inside = 0;
            v = (d[2] - u * ab[2]) / ac[2];
        }
    }

    if (!(v >= kZero) || v + u > kOne || !inside) {
        /* squared distances from the hit to the corners and edges */
        long double proj;
        float projf, len, mbc;

        m3c = !(dcs >= da) ? dcs : da;
        m20 = !(dbs >= da) ? dbs : da;
        mbc = !(dcs >= dbs) ? dcs : dbs;
        proj = edge_proj(d, ac, &len);
        projf = proj;
        if (proj > kZero && projf < len) {
            float px, py;

            ac[0] = projf * ac[0];
            ac[1] = projf * ac[1];
            ac[2] = projf * ac[2];
            px = ac[0] + a[0];
            py = ac[1] + a[1];
            ez = (ac[2] + a[2]) - h->z;
            ex = sq3(px - h->x, py - h->y, ez);
            if (!(m3c < ex))
                m3c = ex;
        }
        m3c = nearness(m3c);
        proj = edge_proj(d, ab, &len);
        projf = proj;
        if (proj > kZero && projf < len) {
            float px, py;

            ab[0] = projf * ab[0];
            ab[1] = projf * ab[1];
            ab[2] = projf * ab[2];
            px = ab[0] + a[0];
            py = ab[1] + a[1];
            ez = (ab[2] + a[2]) - h->z;
            ex = sq3(px - h->x, py - h->y, ez);
            if (!(m20 < ex))
                m20 = ex;
        }
        m20 = nearness(m20);
        proj = edge_proj(db, bc, &len);
        projf = proj;
        if (proj > kZero && projf < len) {
            long double x = projf * bc[0];
            float px, py, d2;

            bc[1] = projf * bc[1];
            bc[2] = projf * bc[2];
            px = x + b[0];
            py = bc[1] + b[1];
            ez = (bc[2] + b[2]) - h->z;
            d2 = sq3(px - h->x, py - h->y, ez);
            if (!(mbc < d2))
                mbc = d2;
        }
        m10 = nearness(mbc);
    }

    /* cut the longest edge while the triangle is too big for its distance */
    ra = m3c * kSizeScale + kSizeMin;
    rb = m20 * kSizeScale + kSizeMin;
    rc = m10 * kSizeScale + kSizeMin;
    if (lab > ra * ra || !(rb * rb >= lbc) || !(rc * rc >= lca)) {
        Random_Float(gShatterRandom);
        if (!(lab < lbc) && !(lab < lca)) {
            split(a, b, c, split_at(lca, lbc, lab));
            return;
        }
        if (!(lbc < lab) && !(lbc < lca)) {
            split(b, c, a, split_at(lab, lca, lbc));
            return;
        }
        if (!(lca < lab) && !(lca < lbc)) {
            split(c, a, b, split_at(lbc, lab, lca));
            return;
        }
    }

    if (gShatter244 != 0 && gShatter244 != 1)
        return;
    if (!(m3c >= (!(m20 >= m10) ? m20 : m10)))
        r = m3c;
    else
        r = !(m20 >= m10) ? m20 : m10;
    if (gShatterTask->count >= 1000)
        return;
    s = gShatterShards;
    {
        float cx, cy, cz;
        long double k;

        cx = ((a[0] + b[0]) + c[0]) * kThird;
        cy = ((b[1] + a[1]) + c[1]) * kThird;
        s->v[0] = *(Vec3f *)a;
        cz = ((b[2] + a[2]) + c[2]) * kThird;
        s->v[1] = *(Vec3f *)b;
        s->v[2] = *(Vec3f *)c;
        s->spin = round_int((Random_Float(gShatterRandom) + (kOne - r)) * kHalf * kThirty);
        s->center.x = cx;
        s->center.y = cy;
        s->center.z = cz;
        s->color0 = *(const Vec3f *)gShatterColor;
        s->color = s->color0;
        if (gShatterTen > kZero) {
            s->vel.x = cx - gShatterHitB.x;
            s->vel.y = cy - gShatterHitB.y;
            s->vel.z = cz - gShatterHitB.z;
            k = gShatterTen / x87_sqrt(sq3(s->vel.x, s->vel.y, s->vel.z));
            s->vel.x = s->vel.x * k;
            s->vel.y = s->vel.y * k;
            s->vel.z = s->vel.z * k;
        } else {
            s->vel.x = 0;
            s->vel.y = 0;
            s->vel.z = 0;
        }
    }
    s->vel.x = (Random_Float(gShatterRandom) * kSpeed - kHalfSpeed) * kPerFrame + s->vel.x;
    s->vel.y = (Random_Float(gShatterRandom) * kSpeed - kHalfSpeed) * kPerFrame + s->vel.y;
    s->vel.z = (Random_Float(gShatterRandom) * kSpeed - kHalfSpeed) * kPerFrame + s->vel.z;
    s->active = 1;
    p = gShatterPolys;
    p->v[0] = &s->v[0].x;
    p->v[1] = &s->v[1].x;
    p->v[2] = &s->v[2].x;
    p->color[0] = &s->color.x;
    p->color[1] = &s->color.x;
    p->color[2] = &s->color.x;
    p->unk48 = 0;
    p->unk4C = 0;
    p->normal.x = ab[2] * ac[1] - ab[1] * ac[2];
    p->normal.y = -(ab[2] * ac[0] - ab[0] * ac[2]);
    p->normal.z = ab[1] * ac[0] - ab[0] * ac[1];
    if (fne(p->normal.x, kZero) || fne(p->normal.y, kZero) || fne(p->normal.z, kZero)) {
        long double k = kOneD / x87_sqrt(sq3(p->normal.x, p->normal.y, p->normal.z));

        p->normal.x = p->normal.x * k;
        p->normal.y = p->normal.y * k;
        p->normal.z = p->normal.z * k;
    }
    gShatterShards = s + 1;
    gShatterPolys = p + 1;
    gShatterTask->count++;
}
HOOK(0x44A670, GlassShatter_AddTri);
