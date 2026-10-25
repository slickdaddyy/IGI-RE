#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "dxinit/dxinit.h"
#include "d3d/d3dtypes.h"
#include "boneobj/boneobjtypes.h"
#include "bonemesh/bonemeshvars.h"
#include "dxinit/dxinitvars.h"
#include "lod/lodvars.h"
#include "math/mathvars.h"

FN_EXTERN(Direct3DRender_AlphaBlendOff);
FN_EXTERN(Direct3DRender_SetLightmapStages);
FN_EXTERN(Direct3DRender_SetLitAlphaState);
FN_EXTERN(Direct3DRender_SetLitState);
FN_EXTERN(Direct3DRender_SetTextureStages);
FN_EXTERN(Direct3DRender_SetUnlitAlphaState);
FN_EXTERN(Direct3DRender_SetUnlitState);

FN_EXTERN(Direct3DRender_CopyUV);
FN_EXTERN(Direct3DRender_DeinitModels);
FN_EXTERN(Direct3DRender_DrawBoneMesh);
FN_EXTERN(Direct3DRender_DrawRigidMesh);
FN_EXTERN(Direct3DRender_DrawSplineMesh);
FN_EXTERN(Direct3DRender_ExcludeObject);
FN_EXTERN(Direct3DRender_InitModels);
FN_EXTERN(Direct3DRender_LoadModel);
FN_EXTERN(Direct3DRender_RestoreVertexBuffers);
FN_EXTERN(Direct3DRender_SplineNearZ);
FN_EXTERN(Direct3DRender_UnloadModel);

/* bone mesh vertex: position, normal, uv, weight, output vertex, bone */
typedef struct SkinVertex {
    float p[3];
    float n[3];
    float uv[2];
    float w;
    uint16_t target;
    uint16_t bone;
} SkinVertex;

typedef struct NormalVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
} NormalVertex;

_Static_assert(sizeof(SkinVertex) == 0x28, "SkinVertex");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x495200, void, Direct3D_AddRestoreCallback, (void *fn));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, void *node));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B7430, int, PtrList_Add, (PtrList *list, void *fn));
ORIG(0x4CEA60, int, Model_GetInitMethod, (void));
ORIG(0x4CEA70, int, Model_GetDeinitMethod, (void));
ORIG(0x4CEA80, int, Model_GetLoadMethod, (void));
ORIG(0x4CEA90, int, Model_GetUnloadMethod, (void));
ORIG(0x4CFC30, int, Mesh3D_GetDrawRigidMethod, (void));
ORIG(0x4CFC40, int, Mesh3D_GetDrawBoneMethod, (void));
ORIG(0x4CFC50, int, Mesh3D_GetDrawSplineMethod, (void));
ORIG(0x4CFC60, int, Mesh3D_GetExcludeMethod, (void));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4E5350, uint16_t, SplineObjSegment_GetType, (void));

extern DynVB gVBNormal;
extern MeshVB gMeshVBs[];
extern int gMeshVBCount;
extern const unsigned gMeshFVF[];
extern const unsigned gMeshCaps[];
extern const unsigned gMeshCapsTnL[];
extern int gTexStageIdx;
extern int gAlphaBlendIdx;
extern int gLitAlphaIdx;
extern int gLitIdx;
extern int gLmStageIdx;
extern int gUnlitAlphaIdx;
extern int gOpaqueBlendIdx;
extern int gUnlitIdx;

/* render functions and callbacks that live past this range */
#define Direct3D_DrawRigidMesh (FN(0x49DF60, Direct3DRender_DrawRigidMesh))
#define Direct3D_DrawBoneMesh (FN(0x49F700, Direct3DRender_DrawBoneMesh))
#define Direct3D_DrawSplineMesh (FN(0x49C850, Direct3DRender_DrawSplineMesh))
#define Direct3D_ConvertUV (FN(0x49FAF0, Direct3DRender_CopyUV))
#define Direct3D_DrawLightmapMesh (FN(0x49FEB0, Direct3DRender_ExcludeObject))
#define Direct3D_ModelLoad (FN(0x49FB10, Direct3DRender_LoadModel))
#define Direct3D_ModelUnload (FN(0x49FE60, Direct3DRender_UnloadModel))
#define Direct3D_ModelInit (FN(0x49C7D0, Direct3DRender_InitModels))
#define Direct3D_ModelDeinit (FN(0x49C800, Direct3DRender_DeinitModels))

/* e^x the way the original computes it: 2^(x * log2 e) with frndint, f2xm1 and fscale */
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

/* Sets linear fog start / end so that it matches exponential fog of gFogDensity at the
   near and far side of a sphere around pos. */
void Fog_SetRange(const Vec3d *pos, float radius)
{
    float y = pos->y, z = pos->z;
    long double px = pos->x - gTransContext.pos.x, py = y - gTransContext.pos.y, pz = z - gTransContext.pos.z;
    long double d = gTransContext.matrix.m[8] * pz + gTransContext.matrix.m[7] * py + gTransContext.matrix.m[6] * px;
    float n = d - radius;
    float f = d + radius;
    float a = kOne / x87_exp(gWorldScale * n * gFogDensity);
    long double bl = kOne / x87_exp(gWorldScale * f * gFogDensity);
    float b = bl;
    long double diffl = bl - a;
    float diff = diffl;
    float start, end;

    if (x87_eq(diffl, 0) || x87_eq(a, 0)) {
        start = 0.0f;
        end = 0.0f;
    } else {
        long double t = (b * n - a * f) / diff;

        start = (t - (t - n) / a) * gWorldScale;
        end = gWorldScale * t;
    }
    SET_RS(D3DRENDERSTATE_FOGSTART, f2u(start));
    SET_RS(D3DRENDERSTATE_FOGEND, f2u(end));
}
HOOK(0x49B2D0, Fog_SetRange);

void MeshBatch_Flush(int list);

void MeshBatch_Add(int list, char *mesh, int verts)
{
    MeshBatch *b = &gMeshBatches[list];

    if (b->total + verts > 0xFFFF)
        MeshBatch_Flush(list);
    b->items[b->count] = mesh;
    b->count++;
    b->total += verts;
    if (b->count == 1000)
        MeshBatch_Flush(list);
}
HOOK(0x49B430, MeshBatch_Add);

/* Copies the waiting meshes into a new static vertex buffer. */
void MeshBatch_Flush(int list)
{
    MeshBatch *b = &gMeshBatches[list];
    unsigned fvf = gMeshFVF[list];
    unsigned caps = gD3DTnL ? gMeshCapsTnL[list] : gMeshCaps[list];
    MeshVB *rec = &gMeshVBs[gMeshVBCount++];
    D3DVERTEXBUFFERDESC desc;
    int start = 0, i;

    desc.dwSize = sizeof(desc);
    desc.dwCaps = caps;
    desc.dwFVF = fvf;
    desc.dwNumVertices = b->total;
    gD3D->lpVtbl->CreateVertexBuffer(gD3D, &desc, &rec->vb, 0);
    rec->desc = desc;
    rec->meshes.tail = NULL;
    rec->meshes.head = (Node *)&rec->meshes.tail;
    rec->meshes.tailpred = (Node *)&rec->meshes.head;
    for (i = 0; i < b->count; i++) {
        char *item = b->items[i];
        char *m = *(char **)(item + 0x24);
        char *p = NULL;
        MeshVB *r;

        switch (*(int *)(item + 0x20)) {
        case 0:
            *(uint16_t *)(m + 0x24) = start;
            *(uint16_t *)(m + 0x26) = *(int *)(m + 0xC);
            *(MeshVB **)(m + 0x28) = rec;
            *(int *)(m + 0x20) = 1;
            *(int *)(m + 0x2C) = *(int *)(m + 0x10);
            r = *(MeshVB **)(m + 0x28);
            r->vb->lpVtbl->Lock(r->vb, DDLOCK_WRITEONLY, (void **)&p, NULL);
            memcpy(p + start * 0x20, *(void **)(m + 0x10), *(int *)(m + 0xC) * 0x20);
            r = *(MeshVB **)(m + 0x28);
            r->vb->lpVtbl->Unlock(r->vb);
            rec->stride = 0x20;
            List_AddTail(&rec->meshes, m + 0x18);
            start += *(int *)(m + 0xC);
            break;
        case 1:
            break;
        case 3:
            *(uint16_t *)(m + 0x2C) = start;
            *(uint16_t *)(m + 0x2E) = *(int *)(m + 0x10);
            *(MeshVB **)(m + 0x30) = rec;
            *(int *)(m + 0x28) = 1;
            *(int *)(m + 0x34) = *(int *)(m + 0x14);
            r = *(MeshVB **)(m + 0x30);
            r->vb->lpVtbl->Lock(r->vb, DDLOCK_WRITEONLY, (void **)&p, NULL);
            memcpy(p + start * 0x28, *(void **)(m + 0x14), *(int *)(m + 0x10) * 0x28);
            r = *(MeshVB **)(m + 0x30);
            r->vb->lpVtbl->Unlock(r->vb);
            rec->stride = 0x28;
            List_AddTail(&rec->meshes, m + 0x20);
            start += *(int *)(m + 0x10);
            break;
        default:
            Error_Show("Mesh format not supported");
            for (;;)
                ;
        }
    }
    b->count = 0;
    b->total = 0;
}
HOOK(0x49B4B0, MeshBatch_Flush);

/* Transforms a bone mesh into the dynamic normal vertex buffer and returns the first vertex.
   Without rigid, every vertex is weighted by its bone and the extra vertices after the mesh
   add their weighted share to a vertex. */
int BoneMesh_Skin(char *mesh, char *model, char *bones, int rigid)
{
    int count = *(int *)(mesh + 0x10);
    NormalVertex *out = NULL;
    SkinVertex *v;
    HRESULT hr;
    int start, i;

    if (count > 4000) {
        Error_Show("Bone model too large. Model %s has %d vertices.", model + 0x14, count);
        for (;;)
            ;
    }
    if (gVBNormal.used + count > gVBNormal.size) {
        hr = gVBNormal.vb->lpVtbl->Lock(gVBNormal.vb, DDLOCK_DISCARDCONTENTS | DDLOCK_WRITEONLY, (void **)&out,
                                        NULL);
        start = 0;
        gVBNormal.used = *(int *)(mesh + 0x10);
    } else {
        hr = gVBNormal.vb->lpVtbl->Lock(gVBNormal.vb, DDLOCK_NOOVERWRITE | DDLOCK_WRITEONLY, (void **)&out, NULL);
        start = gVBNormal.used;
        out += start;
        gVBNormal.used = start + *(int *)(mesh + 0x10);
    }
    if (hr || !out)
        return start;
    count = *(int *)(mesh + 0x10);
    v = *(SkinVertex **)(mesh + 0x18);
    if (!rigid) {
        NormalVertex *o = out;

        for (i = count; i > 0; i--, v++, o++) {
            const Bone *b = &(*(Bone **)(bones + 8))[v->bone];
            float x = v->p[0] * b->matrix.m[0] + v->p[2] * b->matrix.m[2] + v->p[1] * b->matrix.m[1];
            float y = v->p[0] * b->matrix.m[3] + v->p[2] * b->matrix.m[5] + v->p[1] * b->matrix.m[4];
            float z = v->p[2] * b->matrix.m[8] + b->matrix.m[7] * v->p[1] + b->matrix.m[6] * v->p[0];
            long double X = x + b->pos.x;
            float Y = y + b->pos.y;
            float Z = z + b->pos.z;

            o->x = X * v->w;
            o->y = Y * v->w;
            o->z = Z * v->w;
            o->u = v->uv[0];
            o->v = v->uv[1];
            o->nx = b->matrix.m[1] * v->n[1] + b->matrix.m[2] * v->n[2] + v->n[0] * b->matrix.m[0];
            o->ny = v->n[2] * b->matrix.m[5] + v->n[1] * b->matrix.m[4] + v->n[0] * b->matrix.m[3];
            o->nz = b->matrix.m[6] * v->n[0] + b->matrix.m[8] * v->n[2] + b->matrix.m[7] * v->n[1];
        }
        v = *(SkinVertex **)(mesh + 0x18) + *(int *)(mesh + 0x10);
        for (i = *(int *)(mesh + 0xC); i > 0; i--, v++) {
            const Bone *b = &(*(Bone **)(bones + 8))[v->bone];
            float x = b->matrix.m[2] * v->p[2] + b->matrix.m[1] * v->p[1] + b->matrix.m[0] * v->p[0];
            float y = b->matrix.m[5] * v->p[2] + b->matrix.m[4] * v->p[1] + b->matrix.m[3] * v->p[0];
            float z = b->matrix.m[8] * v->p[2] + b->matrix.m[7] * v->p[1] + b->matrix.m[6] * v->p[0];
            long double X = x + b->pos.x;
            float Y = y + b->pos.y;
            float Z = z + b->pos.z;
            float w = v->w;
            NormalVertex *t = &out[v->target];

            t->x = w * X + t->x;
            t->y = w * Y + t->y;
            t->z = w * Z + t->z;
        }
    } else {
        NormalVertex *o = out;

        for (i = count; i > 0; i--, v++, o++) {
            const Bone *b = &(*(Bone **)(bones + 8))[v->bone];
            long double x = v->p[0] * b->matrix.m[0] + v->p[2] * b->matrix.m[2] + v->p[1] * b->matrix.m[1];
            float y = v->p[0] * b->matrix.m[3] + v->p[2] * b->matrix.m[5] + v->p[1] * b->matrix.m[4];
            float z = v->p[2] * b->matrix.m[8] + b->matrix.m[7] * v->p[1] + b->matrix.m[6] * v->p[0];

            o->x = x + b->pos.x;
            o->y = y + b->pos.y;
            o->z = z + b->pos.z;
            o->nx = b->matrix.m[1] * v->n[1] + b->matrix.m[2] * v->n[2] + v->n[0] * b->matrix.m[0];
            o->ny = v->n[2] * b->matrix.m[5] + v->n[1] * b->matrix.m[4] + v->n[0] * b->matrix.m[3];
            o->nz = b->matrix.m[6] * v->n[0] + b->matrix.m[8] * v->n[2] + b->matrix.m[7] * v->n[1];
            o->u = v->uv[0];
            o->v = v->uv[1];
        }
    }
    gVBNormal.vb->lpVtbl->Unlock(gVBNormal.vb);
    return start;
}
HOOK(0x49B700, BoneMesh_Skin);

void Direct3DRender_Init(void)
{
    uint8_t slot;
    int m, i;

    m = Mesh3D_GetDrawRigidMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_DrawRigidMesh);
    m = Mesh3D_GetDrawBoneMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_DrawBoneMesh);
    m = Mesh3D_GetDrawSplineMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_DrawSplineMesh);
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gGenericUVFromRenderModeUVMethod, Direct3D_ConvertUV);
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gRenderModeUVFromGenericUVMethod, Direct3D_ConvertUV);
    m = Mesh3D_GetExcludeMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_DrawLightmapMesh);
    m = Model_GetLoadMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_ModelLoad);
    m = Model_GetUnloadMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_ModelUnload);
    m = Model_GetInitMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_ModelInit);
    m = Model_GetDeinitMethod();
    RenderMode_SetMethod(Direct3D_GetRenderMode(), m, Direct3D_ModelDeinit);
    for (i = 0; i < 3; i++) {
        gMeshBatches[i].count = 0;
        gMeshBatches[i].total = 0;
    }
    gMeshVBCount = 0;
    gTexStageIdx = PtrList_Add(gD3DTextureStates, FN(0x49D730, Direct3DRender_SetTextureStages));
    gOpaqueBlendIdx = PtrList_Add(gD3DBlendStates, FN(0x49DB10, Direct3DRender_AlphaBlendOff));
    gLitIdx = PtrList_Add(gD3DRenderStates, FN(0x49D480, Direct3DRender_SetLitState));
    gLmStageIdx = PtrList_Add(gD3DTextureStates, FN(0x49D960, Direct3DRender_SetLightmapStages));
    gUnlitIdx = PtrList_Add(gD3DRenderStates, FN(0x49D810, Direct3DRender_SetUnlitState));
    gAlphaBlendIdx = PtrList_Add(gD3DBlendStates, FN(0x49DB10, Direct3DRender_AlphaBlendOff));
    gLitAlphaIdx = PtrList_Add(gD3DRenderStates, FN(0x49DB20, Direct3DRender_SetLitAlphaState));
    gUnlitAlphaIdx = PtrList_Add(gD3DRenderStates, FN(0x49DDF0, Direct3DRender_SetUnlitAlphaState));
    Direct3D_AddRestoreCallback(FN(0x49BD30, Direct3DRender_RestoreVertexBuffers));
    slot = QCube_GetNearClipEvent();
    TaskType_SetHandler(SplineObjSegment_GetType(), slot, FN(0x49BE10, Direct3DRender_SplineNearZ));
}
HOOK(0x49BB40, Direct3DRender_Init);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 543CB8, 4) const unsigned store_543CB8[3] INIT({274, 530, 274});
ALIAS(gMeshFVF, store_543CB8, 0);
STORE(igd, 543CB8, 4) const unsigned store_543CC4[3] INIT({67584, 67584, 2048});
ALIAS(gMeshCaps, store_543CC4, 0);
STORE(igd, 543CB8, 4) const unsigned store_543CD0[31] INIT({65536, 65536, 0, 1, 1, 0, 1752393037, 1919903264, 544498029, 544501614, 1886418291, 1702130287, 100, 1701736258, 1685024032, 1948281957, 1814065007, 1701278305, 1867325486, 543974756, 1746957093, 622883681, 1702240356, 1667855474, 3044197, 1701080941, 1931812972, 1953459744, 1852793632, 1953654134, 25701});
ALIAS(gMeshCapsTnL, store_543CD0, 0);
STORE(igb, 7205C0, 4) __typeof__(gMeshBatches[0]) store_7205C0[3];
ALIAS(gMeshBatches, store_7205C0, 0);
BSS(B46D04) float gFogDensity;
