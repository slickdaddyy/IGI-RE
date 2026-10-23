/* Line collision against the skinned collision mesh of a BoneDynCubeObj (task slot 9). */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "igi.h"
#include "hook.h"
#include "haze/haze.h"

#define BONE_COLL_MAX 4096

typedef struct BoneCollVert {
    float x, y, z;
    int bone;
} BoneCollVert;

typedef struct BoneCollTri {
    uint16_t v[3];
    uint16_t face;
} BoneCollTri;

typedef struct BoneCollFace {
    int unk0;
    int16_t unk4;
    int16_t unk6;
    uint16_t material;
    uint16_t unkA;
} BoneCollFace;

typedef struct BoneCollMesh {
    int tri_count;
    int vert_count;
    int unk8;
    int unkC;
    BoneCollTri *tris;
    BoneCollVert *verts;
    BoneCollFace *faces;
    void *unk1C;
} BoneCollMesh;

typedef struct BoneModelData {
    char unk0[0x28];
    int unk28;
    BoneCollMesh *coll;
} BoneModelData;

typedef struct BoneModelPart {
    char unk0[0xC];
    BoneModelData *data;
} BoneModelPart;

typedef struct BoneLod {
    char unk0[0xC];
    BoneModelPart *part;
} BoneLod;

typedef struct BoneLineHit {
    void *obj;
    int unk4;
    Vec3d pos;
    float local[3];
    float normal[3];
    float local_normal[3];
    int bone;
    int tri;
    void *material;  /* the hit came from behind the triangle */
    void *material2; /* from the front */
    int close;       /* within 0.1 of an earlier hit */
    int found;
    int unk5C;
    double dist;
    float unk68[2];
} BoneLineHit;

typedef struct BoneLineQuery {
    const Vec3d *starts;
    const Vec3d *ends;
    int count;
    uint8_t slot; /* 0xFF: none, else a handler that must set *slot_arg */
    char unkD[3];
    char *slot_arg;
    int mesh;
    int unk18;
    int unk1C;
    BoneLineHit *results;
} BoneLineQuery;

_Static_assert(sizeof(BoneCollMesh) == 0x20, "BoneCollMesh");
_Static_assert(offsetof(BoneLineHit, normal) == 0x2C, "BoneLineHit.normal");
_Static_assert(offsetof(BoneLineHit, bone) == 0x44, "BoneLineHit.bone");
_Static_assert(offsetof(BoneLineHit, found) == 0x58, "BoneLineHit.found");
_Static_assert(sizeof(BoneLineHit) == 0x70, "BoneLineHit");
_Static_assert(offsetof(BoneLineQuery, results) == 0x20, "BoneLineQuery");

ORIG(0x4BD070, int, sub_4BD070, (BoneLod *lod, BoneCollTri *tri, float u, float v));

static const float kBoxSlack = 0x1.99999ap-4f;   /* 0.1f */
static const double kCloseRange = 0x1.999999999999ap-4; /* 0.1 */
static const float kZeroF = 0.0f, kOneF = 1.0f;
static const double kZero = 0.0, kOne = 1.0;

/* the segment box, grown by 0.1 */
typedef struct BoneBox {
    double min_x, min_y, min_z;
    double max_x, max_y, max_z;
} BoneBox;

static void BoneBox_Set(BoneBox *b, const float *a, const float *p)
{
    b->min_x = (!(a[0] >= p[0]) ? a[0] : p[0]) - kBoxSlack;
    b->min_y = (!(a[1] >= p[1]) ? a[1] : p[1]) - kBoxSlack;
    b->min_z = (!(a[2] >= p[2]) ? a[2] : p[2]) - kBoxSlack;
    b->max_x = (a[0] > p[0] ? a[0] : p[0]) + kBoxSlack;
    b->max_y = (a[1] > p[1] ? a[1] : p[1]) + kBoxSlack;
    b->max_z = (a[2] > p[2] ? a[2] : p[2]) + kBoxSlack;
}

/* equal to zero or NaN */
#define NOT_NONZERO(x) (!((x) < kZero) && !((x) > kZero))

void BoneDynCubeObj_LineHit(BoneDynCubeObj *obj, BoneLineQuery *q)
{
    const Vec3d *start = q->starts, *end = q->ends;
    BoneLineHit *hit = q->results;
    int bone_of[BONE_COLL_MAX];
    float sk[BONE_COLL_MAX][3];
    float bm[9], bp[3];
    BoneCollMesh *cm;
    BoneLod *lod;
    BoneModelData *data;
    char close = 0;
    int i, bone = -1;

    if (!obj->model)
        return;
    lod = obj->model[0];
    data = lod->part->data;
    if (data->unk28)
        return;
    cm = &data->coll[q->mesh];
    if (!cm || !cm->tri_count)
        return;
    if (q->slot != 0xFF) {
        HzHandler fn = gTaskHandlers[q->slot][obj->task.type];
        if (fn) {
            fn(obj, q->slot_arg);
            if (!*q->slot_arg)
                return;
        }
    }

    /* move every vertex by its bone */
    for (i = 0; i < cm->vert_count; i++) {
        const BoneCollVert *v = &cm->verts[i];
        float x, y, z;

        if (v->bone != bone) {
            const Bone *b = &obj->bones.bones[v->bone];
            bone = v->bone;
            memcpy(bm, &b->matrix, sizeof(bm));
            bp[0] = b->pos.x;
            bp[1] = b->pos.y;
            bp[2] = b->pos.z;
        }
        x = (bm[2] * v->z + bm[1] * v->y) + bm[0] * v->x;
        y = (bm[5] * v->z + bm[4] * v->y) + bm[3] * v->x;
        z = (bm[8] * v->z + bm[7] * v->y) + bm[6] * v->x;
        sk[i][0] = bp[0] + x;
        sk[i][1] = bp[1] + y;
        sk[i][2] = bp[2] + z;
        bone_of[i] = bone;
    }

    for (i = 0; i < q->count; i++, start++, end++, hit++) {
        const float *m = (const float *)&obj->matrix;
        BoneCollTri *hit_tri = NULL;
        BoneBox box;
        float a[3], b[3], d[3], h[3];
        float r, len, dot;
        long double c2, d2, x2;
        double best = 0;
        char side = hit->found ? hit->material != NULL : 0;
        char had = (char)hit->found;
        char found = 0;
        int k;

        a[0] = start->x - obj->pos.x;
        a[1] = start->y - obj->pos.y;
        a[2] = start->z - obj->pos.z;
        b[0] = end->x - obj->pos.x;
        b[1] = end->y - obj->pos.y;
        b[2] = end->z - obj->pos.z;
        BoneBox_Set(&box, a, b);
        d[0] = end->x - start->x;
        d[1] = end->y - start->y;
        d[2] = end->z - start->z;
        r = obj->radius;

        /* the bounding sphere against the line, then against the box */
        dot = ((-a[2]) * d[2] + (-a[1]) * d[1]) + (-a[0]) * d[0];
        c2 = (a[0] * a[0] + a[1] * a[1]) + a[2] * a[2];
        d2 = (d[0] * d[0] + d[1] * d[1]) + d[2] * d[2];
        x2 = c2 - (dot * dot) * (kOneF / d2);
        if (!(r * r >= x2))
            continue;
        if (!(r >= box.min_x) || !(r >= box.min_y) || !(r >= box.min_z))
            continue;
        if (-r > box.max_x || -r > box.max_y || -r > box.max_z)
            continue;

        {
            long double ex = b[0] - a[0], ey = b[1] - a[1], ez = b[2] - a[2];
            len = __builtin_sqrtl((ex * ex + ey * ey) + ez * ez);
        }
        h[0] = b[0];
        h[1] = b[1];
        h[2] = b[2];
        if (had) {
            long double hz, ex, ey, ez;

            h[0] = hit->pos.x - obj->pos.x;
            h[1] = hit->pos.y - obj->pos.y;
            hz = hit->pos.z - obj->pos.z;
            h[2] = hz;
            ez = hz - a[2];
            ey = h[1] - a[1];
            ex = h[0] - a[0];
            best = __builtin_sqrtl((ex * ex + ey * ey) + ez * ez);
        }

        for (k = 0; k < cm->tri_count; k++) {
            BoneCollTri *tri = &cm->tris[k];
            const float *v0 = sk[tri->v[0]], *v1 = sk[tri->v[1]], *v2 = sk[tri->v[2]];
            float e1x, e1y, e1z, e2x, e2y, e2z, nx, ny, nz, nyn, ax2, ay2, az2, t, p[3];
            long double e1zl, nyl, den, tl, u, dist;
            double dend, w;
            char back;

            if (!(v0[0] >= box.min_x) && !(v1[0] >= box.min_x) && !(v2[0] >= box.min_x))
                continue;
            if (!(v0[1] >= box.min_y) && !(v1[1] >= box.min_y) && !(v2[1] >= box.min_y))
                continue;
            if (!(v0[2] >= box.min_z) && !(v1[2] >= box.min_z) && !(v2[2] >= box.min_z))
                continue;
            if (v0[0] > box.max_x && v1[0] > box.max_x && v2[0] > box.max_x)
                continue;
            if (v0[1] > box.max_y && v1[1] > box.max_y && v2[1] > box.max_y)
                continue;
            if (v0[2] > box.max_z && v1[2] > box.max_z && v2[2] > box.max_z)
                continue;

            e2x = v2[0] - v0[0];
            e2y = v2[1] - v0[1];
            e2z = v2[2] - v0[2];
            e1x = v1[0] - v0[0];
            e1y = v1[1] - v0[1];
            e1zl = v1[2] - v0[2];
            e1z = e1zl;
            nx = e1zl * e2y - e1y * e2z;
            nyl = e1z * e2x - e1x * e2z;
            nyn = nyl;
            ny = -nyl;
            nz = e1y * e2x - e1x * e2y;
            den = (nz * d[2] + ny * d[1]) + nx * d[0];
            dend = den;
            if (NOT_NONZERO(den))
                continue;
            tl = (((v0[1] - a[1]) * ny + (v0[2] - a[2]) * nz) + (v0[0] - a[0]) * nx) / dend;
            t = tl;
            if (!(tl >= kZeroF) || t > kOneF)
                continue;
            p[0] = t * d[0] + a[0];
            p[1] = t * d[1] + a[1];
            p[2] = t * d[2] + a[2];

            /* barycentric u and w in the plane the normal faces most */
            ax2 = nx < kZeroF ? -nx : nx;
            ay2 = ny < kZeroF ? -ny : ny;
            az2 = nz < kZeroF ? -nz : nz;
            if (ax2 > ay2 && ax2 > az2) {
                float wz, wy;
                if (NOT_NONZERO(nx))
                    continue;
                wz = p[2] - v0[2];
                wy = p[1] - v0[1];
                w = (wz * e2y - wy * e2z) / nx;
                if (!(w >= kZero) || w > kOne)
                    continue;
                if ((e2y < kZeroF ? -e2y : e2y) > (e2z < kZeroF ? -e2z : e2z)) {
                    u = (wy - e1y * w) / e2y;
                } else {
                    if (NOT_NONZERO(e2z))
                        continue;
                    u = (wz - e1z * w) / e2z;
                }
            } else if (ay2 > az2) {
                float wz, wx;
                if (NOT_NONZERO(nyn))
                    continue;
                wz = p[2] - v0[2];
                wx = p[0] - v0[0];
                w = (wz * e2x - wx * e2z) / nyn;
                if (!(w >= kZero) || w > kOne)
                    continue;
                if ((e2x < kZeroF ? -e2x : e2x) > (e2z < kZeroF ? -e2z : e2z)) {
                    u = (wx - e1x * w) / e2x;
                } else {
                    if (NOT_NONZERO(e2z))
                        continue;
                    u = (wz - e1z * w) / e2z;
                }
            } else {
                float wy, wx;
                if (NOT_NONZERO(nz))
                    continue;
                wy = p[1] - v0[1];
                wx = p[0] - v0[0];
                w = (wy * e2x - wx * e2y) / nz;
                if (!(w >= kZero) || w > kOne)
                    continue;
                if ((e2x < kZeroF ? -e2x : e2x) > (e2y < kZeroF ? -e2y : e2y)) {
                    u = (wx - e1x * w) / e2x;
                } else {
                    if (NOT_NONZERO(e2y))
                        continue;
                    u = (wy - e1y * w) / e2y;
                }
            }
            if (!(u >= kZero) || u + w > kOne)
                continue;
            if (sub_4BD070(lod, tri, (float)u, (float)w))
                continue;

            /* nearest hit wins; within 0.1 of it the side decides */
            dist = t * len;
            back = !(dend >= kZero);
            if (!had) {
                close = 0;
            } else if (!(dist + kCloseRange >= best)) {
                close = 0;
            } else if (!(dist >= best)) {
                close = 1;
                if (!back && side)
                    continue;
            } else if (!(dist - kCloseRange >= best)) {
                close = 1;
                if (!back || side)
                    continue;
            } else {
                continue;
            }
            best = dist;
            side = back;
            found = 1;
            had = 1;
            hit_tri = tri;
            h[0] = p[0];
            h[1] = p[1];
            h[2] = p[2];
            BoneBox_Set(&box, a, p);
        }

        if (!found)
            continue;
        if (hit->found && best >= hit->dist)
            continue;
        {
            const float *va = sk[hit_tri->v[0]], *vb = sk[hit_tri->v[1]], *vc = sk[hit_tri->v[2]];
            uint16_t mi = cm->faces[hit_tri->face].material;
            void *mat = mi == 0xFFFF ? *(void **)((char *)gWorld + 0xD40)
                                     : ((void **)((char *)gWorld + 0xD3C))[mi];
            long double cx = vc[0] - va[0], cy = vc[1] - va[1], cz = vc[2] - va[2];
            float bx = vb[0] - va[0], by = vb[1] - va[1];
            long double bz = vb[2] - va[2];
            float nx = bz * cy - by * cz;
            float ny = -(bz * cx - bx * cz);
            float nz = by * cx - bx * cy;
            int b0, b1;

            hit->local_normal[0] = nx;
            hit->local_normal[1] = ny;
            hit->local_normal[2] = nz;
            hit->pos.x = h[0] + obj->pos.x;
            hit->pos.y = h[1] + obj->pos.y;
            hit->pos.z = h[2] + obj->pos.z;
            hit->local[0] = h[0];
            hit->local[1] = h[1];
            hit->local[2] = h[2];
            hit->normal[0] = (m[3] * ny + m[6] * nz) + m[0] * nx;
            hit->normal[1] = (m[4] * ny + m[7] * nz) + m[1] * nx;
            hit->normal[2] = (m[5] * ny + m[8] * nz) + m[2] * nx;
            hit->obj = obj;
            hit->found = 1;
            hit->material = side ? mat : NULL;
            hit->material2 = side ? NULL : mat;
            hit->close = close;
            hit->dist = best;
            hit->tri = hit_tri - cm->tris;
            b0 = bone_of[hit_tri->v[0]];
            b1 = bone_of[hit_tri->v[1]];
            hit->bone = b1 != b0 && b1 == bone_of[hit_tri->v[2]] ? b1 : b0;
        }
    }
}
HOOK(0x4D7620, BoneDynCubeObj_LineHit);
