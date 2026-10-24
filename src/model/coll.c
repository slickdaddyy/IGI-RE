/* Segment queries against the terrain octree. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "coll/coll.h"

static const double kOneOver24 = 1.0 / 24.0;

/* fchs on the x87 value, so gcc cannot move the negation after a rounding */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

ORIG(0x4B2C70, int, Vec3_RayPlane, (Vec3f *out, const Vec3f *p, const Vec3f *d, const Vec3f *n));
ORIG(0x4B2D90, char, Vec3_TriangleCoords, (float *u, float *v, const Vec3f *p, const Vec3f *e1, const Vec3f *e2,
                                           const Vec3f *n));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3190, void, Vec3d_Cross, (Vec3d *out, const Vec3d *a, const Vec3d *b));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const Vec3d *a, const Vec3d *b));
ORIG(0x4C5810, void, Coll_ResetMods, (void));
ORIG(0x4C5830, void, Coll_SaveMods, (void));
ORIG(0x4C5870, void, Coll_RestoreMods, (void));
ORIG(0x4C7770, void, DynCube_SendEventBox, (Terrain *t, int slot, void *query, Vec3d *lo, Vec3d *hi, unsigned int flags,
                                  int tree));
ORIG(0x4C8130, void, DynCube_SendEventLines, (Terrain *t, int slot, void *query, const Vec3d *starts, const Vec3d *ends,
                                  int count, unsigned int flags, int tree));
ORIG(0x4C9A30, int, Coll_Descend_, (Terrain *t, CollCursor *c, CollCursor *unused, const Vec3d *p, int depth));

extern const int gCollChildTable[];
extern int gCollWalkTasks;
extern int gCollBumpCount;
extern int gCollLineCalls;
extern int gCollLineCount;

typedef void (*TaskHandler)(CollTask *task, int arg);

extern TaskHandler gTaskEnterHandlers[];
extern TaskHandler gTaskLeaveHandlers[];
extern const CollBump gCollBumps[];

void Coll_SendEvent(Terrain *t, uint8_t slot, void *arg, const Vec3d *p, unsigned int mask, int tree);
int Coll_LeafLineHit(const short *leaf, const Vec3d *base, const Vec3d *s, const Vec3d *e, Vec3d *hit, int shift,
                     int *side, int *side2, unsigned int flags, Vec3f *normal);

static inline int fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* bsr leaves its destination alone for 0, and the original passes 0 there */
static inline int top_bit(unsigned int v)
{
    return v ? 31 - __builtin_clz(v) : 0;
}

static void enter_tasks(CollTask *t)
{
    do {
        if (t->flags & 0x80)
            gTaskEnterHandlers[t->type](t, t->arg);
        t = t->next;
    } while (t);
}

static void leave_tasks(CollTask *t)
{
    do {
        if (t->flags & 0x100)
            gTaskLeaveHandlers[t->type](t, t->arg);
        t = t->next;
    } while (t);
}

/* Segment s-e against the terrain below cursor c: walks the octree depth
   first with an explicit stack, pushing every child cell whose box the
   segment crosses, and tests the leaves it reaches. */
int Coll_LineHit(CollHit *out, const CollCursor *c, int depth, const Vec3d *s, const Vec3d *e)
{
    CollCursor stack[128];
    int fresh[128], lo[3], hi[3], pos[3], sel[3], order[8];
    Terrain *t = c->terrain;
    int ox, oy, oz, sp = 0, i, m1, m2;
    float dx, dy, dz, ax, ay, az;
    Vec3d hp;
    Vec3f n;

    Coll_SaveMods();
    ox = t->offset[0];
    oy = t->offset[1];
    oz = t->offset[2];
    lo[0] = fistp(!(s->x >= e->x) ? s->x : e->x) + ox;
    lo[1] = fistp(e->y > s->y ? s->y : e->y) + oy;
    lo[2] = fistp(e->z > s->z ? s->z : e->z) + oz;
    hi[0] = fistp(s->x > e->x ? s->x : e->x) + ox;
    hi[1] = fistp(!(e->y >= s->y) ? s->y : e->y) + oy;
    hi[2] = fistp(!(e->z >= s->z) ? s->z : e->z) + oz;
    dx = (float)((long double)e->x - s->x);
    dy = (float)((long double)e->y - s->y);
    dz = (float)((long double)e->z - s->z);
    ax = !(dx >= 0.0f) ? -dx : dx;
    ay = !(dy >= 0.0f) ? -dy : dy;
    az = !(dz >= 0.0f) ? -dz : dz;
    if (ax > ay && ax > az) {
        sel[0] = 0;
        sel[1] = ay > az ? 1 : 2;
        sel[2] = ay > az ? 2 : 1;
    } else if (ay > az) {
        sel[0] = 1;
        sel[1] = ax > az ? 0 : 2;
        sel[2] = ax > az ? 2 : 0;
    } else {
        sel[0] = 2;
        sel[1] = ax > ay ? 0 : 1;
        sel[2] = ax > ay ? 1 : 0;
    }
    pos[0] = dx > 0.0f;
    pos[1] = dy > 0.0f;
    pos[2] = dz > 0.0f;
    /* children in the order the segment passes them, major axis first */
    for (i = 0; i < 8; i++)
        order[i] = (((i >> 2) & 1) ^ pos[sel[2]]) << sel[2] | (((i >> 1) & 1) ^ pos[sel[1]]) << sel[1]
                   | ((i & 1) ^ pos[sel[0]]) << sel[0];
    stack[0] = *c;
    fresh[0] = 1;
    do {
        CollCursor *f = &stack[sp];
        ObjNode *o;
        CollCursor F;
        unsigned int mask;
        int k;

        if (30 - f->level >= 16) {
            Vec3d base;

            base.x = (int)(f->x - f->terrain->offset[0]);
            base.y = (int)(f->y - f->terrain->offset[1]);
            base.z = (int)(f->z - f->terrain->offset[2]);
            if (Coll_LeafLineHit(f->node->leaf, &base, s, e, &hp, 30 - f->level, &m1, &m2, f->orient, &n))
                goto found;
            sp--;
            continue;
        }
        o = f->objects;
        if (!fresh[sp]) {
            if (gCollWalkTasks && o && (o->flags & 0x100))
                leave_tasks(o->tasks);
            sp--;
            continue;
        }
        if (gCollWalkTasks && o && (o->flags & 0x80))
            enter_tasks(o->tasks);
        fresh[sp] = 0;
        F = *f;
        mask = F.node->mask;
        for (k = 0; k < 8; k++) {
            int idx = order[k];
            unsigned int child = gCollChildTable[idx + F.orient * 8];
            CollNode *nn;
            int half, cx, cy, cz, j;
            float hf, hz;
            double wx, wy, wz, minx, miny, minz, maxx, maxy, maxz;

            if (!(mask & (1u << (child & 31))))
                continue;
            nn = F.terrain->ctr + F.node->child[child];
            half = (unsigned int)F.size >> 1;
            cx = ((idx & 1) * 2 - 1) * half + F.x;
            cy = ((idx & 2) - 1) * half + F.y;
            cz = (((idx >> 1) & 2) - 1) * half + F.z;
            if (lo[0] > cx + half || hi[0] < cx - half || lo[1] > cy + half || hi[1] < cy - half
                || lo[2] > cz + half + 0x4001 || hi[2] < cz - half - 0x4001)
                continue;
            hf = (float)half;
            hz = (float)((long double)hf + 16385.0f);
            wx = cx - t->offset[0];
            wy = cy - t->offset[1];
            wz = cz - t->offset[2];
            minx = (double)(wx - (long double)half);
            miny = (double)(wy - (long double)hf);
            minz = (double)(wz - (long double)hz);
            maxx = (double)((long double)half + wx);
            maxy = (double)((long double)hf + wy);
            maxz = (double)((long double)hz + wz);
            /* does the segment cross a face of the cell? major axis first */
            for (j = 0; j < 3; j++) {
                long double tt;
                double p1, p2;

                if (sel[j] == 0) {
                    tt = (((!(dx >= 0.0f) ? -hf : hf) + (long double)wx) - s->x) / ((long double)e->x - s->x);
                    p1 = (double)(((long double)e->y - s->y) * tt + s->y);
                    p2 = (double)(((long double)e->z - s->z) * tt + s->z);
                    if (p1 >= miny && !(p1 > maxy) && p2 >= minz && !(p2 > maxz))
                        goto push;
                } else if (sel[j] == 1) {
                    tt = (((!(dy >= 0.0f) ? -hf : hf) + (long double)wy) - s->y) / ((long double)e->y - s->y);
                    p1 = (double)(((long double)e->x - s->x) * tt + s->x);
                    p2 = (double)(((long double)e->z - s->z) * tt + s->z);
                    if (p1 >= minx && !(p1 > maxx) && p2 >= minz && !(p2 > maxz))
                        goto push;
                } else {
                    long double h = half + 0x4001;

                    tt = (((!(dz >= 0.0f) ? -h : h) + wz) - s->z) / ((long double)e->z - s->z);
                    p1 = (double)(((long double)e->x - s->x) * tt + s->x);
                    p2 = (double)(((long double)e->y - s->y) * tt + s->y);
                    if (p1 >= minx && !(p1 > maxx) && p2 >= miny && !(p2 > maxy))
                        goto push;
                }
            }
            continue;
        push:
            sp++;
            fresh[sp] = 1;
            {
                CollCursor *nc = &stack[sp];
                unsigned int code = F.node->code[child];
                int ci = idx;

                if (F.orient < 4)
                    nc->orient = (code & 4) + ((code + F.orient) & 3);
                else
                    nc->orient = ((code & 4) ^ (F.orient & 4)) + ((F.orient - code) & 3);
                nc->x = cx;
                nc->level = F.level - 1;
                nc->z = cz;
                nc->size = half;
                nc->y = cy;
                nc->node = nn;
                nc->terrain = F.terrain;
                if (sp == 0 && depth == 0)
                    ci = 7 - idx;
                nc->objects = gCollWalkTasks && o ? o->child[ci] : NULL;
            }
        }
    } while (sp >= 0);
    Coll_RestoreMods();
    return 0;
found:
    out->normal = n;
    out->pos = hp;
    out->material = m1 ? t->materials[m1] : NULL;
    out->back = m2 ? t->materials[m2] : NULL;
    out->dist = (double)Vec3d_Distance(&out->pos, s);
    Coll_RestoreMods();
    return 1;
}
HOOK(0x4CC810, Coll_LineHit);

/* Segment s-e against the triangles of one leaf cell. Returns the hit
   nearest to s in hit, with the triangle normal and which side of it s is. */
int Coll_LeafLineHit(const short *leaf, const Vec3d *base, const Vec3d *s, const Vec3d *e, Vec3d *hit, int shift,
                     int *side, int *side2, unsigned int flags, Vec3f *normal)
{
    Vec3d v[64], S = *s, E = *e, best;
    const unsigned int *tris = (const unsigned int *)(leaf + 4);
    const int *verts = (const int *)((const char *)tris + leaf[1]);
    const signed char *heights = NULL;
    float bump_x = 0, bump_y = 0, bump_scale = 0, sign = 1.0f;
    int bump_shift = 0, nverts = leaf[3] + leaf[2], found = 0, i, n;
    double scale;
    Vec3f d, bn = {0, 0, 0};

    if (gCollBumpCount) {
        const CollBump *b = &gCollBumps[gCollBumpCount];

        bump_x = (float)b->x;
        bump_scale = b->scale;
        heights = b->heights;
        bump_y = (float)b->y;
        bump_shift = b->sizeShift + 3;
    }
    if (flags & 4)
        sign = -1.0f;
    scale = (double)((long double)(0x40000000 >> (shift & 31)) * kOneOver24);
    for (i = 0; i < nverts; i++) {
        int p = verts[i];
        long double x = (long double)(((p >> 26) & 0x3F) - 24) * scale;
        double y = (double)((long double)(((p >> 20) & 0x3F) - 24) * scale);
        double z = (double)((long double)(((p >> 14) & 0x3F) - 24) * scale);
        double X, Y, Z;

        if (flags & 4)
            x = -x;
        if (flags & 1) {
            double t = y;

            if (flags & 2) {
                y = (double)x87_neg(x);
                x = t;
            } else {
                y = (double)x;
                x = -(long double)t;
            }
        } else if (flags & 2) {
            x = -x;
            y = -y;
        }
        X = (double)(x + base->x);
        Y = (double)((long double)y + base->y);
        Z = (double)((long double)z + base->z);
        if (gCollBumpCount) {
            long double fx = ((long double)X - bump_x) * bump_scale;
            float fy = (float)(((long double)Y - bump_y) * bump_scale);
            int ix = fistp(fx), iy = fistp(fy);
            int r0 = (iy << (bump_shift & 31)) + iy;
            int r1 = ((iy + 1) << (bump_shift & 31)) + iy + 1 + ix + 1;
            long double tx = fx - ix;
            long double ty = fy - (long double)iy;
            long double h00 = heights[r0 + ix], h01 = heights[r0 + ix + 1], h11 = heights[r1];
            float h10 = heights[r1 - 1];
            long double top = (h01 - h00) * tx + h00;
            long double bottom = (h11 - h10) * tx + h10;

            Z = (double)((((bottom - top) * ty + top) - 64.0) * 256.0f + Z);
        }
        v[i].x = X;
        v[i].y = Y;
        v[i].z = Z;
    }
    d.x = (float)((long double)E.x - S.x);
    d.y = (float)((long double)E.y - S.y);
    d.z = (float)((long double)E.z - S.z);
    n = leaf[0];
    best = *e;
    if (n <= 0)
        return 0;
    do {
        unsigned int tri = *tris++;
        const Vec3d *a = &v[tri & 0xFF], *b = &v[(tri >> 8) & 0xFF], *c = &v[(tri >> 16) & 0xFF];
        Vec3d e1, e2, nd, P;
        Vec3f e1f, e2f, nf, rel, out;
        long double ax, ay, az;
        float ddx;
        double lo, hi;

        e1.x = b->x - a->x;
        e1.y = b->y - a->y;
        e1.z = b->z - a->z;
        e2.x = c->x - a->x;
        e2.y = c->y - a->y;
        e2.z = c->z - a->z;
        Vec3d_Cross(&nd, &e1, &e2);
        Vec3d_Normalize(&nd);
        e1f.x = (float)e1.x;
        e1f.y = (float)e1.y;
        e1f.z = (float)e1.z;
        e2f.x = (float)e2.x;
        e2f.y = (float)e2.y;
        e2f.z = (float)e2.z;
        nf.x = (float)nd.x;
        nf.y = (float)nd.y;
        nf.z = (float)nd.z;
        rel.x = (float)((long double)S.x - a->x);
        rel.y = (float)((long double)S.y - a->y);
        rel.z = (float)((long double)S.z - a->z);
        if (!Vec3_RayPlane(&out, &rel, &d, &nf))
            continue;
        if (!Vec3_TriangleCoords(NULL, NULL, &out, &e1f, &e2f, &nf))
            continue;
        P.x = (double)((long double)out.x + a->x);
        P.y = (double)((long double)out.y + a->y);
        P.z = (double)((long double)out.z + a->z);
        /* keep hits within the segment along its longest axis */
        ddx = (float)((long double)s->x - e->x);
        ay = (long double)s->y - e->y;
        az = (long double)s->z - e->z;
        ax = ddx;
        if (!(ax >= 0.0f))
            ax = -ax;
        if (!(ay >= 0.0f))
            ay = -ay;
        if (!(az >= 0.0f))
            az = -az;
        if (ax > ay && ax > az) {
            if (s->x > e->x) {
                lo = e->x;
                hi = s->x;
            } else {
                lo = s->x;
                hi = e->x;
            }
            if (!(P.x >= lo) || P.x > hi)
                continue;
        } else if (ay > az) {
            if (s->y > e->y) {
                lo = e->y;
                hi = s->y;
            } else {
                lo = s->y;
                hi = e->y;
            }
            if (!(P.y >= lo) || P.y > hi)
                continue;
        } else {
            if (s->z > e->z) {
                lo = e->z;
                hi = s->z;
            } else {
                lo = s->z;
                hi = e->z;
            }
            if (!(P.z >= lo) || P.z > hi)
                continue;
        }
        {
            double dn = (double)Vec3d_DistanceSq(s, &P);

            if (!(Vec3d_DistanceSq(s, &best) > dn))
                continue;
        }
        found = 1;
        best = P;
        bn = nf;
    } while (--n);
    if (!found)
        return 0;
    {
        long double dx = (long double)e->x - s->x, dy = (long double)e->y - s->y, dz = (long double)e->z - s->z;
        float nx = (float)(bn.x * sign), ny = (float)(bn.y * sign), nz = (float)(bn.z * sign);
        int sd = !((long double)nz * dz + (long double)ny * dy + (long double)nx * dx >= 0.0f);

        *side = sd;
        *side2 = sd ^ 1;
        normal->x = nx;
        normal->y = ny;
        normal->z = nz;
        *hit = best;
    }
    return 1;
}
HOOK(0x4CD300, Coll_LeafLineHit);

/* Grows [lo, hi] by v. NaN goes to lo, like the original's compares. */
static inline void grow(double *lo, double *hi, double v)
{
    if (v > *hi)
        *hi = v;
    else if (!(v >= *lo))
        *lo = v;
}

/* Coll_Lines with one more query word, passed to the task handlers. */
void Coll_LinesEx(Terrain *t, CollHit *results, const Vec3d *starts, const Vec3d *ends, int count, char unk,
                  int unk2, unsigned int flags, int unk3)
{
    CollLineQuery q;
    CollCursor c;
    Vec3d hi, lo;
    int vertical = 1, i, depth, top;
    unsigned int a, b, d;

    if (!count)
        return;
    gCollLineCount += count;
    gCollLineCalls++;
    hi = starts[0];
    lo = starts[0];
    for (i = 0; i < count; i++) {
        const Vec3d *s = &starts[i], *e = &ends[i];

        if (s->x < e->x || s->x > e->x || s->y < e->y || s->y > e->y)
            vertical = 0;
        grow(&lo.x, &hi.x, s->x);
        grow(&lo.y, &hi.y, s->y);
        if (!(hi.z >= s->z))
            hi.z = s->z;
        else if (!(s->z >= lo.z))
            lo.z = s->z;
        grow(&lo.x, &hi.x, e->x);
        grow(&lo.y, &hi.y, e->y);
        if (!(hi.z >= e->z))
            hi.z = e->z;
        else if (!(e->z >= lo.z))
            lo.z = e->z;
        results[i].found = 0;
        results[i].task = 0;
        results[i].pos = *e;
    }
    hi.x = hi.x + 2.0;
    hi.y = hi.y + 2.0;
    hi.z = hi.z + 16387.0;
    lo.x = lo.x - 2.0;
    lo.y = lo.y - 2.0;
    lo.z = lo.z - 16387.0;
    q.starts = starts;
    q.ends = ends;
    q.count = count;
    q.slot = unk;
    q.slotArg = (char *)unk2;
    q.mesh = unk3;
    q.allFaces = 0;
    q.magic = 0x41424344;
    q.results = results;
    if (flags & 0x7FFFFFFF) {
        for (i = 0; i < 6; i++) {
            if (i != 1 && i != 0 && i != 4 && i != 5)
                continue;
            if (vertical)
                DynCube_SendEventBox(t, 10, &q, &lo, &hi, flags, i);
            else
                DynCube_SendEventLines(t, 9, &q, starts, ends, count, flags, i);
        }
    }
    if (!(flags & 0x80000000))
        return;
    Coll_ResetMods();
    c.size = 0x40000000;
    c.level = 30;
    c.orient = 0;
    c.x = 0;
    c.y = 0;
    c.z = 0;
    c.node = t->ctr + 1;
    c.terrain = t;
    a = (fistp(hi.x) + t->offset[0]) ^ (fistp(lo.x) + t->offset[0]);
    b = (fistp(hi.y) + t->offset[1]) ^ (fistp(lo.y) + t->offset[1]);
    d = (fistp(hi.z) + t->offset[2]) ^ (fistp(lo.z) + t->offset[2]);
    depth = 30 - top_bit(d | b | a);
    if (depth > 16)
        depth = 16;
    top = Coll_Descend_(t, &c, &c, starts, depth);
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
        q2.results = NULL;
        q2.unk90 = 0;
        q2.unk94 = 0;
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
        r->back = h.back;
        r->normal = h.normal;
        r->found = 1;
        r->task = 0;
    }
}
HOOK(0x4CDB20, Coll_LinesEx);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A4438C) int gCollLineCalls;
BSS(A4438C) int gCollLineCount;
ALIAS(gUnkA4438C, gCollLineCalls, 0);
PAD(igb, A9C4E0, A9C4E0, 0x600);
ALIAS(gTaskEnterHandlers, pad_A9C4E0, 0);
PAD(igb, A9C4E0, A9CAE0, 0x59400);
ALIAS(gTaskLeaveHandlers, pad_A9CAE0, 0);
