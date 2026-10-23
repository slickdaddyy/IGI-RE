/* Segment tests against the skinned collision mesh of a BoneDynCubeObj (task slots 8 and 9). */
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

/* event 8 query, see Coll_LineOfSight */
typedef struct BoneSightQuery {
    const Vec3d *from;
    const Vec3d *to;
    uint8_t slot; /* 0xFF: none, else a handler that clears *slot_arg to skip us */
    char unk9[3];
    char *slot_arg;
    char *world;  /* +0xD3C: float * per material, how much it lets through */
    char blocked;
    char unk15[3];
    float visibility;
} BoneSightQuery;

_Static_assert(sizeof(BoneCollMesh) == 0x20, "BoneCollMesh");
_Static_assert(offsetof(BoneLineHit, normal) == 0x2C, "BoneLineHit.normal");
_Static_assert(offsetof(BoneLineHit, bone) == 0x44, "BoneLineHit.bone");
_Static_assert(offsetof(BoneLineHit, found) == 0x58, "BoneLineHit.found");
_Static_assert(sizeof(BoneLineHit) == 0x70, "BoneLineHit");
_Static_assert(offsetof(BoneLineQuery, results) == 0x20, "BoneLineQuery");
_Static_assert(offsetof(BoneSightQuery, visibility) == 0x18, "BoneSightQuery");

ORIG(0x4BD070, int, sub_4BD070, (BoneLod *lod, BoneCollTri *tri, float u, float v));

static const float kBoxSlack = 0x1.99999ap-4f;   /* 0.1f */
static const double kCloseRange = 0x1.999999999999ap-4; /* 0.1 */
static const float kZeroF = 0.0f, kOneF = 1.0f;
static const double kZero = 0.0, kOne = 1.0;

/* equal to zero or NaN */
#define NOT_NONZERO(x) (!((x) < kZero) && !((x) > kZero))

/* the segment box, grown by 0.1 */
typedef struct BoneBox {
    double min_x, min_y, min_z;
    double max_x, max_y, max_z;
} BoneBox;

static void BoneBox_Set(BoneBox *b, const float *a, long double px, long double py, long double pz)
{
    b->min_x = (!(a[0] >= px) ? a[0] : px) - kBoxSlack;
    b->min_y = (!(a[1] >= py) ? a[1] : py) - kBoxSlack;
    b->min_z = (!(a[2] >= pz) ? a[2] : pz) - kBoxSlack;
    b->max_x = (a[0] > px ? a[0] : px) + kBoxSlack;
    b->max_y = (a[1] > py ? a[1] : py) + kBoxSlack;
    b->max_z = (a[2] > pz ? a[2] : pz) + kBoxSlack;
}

/* the triangle lies wholly outside the box */
static int BoneBox_Outside(const BoneBox *box, const float *v0, const float *v1, const float *v2)
{
    if (!(v0[0] >= box->min_x) && !(v1[0] >= box->min_x) && !(v2[0] >= box->min_x))
        return 1;
    if (!(v0[1] >= box->min_y) && !(v1[1] >= box->min_y) && !(v2[1] >= box->min_y))
        return 1;
    if (!(v0[2] >= box->min_z) && !(v1[2] >= box->min_z) && !(v2[2] >= box->min_z))
        return 1;
    if (v0[0] > box->max_x && v1[0] > box->max_x && v2[0] > box->max_x)
        return 1;
    if (v0[1] > box->max_y && v1[1] > box->max_y && v2[1] > box->max_y)
        return 1;
    if (v0[2] > box->max_z && v1[2] > box->max_z && v2[2] > box->max_z)
        return 1;
    return 0;
}

/* the object's bounding sphere (at the origin) misses the line or the box */
static int BoneColl_SphereMiss(const float *a, const float *d, float r, const BoneBox *box)
{
    float dot = ((-a[2]) * d[2] + (-a[1]) * d[1]) + (-a[0]) * d[0];
    long double c2 = (a[0] * a[0] + a[1] * a[1]) + a[2] * a[2];
    long double d2 = (d[0] * d[0] + d[1] * d[1]) + d[2] * d[2];
    long double x2 = c2 - (dot * dot) * (kOneF / d2);

    if (!(r * r >= x2))
        return 1;
    if (!(r >= box->min_x) || !(r >= box->min_y) || !(r >= box->min_z))
        return 1;
    if (-r > box->max_x || -r > box->max_y || -r > box->max_z)
        return 1;
    return 0;
}

/* moves every vertex by its bone */
static void BoneColl_Skin(BoneDynCubeObj *obj, const BoneCollMesh *cm, float (*sk)[3], int *bone_of)
{
    float bm[9], bp[3];
    int i, bone = -1;

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
        if (bone_of)
            bone_of[i] = bone;
    }
}

/* barycentric u and w of p in the plane the normal faces most; 0 when outside */
static int BoneColl_Bary(const float *p, const float *v0, const float *e1, const float *e2, float nx, float ny,
                         float nyl, float nz, long double *pu, double *pw)
{
    float ax = nx < kZeroF ? -nx : nx;
    float ay = ny < kZeroF ? -ny : ny;
    float az = nz < kZeroF ? -nz : nz;
    long double u;
    double w;

    if (ax > ay && ax > az) {
        float wz, wy;
        if (NOT_NONZERO(nx))
            return 0;
        wz = p[2] - v0[2];
        wy = p[1] - v0[1];
        w = (wz * e2[1] - wy * e2[2]) / nx;
        if (!(w >= kZero) || w > kOne)
            return 0;
        if ((e2[1] < kZeroF ? -e2[1] : e2[1]) > (e2[2] < kZeroF ? -e2[2] : e2[2])) {
            u = (wy - e1[1] * w) / e2[1];
        } else {
            if (NOT_NONZERO(e2[2]))
                return 0;
            u = (wz - e1[2] * w) / e2[2];
        }
    } else if (ay > az) {
        float wz, wx;
        if (NOT_NONZERO(nyl))
            return 0;
        wz = p[2] - v0[2];
        wx = p[0] - v0[0];
        w = (wz * e2[0] - wx * e2[2]) / nyl;
        if (!(w >= kZero) || w > kOne)
            return 0;
        if ((e2[0] < kZeroF ? -e2[0] : e2[0]) > (e2[2] < kZeroF ? -e2[2] : e2[2])) {
            u = (wx - e1[0] * w) / e2[0];
        } else {
            if (NOT_NONZERO(e2[2]))
                return 0;
            u = (wz - e1[2] * w) / e2[2];
        }
    } else {
        float wy, wx;
        if (NOT_NONZERO(nz))
            return 0;
        wy = p[1] - v0[1];
        wx = p[0] - v0[0];
        w = (wy * e2[0] - wx * e2[1]) / nz;
        if (!(w >= kZero) || w > kOne)
            return 0;
        if ((e2[0] < kZeroF ? -e2[0] : e2[0]) > (e2[1] < kZeroF ? -e2[1] : e2[1])) {
            u = (wx - e1[0] * w) / e2[0];
        } else {
            if (NOT_NONZERO(e2[1]))
                return 0;
            u = (wy - e1[1] * w) / e2[1];
        }
    }
    if (!(u >= kZero) || u + w > kOne)
        return 0;
    *pu = u;
    *pw = w;
    return 1;
}

/* both edges from v0 and the face normal; ny comes negated, nyl is it before */
static void BoneColl_Normal(const float *v0, const float *v1, const float *v2, float *e1, float *e2, float *nx,
                            float *ny, float *nyl, float *nz)
{
    long double e1z, y;

    e2[0] = v2[0] - v0[0];
    e2[1] = v2[1] - v0[1];
    e2[2] = v2[2] - v0[2];
    e1[0] = v1[0] - v0[0];
    e1[1] = v1[1] - v0[1];
    e1z = v1[2] - v0[2];
    e1[2] = e1z;
    *nx = e1z * e2[1] - e1[1] * e2[2];
    y = e1[2] * e2[0] - e1[0] * e2[2];
    *nyl = y;
    *ny = -y;
    *nz = e1[1] * e2[0] - e1[0] * e2[1];
}

/* slot 8: lowers q->visibility for every mesh triangle the segment crosses */
void BoneDynCubeObj_CollLine(BoneDynCubeObj *obj, BoneSightQuery *q)
{
    const Vec3d *from = q->from, *to = q->to;
    float sk[BONE_COLL_MAX][3];
    BoneCollMesh *cm;
    BoneLod *lod;
    BoneModelData *data;
    BoneBox box;
    float a[3], d[3];
    long double bx;
    float by, bz;
    int k;

    if (!obj->model)
        return;
    lod = obj->model[0];
    data = lod->part->data;
    if (data->unk28)
        return;
    cm = data->coll;
    if (!cm)
        return;
    if (q->slot != 0xFF) {
        HzHandler fn = gTaskHandlers[q->slot][obj->task.type];
        if (fn) {
            fn(obj, q->slot_arg);
            if (!*q->slot_arg)
                return;
        }
    }
    BoneColl_Skin(obj, cm, sk, NULL);

    a[0] = from->x - obj->pos.x;
    a[1] = from->y - obj->pos.y;
    a[2] = from->z - obj->pos.z;
    bx = to->x - obj->pos.x;
    by = to->y - obj->pos.y;
    bz = to->z - obj->pos.z;
    BoneBox_Set(&box, a, bx, by, bz);
    d[0] = to->x - from->x;
    d[1] = to->y - from->y;
    d[2] = to->z - from->z;
    if (BoneColl_SphereMiss(a, d, obj->radius, &box))
        return;

    for (k = 0; k < cm->tri_count; k++) {
        BoneCollTri *tri = &cm->tris[k];
        const float *v0 = sk[tri->v[0]], *v1 = sk[tri->v[1]], *v2 = sk[tri->v[2]];
        float e1[3], e2[3], nx, ny, nyl, nz, denf, t, p[3];
        long double den, tl, u;
        double w;
        uint16_t mi;
        float *mat;

        if (BoneBox_Outside(&box, v0, v1, v2))
            continue;
        BoneColl_Normal(v0, v1, v2, e1, e2, &nx, &ny, &nyl, &nz);
        den = (nz * d[2] + ny * d[1]) + nx * d[0];
        denf = den;
        if (NOT_NONZERO(den))
            continue;
        tl = (((v0[1] - a[1]) * ny + (v0[0] - a[0]) * nx) + (v0[2] - a[2]) * nz) / den;
        t = tl;
        if (!(tl >= kZeroF) || t > kOneF)
            continue;
        p[0] = t * d[0] + a[0];
        p[1] = t * d[1] + a[1];
        p[2] = t * d[2] + a[2];
        if (!BoneColl_Bary(p, v0, e1, e2, nx, ny, nyl, nz, &u, &w))
            continue;
        if (sub_4BD070(lod, tri, (float)u, (float)w))
            continue;

        q->blocked = 1;
        mi = cm->faces[tri->face].material;
        if (mi == 0xFFFF) {
            q->visibility = 0.0f;
            return;
        }
        mat = ((float **)(q->world + 0xD3C))[mi];
        if (!(denf >= kZeroF))
            q->visibility = *mat * q->visibility;
        if (NOT_NONZERO(*mat)) {
            q->visibility = 0.0f;
            return;
        }
    }
}
HOOK(0x4D88D0, BoneDynCubeObj_CollLine);

/* slot 9: the nearest mesh hit along each segment */
void BoneDynCubeObj_LineHit(BoneDynCubeObj *obj, BoneLineQuery *q)
{
    const Vec3d *start = q->starts, *end = q->ends;
    BoneLineHit *hit = q->results;
    int bone_of[BONE_COLL_MAX];
    float sk[BONE_COLL_MAX][3];
    BoneCollMesh *cm;
    BoneLod *lod;
    BoneModelData *data;
    char close = 0;
    int i;

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
    BoneColl_Skin(obj, cm, sk, bone_of);

    for (i = 0; i < q->count; i++, start++, end++, hit++) {
        const float *m = (const float *)&obj->matrix;
        BoneCollTri *hit_tri = NULL;
        BoneBox box;
        float a[3], b[3], d[3], h[3];
        float len;
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
        BoneBox_Set(&box, a, b[0], b[1], b[2]);
        d[0] = end->x - start->x;
        d[1] = end->y - start->y;
        d[2] = end->z - start->z;
        if (BoneColl_SphereMiss(a, d, obj->radius, &box))
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
            float e1[3], e2[3], nx, ny, nyl, nz, t, p[3];
            long double den, tl, u, dist;
            double dend, w;
            char back;

            if (BoneBox_Outside(&box, v0, v1, v2))
                continue;
            BoneColl_Normal(v0, v1, v2, e1, e2, &nx, &ny, &nyl, &nz);
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
            if (!BoneColl_Bary(p, v0, e1, e2, nx, ny, nyl, nz, &u, &w))
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
            BoneBox_Set(&box, a, p[0], p[1], p[2]);
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
