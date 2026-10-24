#pragma GCC optimize("excess-precision=standard")

#include <stddef.h>
#include <string.h>

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "hook.h"
#include "igi.h"
#include "model/model.h"
#include "d3d/d3dtypes.h"
#include "litmap/litmaptypes.h"
#include "render/rendertypes.h"
#include "transctx/transctxtypes.h"

/* Direct3D mesh renderer: rigid, spline and lightmapped meshes. */

typedef struct VBUpload {
    struct VBUpload *next;
    char unk4[8];
    uint16_t start;
    uint16_t count;
    IDirect3DVertexBuffer7 **vb;
    const void *src;
} VBUpload;

/* a static model vertex buffer (0x7AC0A0) */
typedef struct ModelVB {
    IDirect3DVertexBuffer7 *vb;
    D3DVERTEXBUFFERDESC desc;
    int stride;
    VBUpload *uploads;
    char unk1C[8];
} ModelVB;

typedef struct ModelSlot {
    char unk0[0xFA4];
    int used;
} ModelSlot;

typedef struct LmGroup {
    float center[3];
    uint16_t indexCount;
    uint16_t next;
    uint16_t texture;
    uint16_t lightmap;
    uint16_t base;
    uint16_t vertexCount;
    uint16_t sorted;
    uint8_t unk1A[2];
    uint8_t flags;
    uint8_t unk1D[2];
    uint8_t alpha;
    uint16_t indices[];
} LmGroup;

/* render data of a rigid or spline mesh, vertices are 8 floats */
typedef struct RigidMesh {
    int primType;
    char unk4[8];
    int vertexCount;
    float *vertices;
    MeshGroup *groups;
    char unk18[0xC];
    uint16_t vbBase;
    uint16_t unk26;
    IDirect3DVertexBuffer7 **vb;
} RigidMesh;

/* render data of a lightmapped mesh, vertices are 10 floats */
typedef struct LmMesh {
    int primType;
    char unk4[0xC];
    int vertexCount;
    float *vertices;
    int unk18;
    LmGroup *groups;
    char unk20[8];
    int uploaded;
    uint16_t vbBase;
    uint16_t unk2E;
    IDirect3DVertexBuffer7 **vb;
} LmMesh;

typedef struct ModelObj {
    char task[0x1C];
    uint16_t type;
    char unk1E[2];
    double pos[3];
    char unk38[0xC];
    float radius;
    char unk48[0x24];
    VirModel *model;
    Matrix matrix;
    float scale[3];
    char unkA4[0x34];
    int unkD8;
    LightmapRef *lightmaps;
    char unkE0[0xC];
    int layer;
    char unkF0[0xC];
    float splineMin;
    float splineMax;
    float points[21][3];
    Matrix segs[21];
    char unk548[0x54];
    int segCount;
    float *offset;  /* x, y, z, radius */
    char unk5A4[0x10];
    char straight;  /* not bent along the spline */
    char reverse;
} ModelObj;

/* vertex projected by Direct3DRender_SplineNearZ */
typedef struct ProjVert {
    float x, y, z;
    float sx, sy, rhw;
    int clip;
} ProjVert;

typedef struct NearZQuery {
    int unk0;
    int unk4;
    float z;
} NearZQuery;

typedef struct SplinePrim {
    double pos[3];
    int unk18;
    ModelLod *lod;
    RigidMesh *mesh;
    MeshGroup *group;
    ModelObj *obj;
} SplinePrim;

typedef struct SortedPrim {
    double pos[3];
    Matrix matrix;
    float scale[3];
    int unk4C;
    ModelLod *lod;
    void *mesh;
    void *group;
    int layer;
    union {
        int *anim;
        Lightmaps *lightmap;
    };
    int unk64;
} SortedPrim;

_Static_assert(sizeof(ModelVB) == 0x24, "ModelVB");
_Static_assert(sizeof(ModelSlot) == 0xFA8, "ModelSlot");
_Static_assert(offsetof(LmGroup, indices) == 0x20, "LmGroup");
_Static_assert(offsetof(RigidMesh, vb) == 0x28, "RigidMesh");
_Static_assert(offsetof(LmMesh, vb) == 0x30, "LmMesh");
_Static_assert(offsetof(ModelObj, model) == 0x6C, "ModelObj model");
_Static_assert(offsetof(ModelObj, unkD8) == 0xD8, "ModelObj D8");
_Static_assert(offsetof(ModelObj, layer) == 0xEC, "ModelObj layer");
_Static_assert(offsetof(ModelObj, segs) == 0x200, "ModelObj segs");
_Static_assert(offsetof(ModelObj, segCount) == 0x59C, "ModelObj segCount");
_Static_assert(offsetof(ModelObj, straight) == 0x5B4, "ModelObj straight");
_Static_assert(sizeof(ProjVert) == 0x1C, "ProjVert");
_Static_assert(sizeof(SortedPrim) == 0x68, "SortedPrim");

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x491D40, long double, Display_GetAspect, (void));
ORIG(0x4950F0, void, Direct3D_MakeMatrix, (D3DMATRIX *out, const float *rot, const float *pos));
ORIG(0x497E70, void, TransContext_Select, (TransCtx *v));
ORIG(0x49B2D0, void, Fog_SetRange, (const double *pos, float radius));
ORIG(0x49B4B0, void, MeshBatch_Flush, (int slot));
ORIG(0x4A0340, void, Direct3DRender_ClipTriangleDepth, (ProjVert a, ProjVert b, ProjVert c, float *nearZ, float nearPlane));
ORIG(0x4B72E0, void *, SortRender_Add, (void (*fn)(void *), int size, float key));
ORIG(0x4CFCA0, uint8_t, Mesh3D_GetEvent4, (void));
ORIG(0x4CFCB0, int, ModelObj_GetLOD, (ModelObj *o));
ORIG(0x4D0180, int, Mesh3D_GetSplineMeshLOD, (ModelObj *o));
ORIG(0x4D03D0, int, Mesh3D_Exclude, (float *v, float radius, void *out));
ORIG(0x4D0510, int, Mesh3D_ExcludeRigid, (ModelObj *o, int *inside));
ORIG(0x4D0790, int, Mesh3D_ExcludeSpline, (ModelObj *o, int *inside));
ORIG(0x4D0DE0, void, ModelLod_ApplyTexAnims, (ModelLod *l));
ORIG(0x4D0E20, void, ModelLod_RestoreTexAnims, (ModelLod *l));
ORIG(0x4D0E50, int, ModelLod_GetTexAnimStateSize, (ModelLod *l));
ORIG(0x4D0E60, void, ModelLod_SaveTexAnimState, (ModelLod *l, int *out));
ORIG(0x4D0EA0, void, ModelLod_LoadTexAnimState, (ModelLod *l, const int *in));

extern IDirect3D7 *gD3D;
extern IDirect3DDevice7 *gD3DDevice;
extern ModelSlot gMeshBatches[];
extern int gTexStageIdx;
extern int gAlphaBlendIdx;
extern int gLitAlphaIdx;
extern int gLitIdx;
extern int gLmStageIdx;
extern int gUnlitAlphaIdx;
extern int gMeshVBCount;
extern ProjVert gProjVerts[];
extern int gOpaqueBlendIdx;
extern ModelVB gMeshVBs[];
extern int gUnlitIdx;
extern void (* gTaskHandlers[][0x180])(void *, void *);
extern char *gLightBase;
extern int *gLightIndex;
extern int gLightCount;
extern int gLightStride;
extern DWORD gFogDensity;
extern float gLayerFov[];
extern float gLightColor[];
extern float gFogColor[];
extern TransCtx gView;
extern Scene gScene;
extern float gWorldScale;
extern int gD3DClippingOnIndex;
extern PtrList *gD3DListB;
extern DWORD gD3DZEnable;
extern DynVB gVBNormal;
extern PtrList *gD3DListC;
extern float gNearPlane;
extern PtrList *gD3DStateList;
extern int gD3DClippingOffIndex;
extern PtrList *gD3DListA;
extern DWORD gFogEnable;

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

F32(kZero, 0x00000000);
F32(kHalf, 0x3F000000);
F32(kOne, 0x3F800000);
F32(k255, 0x437F0000);
F32(kMinus255, 0xC37F0000);

#define DEV gD3DDevice
#define SRS(state, value) IDirect3DDevice7_SetRenderState(DEV, (D3DRENDERSTATETYPE)(state), (DWORD)(value))
#define TSS(stage, state, value) \
    IDirect3DDevice7_SetTextureStageState(DEV, (stage), (D3DTEXTURESTAGESTATETYPE)(state), (value))

/* fistp with the current rounding mode, low 32 bits */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

static void use_state(PtrList *l, int i)
{
    if (l->current != i) {
        l->current = i;
        ((void (*)(int))l->items[i])(0);
    }
}

static IDirectDrawSurface7 *lod_texture(ModelLod *l, int i)
{
    return ((Texture *)l->textures[i])->surface;
}

static void scale_matrix(Matrix *rot, const Matrix *m, const float *scale)
{
    long double sx = (long double)gWorldScale * scale[0];
    float sy = gWorldScale * scale[1];
    float sz = gWorldScale * scale[2];

    *rot = *m;
    rot->m[0] = rot->m[0] * sx;
    rot->m[1] = rot->m[1] * sy;
    rot->m[2] = rot->m[2] * sz;
    rot->m[3] = rot->m[3] * sx;
    rot->m[4] = rot->m[4] * sy;
    rot->m[5] = rot->m[5] * sz;
    rot->m[6] = rot->m[6] * sx;
    rot->m[7] = rot->m[7] * sy;
    rot->m[8] = rot->m[8] * sz;
}

/* view space offset of p scaled to Direct3D units; the x difference stays unrounded */
static void view_offset(float *pos, const double *p)
{
    long double dx = p[0] - gView.pos.x;

    pos[1] = p[1] - gView.pos.y;
    pos[2] = p[2] - gView.pos.z;
    pos[0] = gWorldScale * dx;
    pos[1] = gWorldScale * pos[1];
    pos[2] = gWorldScale * pos[2];
}

/* Layer 0 objects are drawn with the field of view of the main camera. */
static void select_layer_view(TransCtx *saved, TransCtx *tc)
{
    float t;

    *saved = gView;
    *tc = *saved;
    tc->fov = gLayerFov[0];
    t = gLayerFov[0] * gScene.h;
    tc->fovY = t / (Display_GetAspect() * gScene.w);
    tc->scaleX = gScene.halfW / tc->fov;
    tc->layer = 0;
    tc->scaleY = Display_GetAspect() * tc->scaleX;
    TransContext_Select(tc);
}

/* fog for an object whose world matrix is m */
static void set_fog_at(const Matrix *m, const double *pos, MeshHeader *h)
{
    double p[3];
    long double x = ((long double)m->m[2] * h->center.z + (long double)m->m[1] * h->center.y)
                    + (long double)m->m[0] * h->center.x;
    float y = ((long double)m->m[5] * h->center.z + (long double)m->m[4] * h->center.y)
              + (long double)m->m[3] * h->center.x;
    float z = ((long double)m->m[8] * h->center.z + (long double)m->m[7] * h->center.y)
              + (long double)m->m[6] * h->center.x;

    p[0] = x + pos[0];
    p[1] = y + pos[1];
    p[2] = z + pos[2];
    Fog_SetRange(p, h->sphere);
}

/* Bends v along the spline of o; returns the segment. tmp gets the last two
   rounded temporaries, which the near z pass reuses. */
static int spline_point(ModelObj *o, const float *v, long double start, long double k, float *out, float *tmp)
{
    long double t = (start + v[0]) * k;
    long double f, p0, c0;
    int seg = x87_fistp(t);
    const Matrix *a, *b;
    float b1, b2, c1, c2;

    if (seg == o->segCount)
        seg--;
    f = t - seg;
    if (kZero.f > f)
        f = kZero.f;
    else if (!(kOne.f >= f))
        f = kOne.f;
    a = &o->segs[seg];
    b = &o->segs[seg + 1];
    p0 = (long double)a->m[2] * v[2] + (long double)a->m[1] * v[1];
    b1 = (long double)a->m[5] * v[2] + (long double)a->m[4] * v[1];
    b2 = (long double)a->m[8] * v[2] + (long double)a->m[7] * v[1];
    p0 = p0 + o->points[seg][0];
    b1 = b1 + o->points[seg][1];
    b2 = b2 + o->points[seg][2];
    c0 = (long double)b->m[2] * v[2] + (long double)b->m[1] * v[1];
    c1 = (long double)b->m[5] * v[2] + (long double)b->m[4] * v[1];
    c2 = (long double)b->m[8] * v[2] + (long double)b->m[7] * v[1];
    c0 = c0 + o->points[seg + 1][0];
    c1 = c1 + o->points[seg + 1][1];
    c2 = c2 + o->points[seg + 1][2];
    out[0] = (c0 - p0) * f + p0;
    out[1] = ((long double)c1 - b1) * f + b1;
    out[2] = ((long double)c2 - b2) * f + b2;
    tmp[0] = b1;
    tmp[1] = b2;
    return seg;
}

static void spline_params(ModelObj *o, long double *start, long double *k)
{
    float inv = kOne.f / ((long double)o->splineMax - o->splineMin);

    *start = -(long double)(o->reverse ? o->splineMax : o->splineMin);
    *k = (long double)o->segCount * inv;
    if (o->reverse)
        *k = -*k;
}

void Direct3DRender_RestoreVertexBuffers(void)
{
    int i;

    for (i = 0; i < gMeshVBCount; i++) {
        ModelVB *m = &gMeshVBs[i];
        VBUpload *n, *next;
        void *p;

        IDirect3DVertexBuffer7_Release(m->vb);
        m->vb = NULL;
        IDirect3D7_CreateVertexBuffer(gD3D, &m->desc, &m->vb, 0);
        for (n = m->uploads; (next = n->next) != NULL; n = next) {
            if (IDirect3DVertexBuffer7_Lock(*n->vb, DDLOCK_WRITEONLY, &p, NULL) == 0) {
                memcpy((char *)p + n->start * m->stride, n->src, n->count * m->stride);
                IDirect3DVertexBuffer7_Unlock(*n->vb);
            }
        }
    }
}
HOOK(0x49BD30, Direct3DRender_RestoreVertexBuffers);

/* Finds the nearest z of the visible part of a spline mesh, for the near plane. */
void Direct3DRender_SplineNearZ(ModelObj *o, NearZQuery *q)
{
    float nearPlane = gNearPlane;
    const float *vm = gView.matrix.m;
    const float *m;
    MeshHeader *h;
    RigidMesh *d;
    MeshGroup *g;
    const float *v;
    float c[3], radius, best, tx, ty, tz, r[9], tmp[2];
    /* kept from the vertex before when one is behind the near plane; the
       original starts with stack garbage or the last spline temporaries */
    float sx = 0, sy = 0, rhw = 0;
    long double dx, dy, dz, zmin;
    int i;

    if (!o->model)
        return;
    h = (MeshHeader *)o->model->lods[0]->mesh->header;
    if (o->offset) {
        float x = o->pos[0] - gView.pos.x;
        float y = o->pos[1] - gView.pos.y;
        float zf = o->pos[2] - gView.pos.z;
        long double z;

        x = x + o->offset[0];
        y = y + o->offset[1];
        z = (long double)zf + o->offset[2];
        c[0] = ((long double)vm[2] * z + (long double)vm[1] * y) + (long double)vm[0] * x;
        c[1] = ((long double)vm[5] * z + (long double)vm[4] * y) + (long double)vm[3] * x;
        c[2] = ((long double)vm[8] * z + (long double)vm[7] * y) + (long double)vm[6] * x;
        radius = o->offset[3];
    } else {
        dx = o->pos[0] - gView.pos.x;
        dy = o->pos[1] - gView.pos.y;
        dz = o->pos[2] - gView.pos.z;
        c[0] = (vm[2] * dz + vm[1] * dy) + vm[0] * dx;
        c[1] = (vm[5] * dz + vm[4] * dy) + vm[3] * dx;
        c[2] = (vm[8] * dz + vm[7] * dy) + vm[6] * dx;
        radius = o->radius;
    }
    if (Mesh3D_Exclude(c, radius, NULL))
        return;
    zmin = (long double)c[2] - radius;
    if (zmin > q->z || h->format)
        return;
    best = q->z;
    d = h->data;
    v = d->vertices;
    m = o->matrix.m;
    if (o->straight) {
        float center = ((long double)o->splineMax + o->splineMin) * kHalf.f;

        for (i = 0; i < d->vertexCount; i++, v += 8) {
            long double x = v[0] - center;
            float y = v[1], z = v[2];

            gProjVerts[i].x = (z * m[2] + x * m[0]) + y * m[1];
            gProjVerts[i].y = (z * m[5] + x * m[3]) + y * m[4];
            gProjVerts[i].z = (z * m[8] + x * m[6]) + y * m[7];
        }
    } else {
        long double start, k;

        spline_params(o, &start, &k);
        for (i = 0; i < d->vertexCount; i++, v += 8)
            spline_point(o, v, start, k, &gProjVerts[i].x, tmp);
        sy = tmp[0];
        rhw = tmp[1];
    }
    dx = o->pos[0] - gView.pos.x;
    dy = o->pos[1] - gView.pos.y;
    dz = o->pos[2] - gView.pos.z;
    tx = (dz * vm[2] + dy * vm[1]) + dx * vm[0];
    ty = (dz * vm[5] + dy * vm[4]) + dx * vm[3];
    tz = (dz * vm[8] + dy * vm[7]) + dx * vm[6];
    for (i = 0; i < 3; i++) {
        r[i * 3 + 0] = ((long double)vm[i * 3 + 2] * m[6] + (long double)vm[i * 3 + 0] * m[0])
                       + (long double)vm[i * 3 + 1] * m[3];
        r[i * 3 + 1] = ((long double)vm[i * 3 + 2] * m[7] + (long double)vm[i * 3 + 0] * m[1])
                       + (long double)vm[i * 3 + 1] * m[4];
        r[i * 3 + 2] = ((long double)vm[i * 3 + 2] * m[8] + (long double)vm[i * 3 + 0] * m[2])
                       + (long double)vm[i * 3 + 1] * m[5];
    }
    for (i = 0; i < d->vertexCount; i++) {
        ProjVert *p = &gProjVerts[i];
        float x = ((long double)r[2] * p->z + (long double)r[1] * p->y) + (long double)r[0] * p->x;
        float y = ((long double)r[5] * p->z + (long double)r[4] * p->y) + (long double)r[3] * p->x;
        float z = ((long double)r[8] * p->z + (long double)r[7] * p->y) + (long double)r[6] * p->x;
        float X = x + tx, Y = y + ty, Z;
        long double zl = (long double)z + tz;
        int clip;

        Z = zl;
        if (!(zl >= nearPlane)) {
            clip = 0x10;
        } else {
            long double inv = kOne.f / Z;
            long double sxl, syl;

            rhw = inv;
            sxl = inv * X * gView.scaleX + gView.centerX;
            sx = sxl;
            syl = (long double)gView.scaleY * rhw * Y + gView.centerY;
            sy = syl;
            clip = (sy > gScene.maxY) << 3 | (sxl > gScene.maxX) << 2 | (!(syl >= gScene.minY)) << 1
                   | (!(sxl >= gScene.minX));
        }
        p->x = X;
        p->y = Y;
        p->z = Z;
        p->sx = sx;
        p->sy = sy;
        p->rhw = rhw;
        p->clip = clip;
        if (!clip && !(Z >= best))
            best = Z;
    }
    if (!(zmin >= nearPlane) && (g = d->groups) != NULL) {
        for (;;) {
            const uint16_t *idx = g->indices;

            for (i = 0; i < g->indexCount / 3; i++, idx += 3) {
                ProjVert *a = &gProjVerts[idx[0] + g->base];
                ProjVert *b = &gProjVerts[idx[1] + g->base];
                ProjVert *cc = &gProjVerts[idx[2] + g->base];

                if ((a->clip | b->clip | cc->clip) && !(cc->clip & a->clip & b->clip)
                    && (!(a->z >= best) || !(b->z >= best) || !(cc->z >= best)))
                    Direct3DRender_ClipTriangleDepth(*a, *b, *cc, &best, nearPlane);
            }
            if (g->next == 0xFFFF)
                break;
            g = (MeshGroup *)((char *)g + g->next + 0x1C);
        }
    }
    q->z = best;
}
HOOK(0x49BE10, Direct3DRender_SplineNearZ);

void Direct3DRender_InitModels(void)
{
    int i;

    for (i = 0; i < 3; i++)
        if (gMeshBatches[i].used)
            MeshBatch_Flush(i);
}
HOOK(0x49C7D0, Direct3DRender_InitModels);

void Direct3DRender_DeinitModels(void)
{
    int i;

    for (i = 0; i < gMeshVBCount; i++) {
        IDirect3DVertexBuffer7_Release(gMeshVBs[i].vb);
        gMeshVBs[i].vb = NULL;
    }
    gMeshVBCount = 0;
}
HOOK(0x49C800, Direct3DRender_DeinitModels);

void Direct3DRender_DrawSortedFaceGroup_Spline(SplinePrim *p);
void Direct3DRender_SetObjectFog(ModelObj *o, MeshHeader *h);
int Direct3DRender_FillSplineVB(RigidMesh *d, ModelObj *o);

static void draw_group(RigidMesh *d, IDirect3DVertexBuffer7 *vb, int start, ModelLod *ml, MeshGroup *g)
{
    if (g->texture != 0xFFFF) {
        IDirect3DDevice7_SetTexture(DEV, 0, lod_texture(ml, g->texture));
        IDirect3DDevice7_DrawIndexedPrimitiveVB(DEV, d->primType, vb, start + g->base, g->vertexCount,
                                                (WORD *)g->indices, g->indexCount, 0);
    } else {
        TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        IDirect3DDevice7_SetTexture(DEV, 0, NULL);
        IDirect3DDevice7_DrawIndexedPrimitiveVB(DEV, d->primType, vb, start + g->base, g->vertexCount,
                                                (WORD *)g->indices, g->indexCount, 0);
        TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    }
}

void Direct3DRender_DrawSplineMesh(ModelObj **prim)
{
    ModelObj *o = *prim;
    VirModel *vm = o->model;
    int inside[4], lod, base;
    ModelLod *ml;
    MeshHeader *h;
    RigidMesh *d;
    MeshGroup *g;
    float pos[3];
    long double dz;
    Matrix rot;
    D3DMATRIX mat;

    if (!vm || Mesh3D_ExcludeSpline(o, inside))
        return;
    lod = Mesh3D_GetSplineMeshLOD(o);
    if (lod == -1)
        return;
    ml = vm->lods[lod];
    h = (MeshHeader *)ml->mesh->header;
    pos[0] = o->pos[0] - gView.pos.x;
    pos[1] = o->pos[1] - gView.pos.y;
    dz = o->pos[2] - gView.pos.z;
    pos[0] = gWorldScale * pos[0];
    pos[1] = gWorldScale * pos[1];
    pos[2] = gWorldScale * dz;
    rot.m[0] = gWorldScale;
    rot.m[1] = rot.m[2] = rot.m[3] = kZero.f;
    rot.m[4] = gWorldScale;
    rot.m[5] = rot.m[6] = rot.m[7] = kZero.f;
    rot.m[8] = gWorldScale;
    Direct3D_MakeMatrix(&mat, rot.m, pos);
    IDirect3DDevice7_SetTransform(DEV, D3DTRANSFORMSTATE_WORLD, &mat);
    if (h->format)
        return;
    d = h->data;
    g = d->groups;
    base = Direct3DRender_FillSplineVB(d, o);
    use_state(gD3DListA, gTexStageIdx);
    use_state(gD3DListB, gOpaqueBlendIdx);
    use_state(gD3DListC, gLitIdx);
    use_state(gD3DStateList, inside[0] ? gD3DClippingOffIndex : gD3DClippingOnIndex);
    for (;;) {
        if (!g->sorted) {
            Direct3DRender_SetObjectFog(o, h);
            draw_group(d, gVBNormal.vb, base, ml, g);
        } else {
            long double x = o->pos[0] - gView.pos.x;
            long double y = o->pos[1] - gView.pos.y;
            long double z = o->pos[2] - gView.pos.z;
            float key = (gView.matrix.m[8] * z + gView.matrix.m[7] * y) + gView.matrix.m[6] * x;
            SplinePrim *p = SortRender_Add((void (*)(void *))Direct3DRender_DrawSortedFaceGroup_Spline,
                                           0x30, key);

            memcpy(p->pos, o->pos, sizeof(p->pos));
            p->lod = ml;
            p->unk18 = o->unkD8;
            p->mesh = d;
            p->group = g;
            p->obj = o;
        }
        if (g->next == 0xFFFF)
            break;
        g = (MeshGroup *)((char *)g + g->next + 0x1C);
    }
}
HOOK(0x49C850, Direct3DRender_DrawSplineMesh);

void Direct3DRender_SetObjectFog(ModelObj *o, MeshHeader *h)
{
    set_fog_at(&o->matrix, o->pos, h);
}
HOOK(0x49CBE0, Direct3DRender_SetObjectFog);

/* Writes the bent mesh into the dynamic vertex buffer, returns its first vertex. */
int Direct3DRender_FillSplineVB(RigidMesh *d, ModelObj *o)
{
    void *lock;
    float *dst;
    const float *v;
    const float *m = o->matrix.m;
    int base, i;
    HRESULT hr;

    if (gVBNormal.used + d->vertexCount > gVBNormal.size) {
        hr = IDirect3DVertexBuffer7_Lock(gVBNormal.vb, DDLOCK_DISCARDCONTENTS | DDLOCK_WRITEONLY, &lock, NULL);
        base = 0;
        dst = lock;
        gVBNormal.used = d->vertexCount;
    } else {
        hr = IDirect3DVertexBuffer7_Lock(gVBNormal.vb, DDLOCK_NOOVERWRITE | DDLOCK_WRITEONLY, &lock, NULL);
        base = gVBNormal.used;
        dst = (float *)((uintptr_t)lock + base * 32);
        gVBNormal.used = base + d->vertexCount;
    }
    if (hr || !dst)
        return base;
    v = d->vertices;
    if (o->straight) {
        float center = ((long double)o->splineMax + o->splineMin) * kHalf.f;

        for (i = 0; i < d->vertexCount; i++, v += 8, dst += 8) {
            long double x = v[0] - center;
            float y = v[1], z = v[2];

            dst[0] = (z * m[2] + y * m[1]) + x * m[0];
            dst[1] = (x * m[3] + z * m[5]) + y * m[4];
            dst[2] = (z * m[8] + y * m[7]) + x * m[6];
            dst[3] = v[3];
            dst[4] = v[4];
            dst[5] = v[5];
            dst[6] = v[6];
            dst[7] = v[7];
        }
    } else {
        long double start, k;
        float tmp[2];

        spline_params(o, &start, &k);
        for (i = 0; i < d->vertexCount; i++, v += 8, dst += 8) {
            const float *a = o->segs[spline_point(o, v, start, k, dst, tmp)].m;

            dst[3] = ((long double)a[2] * v[5] + (long double)a[1] * v[4]) + (long double)v[3] * a[0];
            dst[4] = ((long double)a[4] * v[4] + (long double)a[3] * v[3]) + (long double)a[5] * v[5];
            dst[5] = ((long double)a[7] * v[4] + (long double)a[6] * v[3]) + (long double)a[8] * v[5];
            dst[6] = v[6];
            dst[7] = v[7];
        }
    }
    IDirect3DVertexBuffer7_Unlock(gVBNormal.vb);
    return base;
}
HOOK(0x49CC80, Direct3DRender_FillSplineVB);

static void set_alpha_states(int additive)
{
    if (additive) {
        SRS(D3DRENDERSTATE_LIGHTING, 0);
        TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        SRS(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
        SRS(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
        SRS(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
        SRS(D3DRENDERSTATE_FOGENABLE, 0);
    } else {
        SRS(D3DRENDERSTATE_LIGHTING, 1);
        TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        SRS(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
        SRS(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
        SRS(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
}

static void set_alpha_test(int alpha)
{
    SRS(D3DRENDERSTATE_ALPHATESTENABLE, 1);
    SRS(D3DRENDERSTATE_ALPHAREF, 1);
    SRS(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER);
    SRS(D3DRENDERSTATE_ZWRITEENABLE, 0);
    SRS(D3DRENDERSTATE_TEXTUREFACTOR, (uint32_t)-(alpha + 1) << 24);
}

void Direct3DRender_DrawSortedFaceGroup_Spline(SplinePrim *p)
{
    RigidMesh *d = p->mesh;
    MeshGroup *g = p->group;
    float pos[3];
    Matrix rot;
    D3DMATRIX mat;
    int base;

    use_state(gD3DListA, gTexStageIdx);
    use_state(gD3DListB, gOpaqueBlendIdx);
    use_state(gD3DListC, gLitIdx);
    use_state(gD3DStateList, gD3DClippingOnIndex);
    gD3DListB->current = -1;
    gD3DListC->current = -1;
    set_alpha_states(g->flags & 1);
    if (!(g->flags & 1)) {
        double c[3];

        c[0] = ((MeshHeader *)p->lod->mesh->header)->center.x + p->pos[0];
        c[1] = ((MeshHeader *)p->lod->mesh->header)->center.y + p->pos[1];
        c[2] = ((MeshHeader *)p->lod->mesh->header)->center.z + p->pos[2];
        Fog_SetRange(c, ((MeshHeader *)p->lod->mesh->header)->sphere);
    }
    set_alpha_test(g->alpha);
    view_offset(pos, p->pos);
    rot.m[0] = gWorldScale;
    rot.m[1] = rot.m[2] = rot.m[3] = kZero.f;
    rot.m[4] = gWorldScale;
    rot.m[5] = rot.m[6] = rot.m[7] = kZero.f;
    rot.m[8] = gWorldScale;
    Direct3D_MakeMatrix(&mat, rot.m, pos);
    IDirect3DDevice7_SetTransform(DEV, D3DTRANSFORMSTATE_WORLD, &mat);
    base = Direct3DRender_FillSplineVB(p->mesh, p->obj);
    if (g->texture != 0xFFFF) {
        IDirect3DDevice7_SetTexture(DEV, 0, lod_texture(p->lod, g->texture));
        IDirect3DDevice7_DrawIndexedPrimitiveVB(DEV, d->primType, gVBNormal.vb, g->base + base, g->vertexCount,
                                                (WORD *)g->indices, g->indexCount, 0);
    } else {
        TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        IDirect3DDevice7_SetTexture(DEV, 0, NULL);
        IDirect3DDevice7_DrawIndexedPrimitiveVB(DEV, d->primType, gVBNormal.vb, g->base + base, g->vertexCount,
                                                (WORD *)g->indices, g->indexCount, 0);
        TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    }
}
HOOK(0x49D0B0, Direct3DRender_DrawSortedFaceGroup_Spline);

static void set_lights(void)
{
    D3DLIGHT7 light;
    D3DMATERIAL7 mat;
    int i;

    memset(&light, 0, sizeof(light));
    for (i = 0; i < gLightCount; i++) {
        const float *l = (const float *)(gLightBase + gLightIndex[i] * gLightStride);

        light.dltType = D3DLIGHT_DIRECTIONAL;
        light.dcvDiffuse.r = gWorldScale * l[3];
        light.dcvDiffuse.g = gWorldScale * l[4];
        light.dcvDiffuse.b = gWorldScale * l[5];
        light.dvDirection.x = -l[0];
        light.dvDirection.y = -l[1];
        light.dvDirection.z = -l[2];
        IDirect3DDevice7_SetLight(DEV, i, &light);
        IDirect3DDevice7_LightEnable(DEV, i, TRUE);
    }
    memset(&mat, 0, sizeof(mat));
    mat.diffuse.r = mat.diffuse.g = mat.diffuse.b = kOne.f;
    mat.ambient.r = mat.ambient.g = mat.ambient.b = kOne.f;
    IDirect3DDevice7_SetMaterial(DEV, &mat);
}

static void set_fog(void)
{
    uint32_t r = x87_fistp((long double)gFogColor[0] * kMinus255.f);
    uint32_t g = x87_fistp((long double)gFogColor[1] * kMinus255.f);
    uint32_t b = x87_fistp((long double)gFogColor[2] * k255.f);

    SRS(D3DRENDERSTATE_FOGCOLOR, b - (((r << 8) + g) << 8));
    SRS(D3DRENDERSTATE_FOGDENSITY, gFogDensity);
    SRS(D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_NONE);
    SRS(D3DRENDERSTATE_FOGVERTEXMODE, D3DFOG_LINEAR);
    SRS(D3DRENDERSTATE_FOGENABLE, gFogEnable);
    SRS(D3DRENDERSTATE_ZBIAS, 3);
    SRS(D3DRENDERSTATE_SPECULARENABLE, 0);
}

static void set_ambient(void)
{
    uint32_t r = x87_fistp((long double)gLightColor[0] * k255.f);
    uint32_t g = x87_fistp((long double)gLightColor[1] * k255.f);
    uint32_t b = x87_fistp((long double)gLightColor[2] * k255.f);

    SRS(D3DRENDERSTATE_AMBIENT, (r << 8 | g) << 8 | b);
}

void Direct3DRender_SetLitState(void)
{
    set_lights();
    set_fog();
    set_ambient();
    SRS(D3DRENDERSTATE_CULLMODE, D3DCULL_CCW);
    SRS(D3DRENDERSTATE_LIGHTING, 1);
    SRS(D3DRENDERSTATE_COLORVERTEX, 0);
    SRS(D3DRENDERSTATE_DITHERENABLE, 1);
    SRS(D3DRENDERSTATE_ALPHATESTENABLE, 0);
    SRS(D3DRENDERSTATE_ZENABLE, gD3DZEnable);
    SRS(D3DRENDERSTATE_ZWRITEENABLE, 1);
    SRS(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
}
HOOK(0x49D480, Direct3DRender_SetLitState);

static void set_stage0(void)
{
    TSS(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
    TSS(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
    TSS(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP);
    TSS(0, D3DTSS_MIPFILTER, D3DTFP_POINT);
    TSS(0, D3DTSS_TEXCOORDINDEX, 0);
    TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
}

void Direct3DRender_SetTextureStages(void)
{
    set_stage0();
    TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    TSS(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
}
HOOK(0x49D730, Direct3DRender_SetTextureStages);

void Direct3DRender_SetUnlitState(void)
{
    set_fog();
    SRS(D3DRENDERSTATE_CULLMODE, D3DCULL_CCW);
    SRS(D3DRENDERSTATE_LIGHTING, 0);
    SRS(D3DRENDERSTATE_COLORVERTEX, 0);
    SRS(D3DRENDERSTATE_DITHERENABLE, 1);
    SRS(D3DRENDERSTATE_ALPHATESTENABLE, 0);
    SRS(D3DRENDERSTATE_ZENABLE, gD3DZEnable);
    SRS(D3DRENDERSTATE_ZWRITEENABLE, 1);
    SRS(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
}
HOOK(0x49D810, Direct3DRender_SetUnlitState);

void Direct3DRender_SetLightmapStages(void)
{
    set_stage0();
    TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    TSS(1, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP);
    TSS(1, D3DTSS_MIPFILTER, D3DTFP_NONE);
    TSS(1, D3DTSS_TEXCOORDINDEX, 1);
    TSS(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
    TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
    TSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
    TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    TSS(1, D3DTSS_MINFILTER, D3DTFN_LINEAR);
    TSS(1, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
    TSS(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
}
HOOK(0x49D960, Direct3DRender_SetLightmapStages);

void Direct3DRender_AlphaBlendOff(void)
{
    SRS(D3DRENDERSTATE_ALPHABLENDENABLE, 0);
}
HOOK(0x49DB10, Direct3DRender_AlphaBlendOff);

void Direct3DRender_SetLitAlphaState(void)
{
    set_lights();
    set_fog();
    set_ambient();
    SRS(D3DRENDERSTATE_CULLMODE, D3DCULL_CCW);
    SRS(D3DRENDERSTATE_LIGHTING, 1);
    SRS(D3DRENDERSTATE_COLORVERTEX, 0);
    SRS(D3DRENDERSTATE_DITHERENABLE, 1);
    SRS(D3DRENDERSTATE_ALPHATESTENABLE, 1);
    SRS(D3DRENDERSTATE_ALPHAREF, 1);
    SRS(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER);
    SRS(D3DRENDERSTATE_ZENABLE, gD3DZEnable);
    SRS(D3DRENDERSTATE_ZWRITEENABLE, 0);
    SRS(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
}
HOOK(0x49DB20, Direct3DRender_SetLitAlphaState);

void Direct3DRender_SetUnlitAlphaState(void)
{
    set_fog();
    SRS(D3DRENDERSTATE_CULLMODE, D3DCULL_CCW);
    SRS(D3DRENDERSTATE_LIGHTING, 0);
    SRS(D3DRENDERSTATE_COLORVERTEX, 0);
    SRS(D3DRENDERSTATE_DITHERENABLE, 1);
    SRS(D3DRENDERSTATE_ALPHATESTENABLE, 1);
    SRS(D3DRENDERSTATE_ZENABLE, gD3DZEnable);
    SRS(D3DRENDERSTATE_ZWRITEENABLE, 0);
    SRS(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
    SRS(D3DRENDERSTATE_ALPHAREF, 1);
    SRS(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER);
}
HOOK(0x49DDF0, Direct3DRender_SetUnlitAlphaState);

void Direct3DRender_DrawSortedFaceGroup_Rigid(SortedPrim *p);
void Direct3DRender_DrawSortedFaceGroup_Lightmap(SortedPrim *p);
void Direct3DRender_UploadLightmapMesh(LmMesh *d, LightmapRef *lm);

static void fill_prim(SortedPrim *p, ModelObj *o, ModelLod *ml, void *mesh, void *group)
{
    memcpy(p->pos, o->pos, sizeof(p->pos));
    p->matrix = o->matrix;
    memcpy(p->scale, o->scale, sizeof(p->scale));
    p->unk4C = o->unkD8;
    p->lod = ml;
    p->mesh = mesh;
    p->group = group;
    p->layer = o->layer;
}

static void draw_lm_group(LmMesh *d, int base, LmGroup *g)
{
    IDirect3DDevice7_DrawIndexedPrimitiveVB(DEV, d->primType, *d->vb, g->base + base, g->vertexCount,
                                            (WORD *)g->indices, g->indexCount, 0);
}

void Direct3DRender_DrawRigidMesh(ModelObj **prim)
{
    ModelObj *o = *prim;
    VirModel *vm = o->model;
    int inside[4], lod, base;
    ModelLod *ml;
    MeshHeader *h;
    TransCtx saved, tc;
    float pos[3], vz[3], depth;
    long double dx, dy, dz;
    Matrix rot;
    D3DMATRIX mat;

    if (!vm)
        return;
    if (!vm->lod_count) {
        Error_Show("model %s not converted", vm->name);
        for (;;)
            ;
    }
    if (Mesh3D_ExcludeRigid(o, inside))
        return;
    lod = ModelObj_GetLOD(o);
    if (!o->layer)
        lod = 0;
    else if (lod == -1)
        return;
    ml = vm->lods[lod];
    h = (MeshHeader *)ml->mesh->header;
    if (!h->data)
        return;
    ModelLod_ApplyTexAnims(ml);
    if (!o->layer)
        select_layer_view(&saved, &tc);
    pos[0] = o->pos[0] - gView.pos.x;
    pos[1] = o->pos[1] - gView.pos.y;
    dz = o->pos[2] - gView.pos.z;
    pos[0] = gWorldScale * pos[0];
    pos[1] = gWorldScale * pos[1];
    pos[2] = gWorldScale * dz;
    scale_matrix(&rot, &o->matrix, o->scale);
    Direct3D_MakeMatrix(&mat, rot.m, pos);
    IDirect3DDevice7_SetTransform(DEV, D3DTRANSFORMSTATE_WORLD, &mat);
    vz[0] = ((long double)gView.matrix.m[8] * o->matrix.m[6] + (long double)gView.matrix.m[7] * o->matrix.m[3])
            + (long double)gView.matrix.m[6] * o->matrix.m[0];
    vz[1] = ((long double)gView.matrix.m[8] * o->matrix.m[7] + (long double)gView.matrix.m[7] * o->matrix.m[4])
            + (long double)gView.matrix.m[6] * o->matrix.m[1];
    vz[2] = ((long double)gView.matrix.m[8] * o->matrix.m[8] + (long double)gView.matrix.m[7] * o->matrix.m[5])
            + (long double)gView.matrix.m[6] * o->matrix.m[2];
    dx = o->pos[0] - gView.pos.x;
    dy = o->pos[1] - gView.pos.y;
    dz = o->pos[2] - gView.pos.z;
    depth = (dz * gView.matrix.m[8] + dy * gView.matrix.m[7]) + dx * gView.matrix.m[6];
    if (h->format == 0) {
        RigidMesh *d = h->data;
        MeshGroup *g = d->groups;

        base = d->vbBase;
        use_state(gD3DListA, gTexStageIdx);
        use_state(gD3DListB, gOpaqueBlendIdx);
        use_state(gD3DListC, gLitIdx);
        use_state(gD3DStateList, inside[0] ? gD3DClippingOffIndex : gD3DClippingOnIndex);
        if (g) {
            for (;;) {
                if (!g->sorted) {
                    Direct3DRender_SetObjectFog(o, h);
                    draw_group(d, *d->vb, base, ml, g);
                } else {
                    float gd = ((long double)g->center[2] * vz[2] + (long double)g->center[1] * vz[1])
                               + (long double)g->center[0] * vz[0];
                    int n = ModelLod_GetTexAnimStateSize(ml);
                    SortedPrim *p = SortRender_Add((void (*)(void *))Direct3DRender_DrawSortedFaceGroup_Rigid,
                                                   n + sizeof(SortedPrim), gd + depth);
                    int *anim = n ? (int *)(p + 1) : NULL;

                    p->anim = anim;
                    ModelLod_SaveTexAnimState(ml, anim);
                    fill_prim(p, o, ml, d, g);
                }
                if (g->next == 0xFFFF)
                    break;
                g = (MeshGroup *)((char *)g + g->next + 0x1C);
            }
        }
    } else if (h->format == 3) {
        Lightmaps *set;
        LmMesh *d = h->data;
        LmGroup *g;

        if (o->lightmaps && (set = o->lightmaps[lod].lightmaps) != NULL) {
            g = d->groups;
            base = d->vbBase;
            gTaskHandlers[Mesh3D_GetEvent4()][o->type](o, NULL);
            Direct3DRender_UploadLightmapMesh(d, &o->lightmaps[lod]);
            use_state(gD3DListA, gLmStageIdx);
            use_state(gD3DListB, gOpaqueBlendIdx);
            use_state(gD3DListC, gUnlitIdx);
            use_state(gD3DStateList, inside[0] ? gD3DClippingOffIndex : gD3DClippingOnIndex);
            for (;;) {
                if (g->texture != 0xFFFF) {
                    if (!g->sorted) {
                        Direct3DRender_SetObjectFog(o, h);
                        IDirect3DDevice7_SetTexture(DEV, 0, lod_texture(ml, g->texture));
                        IDirect3DDevice7_SetTexture(DEV, 1, set->tex[g->lightmap].texture.surface);
                        draw_lm_group(d, base, g);
                    } else {
                        SortedPrim *p = SortRender_Add(
                            (void (*)(void *))Direct3DRender_DrawSortedFaceGroup_Lightmap, sizeof(SortedPrim),
                            (((long double)g->center[2] * vz[2] + (long double)g->center[1] * vz[1])
                             + (long double)g->center[0] * vz[0]) + depth);

                        fill_prim(p, o, ml, d, g);
                        p->lightmap = set;
                    }
                }
                if (g->next == 0xFFFF)
                    break;
                g = (LmGroup *)((char *)g + g->next + 0x20);
            }
        } else {
            g = d->groups;
            base = d->vbBase;
            Direct3DRender_UploadLightmapMesh(d, NULL);
            use_state(gD3DListA, gTexStageIdx);
            use_state(gD3DListB, gOpaqueBlendIdx);
            use_state(gD3DListC, gLitIdx);
            use_state(gD3DStateList, inside[0] ? gD3DClippingOffIndex : gD3DClippingOnIndex);
            if (g) {
                for (;;) {
                    if (!g->sorted) {
                        Direct3DRender_SetObjectFog(o, h);
                        if (g->texture != 0xFFFF) {
                            IDirect3DDevice7_SetTexture(DEV, 0, lod_texture(ml, g->texture));
                            draw_lm_group(d, base, g);
                        } else {
                            TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
                            IDirect3DDevice7_SetTexture(DEV, 0, NULL);
                            draw_lm_group(d, base, g);
                            TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                        }
                    } else {
                        SortedPrim *p = SortRender_Add(
                            (void (*)(void *))Direct3DRender_DrawSortedFaceGroup_Lightmap, sizeof(SortedPrim),
                            (((long double)g->center[2] * vz[2] + (long double)g->center[1] * vz[1])
                             + (long double)g->center[0] * vz[0]) + depth);

                        fill_prim(p, o, ml, d, g);
                        p->lightmap = NULL;
                    }
                    if (g->next == 0xFFFF)
                        break;
                    g = (LmGroup *)((char *)g + g->next + 0x20);
                }
            }
        }
    }
    if (!o->layer)
        TransContext_Select(&saved);
    ModelLod_RestoreTexAnims(ml);
}
HOOK(0x49DF60, Direct3DRender_DrawRigidMesh);

/* Copies a lightmapped mesh into its vertex buffer once, scaling the
   lightmap coordinates of each group. */
void Direct3DRender_UploadLightmapMesh(LmMesh *d, LightmapRef *lm)
{
    void *lock = NULL;
    float *dst;
    LmGroup *g;
    int i;

    if (d->uploaded || IDirect3DVertexBuffer7_Lock(*d->vb, DDLOCK_WRITEONLY, &lock, NULL))
        return;
    dst = (float *)lock + d->vbBase * 10;
    memcpy(dst, d->vertices, d->vertexCount * 40);
    if (lm && lm->lightmaps) {
        /* groups without a lightmap keep the scales of the group before;
           the first one reads them from the LightmapRef itself */
        const LightmapTex *t = (const LightmapTex *)lm;

        for (g = d->groups;; g = (LmGroup *)((char *)g + g->next + 0x20)) {
            const float *src = d->vertices + g->base * 10;
            float *out = dst + g->base * 10;

            if (g->lightmap != 0xFFFF)
                t = &lm->lightmaps->tex[g->lightmap];
            for (i = 0; i < g->vertexCount; i++) {
                out[i * 10 + 8] = src[i * 10 + 8] * t->texture.uScale;
                out[i * 10 + 9] = src[i * 10 + 9] * t->texture.vScale;
            }
            if (g->next == 0xFFFF)
                break;
        }
    }
    IDirect3DVertexBuffer7_Unlock(*d->vb);
    d->uploaded = 1;
}
HOOK(0x49EB50, Direct3DRender_UploadLightmapMesh);

void Direct3DRender_DrawSortedFaceGroup_Rigid(SortedPrim *p)
{
    ModelLod *ml = p->lod;
    RigidMesh *d;
    MeshGroup *g = p->group;
    TransCtx saved, tc;
    float pos[3];
    Matrix rot;
    D3DMATRIX mat;

    ModelLod_LoadTexAnimState(ml, p->anim);
    ModelLod_ApplyTexAnims(ml);
    d = p->mesh;
    use_state(gD3DListA, gTexStageIdx);
    use_state(gD3DListB, gOpaqueBlendIdx);
    use_state(gD3DListC, gLitIdx);
    use_state(gD3DStateList, gD3DClippingOnIndex);
    gD3DListB->current = -1;
    gD3DListC->current = -1;
    set_alpha_states(g->flags & 1);
    if (!(g->flags & 1))
        set_fog_at(&p->matrix, p->pos, (MeshHeader *)p->lod->mesh->header);
    set_alpha_test(g->alpha);
    view_offset(pos, p->pos);
    scale_matrix(&rot, &p->matrix, p->scale);
    Direct3D_MakeMatrix(&mat, rot.m, pos);
    IDirect3DDevice7_SetTransform(DEV, D3DTRANSFORMSTATE_WORLD, &mat);
    if (!p->layer)
        select_layer_view(&saved, &tc);
    draw_group(d, *d->vb, d->vbBase, ml, g);
    if (!p->layer)
        TransContext_Select(&saved);
    ModelLod_RestoreTexAnims(ml);
}
HOOK(0x49EC70, Direct3DRender_DrawSortedFaceGroup_Rigid);

void Direct3DRender_DrawSortedFaceGroup_Lightmap(SortedPrim *p)
{
    LmMesh *d = p->mesh;
    LmGroup *g = p->group;
    TransCtx saved, tc;
    float pos[3];
    Matrix rot;
    D3DMATRIX mat;

    if (p->lightmap) {
        use_state(gD3DListA, gLmStageIdx);
        use_state(gD3DListB, gAlphaBlendIdx);
        use_state(gD3DListC, gUnlitAlphaIdx);
    } else {
        use_state(gD3DListA, gTexStageIdx);
        use_state(gD3DListB, gAlphaBlendIdx);
        use_state(gD3DListC, gLitAlphaIdx);
    }
    use_state(gD3DStateList, gD3DClippingOnIndex);
    set_fog_at(&p->matrix, p->pos, (MeshHeader *)p->lod->mesh->header);
    if (g->flags & 1) {
        SRS(D3DRENDERSTATE_LIGHTING, 0);
        TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        SRS(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
        SRS(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
        SRS(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
    } else {
        if (p->lightmap) {
            SRS(D3DRENDERSTATE_LIGHTING, 0);
            TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        } else {
            SRS(D3DRENDERSTATE_LIGHTING, 1);
            TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        }
        SRS(D3DRENDERSTATE_ALPHABLENDENABLE, 1);
        SRS(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
        SRS(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
    SRS(D3DRENDERSTATE_TEXTUREFACTOR, (uint32_t)-(g->alpha + 1) << 24);
    view_offset(pos, p->pos);
    scale_matrix(&rot, &p->matrix, p->scale);
    Direct3D_MakeMatrix(&mat, rot.m, pos);
    IDirect3DDevice7_SetTransform(DEV, D3DTRANSFORMSTATE_WORLD, &mat);
    if (!p->layer)
        select_layer_view(&saved, &tc);
    if (g->texture != 0xFFFF) {
        IDirect3DDevice7_SetTexture(DEV, 0, lod_texture(p->lod, g->texture));
        if (p->lightmap)
            IDirect3DDevice7_SetTexture(DEV, 1, p->lightmap->tex[g->lightmap].texture.surface);
        IDirect3DDevice7_DrawIndexedPrimitiveVB(DEV, d->primType, *d->vb, d->vbBase + g->base, g->vertexCount,
                                                (WORD *)g->indices, g->indexCount, 0);
    }
    if (!p->layer)
        TransContext_Select(&saved);
}
HOOK(0x49F1C0, Direct3DRender_DrawSortedFaceGroup_Lightmap);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(7205B0) int gTexStageIdx;
ALIAS(gStateA0, gTexStageIdx, 0);
BSS(723508) int gUnlitAlphaIdx;
BSS(723508) int gMeshVBCount;
PAD(igb, 723508, 723510, 0x8);
PAD(igb, 723508, 723518, 0x88B84);
ALIAS(gProjVerts, pad_723518, 0);
ALIAS(gStateC3, gUnlitAlphaIdx, 0);
PAD(igb, 7AC0A0, 7AC0A0, 0x1D34);
ALIAS(gMeshVBs, pad_7AC0A0, 0);
BSS(7AC0A0) int gUnlitIdx;
ALIAS(gStateC1, gUnlitIdx, 0);
PAD(igb, B818EC, B818EC, 0x34);
ALIAS(gFogColor, pad_B818EC, 0);
BSS(BCAAE0) TransCtx gView;
ALIAS(gCamDir, gView, 24);
ALIAS(gCamMatrix, gView, 0);
ALIAS(gCamPos, gView, 40);
ALIAS(gCamTanX, gView, 64);
ALIAS(gCamTanY, gView, 68);
ALIAS(gCamZ, gView, 56);
ALIAS(gProj33, gView, 140);
ALIAS(gQCubeActive, gView, 88);
ALIAS(gTransContext, gView, 0);
ALIAS(gTransCtx, gView, 0);
ALIAS(gViewCenterX, gView, 80);
ALIAS(gViewCenterY, gView, 84);
ALIAS(gViewDir, gView, 24);
ALIAS(gViewFov, gView, 64);
ALIAS(gViewFovY, gView, 68);
ALIAS(gViewM, gView, 0);
ALIAS(gViewMatrix, gView, 0);
ALIAS(gViewPos, gView, 40);
ALIAS(gViewScaleX, gView, 72);
ALIAS(gViewScaleY, gView, 76);
ALIAS(gViewUnkBCAAF8, gView, 24);
BSS(BCABEC) float gWorldScale;
ALIAS(gD3DUnkBCABEC, gWorldScale, 0);
ALIAS(gFogZScale, gWorldScale, 0);
ALIAS(gNearPlaneScale, gWorldScale, 0);
BSS(BCACEC) int gD3DClippingOnIndex;
ALIAS(gClipOnIndex, gD3DClippingOnIndex, 0);
BSS(BCAD90) DynVB gVBNormal;
ALIAS(gVertexBuffer, gVBNormal, 0);
BSS(BCADE0) int gD3DClippingOffIndex;
ALIAS(gClipOffIndex, gD3DClippingOffIndex, 0);
