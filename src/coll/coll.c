/* Collision queries against the terrain octree. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "coll/coll.h"

ORIG(0x4C5810, void, Coll_ResetMods, (void));
ORIG(0x4C5830, void, Coll_SaveMods, (void));
ORIG(0x4C5870, void, Coll_RestoreMods, (void));
ORIG(0x4C7770, void, sub_4C7770, (Terrain *t, int slot, void *query, Vec3d *lo, Vec3d *hi, unsigned int flags,
                                  int tree));
ORIG(0x4C8130, void, sub_4C8130, (Terrain *t, int slot, void *query, const Vec3d *starts, const Vec3d *ends,
                                  int count, unsigned int flags, int tree));
ORIG(0x4C9020, int, sub_4C9020, (const short *leaf, Vec3d *base, const Vec3d *p, int depth, int *side,
                                 int orient));
ORIG(0x4CC810, int, Coll_LineHit, (CollHit *out, CollCursor *c, int depth, const Vec3d *start, const Vec3d *end));
ORIG(0x4CDB20, void, Coll_LinesEx, (Terrain *t, CollHit *results, const Vec3d *starts, Vec3d *ends, int count,
                                    int unk, int unk2, unsigned int flags, int unk3));
ORIG(0x4CE090, void, Coll_SendEvent, (Terrain *t, int slot, void *query, const Vec3d *p, unsigned int mask,
                                      int tree));
ORIG(0x4CE390, void, Coll_SendEventAll, (Terrain *t, int slot, void *query, const Vec3d *p, unsigned int mask));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const Vec3d *a, const Vec3d *b));

typedef void (*TaskHandler)(CollTask *task, int arg);

/* Records added by task handlers while a query walks the object octree */
typedef struct CollBump {
    char unk0[0xC];
    int shift;
    float scale;
    int unk14;
    double x, y;
    const signed char *heights;
    int unk2C;
} CollBump;

typedef struct CollHole {
    uint8_t material;
    char unk1[0xB];
    int shift;
    float scale;
    int unk14;
    double x, y;
    const uint8_t *bits;
    int unk2C;
} CollHole;

#define gCollChildTable ((const int *)0x548060)
#define gTaskEnterHandlers ((TaskHandler *)0xA9C4E0)
#define gTaskLeaveHandlers ((TaskHandler *)0xA9CAE0)
#define gCollWalkTasks VAR(int, 0xB97D20)
#define gCollHoleList ((int *)0xB97DA0)
#define gCollHoleCount VAR(int, 0xB97DF8)
#define gCollSavedHoleCount VAR(int, 0xB97DFC)
#define gCollBumps ((CollBump *)0xBA2030)
#define gCollBumpCount VAR(int, 0xBA2058)
#define gCollHoles ((CollHole *)0xBA5080)
#define gCollLineCalls VAR(int, 0xA4438C)
#define gCollLineCount VAR(int, 0xA44390)
#define gCollLeafTests VAR(int, 0xA44398)
#define gCollTriangleTests VAR(int, 0xA4439C)
/* 900 byte TerrainMaterial records; only the low byte of the first field is read */
#define gTerrainMaterials ((const uint8_t *)0xC28FE0)

#define SAR(v, n) ((int)(v) >> ((n) & 31))

static inline int Coll_Fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* Rounds to nearest, then steps down once for negative non-integers. */
static inline int Coll_Floor(double v)
{
    int i = Coll_Fistp(v);

    if (v < 0.0 && (double)i != v)
        i--;
    return i;
}

/* bsr leaves its destination alone for 0, and the original passes 0 there */
static inline int Coll_TopBit(unsigned int v)
{
    return v ? 31 - __builtin_clz(v) : 0;
}

static inline long double Coll_Abs(long double x)
{
    return x < 0.0 ? -x : x;
}

static inline unsigned int Coll_Child(int orient, int x, int y, int z, int level)
{
    return gCollChildTable[orient * 8 + ((SAR(y, level) & 1) * 2 | (SAR(x, level) & 1))] | (SAR(z, level) & 1) << 2;
}

static inline int Coll_Has(const CollNode *n, unsigned int child)
{
    return n->mask & (uint8_t)(1u << (child & 31));
}

static void Coll_EnterTasks(CollTask *t)
{
    do {
        if (t->flags & 0x80)
            gTaskEnterHandlers[t->type](t, t->arg);
        t = t->next;
    } while (t);
}

static void Coll_LeaveTasks(CollTask *t)
{
    do {
        if (t->flags & 0x100)
            gTaskLeaveHandlers[t->type](t, t->arg);
        t = t->next;
    } while (t);
}

static void Coll_InitCursor(CollCursor *c, Terrain *t)
{
    c->terrain = t;
    c->size = 0x40000000;
    c->level = 30;
    c->node = t->nodes + 1;
    c->orient = 0;
    c->x = 0;
    c->y = 0;
    c->z = 0;
}

void Coll_GetPointMaterials(Terrain *t, void **results, const Vec3d *points, int count, char unk, int unk2,
                            unsigned int flags)
{
    CollQuery q;
    CollCursor c;
    Vec3d hi, lo;
    double hy, hz, ly, lz;
    int i, depth, top;
    unsigned int a, b, d;

    Coll_ResetMods();
    lo = points[0];
    hi = points[0];
    Coll_InitCursor(&c, t);
    if (count > 0) {
        memset(q.active, 1, count);
        for (i = 0; i < count; i++) {
            const Vec3d *p = &points[i];

            if (hi.x < p->x)
                hi.x = p->x;
            else if (lo.x > p->x)
                lo.x = p->x;
            if (hi.y < p->y)
                hi.y = p->y;
            else if (lo.y > p->y)
                lo.y = p->y;
            if (hi.z < p->z)
                hi.z = p->z;
            else if (lo.z > p->z)
                lo.z = p->z;
            results[i] = NULL;
        }
    }
    hy = hi.y + 2.0;
    hz = hi.z + 2.0;
    ly = lo.y - 2.0;
    lz = lo.z - 2.0;
    a = (Coll_Fistp((long double)hi.x + 2.0) + t->origin[0]) ^ (Coll_Fistp((long double)lo.x - 2.0) + t->origin[0]);
    b = (Coll_Fistp(hy) + t->origin[1]) ^ (Coll_Fistp(ly) + t->origin[1]);
    d = (Coll_Fistp(hz) + t->origin[2]) ^ (Coll_Fistp(lz) + t->origin[2]);
    depth = 30 - Coll_TopBit(d | b | a);
    if (flags & 0x7FFFFFFF) {
        q.results = results;
        q.unk8C = unk;
        q.count = count;
        q.points = points;
        q.unk90 = unk2;
        q.unk94 = 0;
        Coll_SendEventAll(t, 11, &q, points, flags);
    }
    if (!(flags & 0x80000000) || !t->nodes)
        return;
    if (depth > 16)
        depth = 16;
    top = Coll_Descend(t, &c, &c, points, depth);
    for (i = 0; i < count; i++) {
        void *m = results[i];

        if (q.active[i]) {
            if (!m || ((float *)m)[3] == 0.0f)
                results[i] = Coll_GetLeafMaterial(&c, &points[i], top);
        } else if (!m) {
            results[i] = t->materials[0];
        }
    }
}
HOOK(0x4C9750, Coll_GetPointMaterials);

/* Walks down from the cursor towards p for up to depth levels and returns
   how many it went down. */
int Coll_Descend(Terrain *t, CollCursor *c, CollCursor *unused, const Vec3d *p, int depth)
{
    CollNode *node = c->node;
    CollNode *nodes = c->terrain->nodes;
    ObjNode *obj = t->objects;
    int size = c->size, level = c->level, orient = c->orient;
    int x = c->x, y = c->y, z = c->z;
    int ux, uy, uz, i;

    (void)unused;
    ux = Coll_Floor(p->x) + t->origin[0];
    uy = Coll_Floor(p->y) + t->origin[1];
    uz = Coll_Floor(p->z) + t->origin[2];
    Coll_ResetMods();
    for (i = 0; i < depth; i++) {
        int bx = SAR(ux ^ 0x40000000, level) & 1;
        int by = SAR(uy ^ 0x40000000, level) & 1;
        int bz = SAR(uz ^ 0x40000000, level) & 1;
        unsigned int child = gCollChildTable[orient * 8 + (by * 2 | bx)] | bz * 4;
        unsigned int code;

        if (!Coll_Has(node, child))
            break;
        if (gCollWalkTasks && obj) {
            if (obj->flags & 0x80)
                Coll_EnterTasks(obj->tasks);
            obj = obj->child[(SAR(uz, level - 2) & 4) | (SAR(uy, level - 1) & 2) | (SAR(ux, level) & 1)];
        }
        size >>= 1;
        level--;
        x += bx ? size : -size;
        y += by ? size : -size;
        z += bz ? size : -size;
        code = node->code[child];
        if (orient < 4)
            orient = ((orient + code) & 3) + (code & 4);
        else
            orient = ((orient - code) & 3) + ((code ^ orient) & 4);
        node = nodes + node->child[child];
    }
    c->orient = orient;
    c->size = size;
    c->level = level;
    c->x = x;
    c->y = y;
    c->z = z;
    c->node = node;
    c->objects = obj;
    if (gCollHoleCount > 6)
        gCollHoleCount = 6;
    return i;
}
HOOK(0x4C9A30, Coll_Descend);

void *Coll_GetLeafMaterial(CollCursor *c, const Vec3d *p, int depth)
{
    int size = c->size, level = c->level;
    unsigned int orient = c->orient;
    CollNode *node = c->node;
    Terrain *t = c->terrain;
    CollNode *nodes = t->nodes;
    int ux, uy, uz, side;
    unsigned int child, mask;
    Vec3d base;

    ux = Coll_Floor(p->x);
    uy = Coll_Floor(p->y);
    uz = Coll_Floor(p->z);
    uy += t->origin[1];
    uz += t->origin[2];
    ux += t->origin[0];
    child = Coll_Child(orient, ux ^ 0x40000000, uy ^ 0x40000000, uz ^ 0x40000000, level);
    while (Coll_Has(node, child) && depth < 16) {
        unsigned int code = node->code[child];

        if (orient < 4)
            orient = ((code + orient) & 3) + ((code ^ orient) & 4);
        else
            orient = ((orient - (code & 3)) & 3) + ((code ^ orient) & 4);
        node = nodes + node->child[child];
        size >>= 1;
        level--;
        child = Coll_Child(orient, ux ^ 0x40000000, uy ^ 0x40000000, uz ^ 0x40000000, level);
        depth++;
    }
    t = c->terrain;
    mask = ~((0x40000000 >> ((depth - 1) & 31)) - 1);
    base.x = (int)((mask & ux) - t->origin[0] + size);
    base.y = (int)((mask & uy) - t->origin[1] + size);
    base.z = (int)((mask & uz) - t->origin[2] + size);
    sub_4C9020(node->leaf, &base, p, depth, &side, orient);
    return c->terrain->materials[side];
}
HOOK(0x4C9D70, Coll_GetLeafMaterial);

/* Vertical ray against the triangles of one leaf cell. Finds the highest
   triangle at or below p (up == 0) or the lowest at or above it. The leaf
   is: triangle count, byte offset of the vertices, two vertex counts, then
   triangles (three byte indices) and packed 6 bit vertex coordinates. */
int Coll_LeafHit(const short *leaf, const Vec3d *base, const Vec3d *p, Vec3d *hit, int shift, int *side,
                 unsigned int flags, int up, Vec3f *normal)
{
    Vec3f v[64];
    float bump_x = 0, bump_y = 0, bump_scale = 0, scale, dx, dy, zlim, best, sign;
    int bump_shift = 0, nverts, found, i, n, i0, i1, i2, k0 = 0, k1 = 0, k2 = 0;
    const signed char *heights = NULL;
    const unsigned int *tris;
    const int *verts;
    Vec3f bn;

    if (gCollBumpCount) {
        const CollBump *bump = &gCollBumps[gCollBumpCount];

        bump_x = (float)((long double)bump->x - base->x);
        bump_y = (float)((long double)bump->y - base->y);
        bump_shift = bump->shift + 3;
        bump_scale = bump->scale;
        heights = bump->heights;
    }
    gCollLeafTests++;
    sign = 1.0f;
    tris = (const unsigned int *)(leaf + 4);
    verts = (const int *)((const char *)tris + leaf[1]);
    nverts = leaf[3] + leaf[2];
    if (flags & 4)
        sign = -1.0f;
    scale = (float)((long double)(0x40000000 >> (shift & 31)) * (1.0 / 24.0));
    dx = (float)((long double)p->x - base->x);
    dy = (float)((long double)p->y - base->y);
    if (!up) {
        zlim = (float)((long double)p->z - base->z + 0.1f);
        best = -268435456.0f;
    } else {
        zlim = (float)((long double)p->z - base->z - 0.1f);
        best = 268435456.0f;
    }

    i = 0;
    do {
        int packed = verts[i];
        long double x = (long double)(((packed >> 26) & 0x3F) - 24) * scale;
        float y = (float)((long double)(((packed >> 20) & 0x3F) - 24) * scale);
        float z = (float)((long double)(((packed >> 14) & 0x3F) - 24) * scale);

        if (flags & 4)
            x = -x;
        if (flags & 1) {
            float t = y;

            if (flags & 2) {
                y = (float)-x;
                x = t;
            } else {
                y = (float)x;
                x = -(long double)t;
            }
        } else if (flags & 2) {
            x = -x;
            y = -y;
        }
        if (gCollBumpCount) {
            long double fx = (x - bump_x) * bump_scale;
            float fy = (float)((y - bump_y) * bump_scale);
            int ix = Coll_Fistp(fx);
            int iy = Coll_Fistp(fy);
            long double tx = fx - ix;
            long double ty = fy - (long double)iy;
            const signed char *r0 = heights + (iy << (bump_shift & 31)) + iy + ix;
            const signed char *r1 = heights + ((iy + 1) << (bump_shift & 31)) + iy + 1 + ix + 1;
            long double h00 = r0[0], h01 = r0[1];
            float h10 = r1[-1], h11 = r1[0];
            long double top = (h01 - h00) * tx + h00;
            long double bottom = (h11 - h10) * tx + h10;

            z = (float)((((bottom - top) * ty + top) - 64.0f) * 256.0f + z);
        }
        v[i].x = (float)x;
        v[i].y = y;
        v[i].z = z;
    } while (++i < nverts);

    found = 0;
    n = leaf[0];
    if (n <= 0)
        return 0;
    do {
        unsigned int tri = *tris++;
        const Vec3f *a, *b, *c, *lo, *mid, *hi;
        float lox, hix, ax, az, bz, f2, t, hz;
        long double f, bx, d;

        i0 = tri & 0xFF;
        i1 = (tri >> 8) & 0xFF;
        i2 = (tri >> 16) & 0xFF;
        a = &v[i0];
        b = &v[i1];
        c = &v[i2];

        lox = a->x;
        hix = a->x;
        if (b->x < lox)
            lox = b->x;
        else if (b->x > lox)
            hix = b->x;
        if (c->x < lox)
            lox = c->x;
        else if (c->x > hix)
            hix = c->x;
        if (lox - 0.01f > dx || !(hix + 0.01f >= dx))
            continue;
        lox = a->y;
        hix = a->y;
        if (b->y < a->y)
            lox = b->y;
        else if (b->y > a->y)
            hix = b->y;
        if (c->y < lox)
            lox = c->y;
        else if (c->y > hix)
            hix = c->y;
        if (lox - 0.01f > dy || !(hix + 0.01f >= dy))
            continue;

        /* sort by y */
        if (b->y < a->y) {
            if (c->y < a->y) {
                hi = a;
                if (c->y < b->y) {
                    lo = c;
                    mid = b;
                } else {
                    lo = b;
                    mid = c;
                }
            } else {
                lo = b;
                mid = a;
                hi = c;
            }
        } else if (c->y < b->y) {
            hi = b;
            if (c->y < a->y) {
                lo = c;
                mid = a;
            } else {
                lo = a;
                mid = c;
            }
        } else {
            lo = a;
            mid = b;
            hi = c;
        }

        /* the two edges crossing y = dy */
        t = (float)(dy - lo->y);
        f = t / (hi->y - lo->y);
        ax = (float)((hi->x - lo->x) * f + lo->x);
        az = (float)((hi->z - lo->z) * f + lo->z);
        if (dy > mid->y || lo->y == mid->y) {
            f2 = (float)((dy - mid->y) / (hi->y - mid->y));
            bx = (hi->x - mid->x) * f2 + mid->x;
            bz = (float)((hi->z - mid->z) * f2 + mid->z);
        } else {
            f2 = (float)(t / (mid->y - lo->y));
            bx = (mid->x - lo->x) * f2 + lo->x;
            bz = (float)((mid->z - lo->z) * f2 + lo->z);
        }
        if (!((!(ax - 0.01f > dx) && bx + 0.01f >= dx) || (ax + 0.01f >= dx && !(bx - 0.01f > dx))))
            continue;
        gCollTriangleTests++;
        d = bx - ax;
        hz = az;
        if (d != 0.0f)
            hz = (float)((dx - ax) / d * (bz - az) + az);
        if (!up) {
            if (!(hz >= best) || hz > zlim)
                continue;
        } else {
            if (hz > best || !(hz >= zlim))
                continue;
        }
        {
            long double e1x = b->x - a->x, e1y = b->y - a->y, e1z = b->z - a->z;
            float e2x = (float)(c->x - a->x), e2y = (float)(c->y - a->y);
            long double e2z = c->z - a->z;

            bn.x = (float)(e2z * e1y - e2y * e1z);
            bn.y = (float)-(e2z * e1x - e2x * e1z);
            bn.z = (float)(e2y * e1x - e2x * e1y);
            if (bn.x != 0.0f || bn.y != 0.0f || bn.z != 0.0f) {
                long double len = bn.x * bn.x + bn.y * bn.y + bn.z * bn.z;
                long double inv;

                __asm__("fsqrt" : "+t"(len));
                inv = 1.0 / len;
                bn.x = (float)(bn.x * inv);
                bn.y = (float)(bn.y * inv);
                bn.z = (float)(bn.z * inv);
            }
        }
        found = 1;
        best = hz;
        k0 = i0;
        k1 = i1;
        k2 = i2;
    } while (--n);
    if (!found)
        return 0;
    {
        const Vec3f *a = &v[k0], *b = &v[k1], *c = &v[k2];

        if (((b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y)) * sign < 0.0)
            *side = up;
        else
            *side = up ^ 1;
    }
    normal->x = (float)(bn.x * sign);
    normal->y = (float)(bn.y * sign);
    normal->z = (float)(bn.z * sign);
    hit->x = (double)(dx + base->x);
    hit->y = (double)(dy + base->y);
    hit->z = (double)(best + base->z);
    return 1;
}
HOOK(0x4CA010, Coll_LeafHit);

/* Shared body of the three DropPoints variants: vertical segments from each
   point to point + dz, against tasks (through the line query) and terrain. */
static void Coll_Drop(Terrain *t, CollHit *results, const Vec3d *points, int count, const double *dz, int unk,
                      int unk2, unsigned int flags, int unk3, int variant)
{
    Vec3d ends[20];
    CollCursor c;
    Vec3d hi, lo;
    double zlo;
    int i, depth, top;
    unsigned int a, b, d;

    Coll_ResetMods();
    Coll_InitCursor(&c, t);
    for (i = 0; i < count; i++) {
        ends[i] = points[i];
        ends[i].z = (double)((long double)ends[i].z + dz[i]);
        results[i].found = 0;
    }
    lo = points[0];
    hi = points[0];
    zlo = lo.z;
    for (i = 0; i < count; i++) {
        const Vec3d *p = &points[i];

        if (hi.x < p->x)
            hi.x = p->x;
        else if (lo.x > p->x)
            lo.x = p->x;
        if (hi.y < p->y)
            hi.y = p->y;
        else if (lo.y > p->y)
            lo.y = p->y;
        if (hi.z < p->z)
            hi.z = p->z;
        else if (zlo > p->z)
            zlo = p->z;
        if (hi.z < ends[i].z)
            hi.z = ends[i].z;
        else if (zlo > ends[i].z)
            zlo = ends[i].z;
    }
    hi.x = hi.x + 2.0;
    hi.y = hi.y + 2.0;
    hi.z = hi.z + 16387.0;
    lo.x = lo.x - 2.0;
    lo.y = lo.y - 2.0;
    /* y and z are paired up crosswise here, unlike in Coll_Lines */
    a = (Coll_Fistp(hi.x) + t->origin[0]) ^ (Coll_Fistp(lo.x) + t->origin[0]);
    b = (Coll_Fistp(hi.y) + t->origin[1]) ^ (Coll_Fistp((long double)zlo - 16387.0) + t->origin[1]);
    d = (Coll_Fistp(hi.z) + t->origin[2]) ^ (Coll_Fistp(lo.y) + t->origin[2]);
    depth = 30 - Coll_TopBit(d | b | a);
    if (depth > 16)
        depth = 16;
    top = Coll_Descend(t, &c, &c, points, depth);
    if (variant == 0)
        gCollSavedHoleCount = gCollHoleCount;
    if (flags & 0x7FFFFFFF) {
        if (variant == 2)
            Coll_LinesEx(t, results, points, ends, count, unk, unk2, flags & 0x7FFFFFFF, unk3);
        else
            Coll_Lines(t, results, points, ends, count, unk, unk2, flags & 0x7FFFFFFF);
    }
    if (!(flags & 0x80000000))
        return;
    for (i = 0; i < count; i++) {
        CollHit h;
        CollQuery q;
        CollHit *r = &results[i];
        int range = Coll_Fistp(dz[i]);

        if (range < 0)
            range = -range;
        if (!Coll_VerticalHit(&h, &c, &points[i], dz[i] > 0.0, range, top, variant != 0))
            continue;
        if (r->found && !(Coll_Abs((long double)points[i].z - h.pos.z) < Coll_Abs((long double)points[i].z - r->pos.z)))
            continue;
        q.points = &h.pos;
        q.active[0] = 1;
        q.unk8C = (char)0xFF;
        q.count = 1;
        q.results = NULL;
        q.unk90 = 0;
        q.unk94 = 0;
        Coll_SendEvent(t, 11, &q, &h.pos, 0x20000000, 4);
        if (!q.active[0])
            continue;
        r->dist = (double)Coll_Abs((long double)h.pos.z - points[i].z);
        r->unk0 = 0;
        r->pos = h.pos;
        r->found = 1;
        r->normal = h.normal;
        if (variant != 0) {
            r->material = h.material;
            r->material2 = h.material2;
        }
    }
}

void Coll_DropPoints(Terrain *t, CollHit *results, const Vec3d *points, int count, const double *dz, int unk,
                     int unk2, unsigned int flags)
{
    Coll_Drop(t, results, points, count, dz, unk, unk2, flags, 0, 0);
}
HOOK(0x4CABF0, Coll_DropPoints);

/* Finds the first terrain triangle straight below (up == 0) or above p
   within range, searching the octree depth first from the cursor. */
int Coll_VerticalHit(CollHit *out, CollCursor *c, const Vec3d *p, int up, int range, int depth, int material)
{
    unsigned int tries[32];
    int zs[32], orients[32];
    CollNode *ns[32];
    ObjNode *objs[32];
    CollNode *nodes = c->terrain->nodes, *node = c->node;
    ObjNode *obj = c->objects;
    Terrain *t;
    int size = c->size, level = c->level, lv1;
    int start = depth;
    int ix, iy, iz, ox, oy, oz, ux, uy, uz, xx, yy, zz, z, orient, side;
    unsigned int child;
    Vec3d base, hit;
    Vec3f normal;

    out->found = 0;
    ix = Coll_Floor(p->x);
    iy = Coll_Floor(p->y);
    iz = Coll_Floor(p->z);
    iz += up ? -0x4001 : 0x4001;
    t = c->terrain;
    ns[depth] = node;
    ox = t->origin[0];
    oy = t->origin[1];
    oz = t->origin[2];
    ux = ox + ix;
    uy = iy + oy;
    uz = iz + oz;
    xx = ux ^ 0x40000000;
    yy = uy ^ 0x40000000;
    zz = uz ^ 0x40000000;
    z = c->z;
    orient = (uint8_t)c->orient;
    orients[depth] = orient;
    child = Coll_Child(orient, xx, yy, zz, level);
    tries[depth] = ((child >> 2) & 1) ^ up;
    zs[depth] = z;
    objs[depth] = obj;
    Coll_SaveMods();
    if (gCollWalkTasks && obj && (obj->flags & 0x80))
        Coll_EnterTasks(obj->tasks);
    if (Coll_Has(node, child)) {
        lv1 = level - 1;
        do {
            unsigned int code;

            if (depth >= 16)
                break;
            if (gCollWalkTasks && obj) {
                obj = obj->child[(SAR(uz, lv1 - 1) & 4) | (SAR(uy, lv1) & 2) | (SAR(ux, level) & 1)];
                if (obj && (obj->flags & 0x80))
                    Coll_EnterTasks(obj->tasks);
            }
            depth++;
            size >>= 1;
            level--;
            lv1--;
            z += (child & 4) ? size : -size;
            code = node->code[child];
            if (orient < 4)
                orient = ((code ^ orient) & 4) + ((code + orient) & 3);
            else
                orient = ((orient - (code & 3)) & 3) + ((code ^ orient) & 4);
            node = nodes + node->child[child];
            child = Coll_Child(orient, xx, yy, zz, level);
            orients[depth] = orient;
            ns[depth] = node;
            zs[depth] = z;
            tries[depth] = ((child >> 2) & 1) ^ up;
            objs[depth] = obj;
        } while (Coll_Has(node, child));
    }
    lv1 = level - 1;
    for (;;) {
        if (depth == 16) {
            base.x = (int)((ux & 0xFFFF8000) - ox + size);
            base.y = (int)((uy & 0xFFFF8000) - oy + size);
            base.z = z - oz;
            if (Coll_LeafHit(node->leaf, &base, p, &hit, 16, &side, orient, up, &normal)) {
                if (Coll_Abs((long double)p->z - hit.z) > range)
                    break;
                out->normal = normal;
                out->pos = hit;
                out->found = 1;
                out->dist = (double)Coll_Abs((long double)p->z - hit.z);
                if (material) {
                    unsigned int m = Coll_GetHitMaterial(c->terrain, &out->pos);
                    int above = side ? m : 0;
                    int below = (side ^ 1) ? m : 0;

                    out->material = c->terrain->materials[above];
                    out->material2 = c->terrain->materials[below];
                }
                Coll_RestoreMods();
                return 1;
            }
        }
        if (tries[depth] && depth < 16) {
            unsigned int d = --tries[depth] ^ up;
            int bx = SAR(xx, level) & 1;
            int oi = (SAR(yy, lv1) & 2) | d << 2 | bx;
            unsigned int code;

            child = gCollChildTable[orient * 8 + (((d * 2) | (SAR(yy, level) & 1)) << 1 | bx)];
            if (!Coll_Has(node, child))
                continue;
            depth++;
            size >>= 1;
            level--;
            lv1--;
            z += d ? size : -size;
            code = node->code[child];
            if (orient < 4)
                orient = ((code ^ orient) & 4) + ((code + orient) & 3);
            else
                orient = ((orient - (code & 3)) & 3) + ((code ^ orient) & 4);
            tries[depth] = 2;
            orients[depth] = orient;
            node = nodes + node->child[child];
            zs[depth] = z;
            ns[depth] = node;
            if (gCollWalkTasks && obj) {
                if (depth == 0)
                    oi = 7 - oi;
                obj = obj->child[oi];
                if (obj && (obj->flags & 0x80))
                    Coll_EnterTasks(obj->tasks);
            }
            objs[depth] = obj;
            continue;
        }
        size <<= 1;
        level++;
        lv1++;
        depth--;
        if (depth < start)
            break;
        if (gCollWalkTasks && obj && (obj->flags & 0x100))
            Coll_LeaveTasks(obj->tasks);
        z = zs[depth];
        obj = objs[depth];
        node = ns[depth];
        orient = orients[depth];
    }
    Coll_RestoreMods();
    return 0;
}
HOOK(0x4CB050, Coll_VerticalHit);

/* Material id at a hit point: the last hole mask covering it wins, else 0. */
unsigned int Coll_GetHitMaterial(Terrain *t, const Vec3d *p)
{
    int idx = 0, i;

    if (!gCollWalkTasks) {
        Coll_ResetMods();
        Coll_SendEventAll(t, 13, NULL, p, 0x20000000);
    }
    for (i = gCollHoleCount - 1; i >= 0; i--) {
        const CollHole *h = &gCollHoles[gCollHoleList[i]];
        long double fx = ((long double)p->x - h->x) * h->scale;
        float fy = (float)(((long double)p->y - h->y) * h->scale);
        int ix = Coll_Fistp(fx);
        int iy = Coll_Fistp(fy);
        long double tx = fx - ix;
        long double ty = fy - (long double)iy;
        const uint8_t *r0 = h->bits + (iy << (h->shift & 31));
        const uint8_t *r1 = h->bits + ((iy + 1) << (h->shift & 31));
        int b00 = (r0[ix >> 3] >> (ix & 7)) & 1;
        int b01 = (r0[(ix + 1) >> 3] >> ((ix + 1) & 7)) & 1;
        int b11 = (r1[(ix + 1) >> 3] >> ((ix + 1) & 7)) & 1;
        float b10 = (r1[ix >> 3] >> (ix & 7)) & 1;
        long double top = (long double)(b01 - b00) * tx + b00;
        long double bottom = (b11 - b10) * tx + b10;

        if ((bottom - top) * ty + top >= 0.3f) {
            idx = h->material;
            break;
        }
    }
    if (!gCollWalkTasks)
        Coll_ResetMods();
    return gTerrainMaterials[idx * 900];
}
HOOK(0x4CB810, Coll_GetHitMaterial);

void Coll_DropPointsMat(Terrain *t, CollHit *results, const Vec3d *points, int count, const double *dz, int unk,
                        int unk2, unsigned int flags)
{
    Coll_Drop(t, results, points, count, dz, unk, unk2, flags, 0, 1);
}
HOOK(0x4CB9F0, Coll_DropPointsMat);

void Coll_DropPointsEx(Terrain *t, CollHit *results, const Vec3d *points, int count, const double *dz, int unk,
                       int unk2, unsigned int flags, int unk3)
{
    Coll_Drop(t, results, points, count, dz, unk, unk2, flags, unk3, 2);
}
HOOK(0x4CBE40, Coll_DropPointsEx);

/* Segments from starts[i] to ends[i], against tasks and terrain. results[i]
   keeps the hit nearest to the start. */
void Coll_Lines(Terrain *t, CollHit *results, const Vec3d *starts, const Vec3d *ends, int count, char unk,
                int unk2, unsigned int flags)
{
    CollLineQuery q;
    CollCursor c;
    Vec3d hi, lo;
    double zhi;
    int vertical = 1, i, depth, top;
    unsigned int a, b, d;

    if (!count)
        return;
    gCollLineCount += count;
    gCollLineCalls++;
    hi = starts[0];
    lo = starts[0];
    zhi = hi.z;
    for (i = 0; i < count; i++) {
        const Vec3d *s = &starts[i], *e = &ends[i];

        if (s->x != e->x || s->y != e->y)
            vertical = 0;
        if (s->x > hi.x)
            hi.x = s->x;
        else if (s->x < lo.x)
            lo.x = s->x;
        if (s->y > hi.y)
            hi.y = s->y;
        else if (s->y < lo.y)
            lo.y = s->y;
        if (zhi < s->z)
            zhi = s->z;
        else if (s->z < lo.z)
            lo.z = s->z;
        if (e->x > hi.x)
            hi.x = e->x;
        else if (e->x < lo.x)
            lo.x = e->x;
        if (e->y > hi.y)
            hi.y = e->y;
        else if (e->y < lo.y)
            lo.y = e->y;
        if (zhi < e->z)
            zhi = e->z;
        else if (e->z < lo.z)
            lo.z = e->z;
        results[i].found = 0;
        results[i].unk0 = 0;
        results[i].pos = *e;
    }
    hi.x = hi.x + 2.0;
    hi.y = hi.y + 2.0;
    hi.z = zhi + 16387.0;
    lo.x = lo.x - 2.0;
    lo.y = lo.y - 2.0;
    lo.z = lo.z - 16387.0;
    q.starts = starts;
    q.ends = ends;
    q.count = count;
    q.unkC = unk;
    q.unk10 = unk2;
    q.unk14 = 0;
    q.unk18 = 0;
    q.magic = 0x41424344;
    q.results = results;
    if (flags & 0x7FFFFFFF) {
        for (i = 0; i < 6; i++) {
            if (i != 0 && i != 1 && i != 4 && i != 5)
                continue;
            if (vertical)
                sub_4C7770(t, 10, &q, &lo, &hi, flags, i);
            else
                sub_4C8130(t, 9, &q, starts, ends, count, flags, i);
        }
    }
    if (!(flags & 0x80000000))
        return;
    Coll_ResetMods();
    Coll_InitCursor(&c, t);
    a = (Coll_Fistp(hi.x) + t->origin[0]) ^ (Coll_Fistp(lo.x) + t->origin[0]);
    b = (Coll_Fistp(hi.y) + t->origin[1]) ^ (Coll_Fistp(lo.y) + t->origin[1]);
    d = (Coll_Fistp(hi.z) + t->origin[2]) ^ (Coll_Fistp(lo.z) + t->origin[2]);
    depth = 30 - Coll_TopBit(d | b | a);
    if (depth > 16)
        depth = 16;
    top = Coll_Descend(t, &c, &c, starts, depth);
    for (i = 0; i < count; i++) {
        CollHit h;
        CollQuery q2;
        CollHit *r = &results[i];

        if (!Coll_LineHit(&h, &c, top, &starts[i], &ends[i]))
            continue;
        if (r->found) {
            double dn = (double)Vec3d_DistanceSq(&starts[i], &h.pos);

            if (!(Vec3d_DistanceSq(&starts[i], &r->pos) > dn))
                continue;
        }
        q2.unk90 = 0;
        q2.unk94 = 0;
        q2.results = NULL;
        q2.active[0] = 1;
        q2.unk8C = (char)0xFF;
        q2.count = 1;
        q2.points = &h.pos;
        Coll_SendEvent(t, 11, &q2, &h.pos, 0x20000000, 4);
        if (!q2.active[0])
            continue;
        r->dist = h.dist;
        r->pos = h.pos;
        r->material = h.material;
        r->material2 = h.material2;
        r->normal = h.normal;
        r->found = 1;
        r->unk0 = 0;
    }
}
HOOK(0x4CC2A0, Coll_Lines);
