/* Finds the vertices of a lightmapped object that a Dirlight reaches, by
   casting rays along the light through a grid in front of the object. */
#pragma GCC optimize("excess-precision=standard")
#pragma GCC optimize("rounding-math")

#include "hook.h"
#include "dirlight/dirlight.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B4010, void, Matrix_SetDirection, (Matrix *m, const Vec3f *dir));
ORIG(0x4CE410, char, Coll_LineOfSight, (void *world, const Vec3d *from, const Vec3d *to, int event, int unk,
                                        int filter, int flags, float *hit));
ORIG(0x521E40, void, Hemicube_DrawLightmapSet, (LightmapInfo *info, const Vec3d *origin, const Matrix *m, int *ids, float *depth,
                                  int w, int h, float centerX, float centerY, int pass, int unk1, int unk2, int unk3,
                                  float radius, int unk4));

extern void *gCurrentWorld;

static const float kOne = 1.0f;
static const float kHalf = 0.5f;
static const double kOneD = 1.0;
static const float kDepthBias = 40.96f;
static const float kTenth = 0.1f;

#define OBJ_A4(t) (*(int *)((char *)(t) + 0xA4))

void Dirlight_TraceVertices(Dirlight *dl, TraceArg *arg)
{
    LightmapInfo *info = arg->info;
    int *ids = arg->ids;
    float *weights = arg->weights;
    int size = info->unkA0;
    float sizeF = size;
    VertexLight *out = &arg->lights->lights[*arg->index];
    float scale = kOne / ((long double)size * sizeF);
    float zero[3] = { 0, 0, 0 };
    int *saved, *idbuf;
    float *depth, *M, half, radius, r1, r2, ty, tz, uy, uz, ny, nz;
    long double r0, k, P, Q, S;
    Vec3f v, a, b, c, center;
    Vec3d O, O2, cur;
    LightmapSet *set;
    Matrix m;
    int found, pass, scan, row, col, i, j, n;

    saved = Mem_Alloc(info->set->count * 4, 4);
    for (i = 0; i < info->set->count; i++) {
        Task *obj = (Task *)info->set->refs[i].obj;

        saved[i] = OBJ_A4(obj);
        OBJ_A4(obj) = -1;
    }
    idbuf = Mem_Alloc(size * size * 4, 4);
    depth = Mem_Alloc(size * size * 4, 4);
    half = sizeF * kHalf - kHalf;

    found = 0;
    v = dl->dir;
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
    set = info->set;
    center = set->center;
    M = set->refs[0].matrix->m;
    r0 = center.z * M[2] + center.y * M[1] + center.x * M[0];
    r1 = center.z * M[5] + center.y * M[4] + center.x * M[3];
    r2 = center.z * M[8] + center.y * M[7] + center.x * M[6];
    O.x = r0 + set->refs[0].pos->x;
    O.y = r1 + set->refs[0].pos->y;
    O.z = r2 + set->refs[0].pos->z;
    radius = set->radius;
    Matrix_SetDirection(&m, &v);
    k = radius / __builtin_sqrtl(v.x * v.x + v.y * v.y + v.z * v.z);
    O2 = O;
    v.x = v.x * k;
    v.y = v.y * k;
    v.z = v.z * k;
    O.x = O.x - v.x;
    O.y = O.y - v.y;
    O.z = O.z - v.z;
    P = O2.x - v.x;
    O2.y = O2.y - v.y;
    O2.z = O2.z - v.z;
    if (!(feq(v.x, 0) && feq(v.y, 0) && feq(v.z, 0))) {
        k = kOneD / __builtin_sqrtl(v.x * v.x + v.y * v.y + v.z * v.z);
        v.x = v.x * k;
        v.y = v.y * k;
        v.z = v.z * k;
    }

    /* a and b step one texel along a row and a column of the grid */
    S = (radius + radius) / sizeF;
    a.x = m.m[0] * S + zero[0];
    a.y = m.m[1] * S + zero[1];
    a.z = m.m[2] * S + zero[2];
    b.x = m.m[3] * S + zero[0];
    b.y = m.m[4] * S + zero[1];
    b.z = m.m[5] * S + zero[2];
    Q = P + -(half * a.x);
    ty = -(half * a.y);
    tz = -(half * a.z);
    O2.y = ty + O2.y;
    O2.z = tz + O2.z;
    uy = -(half * b.y);
    uz = -(half * b.z);
    O2.x = -(half * b.x) + Q;
    O2.y = uy + O2.y;
    O2.z = uz + O2.z;
    c.x = -(a.x * sizeF) + b.x;
    ny = -(a.y * sizeF);
    nz = -(a.z * sizeF);
    c.y = ny + b.y;
    c.z = nz + b.z;

    for (pass = 0; pass < 5; pass++) {
        cur = O2;
        for (scan = 0; scan < 2; scan++) {
            float *p = depth;

            Hemicube_DrawLightmapSet(info, &O, &m, idbuf, depth, size, size, half, half, pass, 1, scan, !scan, radius, 0);
            for (row = 0; row < size; row++) {
                for (col = 0; col < size; col++, p++) {
                    int id = idbuf[p - depth];

                    if (id != -1) {
                        long double d = *p - kDepthBias;
                        float dx = d * v.x;
                        float dy = v.y * d;
                        Vec3d to = cur;
                        float hit;

                        to.x = dx + cur.x;
                        to.y = dy + to.y;
                        to.z = d * v.z + to.z;
                        Coll_LineOfSight(gCurrentWorld, &cur, &to, 0xFF, 0, -1, 9, &hit);
                        if (hit > 0) {
                            long double w = hit * scale;

                            for (j = found - 1; j >= 0; j--)
                                if (ids[j] == id)
                                    break;
                            if (j >= 0) {
                                weights[j] = w + weights[j];
                            } else {
                                ids[found] = id;
                                weights[found] = w;
                                found++;
                            }
                        }
                    }
                    cur.x = a.x + cur.x;
                    cur.y = a.y + cur.y;
                    cur.z = a.z + cur.z;
                }
                cur.x = c.x + cur.x;
                cur.y = c.y + cur.y;
                cur.z = c.z + cur.z;
            }
        }
    }

    /* keep the brighter of the traced and the facing-angle light */
    for (j = 0; j < found; j++) {
        float *area = arg->areas[ids[j]];
        float *nrm = arg->normals[ids[j]];

        if (*area > 0) {
            float *R = info->set->refs[0].matrix->m;
            long double R0 = R[2] * nrm[2] + R[1] * nrm[1] + nrm[0] * R[0];
            long double R1 = R[3] * nrm[0] + R[5] * nrm[2] + R[4] * nrm[1];
            long double R2 = R[6] * nrm[0] + R[8] * nrm[2] + R[7] * nrm[1];
            long double d2 = arg->info->set->radius + arg->info->set->radius;
            float g = *area / (d2 * d2);
            long double L = (R2 * dl->dir.z + R1 * dl->dir.y + R0 * dl->dir.x) * g;

            if (L >= 0 && !((L - weights[j]) / g >= kTenth))
                weights[j] = L;
        }
    }
    if (!found) {
        out->verts = NULL;
        out->weights = NULL;
    } else {
        out->verts = Mem_Alloc(found * 4, 4);
        out->weights = Mem_Alloc(found * 4, 4);
        memcpy(out->verts, ids, found * 4);
        memcpy(out->weights, weights, found * 4);
    }
    out->count = found;
    (*arg->index)++;

    Mem_Free(depth);
    Mem_Free(idbuf);
    n = info->set->count;
    for (i = n - 1; i >= 0; i--)
        OBJ_A4(info->set->refs[i].obj) = saved[i];
    Mem_Free(saved);
}
HOOK(0x525460, Dirlight_TraceVertices);
