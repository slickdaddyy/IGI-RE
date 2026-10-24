/* Builds the lightmap textures of a LOD, or loads them from its .olm file.
   For every texel it finds the triangles that cover it, clipping the texel
   square against each triangle in texture space, and keeps the texel's
   lit area, surface normal and 3D position for the light passes. */
#pragma GCC optimize("excess-precision=standard")
#pragma GCC optimize("rounding-math")

#include <float.h>

#include "hook.h"
#include "dirlight/dirlight.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B5B60, int, Resource_Exists, (const char *path));
ORIG(0x4B5F00, void *, Resource_Load, (const char *path, int flags));
ORIG(0x4B63D0, void, Resource_Flush, (void *res));
ORIG(0x4B2950, long double, Vec2_Distance, (const float *a, const float *b));
ORIG(0x4B2D90, char, Vec3_TriangleCoords, (float *s, float *t, const float *p, const float *e1, const float *e2,
                                           const float *n));
ORIG(0x5210C0, const char *, Lightmap_GetDir, (void));
ORIG(0x52D440, void, DateTime_Get, (int *time));
ORIG(0x52D630, int, DateTime_Compare, (const int *a, const int *b));

#define gLightmapCountA839F4 VAR(int, 0xA839F4)
#define gLightmapCountA83B4C VAR(int, 0xA83B4C)
#define gLightmapBuilt VAR(int, 0xA83D28)

#define LIGHTMAP_MAGIC 0x3DF5C28F

typedef struct LightmapVertex {
    Vec3f pos;
    char unkC[0x14];
    float uv[2];
} LightmapVertex;

/* a run of triangles; the next one starts next bytes after the indices */
typedef struct LightmapChunk {
    char unk0[0xC];
    uint16_t indexCount;
    uint16_t next;
    uint8_t unk10;
    char unk11;
    uint16_t texture;
    uint16_t base;
    char unk16[8];
    uint8_t unk1E;
    char unk1F;
    uint16_t index[];
} LightmapChunk;

typedef struct LightmapMesh {
    int unk0;
    int texCount;
    int unk8;
    char unkC[8];
    LightmapVertex *verts;
    short (*sizes)[4];
    LightmapChunk *chunks;
    char unk20[8];
    int unk28;
} LightmapMesh;

typedef struct LightmapMeshHeader {
    float version;
    int time[7];
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

_Static_assert(sizeof(LightmapVertex) == 0x28, "LightmapVertex");
_Static_assert(offsetof(LightmapChunk, index) == 0x20, "LightmapChunk");
_Static_assert(offsetof(LightmapMesh, unk28) == 0x28, "LightmapMesh");
_Static_assert(offsetof(LightmapMeshHeader, mesh) == 0x24, "LightmapMeshHeader");

static const float kHalf = 0.5f;
static const float kZero = 0.0f;
static const double kOneD = 1.0;

/* x87 compare results, where an unordered compare sets every flag */
#define LT(a, b) (!((a) >= (b)))
#define LE(a, b) (!((a) > (b)))

/* Heron's formula on three side lengths, as the exe orders it */
static inline long double heron(long double a, long double b, long double c)
{
    long double s = (a + b + c) * kHalf;

    return (s - a) * (s - b) * (s - c) * s;
}

Lightmaps *Lightmap_BuildLod(void *lodp, int lodIndex, float scale, const char *name, int index, int full, int flags,
                             int unk)
{
    LightmapMeshHeader *mh = ((MeshLod *)lodp)->data->header;
    LightmapMesh *mesh;
    Lightmaps *lm;
    char path[0x200];
    int i, row, col, k, n;

    if (mh->format != 3)
        return NULL;
    mesh = mh->mesh;
    if (!mesh->unk8 || !mesh)
        return NULL;
    mesh->unk28 = 0;
    crt_sprintf(path, "%s/%s_%05d.olm", Lightmap_GetDir(), name, index);
    if (Resource_Exists(path)) {
        Lightmaps *file = Resource_Load(path, 0);

        if (file->magic == LIGHTMAP_MAGIC && feq(mh->version, file->version) &&
            DateTime_Compare(mh->time, file->time) < 0 && mesh->texCount == file->count) {
            if (!full) {
                char *px;

                file = Resource_Load(path, 0);
                file->info = NULL;
                file->loaded = 1;
                file->built = 0;
                px = (char *)&file->tex[file->count];
                for (i = 0; i < file->count; i++) {
                    LightmapTex *t = &file->tex[i];

                    n = t->width * t->height;
                    t->pixels = px;
                    px += n * 4;
                    LightmapTex_Alloc(t);
                }
                gLightmapCountA83B4C += file->texels;
                gLightmapCountA839F4 += file->texels2;
                return file;
            }
        } else {
            Resource_Flush(file);
        }
    }

    lm = Mem_Alloc(mesh->texCount * sizeof(LightmapTex) + sizeof(Lightmaps), 4);
    lm->count = mesh->texCount;
    lm->info = Mem_Alloc(mesh->texCount * sizeof(LightmapTexInfo), 4);
    lm->loaded = 0;
    lm->built = 1;
    gLightmapBuilt = 1;
    lm->texels = 0;
    lm->texels2 = 0;
    memcpy(&lm->version, &(uint32_t){ 0x3DCCCCCD }, 4); /* 0.1f, kept exact under any rounding mode */
    lm->magic = LIGHTMAP_MAGIC;
    DateTime_Get(lm->time);

    for (i = 0; i < lm->count; i++) {
        short *size = mesh->sizes[i];
        LightmapTex *t = &lm->tex[i];
        LightmapTexInfo *ti = lm->info ? &lm->info[i] : NULL;

        t->width = size[0];
        t->height = size[1];
        t->pixels = Mem_Alloc(t->height * t->width * 4, 4);
        n = 0;
        for (row = 0; row < t->height; row++)
            for (col = 0; col < t->width; col++)
                ((int *)t->pixels)[n++] = -1;
        LightmapTex_Alloc(t);
        if (!ti)
            continue;
        ti->unk8 = Mem_Alloc(t->height * t->width * 12, 4);
        ti->unkC = Mem_Alloc(t->height * t->width * 12, 4);
        ti->weights = Mem_Alloc(t->height * t->width * 4, 4);
        ti->texelIds = Mem_Alloc(t->height * t->width * 4, 4);
        ti->normals = Mem_Alloc(t->height * t->width * 12, 4);
        ti->positions = Mem_Alloc(t->height * t->width * 12, 4);
        ti->unk14 = Mem_Alloc(t->height * t->width, 4);
        ti->unk18 = Mem_Alloc(t->height * t->width, 4);
        ti->firstTexel = gLightmapCountA83B4C;
        ti->litCount = 0;

        k = 0;
        for (row = 0; row < t->height; row++) {
            float rowF, row1F;

            if (t->width <= 0)
                continue;
            rowF = row;
            row1F = row + 1;
            for (col = 0; col < t->width; col++, k++) {
                int w = t->width;
                float u0 = (long double)col / w;
                float Hf = t->height;
                float v0 = rowF / Hf;
                float u1 = (long double)(col + 1) / w;
                float v1 = row1F / Hf;
                float center[2], nearPt[2] = { 0, 0 }, best = FLT_MAX;
                uint16_t found3[3], near3[3], *found = NULL, *nearIdx = NULL;
                LightmapChunk *ch, *chunk, *foundChunk = NULL, *nearChunk = NULL;

                center[0] = (u1 + u0) * kHalf;
                center[1] = (v1 + v0) * kHalf;
                ti->weights[k] = 0;
                for (ch = mesh->chunks;; ch = (LightmapChunk *)((char *)ch + 0x20 + ch->next)) {
                    uint16_t *idx = ch->index;
                    int tri;

                    for (tri = 0; tri < ch->indexCount / 3; tri++, idx += 3) {
                        uint16_t a = idx[0] + ch->base, b = idx[1] + ch->base, c = idx[2] + ch->base;
                        LightmapVertex *A, *B, *C;
                        float *uv[3], poly[17][6], *app;
                        int clipped[16] = { 0 }, side, npoly, e, p, j, k0 = 0;
                        long double dCA, lAB, lBC, lCA, q;
                        float dAB, dBC, uvArea, area3, sum;

                        if (a == b || b == c || c == a || ch->texture != i)
                            continue;
                        A = &mesh->verts[a];
                        B = &mesh->verts[b];
                        C = &mesh->verts[c];
                        uv[0] = A->uv;
                        uv[1] = B->uv;
                        uv[2] = C->uv;
                        if (LT(uv[0][0], u0) && LT(uv[1][0], u0) && LT(uv[2][0], u0))
                            continue;
                        if (!LE(uv[0][0], u1) && !LE(uv[1][0], u1) && !LE(uv[2][0], u1))
                            continue;
                        if (LT(uv[0][1], v0) && LT(uv[1][1], v0) && LT(uv[2][1], v0))
                            continue;
                        if (!LE(uv[0][1], v1) && !LE(uv[1][1], v1) && !LE(uv[2][1], v1))
                            continue;
                        side = !LE((uv[2][1] - uv[0][1]) * (uv[1][0] - uv[0][0]) -
                                   (uv[1][1] - uv[0][1]) * (uv[2][0] - uv[0][0]), 0);

                        dAB = Vec2_Distance(uv[0], uv[1]);
                        dBC = Vec2_Distance(uv[1], uv[2]);
                        dCA = Vec2_Distance(uv[2], uv[0]);
                        q = heron(dCA, dBC, dAB);
                        uvArea = LT(q, 0) ? 0 : __builtin_sqrtl(q);
                        {
                            long double x = B->pos.x - A->pos.x, y = B->pos.y - A->pos.y, z = B->pos.z - A->pos.z;

                            lAB = __builtin_sqrtl(x * x + y * y + z * z);
                            x = C->pos.x - B->pos.x;
                            y = C->pos.y - B->pos.y;
                            z = C->pos.z - B->pos.z;
                            lBC = __builtin_sqrtl(x * x + y * y + z * z);
                            x = A->pos.x - C->pos.x;
                            y = A->pos.y - C->pos.y;
                            z = A->pos.z - C->pos.z;
                            lCA = __builtin_sqrtl(x * x + y * y + z * z);
                        }
                        q = heron(lCA, lBC, lAB);
                        area3 = LT(q, 0) ? 0 : __builtin_sqrtl(q);

                        /* the texel square as two triangles, clipped by each edge */
                        poly[0][0] = u0;
                        poly[0][1] = v0;
                        poly[0][2] = u1;
                        poly[0][3] = v0;
                        poly[0][4] = u0;
                        poly[0][5] = v1;
                        poly[1][0] = u1;
                        poly[1][1] = v0;
                        poly[1][2] = u1;
                        poly[1][3] = v1;
                        poly[1][4] = u0;
                        poly[1][5] = v1;
                        npoly = 2;
                        for (e = 1; e <= 3; e++) {
                            float *from = side ? uv[e - 1] : uv[e % 3];
                            float *to = side ? uv[e % 3] : uv[e - 1];
                            float du = to[0] - from[0], dv = to[1] - from[1];
                            int count = npoly;

                            app = &poly[npoly][2];
                            for (p = 0; p < count; p++) {
                                float *pt = poly[p], adu = du, adv = dv;
                                int in[3], cnt = 0, j1, j2;

                                if (clipped[p])
                                    continue;
                                if (LT(dv, 0))
                                    adv = -adv;
                                if (LT(du, 0))
                                    adu = -adu;
                                if (!LE(adv, adu)) {
                                    for (j = 0; j < 3; j++) {
                                        long double x = (pt[2 * j + 1] - from[1]) * du / dv + from[0];

                                        in[j] = !LT(from[1], to[1]) ? !LT(x, pt[2 * j]) : LE(x, pt[2 * j]);
                                        cnt += in[j];
                                    }
                                } else if (feq(du, 0)) {
                                    clipped[p] = 1;
                                    continue;
                                } else {
                                    for (j = 0; j < 3; j++) {
                                        long double y = (pt[2 * j] - from[0]) * dv / du + from[1];

                                        in[j] = LE(from[0], to[0]) ? !LT(y, pt[2 * j + 1]) : LE(y, pt[2 * j + 1]);
                                        cnt += in[j];
                                    }
                                }
                                if (cnt == 0)
                                    continue;
                                if (cnt == 3) {
                                    clipped[p] = 1;
                                    continue;
                                }
                                if (cnt == 2) {
                                    for (j = 0; j < 3; j++)
                                        if (!in[j])
                                            k0 = j;
                                    j1 = (k0 + 1) % 3;
                                    j2 = (k0 + 2) % 3;
                                    Line2_Intersect(pt + 2 * j1, pt + 2 * j1, pt + 2 * k0, from, to);
                                    Line2_Intersect(pt + 2 * j2, pt + 2 * j2, pt + 2 * k0, from, to);
                                } else {
                                    for (j = 0; j < 3; j++)
                                        if (in[j])
                                            k0 = j;
                                    j1 = (k0 + 1) % 3;
                                    j2 = (k0 + 2) % 3;
                                    memcpy(app - 2, pt + 2 * j1, 8);
                                    Line2_Intersect(app, pt + 2 * j2, pt + 2 * k0, from, to);
                                    Line2_Intersect(app + 2, pt + 2 * j1, pt + 2 * k0, from, to);
                                    npoly++;
                                    app += 6;
                                    Line2_Intersect(pt + 2 * k0, pt + 2 * j2, pt + 2 * k0, from, to);
                                }
                            }
                        }

                        if (!found) {
                            int inside = 1;

                            for (e = 1; e <= 3; e++) {
                                float *from = side ? uv[e - 1] : uv[e % 3];
                                float *to = side ? uv[e % 3] : uv[e - 1];
                                long double du = to[0] - from[0], dv = to[1] - from[1], adu = du, adv = dv;
                                int res;

                                if (LT(dv, 0))
                                    adv = -adv;
                                if (LT(du, 0))
                                    adu = -adu;
                                if (!LE(adv, adu)) {
                                    long double x = (center[1] - from[1]) * du / dv + from[0];

                                    res = !LT(from[1], to[1]) ? !LT(x, center[0]) : LE(x, center[0]);
                                    inside &= !res;
                                } else if (feq(du, 0)) {
                                    inside = 0;
                                } else {
                                    long double y = (center[0] - from[0]) * dv / du + from[1];

                                    res = LE(from[0], to[0]) ? !LT(y, center[1]) : LE(y, center[1]);
                                    inside &= !res;
                                }
                            }
                            if (inside) {
                                found3[0] = a;
                                found3[1] = b;
                                found3[2] = c;
                                found = found3;
                                foundChunk = ch;
                            }
                        }

                        sum = 0;
                        for (p = 0; p < npoly; p++) {
                            float d01, d12;
                            long double d20;

                            if (clipped[p])
                                continue;
                            d01 = Vec2_Distance(poly[p], poly[p] + 2);
                            d12 = Vec2_Distance(poly[p] + 2, poly[p] + 4);
                            d20 = Vec2_Distance(poly[p] + 4, poly[p]);
                            q = heron(d20, d12, d01);
                            sum = (LT(q, 0) ? kZero : __builtin_sqrtl(q)) + sum;
                        }
                        if (LE(sum, 0))
                            continue;
                        if (!feq(uvArea, 0))
                            ti->weights[k] = sum * area3 / uvArea + ti->weights[k];
                        for (p = 0; p < npoly; p++) {
                            if (clipped[p])
                                continue;
                            for (j = 0; j < 3; j++) {
                                long double d = Vec2_Distance(poly[p] + 2 * j, center);

                                if (!LE(best, d)) {
                                    best = d;
                                    near3[0] = a;
                                    near3[1] = b;
                                    near3[2] = c;
                                    nearIdx = near3;
                                    nearChunk = ch;
                                    nearPt[0] = poly[p][2 * j];
                                    nearPt[1] = poly[p][2 * j + 1];
                                }
                            }
                        }
                    }
                    if (ch->next == 0xFFFF)
                        break;
                }

                if (!found) {
                    center[0] = nearPt[0];
                    found = nearIdx;
                    center[1] = nearPt[1];
                    chunk = nearChunk;
                } else {
                    chunk = foundChunk;
                }
                if (!LE(ti->weights[k], 0)) {
                    ti->texelIds[k] = gLightmapCountA83B4C;
                    ti->unk14[k] = chunk->unk1E;
                    ti->unk18[k] = chunk->unk10;
                    if (chunk->unk1E && !lodIndex) {
                        gLightmapCountA839F4++;
                        lm->texels2++;
                    }
                    gLightmapCountA83B4C++;
                    lm->texels++;
                    ti->litCount++;
                } else {
                    ti->texelIds[k] = -1;
                }
                if (found) {
                    LightmapVertex *A = &mesh->verts[found[0]], *B = &mesh->verts[found[1]];
                    LightmapVertex *C = &mesh->verts[found[2]];
                    float up[3] = { 0, 0, 1.0f };
                    float e1[3], e2[3], nx, ny, nz, nnx, nny, s, t, p1[3], p2[3], pt[3], py, pz, sy, sz;
                    float tx, ty, tz;
                    long double e2z, nzv, sx;
                    Vec3f *out;

                    if (LE((C->uv[1] - A->uv[1]) * (B->uv[0] - A->uv[0]) - (C->uv[0] - A->uv[0]) * (B->uv[1] - A->uv[1]),
                           0))
                        up[2] = -1.0f;
                    e1[0] = B->pos.x - A->pos.x;
                    e1[1] = B->pos.y - A->pos.y;
                    e1[2] = B->pos.z - A->pos.z;
                    e2[0] = C->pos.x - A->pos.x;
                    e2[1] = C->pos.y - A->pos.y;
                    e2z = C->pos.z - A->pos.z;
                    e2[2] = e2z;
                    nx = e2z * e1[1] - e2[1] * e1[2];
                    ny = -(e2[2] * e1[0] - e2[0] * e1[2]);
                    nz = e2[1] * e1[0] - e2[0] * e1[1];
                    if (!(feq(nx, 0) && feq(ny, 0) && feq(nz, 0))) {
                        long double r = kOneD / __builtin_sqrtl(nx * nx + ny * ny + nz * nz);

                        nnx = nx * r;
                        nny = ny * r;
                        nzv = r * nz;
                    } else {
                        nnx = nx;
                        nny = ny;
                        nzv = nz;
                    }
                    out = &ti->normals[k];
                    out->x = -nnx;
                    out->y = -nny;
                    out->z = -nzv;

                    p1[0] = B->uv[0] - A->uv[0];
                    p1[1] = B->uv[1] - A->uv[1];
                    p1[2] = 0;
                    p2[0] = C->uv[0] - A->uv[0];
                    p2[1] = C->uv[1] - A->uv[1];
                    p2[2] = 0;
                    pt[0] = center[0] - A->uv[0];
                    pt[1] = center[1] - A->uv[1];
                    pt[2] = 0;
                    Vec3_TriangleCoords(&s, &t, pt, p1, p2, up);
                    sx = s * e1[0];
                    sy = s * e1[1];
                    sz = s * e1[2];
                    tx = t * e2[0];
                    ty = t * e2[1];
                    tz = t * e2[2];
                    py = A->pos.y + sy;
                    pz = A->pos.z + sz;
                    out = &ti->positions[k];
                    out->x = (sx + A->pos.x) + tx;
                    out->y = py + ty;
                    out->z = pz + tz;
                }
            }
        }
    }
    return lm;
}
HOOK(0x523D40, Lightmap_BuildLod);
