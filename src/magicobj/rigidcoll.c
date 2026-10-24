/* RigidDynCubeObj: segment test against the collision mesh (slot 8) and
   creation of the magic objects at the model's attachment points. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "magicobj/magicobj.h"
#include "math/x87.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4BD070, int, Qvm_GetArgLine, (void *lod, void *tri, float v, float u));
ORIG(0x4D0950, float, VirModel_GetRadius, (void **model));
ORIG(0x4D0CF0, void *, ModelLod_GetAttach, (void *lod, int i));
ORIG(0x4D0D10, int, ModelLod_GetAttachCount, (void *lod));

extern MagicHandler gTaskHandlers[][0x180];
extern MagicObjDef gMagicObjDefs[];
extern int gMagicObjDefCount;
extern float kZero;
extern float kOne;
extern double kZeroD;
extern double kOneD;

/* event 8 query, see Coll_LineOfSight */
typedef struct RigidLineQuery {
    double *from;
    double *to;
    uint8_t filterEvent;    /* 0xFF = no filter */
    char *filter;           /* the filter handler clears its first byte to skip us */
    char *world;            /* +0xD3C: float * per material, how much it lets through */
    char blocked;
    float visibility;
} RigidLineQuery;

typedef struct CollTri {
    uint16_t v[3];
    uint16_t surface;
} CollTri;

typedef struct CollSurface {
    int unk0;
    uint16_t unk4;      /* only 0xFFFF surfaces are tested */
    uint16_t unk6;
    uint16_t material;  /* 0xFFFF = blocks everything */
    uint16_t unkA;
} CollSurface;

typedef struct CollMesh {
    int triCount;
    int vertCount;
    int unk08[2];
    CollTri *tris;
    float (*verts)[4];
    CollSurface *surfaces;
    RigidCollNode *nodes;
} CollMesh;

/* the segment in model space and its bounding box */
typedef struct CollSeg {
    float a[3];
    float d[3];
    double invLen2;
    float min[3];
    float max[3];
} CollSeg;

static float coll_max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

static int coll_sphere_hit(const CollSeg *s, const RigidCollNode *n)
{
    float cx = n->center[0] - s->a[0];
    float cy = n->center[1] - s->a[1];
    long double czl = n->center[2] - s->a[2];
    float cz = czl;
    float dot = cz * s->d[2] + cy * s->d[1] + cx * s->d[0];
    float r = n->radius;
    long double dist = cx * cx + cy * cy + czl * czl - dot * dot * s->invLen2;

    if (!(r * r >= dist))
        return 0;
    if (!(n->radius + n->center[0] >= s->min[0]) || !(n->center[1] + n->radius >= s->min[1]) ||
        !(n->center[2] + n->radius >= s->min[2]))
        return 0;
    if (n->center[0] - n->radius > s->max[0] || n->center[1] - n->radius > s->max[1] ||
        n->center[2] - n->radius > s->max[2])
        return 0;
    return 1;
}

static int coll_is_zero(long double v)
{
    return !(v < 0 || v > 0);
}

static float coll_abs(float v)
{
    return v < 0 ? -v : v;
}

/* 0 = keep going, 1 = stop */
static int coll_node_tris(RigidDynCubeObj *o, RigidLineQuery *q, CollMesh *mesh, float (*verts)[4], void *lod,
                          const CollSeg *s, const RigidCollNode *n)
{
    CollTri *t = &mesh->tris[n->firstTri];
    int j;

    for (j = n->firstTri; j < n->firstTri + n->triCount; j++, t++) {
        const float *v0 = verts[t->v[0]];
        const float *v1 = verts[t->v[1]];
        const float *v2 = verts[t->v[2]];
        CollSurface *sf = &mesh->surfaces[t->surface];
        float e1[3], e2[3], nx, ny, nz, nyNeg, denf, sf32, px, py, pz;
        long double den, sl, v;
        double u;
        float *mat;

        if (sf->unk4 != 0xFFFF)
            continue;
        if (s->min[0] > v0[0] && s->min[0] > v1[0] && s->min[0] > v2[0])
            continue;
        if (s->min[1] > v0[1] && s->min[1] > v1[1] && s->min[1] > v2[1])
            continue;
        if (s->min[2] > v0[2] && s->min[2] > v1[2] && s->min[2] > v2[2])
            continue;
        if (!(s->max[0] >= v0[0]) && !(s->max[0] >= v1[0]) && !(s->max[0] >= v2[0]))
            continue;
        if (!(s->max[1] >= v0[1]) && !(s->max[1] >= v1[1]) && !(s->max[1] >= v2[1]))
            continue;
        if (!(s->max[2] >= v0[2]) && !(s->max[2] >= v1[2]) && !(s->max[2] >= v2[2]))
            continue;

        e2[0] = v2[0] - v0[0];
        e2[1] = v2[1] - v0[1];
        e2[2] = v2[2] - v0[2];
        e1[0] = v1[0] - v0[0];
        e1[1] = v1[1] - v0[1];
        {
            long double z = v1[2] - v0[2];

            e1[2] = z;
            nx = z * e2[1] - e1[1] * e2[2];
        }
        {
            long double y = e1[2] * e2[0] - e1[0] * e2[2];

            ny = y;
            nyNeg = -y;
        }
        nz = e1[1] * e2[0] - e1[0] * e2[1];
        den = nz * s->d[2] + nyNeg * s->d[1] + nx * s->d[0];
        denf = den;
        if (!(den < kZeroD || den > kZeroD))
            continue;
        sl = ((v0[2] - s->a[2]) * nz + (v0[1] - s->a[1]) * nyNeg + (v0[0] - s->a[0]) * nx) / den;
        sf32 = sl;
        if (!(sl >= kZero) || sf32 > kOne)
            continue;
        px = sf32 * s->d[0] + s->a[0];
        py = sf32 * s->d[1] + s->a[1];
        pz = sf32 * s->d[2] + s->a[2];

        /* project onto the plane where the normal is largest */
        if (coll_abs(nx) > coll_abs(nyNeg) && coll_abs(nx) > coll_abs(nz)) {
            float qz, qy;

            if (coll_is_zero(nx))
                continue;
            qz = pz - v0[2];
            qy = py - v0[1];
            u = (qz * e2[1] - qy * e2[2]) / nx;
            if (!(u >= kZeroD) || u > kOneD)
                continue;
            if (coll_abs(e2[1]) > coll_abs(e2[2])) {
                v = (qy - e1[1] * u) / e2[1];
            } else {
                if (coll_is_zero(e2[2]))
                    continue;
                v = (qz - e1[2] * u) / e2[2];
            }
        } else if (coll_abs(nyNeg) > coll_abs(nz)) {
            float qz, qx;

            if (coll_is_zero(ny))
                continue;
            qz = pz - v0[2];
            qx = px - v0[0];
            u = (qz * e2[0] - qx * e2[2]) / ny;
            if (!(u >= kZeroD) || u > kOneD)
                continue;
            if (coll_abs(e2[0]) > coll_abs(e2[2])) {
                v = (qx - e1[0] * u) / e2[0];
            } else {
                if (coll_is_zero(e2[2]))
                    continue;
                v = (qz - e1[2] * u) / e2[2];
            }
        } else {
            float qy, qx;

            if (coll_is_zero(nz))
                continue;
            qy = py - v0[1];
            qx = px - v0[0];
            u = (qy * e2[0] - qx * e2[1]) / nz;
            if (!(u >= kZeroD) || u > kOneD)
                continue;
            if (coll_abs(e2[0]) > coll_abs(e2[1])) {
                v = (qx - e1[0] * u) / e2[0];
            } else {
                if (coll_is_zero(e2[1]))
                    continue;
                v = (qy - e1[1] * u) / e2[1];
            }
        }
        if (!(v >= kZeroD) || v + u > kOneD)
            continue;

        if (Qvm_GetArgLine(lod, t, (float)v, (float)u))
            continue;
        q->blocked = 1;
        if (sf->material == 0xFFFF) {
            q->visibility = 0.0f;
            return 1;
        }
        mat = *(float **)(q->world + 0xD3C + sf->material * 4);
        if (!(denf >= kZero))
            q->visibility = *mat * q->visibility;
        if (!(*mat < kZero || *mat > kZero)) {
            q->visibility = 0.0f;
            return 1;
        }
    }
    return 0;
}

/* slot 8: lowers q->visibility for every mesh triangle the segment crosses */
void RigidDynCubeObj_CollLine(RigidDynCubeObj *o, RigidLineQuery *q)
{
    double *p0 = q->from;
    double *p1 = q->to;
    RigidCollNode *stack[100];
    RigidCollNode *nodes, *n;
    CollMesh *mesh;
    CollSeg s;
    void *lod;
    char *info;
    float (*verts)[4];
    const float *M = o->matrix.m;
    long double rs, dx, dy, dz, bz, d0, d1, d2;
    float sc, r, t0, t1, t2, bx, by;
    int sp, first;

    if (!o->model || o->lod == -1)
        return;
    lod = o->model[o->lod];
    info = *(char **)(*(char **)((char *)lod + 0xC) + 0xC);
    if (*(int *)(info + 0x28))
        return;
    mesh = *(CollMesh **)(info + 0x2C);
    if (!mesh || !mesh->triCount)
        return;
    verts = mesh->verts;
    if (q->blocked && !(q->visibility < kZero || q->visibility > kZero))
        return;
    if (q->filterEvent != 0xFF) {
        MagicHandler h = gTaskHandlers[q->filterEvent][o->task.type];

        if (h) {
            h(o, q->filter);
            if (!*q->filter)
                return;
        }
    }
    nodes = mesh->nodes;

    sc = coll_max_scale(o);
    rs = VirModel_GetRadius(o->model) * (long double)sc;
    r = rs;
    if (o->pos.x - rs > p0[0] && o->pos.x - rs > p1[0])
        return;
    if (!(r + o->pos.x >= p0[0]) && !(r + o->pos.x >= p1[0]))
        return;
    if (o->pos.y - r > p0[1] && o->pos.y - r > p1[1])
        return;
    if (!(r + o->pos.y >= p0[1]) && !(r + o->pos.y >= p1[1]))
        return;
    if (o->pos.z - r > p0[2] && o->pos.z - r > p1[2])
        return;
    if (!(r + o->pos.z >= p0[2]) && !(r + o->pos.z >= p1[2]))
        return;

    /* both ends into model space (the inverse rotation, divided by scale x) */
    dx = p0[0] - o->pos.x;
    dy = p0[1] - o->pos.y;
    dz = p0[2] - o->pos.z;
    t0 = dy * M[3] + dz * M[6] + dx * M[0];
    t1 = dz * M[7] + dx * M[1] + dy * M[4];
    t2 = dy * M[5] + dz * M[8] + dx * M[2];
    s.a[0] = t0 / o->scale[0];
    s.a[1] = t1 / o->scale[0];
    s.a[2] = t2 / o->scale[0];
    dx = p1[0] - o->pos.x;
    dy = p1[1] - o->pos.y;
    dz = p1[2] - o->pos.z;
    t0 = dz * M[6] + dx * M[0] + dy * M[3];
    t1 = dz * M[7] + dx * M[1] + dy * M[4];
    t2 = dz * M[8] + dx * M[2] + dy * M[5];
    bx = t0 / o->scale[0];
    by = t1 / o->scale[0];
    bz = t2 / o->scale[0];

    s.min[0] = s.a[0] >= bx ? bx : s.a[0];
    s.min[1] = s.a[1] >= by ? by : s.a[1];
    s.min[2] = s.a[2] >= bz ? (float)bz : s.a[2];
    s.max[0] = s.a[0] > bx ? s.a[0] : bx;
    s.max[1] = s.a[1] > by ? s.a[1] : by;
    s.max[2] = s.a[2] > bz ? s.a[2] : (float)bz;
    d2 = bz - s.a[2];
    d1 = by - s.a[1];
    d0 = bx - s.a[0];
    s.d[0] = d0;
    s.d[1] = d1;
    s.invLen2 = kOneD / (d0 * d0 + d1 * d1 + d2 * d2);
    s.d[2] = d2;

    /* walk the sphere tree; the root is not tested, and a child that fails
       the test still has its own triangles tested */
    sp = 0;
    first = 1;
    n = nodes;
    stack[0] = n;
    for (;;) {
        if (!first) {
            if (!n->next) {
                if (--sp < 0)
                    return;
                n = stack[sp];
                continue;
            }
            n = &nodes[n->next];
            stack[sp] = n;
            if (!coll_sphere_hit(&s, n))
                continue;
        }
        first = 0;
        while (n->child) {
            n = &nodes[n->child];
            stack[++sp] = n;
            if (!coll_sphere_hit(&s, n))
                break;
        }
        if (n->triCount && coll_node_tris(o, q, mesh, verts, lod, &s, n))
            return;
    }
}
HOOK(0x4C35F0, RigidDynCubeObj_CollLine);

/* one attachment point of a model LOD (0x44 bytes) */
typedef struct MagicAttach {
    char name[16];
    float pos[3];
    float m[9];
    int unk40;
} MagicAttach;

static int vec_is_zero(const float *v)
{
    return !(v[0] < kZero || v[0] > kZero) && !(v[1] < kZero || v[1] > kZero) && !(v[2] < kZero || v[2] > kZero);
}

static void vec_scale(float *v, long double k)
{
    v[0] = v[0] * k;
    v[1] = v[1] * k;
    v[2] = v[2] * k;
}

/* Creates a task of the DefineMagicObj type named by each attachment point
   of the model, placed and turned with the object. */
void RigidDynCubeObj_CreateMagicObjs(void *parent, RigidDynCubeObj *o, void *owner)
{
    int count, i, j;

    if (!o->model)
        return;
    count = ModelLod_GetAttachCount(o->model[0]);
    for (i = 0; i < count; i++) {
        MagicAttach *att = ModelLod_GetAttach(o->model[0], i);
        const float *M = o->matrix.m;
        const float *a = att->m;
        MagicObjDef *def = NULL;
        MagicCreateArg arg, arg2;
        MagicHandler h;
        Task *t;
        double rx, ry, rz;
        float c0, c1, e[3], f[3], *W, *E, *F, *R, *P;
        long double c2;

        arg.offset.x = att->pos[0];
        arg.offset.y = att->pos[1];
        arg.offset.z = att->pos[2];
        rx = M[1] * att->pos[1] + M[2] * att->pos[2] + M[0] * att->pos[0];
        ry = M[4] * att->pos[1] + M[5] * att->pos[2] + M[3] * att->pos[0];
        rz = M[8] * att->pos[2] + M[6] * att->pos[0] + M[7] * att->pos[1];
        arg.pos.x = rx + o->pos.x;
        arg.pos.y = ry + o->pos.y;
        arg.pos.z = rz + o->pos.z;

        /* local frame from the first column of the attachment matrix */
        R = arg.local.m;
        W = R;
        E = R + 3;
        F = R + 6;
        c0 = a[7] * a[3] - a[4] * a[6];
        c1 = -(a[7] * a[0] - a[1] * a[6]);
        c2 = a[4] * a[0] - a[1] * a[3];
        memcpy(&W[0], &a[0], 4);
        memcpy(&W[1], &a[3], 4);
        memcpy(&W[2], &a[6], 4);
        if (!(vec_is_zero(W)))
            vec_scale(W, kOneD / x87_sqrt(a[0] * a[0] + a[3] * a[3] + a[6] * a[6]));
        e[0] = c1 * a[6] - c2 * a[3];
        e[1] = -(c0 * a[6] - c2 * a[0]);
        e[2] = c0 * a[3] - c1 * a[0];
        f[0] = e[2] * W[1] - e[1] * W[2];
        f[1] = -(e[2] * W[0] - e[0] * W[2]);
        f[2] = e[1] * W[0] - e[0] * W[1];
        memcpy(E, e, sizeof(e));
        if (!vec_is_zero(e))
            vec_scale(E, kOneD / x87_sqrt(e[0] * e[0] + e[2] * e[2] + e[1] * e[1]));
        memcpy(F, f, sizeof(f));
        if (!vec_is_zero(f))
            vec_scale(F, kOneD / x87_sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]));
        arg.local.count = 0;

        /* world frame: object matrix times the local one, made orthonormal
           again (the original also does that a second time into a copy it
           never reads when the matrix counter passes 1000) */
        P = arg.matrix.m;
        P[0] = R[3] * M[1] + R[6] * M[2] + R[0] * M[0];
        P[1] = R[4] * M[1] + R[7] * M[2] + R[1] * M[0];
        P[2] = R[5] * M[1] + R[8] * M[2] + R[2] * M[0];
        P[3] = R[3] * M[4] + R[6] * M[5] + R[0] * M[3];
        P[4] = R[4] * M[4] + R[7] * M[5] + R[1] * M[3];
        P[5] = R[5] * M[4] + R[8] * M[5] + R[2] * M[3];
        P[6] = R[6] * M[8] + R[0] * M[6] + R[3] * M[7];
        P[7] = R[7] * M[8] + R[1] * M[6] + R[4] * M[7];
        P[8] = R[8] * M[8] + R[2] * M[6] + R[5] * M[7];
        {
            float c[3], d[3];

            c[0] = P[2] * P[7] - P[1] * P[8];
            c[1] = -(P[6] * P[2] - P[0] * P[8]);
            c[2] = P[6] * P[1] - P[0] * P[7];
            if (!vec_is_zero(P))
                vec_scale(P, kOneD / x87_sqrt(P[0] * P[0] + P[1] * P[1] + P[2] * P[2]));
            d[0] = c[2] * P[1] - c[1] * P[2];
            d[1] = -(c[2] * P[0] - c[0] * P[2]);
            d[2] = c[1] * P[0] - c[0] * P[1];
            memcpy(P + 3, c, sizeof(c));
            if (!vec_is_zero(c))
                vec_scale(P + 3, kOneD / x87_sqrt(c[2] * c[2] + c[1] * c[1] + c[0] * c[0]));
            memcpy(P + 6, d, sizeof(d));
            if (!vec_is_zero(d))
                vec_scale(P + 6, kOneD / x87_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
        }
        arg.matrix.count = 0;

        for (j = 0; j < gMagicObjDefCount && !def; j++)
            if (strcmp(att->name, gMagicObjDefs[j].name) == 0)
                def = &gMagicObjDefs[j];
        if (!def) {
            Error_Show("Magic object type \"%s\" not defined (Created from model \"%s\")", att->name,
                       (char *)o->model + 0x14);
            for (;;)
                ;
        }
        arg.data = def->data;
        arg.world = (void *)(intptr_t)o->world;
        arg.owner = owner;
        arg.model = def->model;
        t = Task_New(parent, def->type, &arg);
        /* only these four fields are set, like the original */
        arg2.data = def->data;
        arg2.model = def->model;
        arg2.world = (void *)(intptr_t)o->world;
        arg2.owner = owner;
        h = gTaskHandlers[(uint8_t)RigidDynCubeObj_GetEvent3()][t->type];
        if (h)
            h(t, &arg2);
    }
}
HOOK(0x4C4950, RigidDynCubeObj_CreateMagicObjs);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5333B0) double kZeroD INIT(0.0);
ALIAS(kZero64, kZeroD, 0);
DATA(5333F8) double kOneD INIT(1.0);
ALIAS(kBOneD, kOneD, 0);
ALIAS(kOne64, kOneD, 0);
