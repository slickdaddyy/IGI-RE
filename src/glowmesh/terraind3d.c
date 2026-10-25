/* TerrainMesh_DrawD3D: this frame's terrain meshes through Direct3D, with
   distance morphing, fog, backface and screen culling, and near plane and
   screen edge clipping of the triangles that need it. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "hook.h"
#include "glowmesh/glowmesh.h"
#include "res/restypes.h"

typedef struct TdTLVertex {
    float sx, sy, sz, rhw;
    uint32_t color, specular;
    float tu, tv;
} TdTLVertex;

/* a corner passed to TerrDraw_ClipTriangle */
typedef struct TdClipVtx {
    float sx, sy, q, rhw;
    uint32_t color, specular;
    float u, v;
    uint8_t code;
    uint8_t pad21[3];
    float *pos;
    float c[6];
} TdClipVtx;

typedef struct TdClipOut {
    TdTLVertex *verts;
    int *vertCount;
    uint16_t *indices;
    int *triCount;
    int base;
} TdClipOut;

typedef struct TdNode {
    struct TdNode *next;
    struct TdNode *prev;
} TdNode;

typedef struct TdChunk {
    TdNode node;
    uint16_t tris[0x2EE0 / 2];
    int count;
    char *texture; /* +0x20: the surface */
    int pass;
} TdChunk;

typedef struct TdVert {
    float x, y, z;
    union {
        float height;
        int parent;
    };
} TdVert;

typedef struct TdMesh {
    double center[3];
    uint32_t vertCount; /* first-kind count << 16 | count */
    int level;
    TdVert verts[];
} TdMesh;

_Static_assert(sizeof(TdClipVtx) == 0x40, "TdClipVtx");
_Static_assert(sizeof(TdChunk) == 0x2EF4, "TdChunk");
_Static_assert(sizeof(TdMesh) == 0x20, "TdMesh");

ORIG(0x491CF0, int *, Display_GetMode, (void));
ORIG(0x514680, TdChunk *, TerrChunk_Get, (char *texture, int pass));
ORIG(0x5144E0, void, Terrain_SetBlendMode, (int mode));
ORIG(0x514760, void, TerrDraw_ClipTriangle, (TdClipVtx a, TdClipVtx b, TdClipVtx c, TdClipOut *out));
ORIG(0x4AF960, void, List_Remove, (TdNode *node));

extern struct IDirect3DDevice7 *gD3DDevice;
extern IDirect3DVertexBuffer7 *gTerrainVB;
extern TdTLVertex *gTerrVBData;
extern int gTerrVertCount;
extern uint32_t gTerrainFogColorD3D;
extern int gTerrMirror;
extern float gClipZBias;
extern float gClipZScale;
extern D3DVIEWPORT7 gTerrViewport;
extern Pool gTerrainChunkPool;
extern float gTerrainFogScale;
extern float gTerrainFogColor[];
extern TdMesh *gTerrainMeshes[];
extern int gTerrainMeshCount;
extern float gTerrainScaleY;
extern float gTerrainScaleX;
extern float gTerrainCenterX;
extern float gTerrainCenterY;
extern int gMorphStart;
extern float gMorphScale;
extern int gTerrainD3DUnusedInt;
extern int gTerrMeshTotal;
extern float gWorldScale;
extern float gClipRhwScale;
extern uint32_t gD3DZEnable;
extern float gLayer1MaxZ;
extern float gD3DMinZ;
extern float gLayer1MinZ;
extern float gD3DMaxZ;
extern float gFarZ;
extern PtrList *gD3DTextureStates;
extern PtrList *gD3DBlendStates;
extern PtrList *gD3DRenderStates;
extern PtrList *gD3DClipStates;

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(kMinusOne, 0xBF800000);
F32(k255, 0x437F0000);
F32(kMinus255, 0xC37F0000);
F32(kMorphBase, 0x3EC00000);  /* 0.375 */
F32(kSizeRadius, 0x3FDD70A4); /* 1.73 */
F32(kCull, 0xBA83126F);       /* -0.001 */
F64(kMorphLow, 0x3F847AE147AE147BULL);  /* 0.01 */
F64(kMorphHigh, 0x3FEFAE147AE147AEULL); /* 0.99 */

/* the parts of the original stack frame that overlap or are read stale */
#define F_MATRIX 0x1A8
#define F_USED 0x1A8 /* 0: a visible face uses the vertex */
#define F_CODES 0x200
#define F_FLAGS 0x240
#define F_LMSLOTS 0x280 /* inside the face flags */
#define F_MATSLOTS 0x340
#define F_MORPH 0x400
#define F_TL 0x500
#define F_COLORS 0xD00
#define F_SIZE 0x1500

#define TL(F, j) ((TdTLVertex *)((F) + F_TL) + (j))
#define MORPH(F) ((float *)((F) + F_MORPH))

/* e^x as the original computes it: 2^(x * log2 e) with frndint, f2xm1 and fscale */
static __attribute__((noinline)) long double x87_exp(long double x)
{
    long double r;

    __asm__("fldl2e\n\t"
            "fmulp\n\t"
            "fld %%st(0)\n\t"
            "frndint\n\t"
            "fxch %%st(1)\n\t"
            "fsub %%st(1), %%st\n\t"
            "f2xm1\n\t"
            "fld1\n\t"
            "faddp\n\t"
            "fscale\n\t"
            "fstp %%st(1)"
            : "=t"(r)
            : "0"(x));
    return r;
}

/* fchs on the x87 value, so gcc cannot move the negation after a rounding */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

static uint32_t grey3(uint32_t v)
{
    return v | (v << 8 | v) << 8;
}

typedef struct TdScreen {
    float minX, minY, maxX, maxY;
} TdScreen;

/* view space vertex to screen; 1 when it is behind the near plane */
static int td_project(TdTLVertex *tl, uint8_t *code, const TdScreen *scr, float vx, float vy, float vz)
{
    long double q, sx, sy, r;
    float qf, syf;

    if (!(vz >= gNearPlane)) {
        tl->sx = vx;
        tl->sy = vy;
        tl->sz = vz;
        tl->rhw = 0;
        tl->specular = 0xFF000000;
        *code = 0x10;
        return 1;
    }
    q = kOne.f / vz;
    qf = q;
    sx = q * vx * gTerrainScaleX + gTerrainCenterX;
    sy = qf * vy * gTerrainScaleY + gTerrainCenterY;
    syf = sy;
    tl->sx = sx;
    tl->sy = syf;
    tl->rhw = qf * gClipRhwScale;
    tl->sz = qf * gClipZScale + gClipZBias;
    r = kOne.f / x87_exp(vz * gWorldScale * gTerrainFogScale);
    if (!(r >= kZero.f))
        r = kZero.f;
    tl->specular = (uint32_t)x87_fistp(r * k255.f) << 24;
    *code = (syf > scr->maxY) << 3 | (sx > scr->maxX) << 2 | !(sy >= scr->minY) << 1 | !(sx >= scr->minX);
    return 0;
}

/* copies the stream's colour and uv into the used vertices and those into the vertex buffer */
static const uint32_t *td_stream_verts(uint8_t *F, const uint32_t *w, int n0, int n, int lightmap, int *count)
{
    TdTLVertex *vb = gTerrVBData;
    int j = 0;

    do {
        if (!F[F_USED + j]) {
            TdTLVertex *t = TL(F, j);
            t->color = lightmap ? 0xFFFFFF : *(uint32_t *)(F + F_COLORS + (w[0] & 0xFFFF));
            t->tu = ((const float *)w)[1];
            t->tv = ((const float *)w)[2];
            vb[j] = *t;
        }
        w += 3;
    } while (++j < n0);
    if (j != n) {
        do {
            if (!F[F_USED + j]) {
                TdTLVertex *t = TL(F, j);
                float f = MORPH(F)[(int)w[0] >> 16];
                t->color = lightmap ? 0xFFFFFF : *(uint32_t *)(F + F_COLORS + (w[0] & 0xFFFF));
                t->tu = ((const float *)w)[3] * f + ((const float *)w)[1];
                t->tv = ((const float *)w)[4] * f + ((const float *)w)[2];
                vb[j] = *t;
            }
            w += 5;
        } while (++j < n);
    }
    *count = j;
    return w;
}

/* corner v of a clipped face into its slot: colour, uv and fog times q */
static void td_fill_slot(uint8_t *F, uint8_t *s, int v)
{
    TdClipVtx *c = (TdClipVtx *)s;
    uint8_t code = F[F_CODES + v];

    memcpy(s, TL(F, v), sizeof(TdTLVertex));
    c->code = code;
    if (!(code & 0x10)) {
        long double w = gWorldScale * c->rhw;
        float wf = w;

        c->q = wf;
        c->c[0] = (long double)(int)(c->color >> 16 & 0xFF) * w;
        c->c[1] = (long double)(int)(c->color >> 8 & 0xFF) * wf;
        c->c[2] = (long double)(int)(c->color & 0xFF) * wf;
        c->c[3] = c->u * wf;
        c->c[4] = c->v * wf;
        c->c[5] = (long double)(int)(c->specular >> 24) * wf;
    }
}

/* the faces of one stream into its render chunk */
static void td_stream_faces(uint8_t *F, int slots, const uint32_t *faces, int faceCount, TdChunk *chunk,
                            TdTLVertex *vb, int *vcount, int base)
{
    uint32_t idx = ((uint32_t)base << 16) + base;
    uint8_t *tris = (uint8_t *)chunk->tris + chunk->count * 6, *tp = tris;
    const uint32_t *fp = faces, *end = faces + faceCount;
    const uint8_t *fl = F + F_FLAGS;

    do {
        uint8_t flag = *fl++;
        uint32_t face = *fp;

        if (!flag) {
            *(uint32_t *)tp = (face & 0xFF00FF) + idx;
            *(uint16_t *)(tp + 4) = (uint16_t)((face >> 8 & 0xFF) + idx);
            tp += 6;
        } else if (!(flag & 0x20)) {
            int triCount = (int)(tp - tris) / 6;
            TdClipOut out;

            td_fill_slot(F, F + slots, face & 0xFF);
            td_fill_slot(F, F + slots + 0x40, face >> 16 & 0xFF);
            td_fill_slot(F, F + slots + 0x80, face >> 8 & 0xFF);
            out.verts = vb;
            out.vertCount = vcount;
            out.indices = (uint16_t *)tris;
            out.triCount = &triCount;
            out.base = base;
            TerrDraw_ClipTriangle(*(TdClipVtx *)(F + slots), *(TdClipVtx *)(F + slots + 0x40),
                                  *(TdClipVtx *)(F + slots + 0x80), &out);
            tp = tris + triCount * 6;
        }
    } while (++fp < end);
    chunk->count += (int)(tp - tris) / 6;
}

void TerrainMesh_DrawD3D(void)
{
    uint8_t F[F_SIZE] __attribute__((aligned(8)));
    float *ident = (float *)(F + F_MATRIX);
    TdScreen scr;
    double vp[3];
    float m[10], fC, morphK, vpx, vpy, vpz;
    long double zr, range;
    int count, mi, r, g, b;

    scr.minX = gScene.minX;
    scr.minY = gScene.minY;
    scr.maxX = gScene.maxX;
    scr.maxY = gScene.maxY;
    zr = kOne.f / (kOne.f - gD3DMinZ / gD3DMaxZ);
    range = gLayer1MaxZ - gLayer1MinZ;
    gClipZScale = x87_neg(range * gClipRhwScale * gD3DMinZ * zr);
    gClipZBias = range * zr + gLayer1MinZ;
    gD3DTextureStates->current = -1;
    gD3DBlendStates->current = -1;
    gD3DRenderStates->current = -1;
    gD3DClipStates->current = -1;
    gTerrViewport.dwX = 0;
    gTerrViewport.dwY = 0;
    gTerrViewport.dwWidth = Display_GetMode()[1];
    gTerrViewport.dwHeight = Display_GetMode()[2];
    gTerrViewport.dvMinZ = gLayer1MinZ;
    gTerrViewport.dvMaxZ = gLayer1MaxZ;
    r = x87_fistp(gTerrainFogColor[0] * kMinus255.f);
    g = x87_fistp(gTerrainFogColor[1] * kMinus255.f);
    b = x87_fistp(gTerrainFogColor[2] * k255.f);
    gTerrainFogColorD3D = (uint32_t)b - ((((uint32_t)r << 8) + g) << 8);

    memset(ident, 0, 16 * sizeof(float));
    ident[0] = ident[5] = ident[10] = ident[15] = 1.0f;
    IDirect3DDevice7_SetTransform(gD3DDevice, D3DTRANSFORMSTATE_WORLD, (D3DMATRIX *)ident);
    IDirect3DDevice7_SetViewport(gD3DDevice, &gTerrViewport);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x88, 1);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x26, *(DWORD *)&gTerrainFogScale);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x23, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x8C, 3);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x30, 1);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x24, 0x461C4000); /* 10000.0f */
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x25, 0x46EA6000); /* 30000.0f */
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x2F, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x1B, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x16, 1);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x89, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x8D, 1);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x1A, 1);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x0F, 0);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x07, gD3DZEnable);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x0E, 1);
    IDirect3DDevice7_SetRenderState(gD3DDevice, 0x1D, 0);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x11, 2);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x10, 2);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x0C, 3);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x12, 2);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x0B, 0);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x04, 1);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x01, 4);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x02, 2);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 0, 0x03, 0);
    IDirect3DDevice7_SetTextureStageState(gD3DDevice, 1, 0x01, 1);
    gTerrVertCount = 0;
    if (IDirect3DVertexBuffer7_Lock(gTerrainVB, 0x2020, (void **)&gTerrVBData, NULL))
        return;

    count = gTerrainMeshCount;
    gTerrMeshTotal += count;
    fC = gMorphStart;
    memcpy(vp, &gTransContext.pos, sizeof(vp));
    gTerrainD3DUnusedInt = 0;
    memcpy(m, &gTransContext.matrix, sizeof(m));
    morphK = gMorphScale;
    vpx = vp[0];
    vpy = vp[1];
    vpz = vp[2];

    for (mi = 0; mi < count; mi++) {
        TdMesh *mesh = gTerrainMeshes[mi];
        const uint32_t *w, *faces;
        float nvx = -vpx, nvy = -vpy, nvz = -vpz, mscale, sign0, sign;
        int n0, n, level, size, j, bits, behind, cnt, faceCount, first, mat, pass, all, k, pos, vcount;
        unsigned int flags;
        long double dx, dy, dz;
        uint8_t *fl;

        if (!mesh)
            continue;
        memcpy(F + 0x1E8, mesh->center, sizeof(mesh->center));
        n0 = (int)mesh->vertCount >> 16;
        n = mesh->vertCount & 0xFFFF;
        level = mesh->level;
        mscale = kMorphBase.f / (long double)(int)(1u << ((25 - level) & 31));
        size = 0x40000000 >> (level & 31);
        dx = mesh->center[0] - vp[0];
        dy = mesh->center[1] - vp[1];
        dz = mesh->center[2] - vp[2];
        if (!((long double)size * kSizeRadius.f + ((m[8] * dz + m[7] * dy) + m[6] * dx) < gFarZ))
            continue;

        /* first-kind vertices: morph factor from the distance, colours, projection */
        bits = 0;
        behind = 0;
        j = 0;
        do {
            const TdVert *v = &mesh->verts[j];
            uint32_t *col = (uint32_t *)(F + F_COLORS) + 8 * j;
            long double x = v->x + nvx, y, z, ax, ay, az, mx, f, gg, Z;
            float yf, zz, vx, vy, vz;
            uint32_t c1, c2, inv;

            ax = !(x >= kZero.f) ? x87_neg(x) : x;
            y = v->y + nvy;
            yf = y;
            ay = !(y >= kZero.f) ? x87_neg(y) : y;
            mx = ay > ax ? (long double)(float)ay : ax;
            z = v->z + nvz;
            az = !(z >= kZero.f) ? x87_neg(z) : z;
            if (az > mx)
                mx = (float)az;
            f = (mx * mscale - fC) * morphK;
            if (!(f >= kZero.f))
                f = kZero.f;
            else if (f > kOne.f)
                f = kOne.f;
            MORPH(F)[j] = f;
            if (f > kMorphLow.d)
                bits |= 2;
            if (!(f >= kMorphHigh.d))
                bits |= 1;
            gg = kOne.f - f;
            zz = gg * v->height + v->z;
            c1 = grey3(x87_fistp(gg * k255.f));
            c2 = grey3(x87_fistp(gg * k255.f * gg));
            inv = 0xFFFFFF - c1;
            Z = zz + nvz;
            vx = (Z * m[2] + yf * m[1]) + x * m[0];
            vy = (Z * m[5] + yf * m[4]) + x * m[3];
            vz = (Z * m[8] + yf * m[7]) + x * m[6];
            behind |= td_project(TL(F, j), F + F_CODES + j, &scr, vx, vy, vz);
            col[0] = 0;
            col[1] = 0;
            col[2] = c1 - c2;
            col[3] = inv - c1 + c2;
            col[4] = c2;
            col[5] = c1 - c2;
            col[6] = c1;
            col[7] = inv;
            F[F_USED + j] = 1;
        } while (++j < n0);

        /* later vertices move towards their parent by the parent's factor */
        if (j != n) {
            do {
                const TdVert *v = &mesh->verts[j], *P = &mesh->verts[v->parent];
                float f = MORPH(F)[v->parent], y2, z2, vx, vy, vz;
                long double X = (P->x - v->x) * f + v->x, Y, Z;

                y2 = (P->y - v->y) * f + v->y;
                z2 = (P->z - v->z) * f + v->z;
                X = X + nvx;
                Y = y2 + nvy;
                Z = z2 + nvz;
                vx = (Z * m[2] + Y * m[1]) + X * m[0];
                vy = (Z * m[5] + Y * m[4]) + X * m[3];
                vz = (Z * m[8] + Y * m[7]) + X * m[6];
                behind |= td_project(TL(F, j), F + F_CODES + j, &scr, vx, vy, vz);
                F[F_USED + j] = 1;
            } while (++j < n);
        }

        w = (const uint32_t *)&mesh->verts[j];
        flags = w[0];
        faces = (const uint32_t *)w[1];
        cnt = w[2];
        faceCount = w[3];
        w += 4;
        gTerrMirror = flags >> 2 & 1;

        if (behind) {
            /* every face goes to the clipper */
            if (faceCount > 0)
                memset(F + F_FLAGS, 0x10, faceCount);
            if (n > 0)
                memset(F + F_USED, 0, n);
        } else {
            /* strips: backface and screen culling, marking the vertices in use */
            float sxA = 0, syA = 0, sxB = 0, syB = 0, sxC = 0, syC = 0;
            uint8_t cA = 0, cB = 0, cC = 0, *uA = NULL, *uB = NULL, *uC = NULL;
            const uint32_t *fp = faces;

            sign0 = gTerrMirror ? kMinusOne.f : kOne.f;
            sign = sign0;
            fl = F + F_FLAGS;
            all = -1;
            pos = 0;
            k = 0;
            do {
                uint32_t face = *fp++;
                int c = face & 0xFF, culled = 1;
                uint8_t flag;

                if (pos) {
                    sxA = sxB;
                    syA = syB;
                    sxB = sxC;
                    syB = syC;
                    cA = cB;
                    cB = cC;
                    uA = uB;
                    uB = uC;
                } else {
                    int a = face >> 16 & 0xFF, bb = face >> 8 & 0xFF;

                    uA = F + F_USED + a;
                    cA = F[F_CODES + a];
                    sxA = TL(F, a)->sx;
                    syA = TL(F, a)->sy;
                    uB = F + F_USED + bb;
                    cB = F[F_CODES + bb];
                    sxB = TL(F, bb)->sx;
                    syB = TL(F, bb)->sy;
                }
                uC = F + F_USED + c;
                cC = F[F_CODES + c];
                sxC = TL(F, c)->sx;
                syC = TL(F, c)->sy;
                flag = cA | cB | cC | 0x20;
                if (!(cA & cB & cC)) {
                    long double cross = ((long double)(sxB - sxC) * (syA - syC) - (sxA - sxC) * (syB - syC)) * sign;

                    if (!(cross >= kCull.f)) {
                        *uC = 0;
                        *uB = 0;
                        *uA = 0;
                        flag ^= 0x20;
                        culled = 0;
                    }
                }
                *fl++ = flag;
                sign = sign * kMinusOne.f;
                all &= culled;
                pos++;
                if (face & 0x10000000) {
                    pos = 0;
                    sign = sign0;
                }
            } while (++k < faceCount);
            if (all)
                continue;
        }

        /* two streams per material, chosen by the morph factors */
        first = 1;
        mat = 0;
        do {
            for (pass = 1; pass < 3; pass++) {
                TdChunk *chunk;
                TdTLVertex *vb;
                char *tex;
                int base;

                if (!(bits & pass)) {
                    w = (const uint32_t *)((const char *)w + n * 20 - n0 * 8 + 4);
                    continue;
                }
                tex = (char *)w[0];
                w++;
                vb = gTerrVBData;
                base = gTerrVertCount;
                w = td_stream_verts(F, w, n0, n, 0, &vcount);
                chunk = TerrChunk_Get(tex, first ? 0 : 1);
                first = 0;
                td_stream_faces(F, F_MATSLOTS, faces, faceCount, chunk, vb, &vcount, base);
                gTerrVBData += vcount;
                gTerrVertCount += vcount;
            }
        } while (++mat < cnt);

        /* lightmap stream */
        {
            char *tex = (char *)w[0];
            TdChunk *chunk;
            TdTLVertex *vb;
            int base;

            w++;
            if (!tex)
                continue;
            base = gTerrVertCount;
            w = td_stream_verts(F, w, n0, n, 1, &vcount);
            chunk = TerrChunk_Get(tex, 2);
            vb = gTerrVBData;
            td_stream_faces(F, F_LMSLOTS, faces, faceCount, chunk, vb, &vcount, base);
            gTerrVBData += vcount;
            gTerrVertCount += vcount;
        }
    }

    IDirect3DVertexBuffer7_Unlock(gTerrainVB);
    for (mi = 0; mi < 3; mi++) {
        int i = gTerrainChunkPool.used;

        Terrain_SetBlendMode(mi);
        while (i) {
            TdChunk *c = (TdChunk *)(gTerrainChunkPool.data + gTerrainChunkPool.free[--i] * gTerrainChunkPool.stride);

            if (c->count && c->pass == mi) {
                IDirect3DDevice7_SetTexture(gD3DDevice, 0, *(IDirectDrawSurface7 **)(c->texture + 0x20));
                IDirect3DDevice7_DrawIndexedPrimitiveVB(gD3DDevice, D3DPT_TRIANGLELIST, gTerrainVB, 0, gTerrVertCount,
                                                        c->tris, c->count * 3, 0);
                c->count = 0;
            }
        }
    }

    /* every chunk back to the pool */
    while (gTerrainChunkPool.used) {
        TdChunk *c = (TdChunk *)(gTerrainChunkPool.data + gTerrainChunkPool.stride * gTerrainChunkPool.free[0]);
        int last = --gTerrainChunkPool.used, i = *(int *)((char *)c + gTerrainChunkPool.size);

        if (i != last) {
            char *o = gTerrainChunkPool.data + gTerrainChunkPool.free[last] * gTerrainChunkPool.stride;
            int jj = *(int *)(o + gTerrainChunkPool.size), t = gTerrainChunkPool.free[i];

            gTerrainChunkPool.free[i] = gTerrainChunkPool.free[jj];
            gTerrainChunkPool.free[jj] = t;
            *(int *)(o + gTerrainChunkPool.size) = i;
        }
        List_Remove(&c->node);
    }
}
HOOK(0x512C30, TerrainMesh_DrawD3D);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A7E06C) int gTerrMirror;
BSS(A7E074) float gClipZBias;
BSS(A7E074) int gTerrVertCount;
BSS(A7F860) D3DVIEWPORT7 gTerrViewport;
BSS(A7F860) Pool gTerrainChunkPool;
BSS(A7F894) float gClipZScale;
PAD(igb, A7F894, A7F898, 0x4);
BSS(A7F894) TdTLVertex *gTerrVBData;
BSS(BCAD70) float gClipRhwScale;
BSS(BCAD8C) float gLayer1MaxZ;
BSS(BCADB4) float gLayer1MinZ;
BSS(BCADCC) float gFarZ;
BSS(C28FB8) int gTerrainD3DUnusedInt;
