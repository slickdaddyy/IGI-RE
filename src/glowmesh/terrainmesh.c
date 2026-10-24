/* TerrainMesh: terrain drawn as meshes of the QCube, its lighting, near
   plane depth and the near plane clipping of its triangles. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "glowmesh/glowmesh.h"

FN_EXTERN(crt_CIfmod);

ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0F80, int, RenderMode_GetCurrent, (void));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x516600, void, TerrDraw_Init, (void));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B7CF0, void, Palette_Create, (void *palette, int *colors, int unk));
ORIG(0x4B7D30, void, Palette_Free, (void *palette));
ORIG(0x4C6160, void, Terrain_LoadLightmaps, (void *terrain));
ORIG(0x4D2A20, void, QCube_FlushCache, (void));
ORIG(0x499FF0, int, Float_Log2, (float f));
ORIG(0x518240, int, ViewSlot_GetCurrent, (void));
ORIG(0x512C30, void, TerrainMesh_DrawD3D, (void));
ORIG(0x4C1780, void, Debug_Log, (int unk));
ORIG(0x4DA790, void, Fog_Compute, (float *out, const double *pos, float depth, float scale, void *layers, int count));

/* view space vertex with its screen position and outcode, as
   Direct3DRender_ClipTriangleDepth takes them */
typedef struct ClipVertex {
    float x, y, z;
    float sx, sy, rhw;
    int code;
} ClipVertex;

typedef struct TerrainMeshVertex {
    float x, y, z;
    float unkC; /* height map height, or for the later vertices the parent vertex index */
} TerrainMeshVertex;

/* follows the vertices; each face packs three vertex indices in its low bytes */
typedef struct TerrainMeshFaces {
    int unk0;
    uint32_t *faces;
    int unk8;
    int count;
} TerrainMeshFaces;

typedef struct TerrainMesh {
    double center[3];
    uint32_t vertCount; /* low 16 bits */
    int type;           /* only 0x10 meshes count */
    TerrainMeshVertex verts[];
} TerrainMesh;

typedef struct Terrain {
    char unk0[0xDBC];
    int textures;
    char unkDC0[0x34];
    void *palette;
} Terrain;

typedef struct TerrainFog {
    float value;
    float delta;
} TerrainFog;

/* a vertex of a clipped triangle */
typedef struct TerrainClipVertex {
    float sx, sy, q;
    float c[4];
    int unk1C;
    float u, v;
    int unk28, unk2C;
} TerrainClipVertex;

typedef struct Pool {
    char *data;
    int *free;
    int used;
    int count;
    int stride;
    int size;
} Pool;

/* a slot of the QCube cache; data is the cell's mesh */
typedef struct QCacheEntry {
    int cell[4];
    int frame;
    int kind; /* pool the data comes from */
    char *data;
} QCacheEntry;

/* a terrain cell QCube wants built this frame */
typedef struct TerrainCell {
    double pos[3];
    int level;
    unsigned int flags;      /* 1 swaps x and y, 2 and 4 mirror */
    const int16_t *shape;    /* face count, code offset, vertex counts, faces, codes */
    unsigned int maps;       /* low 16 bits lightmap (0xFFFF none), bits 16-23 height map */
    unsigned int materials;  /* four material maps, one per byte */
    QCacheEntry *cache;
} TerrainCell;

typedef struct TerrainLightMapInfo {
    float scale;
    float bias;
    double x0, y0;
    int texture;
    char unk1C[0x14];
} TerrainLightMapInfo;

typedef struct TerrainHeightMapInfo {
    char unk0[0xC];
    int shift;
    float scale;
    int unk14;
    double x0, y0;
    const signed char *data;
    int unk2C;
} TerrainHeightMapInfo;

typedef struct TerrainMaterialMapInfo {
    int id;
    char unk4[8];
    int shift;
    float scale;
    int unk14;
    double x0, y0;
    const uint8_t *bits;
    int unk2C;
} TerrainMaterialMapInfo;

/* texture placement of a material at one level, 0xC28FE4 + material * 900 + level * 28 */
typedef struct TerrainLevelMat {
    char unk0[0xC];
    float scaleA;
    int tileA;
    float scaleB;
    int tileB;
} TerrainLevelMat;

typedef struct TerrainMatEntry {
    int shift; /* 0 ends the list */
    float scale;
    float x0, y0;
    const uint8_t *bits;
    int id;
} TerrainMatEntry;

typedef struct TerrainVertRec {
    int idx;
    float dx, dy, dz;
    int unk10;
    int word; /* material << 16 | material of the parent vertex, 0xFF if none or the same */
    int unk18;
    float lu, lv;
} TerrainVertRec;

typedef struct TerrainMatRec {
    float f[8];
    int tileB;
    int tileA;
} TerrainMatRec;

/* argument of TerrainMesh_DrawTriangle */
typedef struct TerrainPoly {
    double pos[3][3];
    float color[3][3];
    float uv[3][2];
    void *fogLayers;
    int mode;
    int unk8C;
    int fogCount;
} TerrainPoly;

_Static_assert(sizeof(ClipVertex) == 0x1C, "ClipVertex");
_Static_assert(sizeof(TerrainCell) == 0x30, "TerrainCell");
_Static_assert(sizeof(TerrainLightMapInfo) == 0x30, "TerrainLightMapInfo");
_Static_assert(sizeof(TerrainHeightMapInfo) == 0x30, "TerrainHeightMapInfo");
_Static_assert(sizeof(TerrainMaterialMapInfo) == 0x30, "TerrainMaterialMapInfo");
_Static_assert(sizeof(TerrainLevelMat) == 0x1C, "TerrainLevelMat");
_Static_assert(sizeof(TerrainVertRec) == 0x24, "TerrainVertRec");
_Static_assert(sizeof(TerrainMatRec) == 0x28, "TerrainMatRec");
_Static_assert(offsetof(TerrainPoly, fogLayers) == 0x84, "TerrainPoly");

_Static_assert(sizeof(TerrainMesh) == 0x20, "TerrainMesh");
_Static_assert(sizeof(TerrainClipVertex) == 0x30, "TerrainClipVertex");

ORIG(0x4A0340, void, Direct3DRender_ClipTriangleDepth, (ClipVertex a, ClipVertex b, ClipVertex c, float *minZ,
                                                        float nearZ));

#define gTerrainMeshes ((TerrainMesh **)0xB12C60)
#define gTerrainMeshCount VAR(int, 0xB16C9C)
#define gTerrainCellCount VAR(int, 0xB16CA4)
#define gTerrainCells ((TerrainCell *)0xB16CE0)
#define gTerrainPool0 VAR(Pool, 0xB12B60)
#define gTerrainPool1 VAR(Pool, 0xB12C40)
#define gTerrainPool2 VAR(Pool, 0xB16CC0)
#define gQCubeOriginD ((double *)0xB12BA0)
#define gTerrainLightMaps ((TerrainLightMapInfo *)0xBA80B0)
#define gTerrainHeightMaps ((TerrainHeightMapInfo *)0xBA2030)
#define gTerrainMaterialMaps ((TerrainMaterialMapInfo *)0xBA5050)
#define gTerrainTileMaps ((uint8_t (*)[8])0xC30080)
#define gTerrainMeshRenderAll VAR(int, 0xA7DA10)
#define gLights VAR(char *, 0xB041E0)
#define gLightIndex VAR(int *, 0xB041E4)
#define gLightCount VAR(int, 0xB041E8)
#define gLightStride VAR(int, 0xB041F0)
#define gTerrainFogScale VAR(float, 0xA7DA18)
#define gTerrainAmbient ((float *)0xA7DA20)
#define gTerrainFogColor ((float *)0xA7DA30)
#define gUnkA7DA40 VAR(int, 0xA7DA40)
#define gTerrainFog ((TerrainFog *)0xC310A0)
#define gTerrainTextures VAR(int, 0xC30060)
#define gTerrainBottom VAR(float, 0xC31080)
#define gTerrainRight VAR(float, 0xC31084)
#define gUnkC31088 VAR(int, 0xC31088)
#define gUnkC3108C VAR(char, 0xC3108C)
#define gUnkC31090 VAR(int, 0xC31090)
#define gTerrainScaleY VAR(float, 0xC311A0)
#define gTerrainScaleX VAR(float, 0xC311A4)
#define gTerrainTop VAR(float, 0xC311A8)
#define gTerrainLeft VAR(float, 0xC311AC)
#define gTerrainCenterX VAR(float, 0xC311B0)
#define gTerrainCenterY VAR(float, 0xC311B4)
#define gTerrainInvSize VAR(float, 0xC311B8)
#define gUnkC28FBC VAR(int, 0xC28FBC)
#define gUnkC28FC0 VAR(float, 0xC28FC0)
#define gTerrainView ((float *)0x943B50) /* 3x3 matrix, unk24, then the view origin */
#define gTerrainTint ((float *)0xBA2030)
#define gTerrainClipUV ((double (*)[2])0xA7DA70)
#define gTerrainClipPos ((double (*)[3])0xA7DAB0)
#define gTerrainClipVerts ((TerrainClipVertex *)0xA7DB10)

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(k255, 0x437F0000);
F32(k130, 0x43020000);
F32(k16384, 0x46800000);
F32(k8Thirds, 0x402AAAAB);
F32(kClipQ, 0x3E480000);
F32(k256, 0x43800000);
F32(k256th, 0x3B800000);
F32(kHalf, 0x3F000000);
F32(k80, 0x42A00000);
F32(k65535, 0x477FFF00);
F32(kMapBlend, 0x3F500000);
F32(kUVBlend, 0x3EA9FBE7);
F64(kNearBias, 0x40DBB67AE0000000ULL);
F64(kThreeEighths, 0x3FD8000000000000ULL);
F64(kClipZ, 0x40799999A0000000ULL);
F64(kInvClipZ, 0x3F63FFFFFB000001ULL);
F64(kClipUVScale, 0x3FC8FFFFF9C00001ULL);
F64(kOneD, 0x3FF0000000000000ULL);
F64(kThree, 0x4008000000000000ULL);
F64(kHeightBias, 0x4050000000000000ULL);
F64(kThird, 0x3FD5555555555555ULL);
F64(kMapPeriod, 0x4109000000000000ULL);
F64(kMapScale, 0x3F54666666666666ULL);
F64(kMaxZScale, 0x3EA0000000000000ULL);

/* Lowers arg[2] to the nearest view depth of the terrain meshes. A vertex
   in front of the near plane keeps the screen position of the previous one. */
void TerrainMesh_UpdateNearZ(float *arg)
{
    float nearZ = gNearPlane;
    float a, b, z, sx = 0, sy = 0, rhw = 0;
    ClipVertex v[64];
    TerrainMeshVertex *p;
    TerrainMeshFaces *f;
    uint32_t *w;
    int i, j, n, code;

    for (i = 0; i < gTerrainMeshCount; i++) {
        TerrainMesh *m = gTerrainMeshes[i];
        double c[3];
        long double dist;

        n = m->vertCount & 0xFFFF;
        if (m->type != 0x10)
            continue;
        c[0] = m->center[0] - gViewPos[0];
        c[1] = m->center[1] - gViewPos[1];
        c[2] = m->center[2] - gViewPos[2];
        dist = gViewMatrix[8] * c[2] + gViewMatrix[7] * c[1] + gViewMatrix[6] * c[0] - kNearBias.d;
        if (!(arg[2] > dist))
            continue;
        p = m->verts;
        for (j = 0; j < n; j++, p++) {
            long double vx = p->x - gViewPos[0];
            long double vy = p->y - gViewPos[1];
            long double vz = p->z - gViewPos[2];

            a = gViewMatrix[2] * vz + gViewMatrix[1] * vy + gViewMatrix[0] * vx;
            b = gViewMatrix[5] * vz + gViewMatrix[4] * vy + gViewMatrix[3] * vx;
            z = gViewMatrix[8] * vz + gViewMatrix[7] * vy + gViewMatrix[6] * vx;
            if (!(z >= nearZ)) {
                code = 0x10;
            } else {
                long double r = kOne.f / z, px, py;

                rhw = r;
                px = r * a * gViewScaleX + gViewCenterX;
                sx = px;
                py = gViewScaleY * rhw * b + gViewCenterY;
                sy = py;
                code = (sy > gSceneMaxY) << 3 | (px > gSceneMaxX) << 2 | !(py >= gSceneMinY) << 1 |
                       !(px >= gSceneMinX);
            }
            v[j].x = a;
            v[j].y = b;
            v[j].z = z;
            v[j].sx = sx;
            v[j].sy = sy;
            v[j].rhw = rhw;
            v[j].code = code;
        }
        f = (TerrainMeshFaces *)p;
        w = f->faces;
        for (j = f->count; j > 0; j--, w++)
            Direct3DRender_ClipTriangleDepth(v[*w & 0xFF], v[*w >> 8 & 0xFF], v[*w >> 16 & 0xFF], &arg[2], nearZ);
    }
}
HOOK(0x50F770, TerrainMesh_UpdateNearZ);

void TerrainMesh_Init(void)
{
    gTerrainMeshRenderAll = RenderMode_CreateMethod("TerrainMesh_RenderAll");
    TerrDraw_Init();
}
HOOK(0x50FAC0, TerrainMesh_Init);

/* Builds the terrain palette from the first light and the ambient colour,
   reloads the lightmaps and fills the fog table of the QCube levels. */
void TerrainMesh_SetupLighting(Terrain *t)
{
    int pal[256];
    float r, g, b;
    long double ar, ag, ab, f, e;
    float *c;
    int i, s;

    if (!gLightCount)
        return;
    c = (float *)(gLights + *gLightIndex * gLightStride + 0xC);
    r = c[0];
    g = c[1];
    b = c[2];
    ar = gTerrainAmbient[0] * k255.f;
    ag = gTerrainAmbient[1] * k255.f;
    ab = gTerrainAmbient[2] * k255.f;
    f = kZero.f;
    for (i = 0; i < 256; i++) {
        unsigned int R = x87_fistp(r * f + ar);
        unsigned int G = x87_fistp(g * f + ag);
        unsigned int B = x87_fistp(b * f + ab);

        if (R > 0xFF)
            R = 0xFF;
        if (G > 0xFF)
            G = 0xFF;
        if (B > 0xFF)
            B = 0xFF;
        pal[i] = (R << 8 | G) << 8 | B;
        f += kOne.f;
    }
    if (t->palette) {
        Mem_PushMode(1);
        Palette_Free(t->palette);
        Palette_Create(t->palette, pal, 1);
        Mem_PopMode();
    }
    Mem_PushMode(1);
    Terrain_LoadLightmaps(t);
    Mem_PopMode();
    QCube_FlushCache();
    for (i = 0, s = 25; s > -6; i++, s--) {
        long double d = (long double)(int)(1u << (s & 31)) * gQCubeMinSize;

        d = d + d;
        d += 0x40000000 >> i;
        e = ((long double)Float_Log2((float)d) - k130.f) * gTerrainFogScale;
        if (kZero.f > e)
            e = kZero.f;
        else if (k255.f < e)
            e = k255.f;
        gTerrainFog[i].value = e;
    }
    e = (long double)gTerrainFog[0].value - gTerrainFog[1].value;
    gTerrainFog[0].delta = e + e;
    for (i = 1; i < 31; i++)
        gTerrainFog[i].delta = gTerrainFog[i - 1].value - gTerrainFog[i].value;
}
HOOK(0x50FAE0, TerrainMesh_SetupLighting);

void TerrainMesh_SetTextures(Terrain *t)
{
    gTerrainTextures = t->textures;
}
HOOK(0x50FD00, TerrainMesh_SetTextures);

/* fog colour clamped to 0..1, scale to 0..16384 */
void TerrainMesh_SetFog(const float *rgb, float scale)
{
    int i;

    memcpy(gTerrainFogColor, rgb, 12);
    gTerrainFogScale = scale;
    for (i = 0; i < 3; i++) {
        if (!(gTerrainFogColor[i] >= kZero.f))
            gTerrainFogColor[i] = 0.0f;
        else if (gTerrainFogColor[i] > kOne.f)
            gTerrainFogColor[i] = 1.0f;
    }
    if (!(scale >= kZero.f))
        gTerrainFogScale = 0.0f;
    else if (scale > k16384.f)
        gTerrainFogScale = 16384.0f;
}
HOOK(0x50FD20, TerrainMesh_SetFog);

void TerrainMesh_SetAmbient(const float *rgb)
{
    memcpy(gTerrainAmbient, rgb, 12);
}
HOOK(0x50FE40, TerrainMesh_SetAmbient);

/* screen layout and view transform shared by the two frame setups */
static void setup_view(void)
{
    float py, pz, t0, t1, t2;
    long double nx, ny, nz;
    char c;

    gTerrainRight = gSceneW + gSceneX;
    gTerrainLeft = gSceneX;
    gTerrainScaleY = gViewScaleY;
    gTerrainBottom = gSceneH + gSceneY;
    gTerrainTop = gSceneY;
    gTerrainCenterX = gSceneHalfW + gSceneX;
    gTerrainScaleX = gViewScaleX;
    gTerrainCenterY = gSceneHalfH + gSceneY;
    gTerrainInvSize = kOne.f / (long double)(gQCubeMinSize - 0x40);
    gUnkC31088 = x87_fistp((long double)(gQCubeMinSize + 0x40) * kThreeEighths.d);
    gUnkC28FC0 = gTerrainInvSize * k8Thirds.f;
    c = ViewSlot_GetCurrent();
    nx = -(long double)gViewPos[0];
    py = gViewPos[1];
    pz = gViewPos[2];
    gUnkC3108C = c;
    ny = -(long double)py;
    nz = -(long double)pz;
    t0 = gViewMatrix[2] * nz + gViewMatrix[1] * ny + gViewMatrix[0] * nx;
    t1 = gViewMatrix[5] * nz + gViewMatrix[4] * ny + gViewMatrix[3] * nx;
    t2 = gViewMatrix[8] * nz + gViewMatrix[7] * ny + gViewMatrix[6] * nx;
    memcpy(gTerrainView, gViewMatrix, 40);
    gTerrainView[10] = t0;
    gTerrainView[11] = t1;
    gTerrainView[12] = t2;
}

void TerrainMesh_Build(void);

void TerrainMesh_Update(void)
{
    setup_view();
    gUnkC28FBC = 0;
    gUnkC31090 = 0;
    if (gTerrainCellCount)
        TerrainMesh_Build();
}
HOOK(0x50FE60, TerrainMesh_Update);

/* x86 shift: count taken modulo 32, no overflow trouble */
#define SHL(v, n) ((int)((unsigned int)(v) << ((n) & 31)))

static int pool_take(Pool *p)
{
    int i = p->used;
    char *e = p->data + p->free[i] * p->stride;

    *(int *)(e + p->size) = i;
    p->used = i + 1;
    return (int)(uintptr_t)e;
}

/* the last used slot takes the place of the freed one */
static void pool_release(Pool *p, char *elem)
{
    int slot = *(int *)(elem + p->size);
    int last = --p->used;

    if (slot != last) {
        char *le = p->data + p->free[last] * p->stride;
        int tmp = p->free[slot];
        int other = *(int *)(le + p->size);

        p->free[slot] = p->free[other];
        p->free[other] = tmp;
        *(int *)(le + p->size) = slot;
    }
}

/* shape code: three 6 bit coordinates (offset by 24), turned by the cell flags */
static void vertex_pos(uint32_t code, unsigned int flags, unsigned int f1, double scale, const double *pos,
                       long double *x, long double *y, long double *z)
{
    int cz = (code >> 14 & 0x3F) - 0x18, a = (code >> 20 & 0x3F) - 0x18, b = (code >> 26 & 0x3F) - 0x18, X, Y;

    if (flags & 4)
        b = -b;
    if (f1) {
        if (flags & 2) {
            X = a;
            Y = -b;
        } else {
            X = -a;
            Y = b;
        }
    } else if (flags & 2) {
        X = -b;
        Y = -a;
    } else {
        X = b;
        Y = a;
    }
    *x = (long double)X * scale / kThree.d + pos[0];
    *y = (long double)Y * scale / kThree.d + pos[1];
    *z = (long double)cz * scale / kThree.d + pos[2];
}

/* height of the height map at (x, y), bilinear between the four samples */
static long double height_at(long double x, long double y, const signed char *map, float x0, float y0, float scale,
                       int shift)
{
    long double u = (x - x0) * scale, fu, fv, a1;
    float v = (y - y0) * scale, h01, h10, a0;
    int iu = x87_fistp(u), iv = x87_fistp(v), row, row2;
    long double h00;

    fu = u - iu;
    fv = v - (long double)iv;
    row = iv + SHL(iv, shift);
    h00 = map[row + iu];
    h01 = map[row + iu + 1];
    row2 = (iv + 1) + SHL(iv + 1, shift) + iu + 1;
    h10 = map[row2 - 1];
    a0 = (h01 - h00) * fu + h00;
    a1 = ((long double)map[row2] - h10) * fu + h10;
    return ((a1 - a0) * fv + a0 - kHeightBias.d) * k256.f;
}

/* Builds the meshes of the terrain cells QCube picked this frame: vertices
   from the cell's shape (mirrored by the cell flags, raised by the height
   map), then per vertex its material from the material maps, then two
   texture streams per material and one lightmap stream. A mesh that
   outgrows its pool element moves to a bigger pool. */
void TerrainMesh_Build(void)
{
    TerrainMatEntry mats[5];
    TerrainVertRec rec[63];
    TerrainMatRec mrec[6];
    int list[32];
    unsigned int key = 0xFFFFFFFF;
    float lmScale = 0, lmBias = 0, lmX0 = 0, lmY0 = 0, hmX0 = 0, hmY0 = 0, hmScale = 0;
    int lmTexture = 0, hmShift = 0, count, ci;
    const signed char *hmap = NULL;

    gUnkC31090 += gTerrainCellCount;
    mats[4].shift = 0;
    count = gTerrainCellCount;
    for (ci = 0; ci < count; ci++) {
        TerrainCell *cell = &gTerrainCells[ci];
        QCacheEntry *cache = cell->cache;
        unsigned int flags = cell->flags, f1 = flags & 1, mask, lm;
        int level = cell->level, height = cell->maps >> 16 & 0xFF, hasLm, n0, n, size, total, cnt, i, j, m;
        double pos[3], scale, px, py, pz;
        const int16_t *shape;
        const uint32_t *codes, *c;
        TerrainMesh *mesh;
        TerrainMeshVertex *v, *vend;
        uint32_t *w;
        long double L;

        memcpy(pos, cell->pos, sizeof(pos));
        lm = cell->maps & 0xFFFF;
        if (gTerrainPool0.used <= gTerrainPool0.count - 3) {
            cache->data = (char *)pool_take(&gTerrainPool0);
            cache->kind = 0;
        } else if (gTerrainPool1.used <= gTerrainPool1.count - 3) {
            cache->data = (char *)pool_take(&gTerrainPool1);
            cache->kind = 1;
        } else if (gTerrainPool2.used <= gTerrainPool2.count - 3) {
            cache->data = (char *)pool_take(&gTerrainPool2);
            cache->kind = 2;
        } else {
            cache->frame = -1;
            continue;
        }
        gTerrainMeshes[gTerrainMeshCount++] = (TerrainMesh *)cache->data;
        mesh = (TerrainMesh *)cache->data;
        shape = cell->shape;
        codes = (const uint32_t *)((const char *)shape + 8 + shape[1]);
        hasLm = 0;
        if (lm != 0xFFFF) {
            const TerrainLightMapInfo *r = &gTerrainLightMaps[lm];

            hasLm = 1;
            lmScale = r->scale * k256th.f;
            lmX0 = r->x0;
            lmY0 = r->y0;
            lmBias = r->bias * k256th.f;
            lmTexture = r->texture;
        }
        if (level < 0xE) {
            height = 0;
        } else if (height) {
            const TerrainHeightMapInfo *r = &gTerrainHeightMaps[height];

            hmap = r->data;
            hmX0 = r->x0;
            hmY0 = r->y0;
            hmScale = r->scale;
            hmShift = r->shift + 3;
        }
        if (key != cell->materials) {
            int k = cell->materials;

            key = k;
            for (i = 0; i < 4; i++, k >>= 8) {
                int b = k & 0xFF;

                if (b) {
                    const TerrainMaterialMapInfo *r = &gTerrainMaterialMaps[b];

                    mats[i].id = r->id;
                    mats[i].bits = r->bits;
                    mats[i].x0 = r->x0;
                    mats[i].y0 = r->y0;
                    mats[i].scale = r->scale;
                    mats[i].shift = r->shift;
                } else {
                    mats[i].shift = 0;
                }
            }
        }
        n0 = shape[2];
        n = shape[3] + n0;
        size = n * 20 - n0 * 8;
        memcpy(mesh->center, pos, sizeof(pos));
        mesh->vertCount = n0 << 16 | n;
        mesh->type = level;
        scale = (int)(1u << ((0x1B - level) & 31));

        /* vertices */
        c = codes;
        v = mesh->verts;
        i = 0;
        do {
            long double x, y, z;
            float h = kZero.f;

            vertex_pos(*c++, flags, f1, scale, pos, &x, &y, &z);
            if (height) {
                float d = height_at(x, y, hmap, hmX0, hmY0, hmScale, hmShift);

                if (level > 0xE)
                    z = z + d;
                else
                    h = d;
            }
            v->x = x;
            v->y = y;
            v->z = z;
            v->unkC = h;
            v++;
        } while (++i < n0);
        if (n != n0) {
            do {
                long double x, y, z;

                *(int *)&v->unkC = *c & 0x3F;
                vertex_pos(*c++, flags, f1, scale, pos, &x, &y, &z);
                /* not rounded to float here, unlike the first loop */
                if (height)
                    z = z + height_at(x, y, hmap, hmX0, hmY0, hmScale, hmShift);
                v->x = x;
                v->y = y;
                v->z = z;
                v++;
            } while (++i < n);
        }
        vend = v;

        /* materials of the vertices */
        mask = 0;
        v = mesh->verts;
        j = 0;
        do {
            TerrainVertRec *r = &rec[j];
            long double x = v->x;
            float y = v->y, z = v->z;
            int mat = 0, other = 0xFF;

            r->lu = (x - lmX0) * lmScale + lmBias;
            r->lv = (y - lmY0) * lmScale + lmBias;
            if (mats[0].shift == 0) {
                if (j >= n0)
                    r->idx = *(int *)&v->unkC;
            } else {
                TerrainMatEntry *e = mats;

                for (;;) {
                    long double dx = x - e->x0;
                    float dy = y - e->y0;
                    int ix = x87_fistp(e->scale * dx);
                    int iy = x87_fistp(e->scale * dy);

                    if (e->bits[(ix >> 3) + SHL(iy, e->shift)] >> (ix & 7) & 1) {
                        mat = e->id;
                        break;
                    }
                    e++;
                    if (!e->shift)
                        break;
                }
                if (j >= n0) {
                    r->idx = *(int *)&v->unkC;
                    other = rec[r->idx].word >> 16;
                    if (other == mat)
                        other = 0xFF;
                }
            }
            r->dx = x - pos[0];
            r->dy = y - pos[1];
            r->dz = z - pos[2];
            r->word = mat << 16 | other;
            mask |= 1u << (mat & 31);
            v++;
        } while (++j < n);
        px = pos[0] + gQCubeOriginD[0];
        py = pos[1] + gQCubeOriginD[1];
        pz = pos[2] + gQCubeOriginD[2];
        list[0] = 0;
        cnt = 0;
        for (i = 0; mask; mask = (int)mask >> 1, i++)
            if (mask & 1)
                list[cnt++] = i;

        /* move to a bigger pool element when the streams do not fit */
        total = size + ((size + 4) * cnt + n * 8) * 2 + 0x3C;
        w = (uint32_t *)vend;
        if (total >= 0x640 && !(cache->kind == 1 && total < 0xC80) && !(cache->kind == 2 && total < 8000)) {
            int kind = cache->kind, moved = 0;
            char *old = cache->data;
            int bytes = (char *)vend - old;

            if (kind < 1 && total < 0xC80 && gTerrainPool1.used < gTerrainPool1.count - 2) {
                cache->data = (char *)pool_take(&gTerrainPool1);
                memcpy(cache->data, old, bytes);
                cache->kind = 1;
                moved = 1;
            } else if (kind < 2 && total < 8000 && gTerrainPool2.used < gTerrainPool2.count - 2) {
                cache->data = (char *)pool_take(&gTerrainPool2);
                memcpy(cache->data, old, bytes);
                cache->kind = 2;
                moved = 1;
            }
            if (moved) {
                w = (uint32_t *)(cache->data + bytes);
                gTerrainMeshes[gTerrainMeshCount - 1] = (TerrainMesh *)cache->data;
            }
            if (kind == 0)
                pool_release(&gTerrainPool0, old);
            else if (kind == 1)
                pool_release(&gTerrainPool1, old);
            else if (kind == 2)
                pool_release(&gTerrainPool2, old);
            if (!moved) {
                cache->data = NULL;
                cache->frame = -1;
                gTerrainMeshCount--;
                continue;
            }
        }
        w[0] = flags;
        w[1] = (uint32_t)(uintptr_t)(shape + 4);
        w[2] = cnt;
        w[3] = shape[0];
        w += 4;

        /* texture placement of each material at this level */
        L = 0x40000000 >> (level & 31);
        m = 0;
        do {
            int mat = list[m], sh1 = (0x17 - level) & 31, sh2 = (0x16 - level) & 31, ax, ay, bx, by;
            const TerrainLevelMat *t = (const TerrainLevelMat *)((char *)0xC28FE4 + mat * 900 + level * 28);
            TerrainMatRec *r = &mrec[m];
            long double fA = t->scaleA;
            float fB = t->scaleB;

            ax = x87_fistp(fA * px) >> sh1 & 0x7FF;
            ay = x87_fistp(fA * py) >> sh1 & 0x7FF;
            bx = x87_fistp(fB * px) >> sh2 & 0x7FF;
            by = x87_fistp(fB * py) >> sh2 & 0x7FF;
            r->tileA = gTerrainTileMaps[(ay >> 8) + t->tileA * 8][ax >> 8];
            r->tileB = gTerrainTileMaps[(by >> 8) + t->tileB * 8][bx >> 8];
            r->f[7] = fA * kHalf.f / L;
            r->f[6] = ((long double)(ax & 0xFF) + kOne.f) * k256th.f;
            r->f[5] = ((long double)(ay & 0xFF) + kOne.f) * k256th.f;
            r->f[4] = ((long double)(x87_fistp(fA * pz) >> sh1 & 0xFF) + kOne.f) * k256th.f;
            r->f[3] = fB / L;
            r->f[2] = ((long double)(bx & 0xFF) + kOne.f) * k256th.f;
            r->f[1] = ((long double)(by & 0xFF) + kOne.f) * k256th.f;
            r->f[0] = ((long double)(x87_fistp(fB * pz) >> sh2 & 0xFF) + kOne.f) * k256th.f;
        } while (++m < cnt);

        /* two streams per material */
        for (m = 0; m < cnt; m++) {
            const TerrainMatRec *r = &mrec[m];
            long double A0 = r->f[0], A1 = r->f[1], A2 = r->f[2], A3 = r->f[3], A4 = r->f[4];
            float r5 = r->f[5], r6 = r->f[6], r7 = r->f[7];
            uint32_t *sa = w, *sb = (uint32_t *)((char *)w + size + 4);
            unsigned int left = n;

            w = (uint32_t *)((char *)w + 2 * (size + 4));
            *sa++ = gTerrainTextures + 0xC + r->tileB * 0x24;
            *sb++ = gTerrainTextures + 0xC + r->tileA * 0x24;
            c = codes;
            j = 0;
            do {
                const TerrainVertRec *t = &rec[j], *P;
                int sA = *c >> 8 & 3, sB = *c >> 6 & 3, vm = t->word >> 16, other = t->word & 0xFF, k, flag;
                long double q1, r0, e0, e1;
                float q0, r1, dq0, dq1, dr0, dr1;

                c++;
                k = j >= n0 ? t->idx : j;
                P = &rec[k];
                if (sA) {
                    int sel = (sA ^ f1) & 1;
                    float base = sel ? A1 : A2;

                    q1 = t->dz * A3 + A0;
                    q0 = (sel ? t->dy : t->dx) * A3 + base;
                    dq1 = P->dz * A3 + A0 - q1;
                    e0 = (sel ? P->dy : P->dx) * A3 + base;
                } else {
                    q1 = t->dy * A3 + A1;
                    q0 = t->dx * A3 + A2;
                    dq1 = P->dy * A3 + A1 - q1;
                    e0 = P->dx * A3 + A2;
                }
                dq0 = e0 - q0;
                if (sB) {
                    int sel = (sB ^ f1) & 1;
                    float base = sel ? r5 : r6;

                    r0 = (sel ? t->dy : t->dx) * r7 + base;
                    r1 = t->dz * r7 + A4;
                    dr0 = (sel ? P->dy : P->dx) * r7 + base - r0;
                    e1 = P->dz * r7 + A4;
                } else {
                    r0 = t->dx * r7 + r6;
                    r1 = t->dy * r7 + r5;
                    dr0 = P->dx * r7 + r6 - r0;
                    e1 = P->dy * r7 + r5;
                }
                dr1 = e1 - r1;
                if (list[m] == other)
                    flag = 8;
                else if (list[m] == vm)
                    flag = other != 0xFF ? 0x10 : 0x18;
                else
                    flag = 0;
                k = ((k << 5) + flag) | k << 16;
                sa[0] = k;
                ((float *)sa)[1] = q0;
                ((float *)sa)[2] = q1;
                sb[0] = k + 4;
                ((float *)sb)[1] = r0;
                ((float *)sb)[2] = r1;
                sa += 3;
                sb += 3;
                if (j >= n0) {
                    ((float *)sa)[0] = dq0;
                    ((float *)sa)[1] = dq1;
                    ((float *)sb)[0] = dr0;
                    ((float *)sb)[1] = dr1;
                    sa += 2;
                    sb += 2;
                }
                j++;
            } while (--left);
        }

        /* lightmap stream */
        if (hasLm) {
            unsigned int left = n;

            *w++ = lmTexture;
            j = 0;
            do {
                const TerrainVertRec *t = &rec[j];
                int k = j < n0 ? j : t->idx;

                w[0] = k << 5 | k << 16;
                ((float *)w)[1] = t->lu;
                ((float *)w)[2] = t->lv;
                w += 3;
                if (j >= n0) {
                    ((float *)w)[0] = (long double)rec[k].lu - t->lu;
                    ((float *)w)[1] = (long double)rec[k].lv - t->lv;
                    w += 2;
                }
                j++;
            } while (--left);
        } else {
            *w = 0;
        }
    }
}
HOOK(0x510010, TerrainMesh_Build);

void TerrainMesh_Render(void)
{
    setup_view();
    if (gTerrainMeshCount && Direct3D_GetRenderMode() == RenderMode_GetCurrent())
        TerrainMesh_DrawD3D();
}
HOOK(0x511500, TerrainMesh_Render);

void sub_5116B0(Terrain *t)
{
    gUnkA7DA40 = t->textures;
}
HOOK(0x5116B0, sub_5116B0);

void TerrainMesh_ClipTriangle(const TerrainClipVertex *v0, const TerrainClipVertex *v1, const TerrainClipVertex *v2,
                              const double *p0, const double *p1, const double *p2, const double *uv0,
                              const double *uv1, const double *uv2, int code0, int code1, int code2);

/* the C runtime's fmod helper, which takes its arguments on the x87 stack */
static inline long double crt_fmod(long double x, long double y)
{
    long double r;

    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "r"(FN(0x4A56DA, crt_CIfmod)) : "st(1)", "eax", "ecx", "edx", "memory",
                     "cc");
    return r;
}

static long double clamp01(long double v)
{
    if (kZero.f > v)
        return kZero.f;
    if (!(kOne.f >= v))
        return kOne.f;
    return v;
}

/* Projects a lit, fogged triangle and passes it to TerrainMesh_ClipTriangle
   when it crosses the near plane. Mode 0 maps the texture by world x/y, mode
   1 takes the triangle's own uv; other modes leave uv unset (stack garbage in
   the original). */
void TerrainMesh_DrawTriangle(TerrainPoly *t)
{
    double scaleX = gViewScaleX, scaleY = gViewScaleY, left = gSceneX, top = gSceneY;
    double right = (long double)gSceneW + gSceneX;
    double bottom = (long double)gSceneH + gSceneY;
    double centerX = (long double)gSceneHalfW + gSceneX;
    double centerY = (long double)gSceneHalfH + top;
    double c[3], rel[3][3], uv[3][2] = { { 0 } }, view[3][3], t0, t1;
    long double cx, cy, cz, vx, vy, vz, t2, m;
    TerrainClipVertex v[3];
    float k = 0, fog[3], out;
    int code[3], i, j;

    memset(v, 0, sizeof(v));
    cx = (t->pos[1][0] + t->pos[0][0] + t->pos[2][0]) * kThird.d;
    c[0] = cx;
    cy = (t->pos[0][1] + t->pos[1][1] + t->pos[2][1]) * kThird.d;
    c[1] = cy;
    cz = (t->pos[1][2] + t->pos[2][2] + t->pos[0][2]) * kThird.d;
    c[2] = cz;
    rel[0][0] = t->pos[0][0] - cx;
    rel[0][1] = t->pos[0][1] - cy;
    rel[0][2] = t->pos[0][2] - cz;
    rel[1][0] = t->pos[1][0] - cx;
    rel[1][1] = t->pos[1][1] - cy;
    rel[1][2] = t->pos[1][2] - c[2];
    rel[2][0] = t->pos[2][0] - cx;
    rel[2][1] = t->pos[2][1] - c[1];
    rel[2][2] = t->pos[2][2] - c[2];
    Debug_Log(0);
    if (t->mode == 0) {
        double a;
        long double b;

        k = kMapBlend.f;
        a = crt_fmod(c[0], kMapPeriod.d) * kMapScale.d;
        b = crt_fmod(c[1], kMapPeriod.d) * kMapScale.d;
        for (i = 0; i < 3; i++) {
            uv[i][0] = rel[i][0] * kMapScale.d + a;
            uv[i][1] = rel[i][1] * kMapScale.d + b;
        }
    } else if (t->mode == 1) {
        k = kUVBlend.f;
        for (i = 0; i < 3; i++) {
            uv[i][0] = t->uv[i][0];
            uv[i][1] = t->uv[i][1];
        }
    }
    vx = c[0] - gViewPos[0];
    vy = c[1] - gViewPos[1];
    vz = c[2] - gViewPos[2];
    t0 = gViewMatrix[2] * vz + gViewMatrix[1] * vy + gViewMatrix[0] * vx;
    t1 = gViewMatrix[5] * vz + gViewMatrix[4] * vy + gViewMatrix[3] * vx;
    t2 = gViewMatrix[8] * vz + gViewMatrix[7] * vy + gViewMatrix[6] * vx;
    for (i = 0; i < 3; i++) {
        double rx = gViewMatrix[2] * rel[i][2] + gViewMatrix[0] * rel[i][0] + gViewMatrix[1] * rel[i][1];
        double ry = gViewMatrix[5] * rel[i][2] + gViewMatrix[3] * rel[i][0] + gViewMatrix[4] * rel[i][1];
        double rz = gViewMatrix[8] * rel[i][2] + gViewMatrix[6] * rel[i][0] + gViewMatrix[7] * rel[i][1];

        view[i][0] = t0 + rx;
        view[i][1] = t1 + ry;
        view[i][2] = t2 + rz;
    }
    if (view[1][2] > view[0][2])
        m = view[2][2] > view[1][2] ? view[2][2] : view[1][2];
    else
        m = view[0][2];
    m = m * kMaxZScale.d;
    if (m >= kOneD.d)
        m = kOneD.d;
    Debug_Log((unsigned int)x87_fistp((m * (kOne.f - k) + k) * k255.f) << 24);
    for (i = 0; i < 3; i++) {
        long double r, sx, py, q;
        float rhw, sy;

        if (!(view[i][2] > kClipZ.d)) {
            code[i] = 0x10;
            continue;
        }
        r = kOneD.d / view[i][2];
        rhw = r;
        sx = r * view[i][0] * scaleX + centerX;
        py = rhw * view[i][1] * scaleY + centerY;
        sy = py;
        q = rhw * k80.f;
        v[i].u = uv[i][0] * q;
        v[i].v = uv[i][1] * q;
        v[i].sx = sx;
        v[i].sy = py;
        v[i].q = q;
        code[i] = (sx >= right) | !(sx >= left) << 1 | (sy >= bottom) << 2 | !(sy >= top) << 3;
    }
    for (i = 0; i < 3; i++) {
        if (view[i][2] > kZero.f) {
            Fog_Compute(&out, t->pos[i], (float)view[i][2], gTerrainFogScale, t->fogLayers, t->fogCount);
            fog[i] = out * k65535.f;
        } else {
            fog[i] = 0.0f;
        }
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++)
            v[i].c[j] = clamp01(gTerrainTint[j] * t->color[i][j]) * k255.f;
        v[i].c[3] = fog[i];
    }
    if ((code[0] | code[1] | code[2]) & 0x10 && !(code[0] & code[1] & code[2] & 0x10))
        TerrainMesh_ClipTriangle(&v[0], &v[1], &v[2], view[0], view[1], view[2], uv[0], uv[1], uv[2], code[0],
                                 code[1], code[2]);
}
HOOK(0x5116D0, TerrainMesh_DrawTriangle);

/* Moves vertex a of the clip slots onto the near plane along the edge to b. */
void TerrainMesh_ClipEdge(int a, int b)
{
    const double *P = gTerrainClipPos[a], *Q = gTerrainClipPos[b];
    const double *ua = gTerrainClipUV[a], *ub = gTerrainClipUV[b];
    TerrainClipVertex *va = &gTerrainClipVerts[a], *vb = &gTerrainClipVerts[b];
    double u, v, x, y, c0, c1, c2, syp, sy;
    long double t, c3, sx;

    t = (kClipZ.d - P[2]) / ((long double)Q[2] - P[2]);
    u = (ub[0] - ua[0]) * t + ua[0];
    v = (ub[1] - ua[1]) * t + ua[1];
    x = (Q[0] - P[0]) * t + P[0];
    y = (Q[1] - P[1]) * t + P[1];
    c0 = ((double)vb->c[0] - (double)va->c[0]) * t + (double)va->c[0];
    c1 = ((double)vb->c[1] - (double)va->c[1]) * t + (double)va->c[1];
    c2 = ((double)vb->c[2] - (double)va->c[2]) * t + (double)va->c[2];
    c3 = ((long double)vb->c[3] - va->c[3]) * t + va->c[3];
    sx = gViewScaleX * x * kInvClipZ.d + ((long double)gSceneHalfW + gSceneX);
    syp = gViewScaleY * y * kInvClipZ.d;
    sy = ((long double)gSceneHalfH + gSceneY) + syp;
    va->u = u * kClipUVScale.d;
    va->v = v * kClipUVScale.d;
    va->sx = sx;
    va->sy = sy;
    va->q = kClipQ.f;
    va->c[0] = c0;
    va->c[1] = c1;
    va->c[2] = c2;
    va->c[3] = c3;
}
HOOK(0x5122B0, TerrainMesh_ClipEdge);

/* Clips a triangle with one or two vertices in front of the near plane
   (code 0x10). Slot 3 takes a copy of the vertex that turns into two. */
void TerrainMesh_ClipTriangle(const TerrainClipVertex *v0, const TerrainClipVertex *v1, const TerrainClipVertex *v2,
                              const double *p0, const double *p1, const double *p2, const double *uv0,
                              const double *uv1, const double *uv2, int code0, int code1, int code2)
{
    int from;

    memcpy(gTerrainClipPos[0], p0, 24);
    memcpy(&gTerrainClipVerts[0], v0, 48);
    memcpy(gTerrainClipUV[0], uv0, 16);
    memcpy(gTerrainClipPos[1], p1, 24);
    memcpy(&gTerrainClipVerts[1], v1, 48);
    memcpy(gTerrainClipUV[1], uv1, 16);
    memcpy(gTerrainClipPos[2], p2, 24);
    memcpy(&gTerrainClipVerts[2], v2, 48);
    memcpy(gTerrainClipUV[2], uv2, 16);
    if (code0 & 0x10) {
        if (code1 & 0x10) {
            TerrainMesh_ClipEdge(0, 2);
            TerrainMesh_ClipEdge(1, 2);
            return;
        }
        if (code2 & 0x10) {
            TerrainMesh_ClipEdge(0, 1);
            TerrainMesh_ClipEdge(2, 1);
            return;
        }
        from = 0;
    } else if (code1 & 0x10) {
        if (code2 & 0x10) {
            TerrainMesh_ClipEdge(1, 0);
            TerrainMesh_ClipEdge(2, 0);
            return;
        }
        from = 1;
    } else if (code2 & 0x10) {
        from = 2;
    } else {
        return;
    }
    memcpy(gTerrainClipPos[3], gTerrainClipPos[from], 24);
    memcpy(&gTerrainClipVerts[3], &gTerrainClipVerts[from], 48);
    memcpy(gTerrainClipUV[3], gTerrainClipUV[from], 16);
    TerrainMesh_ClipEdge(from, from == 0 ? 2 : 0);
    TerrainMesh_ClipEdge(3, from == 1 ? 2 : 1);
}
HOOK(0x512040, TerrainMesh_ClipTriangle);
