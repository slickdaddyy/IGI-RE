/* Line collision against the collision mesh of a RigidDynCubeObj (task slots 9 and 10). */
#pragma GCC optimize("excess-precision=standard")

#include "igi.h"
#include "hook.h"
#include "magicobj/magicobjtypes.h"
#include "coll/colltypes.h"
#include "task/taskvars.h"

typedef struct RigidCollVert {
    float x, y, z;
    int unkC;
} RigidCollVert;

typedef struct RigidCollTri {
    uint16_t v[3];
    uint16_t face;
} RigidCollTri;

typedef struct RigidCollFace {
    int unk0;
    int16_t unk4; /* -1: always solid, else only when the query asks for all faces */
    int16_t unk6;
    uint16_t material;
    uint16_t unkA;
} RigidCollFace;

typedef struct RigidCollMesh {
    int unk0;
    int unk4;
    int unk8;
    int node_count;
    RigidCollTri *tris;
    RigidCollVert *verts;
    RigidCollFace *faces;
    RigidCollNode *nodes;
} RigidCollMesh;

typedef struct RigidModelData {
    char unk0[0x28];
    int unk28;
    RigidCollMesh *coll;
} RigidModelData;

typedef struct RigidModelPart {
    char unk0[0xC];
    RigidModelData *data;
} RigidModelPart;

typedef struct RigidLod {
    char unk0[0xC];
    RigidModelPart *part;
} RigidLod;

/* the segment in object space, divided by the x scale */
typedef struct RigidLineWalk {
    float p0x, p0y, p0z;
    float dx, dy, dz;
    double inv_len2;
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
} RigidLineWalk;

_Static_assert(sizeof(RigidCollMesh) == 0x20, "RigidCollMesh");

ORIG(0x4BD070, int, Qvm_GetArgLine, (RigidLod *lod, RigidCollTri *tri, float u, float v));
ORIG(0x4D0950, long double, VirModel_GetRadius, (RigidLod **model));

typedef void (*RigidTaskHandler)(void *task, void *arg);

static const float kBoxSlack = 0x1.99999ap-4f;   /* 0.1f */
static const double kCloseRange = 0x1.999999999999ap-4; /* 0.1 */
static const float kZeroF = 0.0f, kOneF = 1.0f;
static const double kZero = 0.0, kOne = 1.0;

static void RigidLineWalk_SetBox(RigidLineWalk *w, float x, float y, float z)
{
    w->min_x = (w->p0x < x ? w->p0x : x) - kBoxSlack;
    w->min_y = (w->p0y < y ? w->p0y : y) - kBoxSlack;
    w->min_z = (w->p0z < z ? w->p0z : z) - kBoxSlack;
    w->max_x = (w->p0x > x ? w->p0x : x) + kBoxSlack;
    w->max_y = (w->p0y > y ? w->p0y : y) + kBoxSlack;
    w->max_z = (w->p0z > z ? w->p0z : z) + kBoxSlack;
}

/* sphere against the infinite line, then against the box. The original sums
   the squared offsets in a different order for a first child. */
static int RigidLineWalk_NodeHit(const RigidLineWalk *w, const RigidCollNode *n, int first_child)
{
    long double cx = n->center[0] - w->p0x, cy = n->center[1] - w->p0y, cz = n->center[2] - w->p0z;
    double dx = cx, dy = cy, dz = cz;
    long double along = (cz * w->dz + cy * w->dy) + cx * w->dx;
    long double d2;

    if (first_child)
        d2 = (dz * dz + dx * dx) + dy * dy;
    else
        d2 = (dx * dx + dy * dy) + dz * dz;
    if (n->radius * n->radius < d2 - along * along * w->inv_len2)
        return 0;
    if (n->radius + n->center[0] < w->min_x || n->center[1] + n->radius < w->min_y || n->center[2] + n->radius < w->min_z)
        return 0;
    if (n->center[0] - n->radius > w->max_x || n->center[1] - n->radius > w->max_y || n->center[2] - n->radius > w->max_z)
        return 0;
    return 1;
}

void RigidDynCubeObj_LineHit(RigidDynCubeObj *obj, CollLineQuery *q)
{
    const Vec3d *start = q->starts, *end = q->ends;
    CollHit *hit = q->results;
    RigidCollNode *stack[100];
    RigidCollMesh *cm;
    RigidCollVert *verts;
    RigidLod *lod;
    RigidModelData *data;
    char close = 0;
    int i;

    if (!obj->model || obj->lod == -1)
        return;
    lod = obj->model[obj->lod];
    data = lod->part->data;
    if (data->unk28)
        return;
    cm = &data->coll[q->mesh];
    if (!cm || !cm->unk0)
        return;
    verts = cm->verts;
    if (q->slot != 0xFF) {
        RigidTaskHandler fn = gTaskHandlers[q->slot][obj->task.type];
        if (fn) {
            fn(obj, q->slotArg);
            if (!*q->slotArg)
                return;
        }
    }

    for (i = 0; i < q->count; i++, start++, end++, hit++) {
        const float *m = obj->matrix.m;
        RigidLineWalk w;
        RigidCollNode *node;
        RigidCollTri *hit_tri = NULL;
        float hx = 0, hy = 0, hz = 0, hu = 0, hv = 0;
        float scale, len, radius, p1x, p1y, p1z;
        long double rad, lim, ax, ay, az, dx, dy, dz;
        double best = 0, dyd, dzd;
        char side = hit->found ? hit->material != NULL : 0;
        char had = (char)hit->found;
        char found = 0;
        int depth = 0;

        scale = obj->scale[2] < obj->scale[1] ? obj->scale[1] : obj->scale[2];
        scale = obj->scale[0] > scale ? obj->scale[0] : scale;
        rad = VirModel_GetRadius((RigidLod **)obj->model) * scale;
        radius = rad;

        /* bounding box of the object against the segment */
        lim = obj->pos.x - rad;
        if (!(lim <= start->x || lim <= end->x))
            continue;
        if (!(radius + obj->pos.x >= start->x || radius + obj->pos.x >= end->x))
            continue;
        if (!(obj->pos.y - radius <= start->y || obj->pos.y - radius <= end->y))
            continue;
        if (!(radius + obj->pos.y >= start->y || radius + obj->pos.y >= end->y))
            continue;
        if (!(obj->pos.z - radius <= start->z || obj->pos.z - radius <= end->z))
            continue;
        if (!(radius + obj->pos.z >= start->z || radius + obj->pos.z >= end->z))
            continue;

        /* both ends into object space; all three are divided by the x scale */
        ax = start->x - obj->pos.x;
        ay = start->y - obj->pos.y;
        az = start->z - obj->pos.z;
        {
            float lx = (az * m[6] + ax * m[0]) + ay * m[3];
            float ly = (az * m[7] + ax * m[1]) + ay * m[4];
            float lz = (az * m[8] + ax * m[2]) + ay * m[5];
            w.p0x = lx / obj->scale[0];
            w.p0y = ly / obj->scale[0];
            w.p0z = lz / obj->scale[0];
        }
        ax = end->x - obj->pos.x;
        ay = end->y - obj->pos.y;
        az = end->z - obj->pos.z;
        {
            float lx = (ay * m[3] + az * m[6]) + ax * m[0];
            float ly = (az * m[7] + ax * m[1]) + ay * m[4];
            float lz = (ay * m[5] + az * m[8]) + ax * m[2];
            long double qz = lz / obj->scale[0];
            p1x = lx / obj->scale[0];
            p1y = ly / obj->scale[0];
            p1z = qz;
            dz = qz - w.p0z;
        }
        dzd = dz;
        dy = p1y - w.p0y;
        dyd = dy;
        dx = p1x - w.p0x;
        {
            long double len2 = (dx * dx + dyd * dyd) + dzd * dzd;
            len = __builtin_sqrtl(len2);
            hx = p1x;
            hy = p1y;
            hz = p1z;

            if (had) {
                long double cx = hit->pos.x - obj->pos.x, cy = hit->pos.y - obj->pos.y;
                float cz = hit->pos.z - obj->pos.z;
                float lx = (cy * m[3] + cz * m[6]) + cx * m[0];
                float ly = (cz * m[7] + cx * m[1]) + cy * m[4];
                float lz = (cy * m[5] + cz * m[8]) + cx * m[2];
                long double qz, ex, ey, ez;
                double exd;
                float d;

                hx = lx / obj->scale[0];
                hy = ly / obj->scale[0];
                qz = lz / obj->scale[0];
                hz = qz;
                ez = qz - w.p0z;
                ey = hy - w.p0y;
                ex = hx - w.p0x;
                exd = ex;
                d = __builtin_sqrtl((ex * exd + ey * ey) + ez * ez);
                best = d;
            }
            RigidLineWalk_SetBox(&w, hx, hy, hz);
            w.inv_len2 = kOne / len2;
        }
        w.dx = dx;
        w.dy = dy;
        w.dz = dz;

        if (!cm->node_count)
            continue;

        node = cm->nodes;
        stack[0] = node;
        for (;;) {
            int k;

            /* go down through the first children that pass */
            while (node->child) {
                node = &cm->nodes[node->child];
                stack[++depth] = node;
                if (!RigidLineWalk_NodeHit(&w, node, 1))
                    break;
            }

            /* the last node tried, even one that failed the sphere test */
            for (k = node->firstTri; k < node->firstTri + node->triCount; k++) {
                RigidCollTri *tri = &cm->tris[k];
                const RigidCollVert *v0 = &verts[tri->v[0]], *v1 = &verts[tri->v[1]], *v2 = &verts[tri->v[2]];
                float e1x, e1y, e1z, e2x, e2y, e2z, nx, ny, nz, nyn, ax2, ay2, az2, t, px, py, pz, fu, fv;
                long double e1zl, nyl, den, tl, a, d;
                double dend, b;
                char back;

                if (cm->faces[tri->face].unk4 != -1 && !q->allFaces)
                    continue;
                if (!(w.min_x <= v0->x || w.min_x <= v1->x || w.min_x <= v2->x))
                    continue;
                if (!(w.min_y <= v0->y || w.min_y <= v1->y || w.min_y <= v2->y))
                    continue;
                if (!(w.min_z <= v0->z || w.min_z <= v1->z || w.min_z <= v2->z))
                    continue;
                if (!(v0->x <= w.max_x || v1->x <= w.max_x || v2->x <= w.max_x))
                    continue;
                if (!(v0->y <= w.max_y || v1->y <= w.max_y || v2->y <= w.max_y))
                    continue;
                if (!(v0->z <= w.max_z || v1->z <= w.max_z || v2->z <= w.max_z))
                    continue;

                e2x = v2->x - v0->x;
                e2y = v2->y - v0->y;
                e2z = v2->z - v0->z;
                e1x = v1->x - v0->x;
                e1y = v1->y - v0->y;
                e1zl = v1->z - v0->z;
                e1z = e1zl;
                nx = e1zl * e2y - e1y * e2z;
                nyl = e1z * e2x - e1x * e2z;
                nyn = nyl;
                ny = -nyl;
                nz = e1y * e2x - e1x * e2y;
                den = (nz * w.dz + ny * w.dy) + nx * w.dx;
                dend = den;
                if (den == kZero)
                    continue;
                tl = (((v0->z - w.p0z) * nz + (v0->y - w.p0y) * ny) + (v0->x - w.p0x) * nx) / dend;
                t = tl;
                if (tl < kZeroF || t > kOneF)
                    continue;
                px = t * w.dx + w.p0x;
                py = t * w.dy + w.p0y;
                pz = t * w.dz + w.p0z;

                /* barycentric u (a) and v (b) in the plane the normal faces most */
                ax2 = nx < kZeroF ? -nx : nx;
                ay2 = ny < kZeroF ? -ny : ny;
                az2 = nz < kZeroF ? -nz : nz;
                if (ax2 > ay2 && ax2 > az2) {
                    float wz, wy;
                    if (nx == kZero)
                        continue;
                    wz = pz - v0->z;
                    wy = py - v0->y;
                    b = (wz * e2y - wy * e2z) / nx;
                    if (b < kZero || b > kOne)
                        continue;
                    if ((e2y < kZeroF ? -e2y : e2y) > (e2z < kZeroF ? -e2z : e2z)) {
                        a = (wy - e1y * b) / e2y;
                    } else {
                        if (e2z == kZeroF)
                            continue;
                        a = (wz - e1z * b) / e2z;
                    }
                } else if (ay2 > az2) {
                    float wz, wx;
                    if (nyn == kZero)
                        continue;
                    wz = pz - v0->z;
                    wx = px - v0->x;
                    b = (wz * e2x - wx * e2z) / nyn;
                    if (b < kZero || b > kOne)
                        continue;
                    if ((e2x < kZeroF ? -e2x : e2x) > (e2z < kZeroF ? -e2z : e2z)) {
                        a = (wx - e1x * b) / e2x;
                    } else {
                        if (e2z == kZeroF)
                            continue;
                        a = (wz - e1z * b) / e2z;
                    }
                } else {
                    float wy, wx;
                    if (nz == kZero)
                        continue;
                    wy = py - v0->y;
                    wx = px - v0->x;
                    b = (wy * e2x - wx * e2y) / nz;
                    if (b < kZero || b > kOne)
                        continue;
                    if ((e2x < kZeroF ? -e2x : e2x) > (e2y < kZeroF ? -e2y : e2y)) {
                        a = (wx - e1x * b) / e2x;
                    } else {
                        if (e2y == kZeroF)
                            continue;
                        a = (wy - e1y * b) / e2y;
                    }
                }
                if (a < kZero || a + b > kOne)
                    continue;
                fv = b;
                fu = a;
                if (Qvm_GetArgLine(lod, tri, fu, fv))
                    continue;

                /* nearest hit wins; within 0.1 of it the side decides */
                d = t * len;
                back = dend < kZero;
                if (!had) {
                    close = 0;
                } else if (d + kCloseRange < best) {
                    close = 0;
                } else if (d < best) {
                    close = 1;
                    if (!back && side)
                        continue;
                } else if (d - kCloseRange < best) {
                    close = 1;
                    if (!back || side)
                        continue;
                } else {
                    continue;
                }
                best = d;
                hx = px;
                hy = py;
                hz = pz;
                hu = fu;
                hv = fv;
                side = back;
                hit_tri = tri;
                found = 1;
                had = 1;
                RigidLineWalk_SetBox(&w, px, py, pz);
            }

            /* next sibling that passes, going back up when there is none */
            for (;;) {
                if (node->next) {
                    node = &cm->nodes[node->next];
                    stack[depth] = node;
                    if (RigidLineWalk_NodeHit(&w, node, 0))
                        break;
                } else {
                    if (--depth < 0)
                        goto walked;
                    node = stack[depth];
                }
            }
        }
    walked:
        if (!found)
            continue;
        if (obj->scale[0] != kOneF || obj->scale[1] != kOneF || obj->scale[2] != kOneF) {
            long double sx = hx * obj->scale[0], sy = hy * obj->scale[1], sz = hz * obj->scale[2];
            float s0x = w.p0x * obj->scale[0], s0y = w.p0y * obj->scale[1];
            double ddz = w.p0z * obj->scale[2] - sz;
            double ddy = s0y - sy;
            long double ddx = s0x - sx;
            best = __builtin_sqrtl((ddx * ddx + ddy * ddy) + ddz * ddz);
        }
        if (hit->found && !(best < hit->dist))
            continue;
        {
            const RigidCollVert *a = &verts[hit_tri->v[0]], *b = &verts[hit_tri->v[1]], *c = &verts[hit_tri->v[2]];
            uint16_t mi = cm->faces[hit_tri->face].material;
            void **mats = (void **)(obj->world + 0xD3C);
            void *mat = mi == 0xFFFF ? mats[1] : mats[mi];
            long double cx = c->x - a->x, cy = c->y - a->y, cz = c->z - a->z;
            float bx = b->x - a->x, by = b->y - a->y;
            long double bz = b->z - a->z;
            float nx = bz * cy - by * cz;
            float ny = -(bz * cx - bx * cz);
            float nz = by * cx - bx * cy;
            float wx, wy;
            long double sx, sy, sz, wz;

            hit->normal.x = nx;
            hit->normal.y = ny;
            hit->normal.z = nz;
            hit->localNormal.x = nx;
            hit->localNormal.y = ny;
            hit->localNormal.z = nz;
            hit->normal.x = (m[2] * nz + m[1] * ny) + m[0] * nx;
            hit->normal.y = (nx * m[3] + m[5] * nz) + m[4] * ny;
            hit->normal.z = (m[8] * nz + m[7] * ny) + m[6] * nx;
            hit->local.x = hx;
            hit->local.y = hy;
            hit->local.z = hz;
            sx = hx * obj->scale[0];
            sy = hy * obj->scale[1];
            sz = hz * obj->scale[2];
            wx = (sy * m[1] + sz * m[2]) + sx * m[0];
            wy = (sz * m[5] + sx * m[3]) + sy * m[4];
            wz = (sy * m[7] + sz * m[8]) + sx * m[6];
            hit->pos.x = wx + obj->pos.x;
            hit->pos.y = wy + obj->pos.y;
            hit->pos.z = wz + obj->pos.z;
            hit->back = side ? NULL : mat;
            hit->material = side ? mat : NULL;
            hit->dist = best;
            hit->task = &obj->task;
            hit->close = close;
            hit->found = 1;
            hit->tri = hit_tri - cm->tris;
            hit->u = hu;
            hit->v = hv;
        }
    }
}
HOOK(0x4C1840, RigidDynCubeObj_LineHit);
