#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "dxinit/dxinit.h"
#include "d3d/d3dtypes.h"
#include "sector/sectortypes.h"
#include "task/taskvars.h"
#include "bonemesh/bonemeshvars.h"
#include "camfeed/camfeedvars.h"
#include "dxinit/dxinitvars.h"
#include "file/filevars.h"
#include "math/mathvars.h"
#include "nodereg/noderegvars.h"
#include "render/rendervars.h"

FN_EXTERN(Task_Nothing);

/* one triangle of a PolyListDynCubeObj (the same layout as HoleTri in explosion) */
typedef struct PolyTri {
    Vec3f *v[3];
    const float *color[3];
    float uv[6];
    float alpha[3];
    Vec3f normal;
    char *texture;
    int unk4C;
} PolyTri;

/* "PolyListDynCubeObj" (0xB8 bytes): loose triangles drawn with the Direct3D device, used for
   bullet holes, gun shot trails and glass shards */
typedef struct PolyList {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    void *world;
    PolyTri *tris;
    float rot[9];
    int unk94;
    int unusedMode;
    int count;
    unsigned flags; /* 8: additive, 0x10: multiply */
    int lit;
    int inWorld;
    int unkAC;
    int unkB0;
    Task **owner;
} PolyList;

_Static_assert(offsetof(PolyList, world) == 0x68, "world");
_Static_assert(offsetof(PolyList, owner) == 0xB4, "owner");
_Static_assert(sizeof(PolyList) == 0xB8, "PolyList");
_Static_assert(sizeof(PolyTri) == 0x50, "PolyTri");

typedef struct PrimArg {
    int unk0;
    int prim;
    int unk8;
    void *obj;
} PrimArg;

typedef struct LitVertex {
    float x, y, z;
    unsigned diffuse;
    float u, v;
} LitVertex;

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (int type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, int, TaskType_New, (const char *name, int size, uint16_t parent, int pool, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x4950F0, void, Direct3D_MakeMatrix, (D3DMATRIX *out, const float *rot, const float *pos));
ORIG(0x497860, void, Primitive_Draw, (PrimArg *arg));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x498070, int, TransContext_GetNearClipMethod, (void));
ORIG(0x49B2D0, void, Fog_SetRange, (const Vec3d *pos, float radius));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B72E0, void *, SortRender_Add, (void (*fn)(void *), int size, uint32_t key));
ORIG(0x4B7430, int, PtrList_Add, (PtrList *list, void *fn));
ORIG(0x4B7450, void, PtrList_Clear, (PtrList *list, int index));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4D97E0, uint8_t, QObj_GetEvent, (void));

#define Direct3D_Nothing (FN(0x4D3710, Task_Nothing))

extern uint16_t gPolyListType;
extern int gPolyListPrim;
extern int gPolyListStageState;
extern int gPolyListNothingB;
extern int gPolyListLightState;
extern int gPolyListNothingD;
extern char gPolyListRandom[];
extern Pool gLightPool;
extern uint32_t gFogEnable;

extern const float kMinus255;
extern const float kNearScale;

static Light *Light_Get(int i)
{
    return (Light *)(gLightPool.data + gLightPool.free[i] * gLightPool.stride);
}

void PolyList_Delete(PolyList *p)
{
    if (p->inWorld)
        DynCube_ListRemove(p->world, p, 0);
    if (p->owner)
        TaskRef_Remove(p->owner);
}
HOOK(0x49A350, PolyList_Delete);

void PolyList_DrawItem(PolyList **item)
{
    PrimArg a;

    a.prim = gPolyListPrim;
    a.obj = *item;
    Primitive_Draw(&a);
}
HOOK(0x49A390, PolyList_DrawItem);

void PolyList_Render(PolyList *p)
{
    if (p->count) {
        long double px = p->pos.x - gTransContext.pos.x, py = p->pos.y - gTransContext.pos.y, pz = p->pos.z - gTransContext.pos.z;
        float depth = gTransContext.matrix.m[8] * pz + gTransContext.matrix.m[7] * py + gTransContext.matrix.m[6] * px;

        *(PolyList **)SortRender_Add((void (*)(void *))PolyList_DrawItem, 4, f2u(depth)) = p;
    }
}
HOOK(0x49A3C0, PolyList_Render);

void PolyList_SetPos(PolyList *p, const Vec3d *pos)
{
    p->pos = *pos;
}
HOOK(0x49A430, PolyList_SetPos);

PolyList *PolyList_New(void *world, void *owner, const Vec3d *pos, const float *rot, PolyTri *tris, int count,
                       unsigned flags, int inWorld, int lit, int unkB0, int unkAC)
{
    PolyList *p = (PolyList *)Task_New(owner, gPolyListType, NULL);

    p->world = world;
    p->tris = tris;
    p->count = count;
    p->flags = flags;
    p->inWorld = inWorld;
    p->lit = lit;
    p->unkB0 = unkB0;
    p->unkAC = unkAC;
    p->unusedMode = 0;
    p->owner = owner ? TaskRef_Add(owner) : NULL;
    p->pos = *pos;
    memcpy(p->rot, rot, 10 * sizeof(float));
    if (inWorld)
        DynCube_ListAdd(world, p, 0);
    return p;
}
HOOK(0x49A450, PolyList_New);

void PolyList_SetUnusedMode(PolyList *p, int v)
{
    p->unusedMode = v;
}
HOOK(0x49A510, PolyList_SetUnusedMode);

void PolyList_Draw(void **msg);
void PolyList_SetStagesCB(void);
void PolyList_SetLightsCB(void);

void PolyList_Init(void)
{
    int type = TaskType_New("PolyListDynCubeObj", sizeof(PolyList), gDynCubeObjType, 0, 0);

    gPolyListType = type;
    TaskType_SetHandler(type, 4, PolyList_Render);
    TaskType_SetHandler2(gPolyListType, 2, PolyList_Delete);
    gPolyListPrim = Primitive_Register("PolyListDynCubeObjPrimitive");
    RenderMode_SetMethod(Direct3D_GetRenderMode(), gPolyListPrim, PolyList_Draw);
    gPolyListStageState = PtrList_Add(gD3DTextureStates, PolyList_SetStagesCB);
    gPolyListNothingB = PtrList_Add(gD3DBlendStates, Direct3D_Nothing);
    gPolyListLightState = PtrList_Add(gD3DRenderStates, PolyList_SetLightsCB);
    gPolyListNothingD = PtrList_Add(gD3DClipStates, Direct3D_Nothing);
}
HOOK(0x49A520, PolyList_Init);

void PolyList_SetStagesCB(void)
{
    SET_TSS(0, 0x11, 2);
    SET_TSS(0, 0x10, 2);
    SET_TSS(0, 0xC, 1);
    SET_TSS(0, 0x12, 2);
    SET_TSS(0, 0xB, 0);
    SET_TSS(0, 4, 3);
    SET_TSS(0, 5, 2);
    SET_TSS(0, 6, 0);
    SET_TSS(0, 1, 4);
    SET_TSS(0, 2, 2);
    SET_TSS(0, 3, 0);
    SET_TSS(1, 1, 1);
}
HOOK(0x49A5E0, PolyList_SetStagesCB);

/* The level lights as Direct3D directional lights, a white material, fog and the
   render states for lit triangles. */
void PolyList_SetLightsCB(void)
{
    D3DLIGHT7 light;
    D3DMATERIAL7 mat;
    unsigned fog;
    int i;

    memset(&light, 0, sizeof(light));
    for (i = 0; i < gLightPool.used; i++) {
        Light *l = Light_Get(i);

        light.dltType = D3DLIGHT_DIRECTIONAL;
        light.dcvDiffuse.r = gWorldScale * l->color.x;
        light.dcvDiffuse.g = gWorldScale * l->color.y;
        light.dcvDiffuse.b = gWorldScale * l->color.z;
        light.dvDirection.x = -l->dir.x;
        light.dvDirection.y = -l->dir.y;
        light.dvDirection.z = -l->dir.z;
        gD3DDevice->lpVtbl->SetLight(gD3DDevice, i, &light);
        gD3DDevice->lpVtbl->LightEnable(gD3DDevice, i, TRUE);
    }
    memset(&mat, 0, sizeof(mat));
    mat.diffuse.r = mat.diffuse.g = mat.diffuse.b = 1.0f;
    mat.ambient.r = mat.ambient.g = mat.ambient.b = 1.0f;
    gD3DDevice->lpVtbl->SetMaterial(gD3DDevice, &mat);
    SET_RS(0x88, 1);
    fog = ((unsigned)x87_fistp(gFogColor[0] * kMinus255) << 8) + (unsigned)x87_fistp(gFogColor[1] * kMinus255);
    SET_RS(0x22, (unsigned)x87_fistp(gFogColor[2] * k255) - (fog << 8));
    SET_RS(0x26, f2u(gFogDensity));
    SET_RS(0x23, 0);
    SET_RS(0x8C, 3);
    SET_RS(0x1C, gFogEnable);
    SET_RS(0x1D, 0);
    SET_RS(0x8B, ((unsigned)x87_fistp(gLightColor[0] * k255) << 8 | (unsigned)x87_fistp(gLightColor[1] * k255)) << 8 |
                     (unsigned)x87_fistp(gLightColor[2] * k255));
    SET_RS(0x1B, 0);
    SET_RS(0x16, 2);
    SET_RS(0x8D, 0);
    SET_RS(0x1A, 1);
    SET_RS(0xF, 1);
    SET_RS(0x18, 0);
    SET_RS(0x19, 5);
    SET_RS(7, gD3DZEnable);
    SET_RS(0xE, 0);
    SET_RS(0x17, 4);
}
HOOK(0x49A6E0, PolyList_SetLightsCB);

static void PolyList_UseState(PtrList *list, int index)
{
    if (list->current != index) {
        list->current = index;
        ((void (*)(int))list->items[index])(0);
    }
}

/* Locks room for 192 vertices in the lit vertex buffer; returns the HRESULT. */
static HRESULT PolyList_Lock(LitVertex **out, int *start)
{
    HRESULT hr;
    LitVertex *p = NULL;

    if (gVBLit.used + 0xC0 > gVBLit.size) {
        hr = gVBLit.vb->lpVtbl->Lock(gVBLit.vb, DDLOCK_DISCARDCONTENTS | DDLOCK_WRITEONLY, (void **)&p, NULL);
        *start = 0;
        gVBLit.used = 0xC0;
    } else {
        hr = gVBLit.vb->lpVtbl->Lock(gVBLit.vb, DDLOCK_NOOVERWRITE | DDLOCK_WRITEONLY, (void **)&p, NULL);
        *start = gVBLit.used;
        p += gVBLit.used;
        gVBLit.used += 0xC0;
    }
    *out = p;
    return hr;
}

/* Draws the triangles in batches of up to 63 that share a texture. */
void PolyList_Draw(void **msg)
{
    PolyList *p = msg[3];
    float light[3] = { 1.0f, 1.0f, 1.0f };
    float amb[3] = { 0.0f, 0.0f, 0.0f };
    float nearPlane, nearScaled, rel[3], rot[10];
    D3DMATRIX world;
    Vec3d pos;
    LitVertex *v;
    int start, i, k, n;

    if (p->lit) {
        long double r = 0, g = 0, b = 0;

        for (i = 0; i < gLightPool.used; i++) {
            Light *l = Light_Get(i);

            if (l->on) {
                r += l->color.x;
                g += l->color.y;
                b += l->color.z;
            }
        }
        light[1] = g;
        light[2] = b;
        light[0] = r * kHalfF;
        amb[0] = gLightColor[0];
        amb[1] = gLightColor[1];
        amb[2] = gLightColor[2];
        light[1] = light[1] * kHalfF;
        light[2] = light[2] * kHalfF;
        for (k = 0; k < 3; k++)
            if (amb[k] + light[k] > kOne)
                light[k] = kOne - amb[k];
    }
    PolyList_UseState(gD3DTextureStates, gPolyListStageState);
    PolyList_UseState(gD3DBlendStates, gPolyListNothingB);
    PolyList_UseState(gD3DRenderStates, gPolyListLightState);
    pos = p->pos;
    pos.x = p->tris[0].v[0]->x + pos.x;
    pos.y = p->tris[0].v[0]->y + pos.y;
    pos.z = p->tris[0].v[0]->z + pos.z;
    if (p->owner && *p->owner) {
        Task *t = *p->owner;
        void (*fn)(void *, void *) = gTaskHandlers[QObj_GetEvent()][t->type];

        if (fn)
            fn(t, &pos);
    }
    Fog_SetRange(&pos, 4096.0f);
    SET_RS(0x89, 0);
    {
        long double rz;

        rel[0] = p->pos.x - gTransContext.pos.x;
        rel[1] = p->pos.y - gTransContext.pos.y;
        rz = p->pos.z - gTransContext.pos.z;
        rel[0] = gWorldScale * rel[0];
        rel[1] = rel[1] * gWorldScale;
        rel[2] = rz * gWorldScale;
    }
    memcpy(rot, p->rot, sizeof(rot));
    for (k = 0; k < 9; k++)
        rot[k] = rot[k] * gWorldScale;
    Direct3D_MakeMatrix(&world, rot, rel);
    gD3DDevice->lpVtbl->SetTransform(gD3DDevice, D3DTRANSFORMSTATE_WORLD, &world);
    if (p->flags & 8) {
        SET_RS(0x1B, 1);
        SET_RS(0x13, 2);
        SET_RS(0x14, 2);
    } else if (p->flags & 0x10) {
        SET_RS(0x1B, 1);
        SET_RS(0x13, 1);
        SET_RS(0x14, 4);
    } else {
        SET_RS(0x1B, 1);
        SET_RS(0x13, 5);
        SET_RS(0x14, 6);
    }
    Random_Init(gPolyListRandom);
    nearPlane = gNearPlane;
    nearScaled = nearPlane * kNearScale;
    gRenderModeCurrent.methods[TransContext_GetNearClipMethod()](&nearScaled);
    if (PolyList_Lock(&v, &start) || !v)
        return;
    for (i = 0, n = 0; i < p->count; i++) {
        PolyTri *t = &p->tris[i];
        char *tex;

        for (k = 0; k < 3; k++, v++) {
            const float *c = t->color[k];

            v->x = t->v[k]->x;
            v->y = t->v[k]->y;
            v->z = t->v[k]->z;
            v->diffuse = (((unsigned)x87_fistp((light[0] * c[0] + amb[0]) * k255) |
                           (unsigned)x87_fistp(t->alpha[k] * k255) << 8) << 8 |
                          (unsigned)x87_fistp((light[1] * c[1] + amb[1]) * k255)) << 8 |
                         (unsigned)x87_fistp((light[2] * c[2] + amb[2]) * k255);
            v->u = t->uv[k * 2];
            v->v = t->uv[k * 2 + 1];
        }
        n++;
        tex = t->texture;
        if (i < p->count - 1 && t[1].texture == tex && n < 0x3F)
            continue;
        if (tex) {
            gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, *(IDirectDrawSurface7 **)(tex + 0x20));
            SET_TSS(0, 4, 2);
            SET_TSS(0, 1, 4);
        } else {
            gD3DDevice->lpVtbl->SetTexture(gD3DDevice, 0, NULL);
            SET_TSS(0, 4, 3);
            SET_TSS(0, 1, 3);
        }
        gVBLit.vb->lpVtbl->Unlock(gVBLit.vb);
        gD3DDevice->lpVtbl->DrawPrimitiveVB(gD3DDevice, D3DPT_TRIANGLELIST, gVBLit.vb, start, n * 3, 0);
        if (PolyList_Lock(&v, &start) || !v)
            return;
        n = 0;
    }
    gVBLit.vb->lpVtbl->Unlock(gVBLit.vb);
    gRenderModeCurrent.methods[TransContext_GetNearClipMethod()](&nearPlane);
}
HOOK(0x49A9B0, PolyList_Draw);

void PolyList_Close(void)
{
    PtrList_Clear(gD3DTextureStates, gPolyListStageState);
    PtrList_Clear(gD3DBlendStates, gPolyListNothingB);
    PtrList_Clear(gD3DRenderStates, gPolyListLightState);
    PtrList_Clear(gD3DClipStates, gPolyListNothingD);
    TaskType_Free(gPolyListType);
    Primitive_Unregister(gPolyListPrim);
}
HOOK(0x49B260, PolyList_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5342D8) const float kMinus255 INIT(-255.0f);
DATA(5342D8) const float kNearScale INIT(1.01f);
ALIAS(kAlphaScale, kMinus255, 0);
DATA(543C64) uint16_t gPolyListType INIT(385);
PAD(igd, 543C64, 543C66, 0x2);
STORE(igb, 720570, 1) char store_720570[12];
ALIAS(gPolyListRandom, store_720570, 0);
BSS(720570) int gPolyListStageState;
PAD(igb, 720570, 720580, 0x20);
BSS(720570) int gPolyListNothingD;
BSS(720570) int gPolyListNothingB;
BSS(720570) int gPolyListLightState;
BSS(BCAD80) DynVB gVBLit;
BSS(C311BC) int gPolyListPrim;
