/* Draws the meshes of a lightmap set into a hemicube face, or into an
   orthographic view along a light, writing texel ids and depths. */
#pragma GCC optimize("excess-precision=standard")

#include <float.h>

#include "igi.h"
#include "hook.h"

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

/* 3x3 rotation, row major, and how many products it went through */
typedef struct Matrix {
    float m[9];
    int count;
} Matrix;

typedef struct LightmapTex {
    char texture[0x24];
    int *pixels;
    short width;
    short height;
} LightmapTex;

typedef struct LightmapTexInfo {
    char unk0[0x24];
    int *texelIds;
} LightmapTexInfo;

typedef struct Lightmaps {
    char unk0[0x38];
    LightmapTexInfo *info;
    LightmapTex tex[];
} Lightmaps;

typedef struct LightmapVertex {
    float pos[3];
    char unkC[0x14];
    float uv[2];
} LightmapVertex;

/* a run of triangles; the next one starts `next` bytes after the indices */
typedef struct LightmapChunk {
    char unk0[0xC];
    uint16_t indexCount;
    uint16_t next;
    char unk10[2];
    uint16_t texture;
    uint16_t base;
    char unk16[2];
    uint16_t unk18;
    char unk1A[6];
    uint16_t index[];
} LightmapChunk;

typedef struct LightmapMesh {
    int unk0;
    int texCount;
    int unk8;
    int unkC;
    int vertCount;
    LightmapVertex *verts;
    void *sizes;
    LightmapChunk *chunks;
} LightmapMesh;

typedef struct LightmapMeshHeader {
    char unk0[0x20];
    int format;
    LightmapMesh *mesh;
} LightmapMeshHeader;

typedef struct MeshLod {
    char unk0[0xC];
    struct {
        char unk0[0xC];
        LightmapMeshHeader *header;
    } *data;
} MeshLod;

typedef struct LightmapRef {
    Lightmaps *lightmaps;
    MeshLod *lod;
    void *obj;
    Matrix *matrix;
    Vec3d *pos;
    int unk14;
    int lodIndex;
    int lodCount;
    int uploaded;
} LightmapRef;

typedef struct LightmapSet {
    int count;
    void *model;
    float radius;
    float center[3];
    LightmapRef refs[];
} LightmapSet;

typedef struct LightmapInfo {
    char unk0[0x88];
    LightmapSet *set;
} LightmapInfo;

_Static_assert(sizeof(LightmapTex) == 0x2C, "LightmapTex");
_Static_assert(offsetof(Lightmaps, tex) == 0x3C, "Lightmaps");
_Static_assert(sizeof(LightmapVertex) == 0x28, "LightmapVertex");
_Static_assert(offsetof(LightmapChunk, unk18) == 0x18, "LightmapChunk.unk18");
_Static_assert(offsetof(LightmapChunk, index) == 0x20, "LightmapChunk");
_Static_assert(offsetof(LightmapMesh, chunks) == 0x1C, "LightmapMesh");
_Static_assert(offsetof(LightmapMeshHeader, mesh) == 0x24, "LightmapMeshHeader");
_Static_assert(sizeof(LightmapRef) == 0x24, "LightmapRef");
_Static_assert(offsetof(LightmapSet, refs) == 0x18, "LightmapSet");

ORIG(0x521110, void, Hemicube_DrawTriangle, (int *color, float *depth, int pitch, int *ids, int idW, int idH,
                                             int minX, int minY, int maxX, int maxY, float *a, float *b, float *c,
                                             float *ta, float *tb, float *tc, int affine));
ORIG(0x521960, void, Hemicube_ClipTriangle, (int *color, float *depth, int pitch, int *ids, int idW, int idH,
                                             int minX, int minY, int maxX, int maxY, float offX, float offY,
                                             float scaleX, float scaleY, float *p0, float *p1, float *p2, float *t0,
                                             float *t1, float *t2, int affine));

/* ids drawn for back faces: 0x10000 times -1 */
#define gLightmapBuffer VAR(int *, 0xA83B28)

#define MAX_VERTS 20000

static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const double kOneD = 1.0;
static const float kNear = 0x1.0624dep+2f;       /* 4.096 */
static const float kBackScale = 0x1.00419ap+0f;  /* 1.001001 */
static const float kFrontScale = 0x1.ff7ceep-1f; /* 0.999 */

/* ordered and not zero, like fcomp + test ah,0x40 */
static inline int nonzero(long double a)
{
    return __builtin_islessgreater(a, 0.0L);
}

/* model space to camera space without the translation */
static void rotate(float *out, const float *q, const float *p)
{
    out[0] = q[2] * p[2] + q[1] * p[1] + q[0] * p[0];
    out[1] = q[5] * p[2] + q[4] * p[1] + q[3] * p[0];
    out[2] = q[8] * p[2] + q[7] * p[1] + q[6] * p[0];
}

/* texture coordinates in texels */
static void texel_uv(float *uv, const LightmapVertex *v, const LightmapTex *tex)
{
    uv[0] = v->uv[0];
    uv[1] = v->uv[1];
    uv[0] = tex->width * uv[0];
    uv[1] = tex->height * uv[1];
}

/* Draws every triangle of the set's meshes at `lod` (the coarsest LOD
   stands in for higher ones) whose chunk flag matches `flagged`. With
   `ortho` the view is a box of half size `radius`, else a perspective
   hemicube face. Back faces are drawn only with `ortho` or a stand-in LOD,
   pushed slightly away and with -1 ids. idMode 0 draws texel ids, 1 the
   lightmap pixels. */
void Hemicube_DrawLightmapSet(LightmapInfo *info, const Vec3d *origin, const Matrix *cam, int *ids, float *depth,
                              int w, int h, float offX, float offY, int lod, int ortho, int flagged, int clear,
                              float radius, int idMode)
{
    LightmapSet *set = info->set;
    float pv[MAX_VERTS][3];
    int *src = NULL;
    int n = w * h, i, k;

    for (i = 0; i < n; i++)
        ids[i] = -1;
    if (clear) {
        float far = ortho ? FLT_MAX : 0;

        for (i = 0; i < n; i++)
            depth[i] = far;
    }

    for (i = 0; i < set->count; i++) {
        LightmapRef *ref = &set->refs[i];
        int last = ref->lodCount <= lod && ref->lodIndex == ref->lodCount - 1;
        LightmapMeshHeader *mh;
        LightmapMesh *mesh;
        LightmapChunk *chunk;
        const float *M = cam->m, *R;
        long double dx, dy, dz;
        float t[3], q[9];

        if (ref->lodIndex != lod && !last)
            continue;
        mh = ref->lod->data->header;
        if (mh->format != 3)
            continue;
        mesh = mh->mesh;
        if (!mesh->unk8 || !mesh)
            continue;

        /* the object's position and rotation in camera space */
        dx = ref->pos->x - origin->x;
        dy = ref->pos->y - origin->y;
        dz = ref->pos->z - origin->z;
        t[0] = dx * M[0] + dz * M[2] + dy * M[1];
        t[1] = dx * M[3] + dz * M[5] + dy * M[4];
        t[2] = dz * M[8] + dy * M[7] + dx * M[6];
        R = ref->matrix->m;
        q[0] = R[6] * M[2] + R[3] * M[1] + R[0] * M[0];
        q[1] = R[7] * M[2] + R[4] * M[1] + R[1] * M[0];
        q[2] = M[0] * R[2] + R[8] * M[2] + R[5] * M[1];
        q[3] = R[0] * M[3] + M[5] * R[6] + M[4] * R[3];
        q[4] = R[1] * M[3] + R[7] * M[5] + R[4] * M[4];
        q[5] = R[8] * M[5] + R[5] * M[4] + R[2] * M[3];
        q[6] = M[8] * R[6] + M[7] * R[3] + R[0] * M[6];
        q[7] = M[8] * R[7] + M[7] * R[4] + R[1] * M[6];
        q[8] = R[2] * M[6] + M[8] * R[8] + M[7] * R[5];
        /* the exe re-orthonormalises the product once more than 1000 products
           went into it, but then keeps using the copy taken before */

        for (k = 0; k < mesh->vertCount; k++) {
            float c[3], *o = pv[k], y;
            long double x, z;

            rotate(c, q, mesh->verts[k].pos);
            x = (long double)c[0] + t[0];
            y = c[1] + t[1];
            z = (long double)c[2] + t[2];
            if (!(z >= kNear)) {
                /* behind the near plane: kept in camera space */
                o[2] = z - kNear;
                o[0] = x;
                o[1] = y;
            } else if (ortho) {
                float half = w >> 1;

                o[2] = z;
                o[0] = x / radius * half + offX;
                o[1] = y / radius * half + offY;
            } else {
                long double r = kOne / z;
                float half = w >> 1;

                o[2] = r;
                o[0] = r * half * x + offX;
                o[1] = half * y * o[2] + offY;
            }
        }

        chunk = mesh->chunks;
        for (;;) {
            const uint16_t *ix = chunk->index;
            int j;

            for (j = 0; j < chunk->indexCount / 3; j++, ix += 3) {
                uint16_t vi[3];
                LightmapTexInfo *ti;
                LightmapTex *tex;
                float *P[3], uv[3][2], B[3][3];

                vi[0] = ix[0] + chunk->base;
                vi[1] = ix[1] + chunk->base;
                vi[2] = ix[2] + chunk->base;
                if (vi[0] == vi[1] || vi[1] == vi[2] || vi[2] == vi[0])
                    continue;
                if ((chunk->unk18 == 0) == (flagged != 0))
                    continue;
                ti = &ref->lightmaps->info[chunk->texture];
                tex = &ref->lightmaps->tex[chunk->texture];
                for (k = 0; k < 3; k++)
                    P[k] = pv[vi[k]];

                if (P[0][2] > kZero && P[1][2] > kZero && P[2][2] > kZero) {
                    /* projected: the facing comes from the screen winding */
                    long double cross = ((long double)P[2][1] - P[0][1]) * ((long double)P[1][0] - P[0][0]) -
                                        ((long double)P[1][1] - P[0][1]) * ((long double)P[2][0] - P[0][0]);

                    if (cross >= kZero && !last) {
                        for (k = 0; k < 3; k++) {
                            texel_uv(uv[k], &mesh->verts[vi[k]], tex);
                            if (!ortho) {
                                uv[k][0] = P[k][2] * uv[k][0];
                                uv[k][1] = P[k][2] * uv[k][1];
                            }
                        }
                        if (idMode == 0)
                            src = ti->texelIds;
                        else if (idMode == 1)
                            src = tex->pixels;
                    } else if (ortho || last) {
                        for (k = 0; k < 3; k++) {
                            texel_uv(uv[k], &mesh->verts[vi[k]], tex);
                            if (!ortho) {
                                uv[k][0] = uv[k][0] * P[k][2];
                                uv[k][1] = uv[k][1] * P[k][2];
                            }
                            B[k][0] = P[k][0];
                            B[k][1] = P[k][1];
                            B[k][2] = P[k][2];
                            B[k][2] = B[k][2] * kBackScale;
                            P[k] = B[k];
                        }
                        if (idMode == 0)
                            src = gLightmapBuffer;
                        else if (idMode == 1)
                            src = tex->pixels;
                    } else {
                        continue;
                    }
                    Hemicube_DrawTriangle(ids, depth, w, src, tex->width, tex->height, 0, 0, w, h, P[0], P[1], P[2],
                                          uv[0], uv[1], uv[2], ortho);
                } else {
                    /* crosses the near plane: the facing comes from the
                       world space normal and the direction to the first
                       vertex */
                    const LightmapVertex *V = mesh->verts;
                    const float *a = V[vi[0]].pos, *b = V[vi[1]].pos, *c = V[vi[2]].pos;
                    long double e1x, e1y, e1z, e2z, nz, s0, s1, s2, W2, dot;
                    float e2x, e2y, nx, ny, wn[3], W0, W1, h1, h2, g1, g2, len, C[3][3];
                    const Vec3d *pos = ref->pos;
                    float half;

                    R = ref->matrix->m;
                    e1x = (long double)b[0] - a[0];
                    e1y = (long double)b[1] - a[1];
                    e1z = (long double)b[2] - a[2];
                    e2x = c[0] - a[0];
                    e2y = c[1] - a[1];
                    e2z = (long double)c[2] - a[2];
                    nx = e2z * e1y - e2y * e1z;
                    ny = -(e2z * e1x - e2x * e1z);
                    nz = e1x * e2y - e2x * e1y;
                    wn[0] = ny * R[1] + nx * R[0] + nz * R[2];
                    wn[1] = ny * R[4] + nx * R[3] + nz * R[5];
                    wn[2] = ny * R[7] + nx * R[6] + nz * R[8];
                    W0 = wn[0];
                    W1 = wn[1];

                    s0 = pos->x - origin->x;
                    g1 = pos->y - origin->y;
                    g2 = pos->z - origin->z;
                    s0 = s0 + (a[2] * R[2] + R[1] * a[1] + R[0] * a[0]);
                    h1 = a[1] * R[4] + a[2] * R[5] + a[0] * R[3];
                    h2 = a[1] * R[7] + a[2] * R[8] + a[0] * R[6];
                    s1 = (long double)h1 + g1;
                    s2 = (long double)h2 + g2;
                    if (nonzero(s0) || nonzero(s1) || nonzero(s2)) {
                        long double r;

                        len = __builtin_sqrtl(s0 * s0 + s1 * s1 + s2 * s2);
                        r = kOneD / len;
                        s0 = s0 * r;
                        s1 = s1 * r;
                        s2 = s2 * r;
                    }
                    if (nonzero(wn[0]) || nonzero(wn[1]) || nonzero(wn[2])) {
                        long double r;

                        len = __builtin_sqrtl((long double)wn[0] * wn[0] + (long double)wn[1] * wn[1] +
                                              (long double)wn[2] * wn[2]);
                        r = kOneD / len;
                        W0 = wn[0] * r;
                        W1 = W1 * r;
                        W2 = r * wn[2];
                    } else {
                        W2 = wn[2];
                    }
                    dot = s2 * W2 + s1 * W1 + s0 * W0;

                    if (dot >= kZero && !last) {
                        /* points in front go back to camera space */
                        for (k = 0; k < 3; k++) {
                            texel_uv(uv[k], &V[vi[k]], tex);
                            if (P[k][2] > kZero) {
                                float cc[3];

                                P[k] = C[k];
                                rotate(cc, q, V[vi[k]].pos);
                                C[k][2] = (long double)cc[2] + t[2];
                                C[k][0] = (long double)cc[0] + t[0];
                                C[k][1] = (long double)cc[1] + t[1];
                            }
                        }
                        if (idMode == 0)
                            src = ti->texelIds;
                        else if (idMode == 1)
                            src = tex->pixels;
                    } else if (ortho || last) {
                        for (k = 0; k < 3; k++) {
                            texel_uv(uv[k], &V[vi[k]], tex);
                            if (!(P[k][2] > kZero)) {
                                C[k][0] = P[k][0];
                                C[k][1] = P[k][1];
                                C[k][2] = P[k][2];
                                C[k][2] = C[k][2] * kBackScale;
                            } else {
                                float cc[3];

                                rotate(cc, q, V[vi[k]].pos);
                                C[k][2] = ((long double)cc[2] + t[2]) * kFrontScale;
                                C[k][0] = (long double)cc[0] + t[0];
                                C[k][1] = (long double)cc[1] + t[1];
                            }
                            P[k] = C[k];
                        }
                        if (idMode == 0)
                            src = gLightmapBuffer;
                        else if (idMode == 1)
                            src = tex->pixels;
                    } else {
                        continue;
                    }
                    half = w >> 1;
                    Hemicube_ClipTriangle(ids, depth, w, src, tex->width, tex->height, 0, 0, w, h, offX, offY, half,
                                          half, P[0], P[1], P[2], uv[0], uv[1], uv[2], ortho);
                }
            }
            if (chunk->next == 0xFFFF)
                break;
            chunk = (LightmapChunk *)((char *)chunk->index + chunk->next);
        }
    }
}
HOOK(0x521E40, Hemicube_DrawLightmapSet);
