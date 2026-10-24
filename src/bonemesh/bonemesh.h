#ifndef BONEMESH_BONEMESH_H
#define BONEMESH_BONEMESH_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "model/model.h"
#include "types.h"

typedef struct Task {
    Node node;
    List children;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskHandler)(Task *task, void *arg);

/* One draw call of a REND chunk. Groups follow each other, next is the
   byte count of the indices, 0xFFFF on the last group. */
typedef struct MeshGroup {
    char unk0[0xC];
    uint16_t indexCount;
    uint16_t next;
    uint16_t texture; /* 0xFFFF = untextured */
    uint16_t start;
    uint16_t vertexCount;
    char unk16[6];
    uint16_t indices[];
} MeshGroup;

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

/* MESH chunk of a .mef mesh (Mesh.header) */
typedef struct MeshHeader {
    char unk0[0x20];
    int format; /* 0 rigid, 1 bone, 3 lightmap */
    MeshRender *render;
    int unk28;
    MeshColl *coll;
    Vec3f center;
    float radius;
} MeshHeader;

/* argument of the LoadModel / UnloadModel methods */
typedef struct MeshLoadArg {
    MeshHeader *header;
    void *res;
} MeshLoadArg;

/* argument of the DrawRigidMesh / DrawBoneMesh methods */
typedef struct MeshDrawArg {
    void *obj;
    int unk4;
    int unk8;
    int unkC;
    int unk10;
} MeshDrawArg;

/* PtrList (0x4B73F0): current index at +4, functions at +0x14 */
typedef struct StateList {
    int unk0;
    int cur;
    int unk8[3];
    void (*fn[])(int);
} StateList;

/* Camera for one view, 0xA8 bytes; the active one is at 0xBCAAE0 */
typedef struct TransCtx {
    float m[9];
    int unk24;
    double pos[3];
    float fov;
    float fovY;
    float scaleX, scaleY;
    float centerX, centerY;
    int unk58;
    float unk5C;
    int layer;
    float proj[16];
    int unkA4;
} TransCtx;

/* The screen area being drawn (0xBCABA0) */
typedef struct SceneArea {
    char unk0[0x18];
    float x, y, w, h;
    float halfW, halfH;
    float minX, minY, maxX, maxY;
} SceneArea;

_Static_assert(sizeof(MeshGroup) == 0x1C, "MeshGroup");
_Static_assert(sizeof(MeshRender) == 0x38, "MeshRender");
_Static_assert(offsetof(MeshHeader, radius) == 0x3C, "MeshHeader");
_Static_assert(sizeof(TransCtx) == 0xA8, "TransCtx");
_Static_assert(offsetof(SceneArea, maxY) == 0x3C, "SceneArea");

extern void *gDevice;
extern TransCtx gTransCtx;
extern SceneArea gSceneArea;
extern float gNearPlaneScale;
extern void (*gRenderFuncs[])(void *);
extern TaskHandler gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern StateList *gStateListA;
extern StateList *gStateListB;
extern StateList *gStateListC;
extern StateList *gStateListD;

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
        if ((list)->cur != (idx)) {          \
            (list)->cur = (idx);             \
            (list)->fn[(list)->cur](0);      \
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
