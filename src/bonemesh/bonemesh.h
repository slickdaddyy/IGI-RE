#ifndef BONEMESH_BONEMESH_H
#define BONEMESH_BONEMESH_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "model/model.h"
#include "task/tasktypes.h"
#include "render/rendertypes.h"
#include "glowmesh/glowmeshtypes.h"
#include "transctx/transctxtypes.h"
#include "res/restypes.h"
#include "core/coretypes.h"

typedef void (*TaskHandler)(Task *task, void *arg);

/* RD3D chunk of a .mef mesh: Direct3D data, filled by the LoadModel method.
   format 0 uses unk10/unk14 (VRTX/REND), format 1 unk18/unk1C, format 3
   unk14/unk1C and the LTMP chunk at unk18. */
typedef struct MeshRender {
    int primType;
    int unk4;
    int unk8;
    void *unkC;
    void *unk10;
    void *unk14;
    void *unk18;
    void *unk1C;
    int unk20[6];
} MeshRender;

/* CMSH chunk */
typedef struct MeshColl {
    char unk0[0x10];
    struct {
        void *faces;  /* CFCE */
        void *verts;  /* CVTX */
        void *mats;   /* CMAT */
        void *sphere; /* CSPH */
        int unk10[4];
    } part[2];
} MeshColl;

/* argument of the LoadModel / UnloadModel methods */
typedef struct MeshLoadArg {
    MeshHeader *header;
    void *res;
} MeshLoadArg;

_Static_assert(sizeof(MeshRender) == 0x38, "MeshRender");

extern struct IDirect3DDevice7 *gD3DDevice;
extern TransContext gTransContext;
extern Scene gScene;
extern float gWorldScale;
extern RenderMode gRenderModeCurrent;
extern TaskHandler gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern PtrList *gD3DTextureStates;
extern PtrList *gD3DBlendStates;
extern PtrList *gD3DRenderStates;
extern PtrList *gD3DClipStates;

/* method idx of a COM object */
#define COM(obj, idx, type) ((type)(*(void ***)(obj))[idx])

#define DEV_SetTransform(d, a, b) COM(d, 11, long (__stdcall *)(void *, int, void *))(d, a, b)
#define DEV_GetTransform(d, a, b) COM(d, 12, long (__stdcall *)(void *, int, void *))(d, a, b)
#define DEV_SetRenderState(d, a, b) COM(d, 20, long (__stdcall *)(void *, int, int))(d, a, b)
#define DEV_DrawPrimitiveVB(d, a, b, c, e, f) \
    COM(d, 31, long (__stdcall *)(void *, int, void *, int, int, int))(d, a, b, c, e, f)
#define DEV_DrawIndexedPrimitiveVB(d, a, b, c, e, f, g, h) \
    COM(d, 32, long (__stdcall *)(void *, int, void *, int, int, void *, int, int))(d, a, b, c, e, f, g, h)
#define DEV_SetTexture(d, a, b) COM(d, 35, long (__stdcall *)(void *, int, void *))(d, a, b)
#define DEV_SetTextureStageState(d, a, b, c) COM(d, 37, long (__stdcall *)(void *, int, int, int))(d, a, b, c)
#define VB_Lock(v, a, b, c) COM(v, 3, long (__stdcall *)(void *, int, void *, unsigned int *))(v, a, b, c)
#define VB_Unlock(v) COM(v, 4, long (__stdcall *)(void *))(v)

/* selects entry idx of a state list unless it is already the current one */
#define STATE_SELECT(list, idx)              \
    do {                                     \
        if ((list)->current != (idx)) {          \
            (list)->current = (idx);             \
            ((void (*)(int))(list)->items[(list)->current])(0);      \
        }                                    \
    } while (0)

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

#endif
