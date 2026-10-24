/* Mesh3D: render methods, level of detail and exclusion tests for rigid, bone and spline meshes. */
#pragma GCC optimize("excess-precision=standard")
#include <stddef.h>
#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "model/model.h"
#include "transctx/transctx.h"

ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x401810, int, TaskEvent_Alloc2, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018E0, void, TaskEvent_Free2, (uint8_t index));
ORIG(0x497670, int, Scene_GetVersion, (void));
ORIG(0x497EC0, int, TransContext_GetSelectCount, (void));
ORIG(0x49A340, void, SplineCache_Reset, (void));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EB0, void, RenderMode_DeleteMethod, (int method));
ORIG(0x4CEC10, VirModel *, VirModel_Find_, (const char *name));

/* The MESH chunk of a .mef file */
typedef struct MeshHeader {
    float version;
    char unk4[0x2C];
    float center[3];
    float sphere; /* radius around center */
    char unk40[0x20];
    int complexity;
    char unk64[0x14];
    float radius;
    int16_t vertex_count;
    int16_t attach_count;
    char unk80[8];
    float (*verts)[4]; /* MVTX */
    char *attach;      /* ATTA, 0x44 bytes each */
} MeshHeader;

/* What the three kinds of mesh objects have in common, at their own offsets */
typedef struct RigidMeshObj {
    char unk0[0x20];
    double pos[3];
    char unk38[0x34];
    VirModel *model;
    float m[9];
    int unk94;
    float scale[3];
} RigidMeshObj;

typedef struct BoneMeshObj {
    char unk0[0x20];
    double pos[3];
    char unk38[0x3C];
    VirModel *model;
    float m[10];
    float radius;
} BoneMeshObj;

typedef struct SplineMeshObj {
    char unk0[0x20];
    double pos[3];
    char unk38[0xC];
    float radius;
    char unk48[0x24];
    VirModel *model;
    char unk70[0x530];
    float *bound; /* center and radius */
} SplineMeshObj;

typedef struct ExcludeArg {
    float *pos; /* view space */
    float radius;
    void *clip;
    int result;
} ExcludeArg;

_Static_assert(sizeof(MeshHeader) == 0x90, "MeshHeader");
_Static_assert(offsetof(RigidMeshObj, scale) == 0x98, "RigidMeshObj");
_Static_assert(offsetof(BoneMeshObj, radius) == 0xA0, "BoneMeshObj");
_Static_assert(offsetof(SplineMeshObj, bound) == 0x5A0, "SplineMeshObj");

extern int gDrawRigidMeshMethod;
extern int gDrawBoneMeshMethod;
extern int gDrawSplineMeshMethod;
extern int gDrawLightmapMeshMethod;
extern int gExcludeObjectMethod;
extern int gGenericUVFromRenderModeUVMethod;
extern int gRenderModeUVFromGenericUVMethod;
extern uint8_t gMesh3DEvent0;
extern uint8_t gMesh3DEvent1;
extern uint8_t gMesh3DEvent2;
extern uint8_t gMesh3DEvent3;
extern uint8_t gMesh3DEvent4;
extern int gMesh3DEvent5;
extern float gMesh3DFade[][3];
extern float gLayerFov[];

/* LOD settings, see notes/lod.md */
extern float gLodScale;
extern float gLodBlend;
extern float gLodRange;
extern float gLodRigidDetail;
extern float gLodBoneDetail;
extern float gLodSplineDetail;
extern float gLodRigidCull;
extern float gLodBoneCull;
extern float gLodSplineCull;

/* Screen edges for the exclusion test, redone when the camera or scene changes */
extern int gExclSelectCount;
extern int gExclSceneVersion;
extern float gExclLeft;
extern float gExclRight;
extern float gExclTop;
extern float gExclBottom;
extern float gExclLeftCos;
extern float gExclRightCos;
extern float gExclTopCos;
extern float gExclBottomCos;

extern void (*gRenderFuncs[])(void *);

static const float kOne = 1.0f;
static const float kBoneSphere = 0x1.68f5c2p+0f; /* 1.41 */
static const float kFadeStep = 0x1.111112p-4f;   /* 1/15 */
static const float kFadeA = 0x1.666666p-1f;      /* 0.7 */
static const float kFadeB = 40.0f;

static MeshHeader *lod_header(const ModelLod *l)
{
    return (MeshHeader *)l->mesh->header;
}

int Mesh3D_GetDrawRigidMethod(void)
{
    return gDrawRigidMeshMethod;
}
HOOK(0x4CFC30, Mesh3D_GetDrawRigidMethod);

int Mesh3D_GetDrawBoneMethod(void)
{
    return gDrawBoneMeshMethod;
}
HOOK(0x4CFC40, Mesh3D_GetDrawBoneMethod);

int Mesh3D_GetDrawSplineMethod(void)
{
    return gDrawSplineMeshMethod;
}
HOOK(0x4CFC50, Mesh3D_GetDrawSplineMethod);

int Mesh3D_GetExcludeMethod(void)
{
    return gExcludeObjectMethod;
}
HOOK(0x4CFC60, Mesh3D_GetExcludeMethod);

uint8_t Mesh3D_GetEvent1(void)
{
    return gMesh3DEvent1;
}
HOOK(0x4CFC70, Mesh3D_GetEvent1);

uint8_t Mesh3D_GetEvent2(void)
{
    return gMesh3DEvent2;
}
HOOK(0x4CFC80, Mesh3D_GetEvent2);

uint8_t Mesh3D_GetEvent3(void)
{
    return gMesh3DEvent3;
}
HOOK(0x4CFC90, Mesh3D_GetEvent3);

uint8_t Mesh3D_GetEvent4(void)
{
    return gMesh3DEvent4;
}
HOOK(0x4CFCA0, Mesh3D_GetEvent4);

/* Largest scale component; a NaN takes the same branches as in the original */
static float max_scale(const float *s)
{
    float m = !(s[1] >= s[0]) ? s[0] : s[1];

    return m > s[2] ? m : s[2];
}

/*
 * Picks the LOD for an object at view depth dist (to the mesh center) with
 * bounding radius r. A model with a distance table (unk24) uses it directly,
 * otherwise each LOD is chosen by its complexity relative to LOD 0. -1 means
 * too far away to draw.
 */
static int lod_select(const VirModel *vm, long double dist, long double r, long double k, float detail, float cull,
                      int warn)
{
    const float *dists = (const float *)vm->unk28;
    int n = (int16_t)vm->lod_count;
    float fov = gTransContext.fov;
    float thr[VIRMODEL_LODS];
    float e, q, inv, d;
    long double t;
    int i;

    if (vm->unk24) {
        d = fov * ((dist - r) * k);
        if (d > dists[4])
            return -1;
        i = 0;
        if (!(dists[0] < 0.0f || dists[0] > 0.0f)) {
            i = n - 1;
            if (i > 1)
                i = 1;
        }
        while (d > dists[i] && i < n - 1)
            i++;
        return i;
    }
    e = (gLodRange - r) * (kOne - gLodBlend) + r;
    t = (dist - e) * k;
    q = t;
    if (!(t > 0.0f))
        return 0;
    inv = kOne / q;
    thr[0] = kOne / (detail * e);
    for (i = 1; i < n; i++) {
        long double ratio;

        if (warn && lod_header(vm->lods[i])->complexity > lod_header(vm->lods[0])->complexity)
            Warning_Show("Detail level %d is more complex than level 0 for model %s.", i, vm->lods[0]->name);
        ratio = (long double)lod_header(vm->lods[i])->complexity / lod_header(vm->lods[0])->complexity;
        thr[i] = ratio * ratio * thr[0];
    }
    i = 0;
    while (i < n - 1 && fov * thr[i] > inv)
        i++;
    if (i == n - 1 && !(cull * e >= fov * q))
        return -1;
    return i;
}

int ModelObj_GetLOD(RigidMeshObj *o)
{
    const TransContext *c = &gTransContext;
    const VirModel *vm = o->model;
    const MeshHeader *h;
    long double x, dist;
    float y, z;

    if (!vm)
        return 0;
    h = lod_header(vm->lods[0]);
    x = (long double)o->pos[0] - c->pos[0] + h->center[0];
    y = o->pos[1] - c->pos[1];
    z = o->pos[2] - c->pos[2];
    dist = c->m[8] * ((long double)z + h->center[2]) + c->m[7] * ((long double)y + h->center[1]) + c->m[6] * x;
    return lod_select(vm, dist, (long double)max_scale(o->scale) * h->sphere, gLodScale, gLodRigidDetail,
                      gLodRigidCull, 1);
}
HOOK(0x4CFCB0, ModelObj_GetLOD);

int Mesh3D_GetBoneMeshLOD(BoneMeshObj *o)
{
    const TransContext *c = &gTransContext;
    const VirModel *vm = o->model;
    const MeshHeader *h;
    long double x, dist;
    float y, z;

    if (!vm)
        return 0;
    h = lod_header(vm->lods[0]);
    x = (long double)o->pos[0] - c->pos[0] + h->center[0];
    y = o->pos[1] - c->pos[1];
    z = o->pos[2] - c->pos[2];
    dist = c->m[8] * ((long double)z + h->center[2]) + c->m[7] * ((long double)y + h->center[1]) + c->m[6] * x;
    return lod_select(vm, dist, o->radius, 1.0L, gLodBoneDetail, gLodBoneCull, 0);
}
HOOK(0x4CFF70, Mesh3D_GetBoneMeshLOD);

int Mesh3D_GetSplineMeshLOD(SplineMeshObj *o)
{
    const TransContext *c = &gTransContext;
    const VirModel *vm = o->model;
    const float *b = o->bound;
    long double x, dist;
    float r;

    if (!vm)
        return 0;
    x = (long double)o->pos[0] - c->pos[0];
    if (b) {
        float y = o->pos[1] - c->pos[1];
        float z = o->pos[2] - c->pos[2];

        dist = c->m[8] * ((long double)z + b[2]) + c->m[7] * ((long double)y + b[1]) + c->m[6] * (x + b[0]);
        r = b[3];
    } else {
        long double y = (long double)o->pos[1] - c->pos[1];
        long double z = (long double)o->pos[2] - c->pos[2];

        dist = c->m[8] * z + c->m[7] * y + c->m[6] * x;
        r = o->radius;
    }
    return lod_select(vm, dist, r, 1.0L, gLodSplineDetail, gLodSplineCull, 0);
}
HOOK(0x4D0180, Mesh3D_GetSplineMeshLOD);

static long double cos_atan(long double x)
{
    long double r;

    __asm__("fpatan\n\tfcos" : "=t"(r) : "0"(1.0L), "u"(x) : "st(1)");
    return r;
}

/* Asks the render mode whether a sphere at pos (view space) is hidden. */
int Mesh3D_Exclude(float *pos, float radius, void *clip)
{
    ExcludeArg arg;

    if (TransContext_GetSelectCount() != gExclSelectCount || Scene_GetVersion() != gExclSceneVersion) {
        float cx = gScene.x + gScene.halfW;
        float cy = gScene.y + gScene.halfH;

        gExclSelectCount = TransContext_GetSelectCount();
        gExclSceneVersion = Scene_GetVersion();
        gExclLeft = (gScene.minX - cx) / gScene.halfW * gTransContext.fov;
        gExclRight = (gScene.maxX - cx) / gScene.halfW * gTransContext.fov;
        gExclTop = (gScene.minY - cy) / gScene.halfH * gTransContext.fovY;
        gExclBottom = (gScene.maxY - cy) / gScene.halfH * gTransContext.fovY;
        gExclLeftCos = cos_atan(gExclLeft);
        gExclRightCos = cos_atan(gExclRight);
        gExclTopCos = cos_atan(gExclTop);
        gExclBottomCos = cos_atan(gExclBottom);
    }
    arg.pos = pos;
    arg.radius = radius;
    arg.clip = clip;
    gRenderFuncs[Mesh3D_GetExcludeMethod()](&arg);
    return arg.result;
}
HOOK(0x4D03D0, Mesh3D_Exclude);

static void view_point(float *out, long double x, long double y, long double z)
{
    const float *m = gTransContext.m;

    out[0] = m[2] * z + m[1] * y + m[0] * x;
    out[1] = m[5] * z + m[4] * y + m[3] * x;
    out[2] = m[8] * z + m[7] * y + m[6] * x;
}

int Mesh3D_ExcludeRigid(RigidMeshObj *o, void *clip)
{
    const TransContext *c = &gTransContext;
    const MeshHeader *h = lod_header(o->model->lods[0]);
    const float *m = o->m;
    float dx = o->pos[0] - c->pos[0];
    float dy = o->pos[1] - c->pos[1];
    float dz = o->pos[2] - c->pos[2];
    long double x = m[2] * h->center[2] + m[1] * h->center[1] + m[0] * h->center[0];
    float y = m[5] * h->center[2] + m[4] * h->center[1] + m[3] * h->center[0];
    float z = m[8] * h->center[2] + m[7] * h->center[1] + m[6] * h->center[0];
    float v[3];

    view_point(v, x + dx, (long double)y + dy, (long double)z + dz);
    return Mesh3D_Exclude(v, max_scale(o->scale) * h->sphere, clip);
}
HOOK(0x4D0510, Mesh3D_ExcludeRigid);

int Mesh3D_ExcludeBone(BoneMeshObj *o, void *clip)
{
    const TransContext *c = &gTransContext;
    float v[3];

    view_point(v, (long double)o->pos[0] - c->pos[0], (long double)o->pos[1] - c->pos[1],
               (long double)o->pos[2] - c->pos[2]);
    return Mesh3D_Exclude(v, lod_header(o->model->lods[0])->sphere * kBoneSphere, clip);
}
HOOK(0x4D06C0, Mesh3D_ExcludeBone);

int Mesh3D_ExcludeSpline(SplineMeshObj *o, void *clip)
{
    const TransContext *c = &gTransContext;
    const float *b = o->bound;
    long double dx = (long double)o->pos[0] - c->pos[0];
    float v[3], r;

    if (b) {
        float x = (float)dx + b[0];
        float y = (float)(o->pos[1] - c->pos[1]) + b[1];
        float z = o->pos[2] - c->pos[2];

        view_point(v, x, y, (long double)z + b[2]);
        r = b[3];
    } else {
        view_point(v, dx, (long double)o->pos[1] - c->pos[1], (long double)o->pos[2] - c->pos[2]);
        r = o->radius;
    }
    return Mesh3D_Exclude(v, r, clip);
}
HOOK(0x4D0790, Mesh3D_ExcludeSpline);

/* Bone meshes have no fixed vertices; the point is always zero. */
void Mesh3D_GetBoneWorldVertex(double *out)
{
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
}
HOOK(0x4D0930, Mesh3D_GetBoneWorldVertex);

float VirModel_GetRadius(VirModel *vm)
{
    if (vm && vm->lods[0])
        return lod_header(vm->lods[0])->radius;
    return kOne;
}
HOOK(0x4D0950, VirModel_GetRadius);

static void scaled_vertex(float *out, const VirModel *vm, const RigidMeshObj *o, int i)
{
    const float *v = lod_header(vm->lods[0])->verts[i];

    out[0] = v[0];
    out[1] = v[1];
    out[2] = v[2];
    out[0] = o->scale[0] * out[0];
    out[1] = o->scale[1] * out[1];
    out[2] = o->scale[2] * out[2];
}

/* Vertex i of the object's LOD 0, scaled but not rotated */
void Mesh3D_GetVertex(float *out, RigidMeshObj *o, int i)
{
    if (o->model && o->model->lods[0])
        scaled_vertex(out, o->model, o, i);
}
HOOK(0x4D0970, Mesh3D_GetVertex);

/* The same for another model, by name */
void Mesh3D_GetModelVertex(float *out, RigidMeshObj *o, int i, const char *name)
{
    VirModel *vm = VirModel_Find_(name);

    if (vm && vm->lods[0])
        scaled_vertex(out, vm, o, i);
}
HOOK(0x4D09E0, Mesh3D_GetModelVertex);

/* Vertex i scaled and rotated, relative to the object position */
void Mesh3D_GetWorldVertex(double *out, RigidMeshObj *o, int i)
{
    const float *v, *m = o->m;
    long double x, zx;
    float y, z, r0, r2;
    long double r1;

    if (!o->model)
        return;
    v = lod_header(o->model->lods[0])->verts[i];
    x = (long double)v[0] * o->scale[0];
    y = v[1] * o->scale[1];
    zx = (long double)v[2] * o->scale[2];
    z = zx;
    r0 = zx * m[2] + y * m[1] + x * m[0];
    r1 = z * m[5] + y * m[4] + x * m[3];
    r2 = z * m[8] + y * m[7] + x * m[6];
    out[0] = r0;
    out[1] = r1;
    out[2] = r2;
}
HOOK(0x4D0A50, Mesh3D_GetWorldVertex);

void Mesh3D_Init(void)
{
    int i;

    for (i = 0; i < 16; i++) {
        long double f = (long double)i * kFadeStep;
        float a = f * kFadeA;

        gMesh3DFade[i][1] = a;
        gMesh3DFade[i][0] = a;
        gMesh3DFade[i][2] = f * f * f * kFadeB + kOne;
    }
    gMesh3DEvent0 = TaskEvent_Alloc(0);
    gMesh3DEvent1 = TaskEvent_Alloc(0);
    gMesh3DEvent2 = TaskEvent_Alloc(0);
    gMesh3DEvent3 = TaskEvent_Alloc(0);
    gMesh3DEvent4 = TaskEvent_Alloc(0);
    gMesh3DEvent5 = TaskEvent_Alloc2(0);
    gGenericUVFromRenderModeUVMethod = RenderMode_CreateMethod("GenericUVFromRenderModeUV");
    gRenderModeUVFromGenericUVMethod = RenderMode_CreateMethod("RenderModeUVFromGenericUV");
    gDrawRigidMeshMethod = RenderMode_CreateMethod("DrawRigidMesh");
    gDrawBoneMeshMethod = RenderMode_CreateMethod("DrawBoneMesh");
    gDrawSplineMeshMethod = RenderMode_CreateMethod("DrawSplineMesh");
    gDrawLightmapMeshMethod = RenderMode_CreateMethod("DrawLightmapMesh");
    gExcludeObjectMethod = RenderMode_CreateMethod("ExcludeObject");
    SplineCache_Reset();
    gLayerFov[0] = kOne;
    gLayerFov[1] = kOne;
    gLayerFov[2] = kOne;
}
HOOK(0x4D0B30, Mesh3D_Init);

void Mesh3D_Shutdown(void)
{
    RenderMode_DeleteMethod(gGenericUVFromRenderModeUVMethod);
    RenderMode_DeleteMethod(gRenderModeUVFromGenericUVMethod);
    RenderMode_DeleteMethod(gDrawRigidMeshMethod);
    RenderMode_DeleteMethod(gDrawBoneMeshMethod);
    RenderMode_DeleteMethod(gDrawSplineMeshMethod);
    RenderMode_DeleteMethod(gDrawLightmapMeshMethod);
    RenderMode_DeleteMethod(gExcludeObjectMethod);
    TaskEvent_Free2(gMesh3DEvent5);
    TaskEvent_Free(gMesh3DEvent1);
    TaskEvent_Free(gMesh3DEvent2);
    TaskEvent_Free(gMesh3DEvent3);
    TaskEvent_Free(gMesh3DEvent0);
    TaskEvent_Free(gMesh3DEvent4);
}
HOOK(0x4D0C50, Mesh3D_Shutdown);

void *ModelLod_GetAttach(ModelLod *l, int i)
{
    return lod_header(l)->attach + i * 0x44;
}
HOOK(0x4D0CF0, ModelLod_GetAttach);

int ModelLod_GetAttachCount(ModelLod *l)
{
    return lod_header(l)->attach_count;
}
HOOK(0x4D0D10, ModelLod_GetAttachCount);

int ModelLod_GetVertexCount(ModelLod *l)
{
    return lod_header(l)->vertex_count;
}
HOOK(0x4D0D20, ModelLod_GetVertexCount);

/* Frees the five per LOD buffers of an object */
void ModelObj_FreeLodData(void **p)
{
    char mode = Mem_GetMode();
    int i;

    if (!mode)
        Mem_SetDefaultMode();
    for (i = 0; i < VIRMODEL_LODS; i++) {
        if (p[i]) {
            Mem_Free(p[i]);
            p[i] = NULL;
        }
    }
    if (!mode)
        Mem_ClearMode();
}
HOOK(0x4D0D30, ModelObj_FreeLodData);

int Mesh3D_GetEvent5(void)
{
    return gMesh3DEvent5;
}
HOOK(0x4D0D80, Mesh3D_GetEvent5);

/*
 * Texture animations (NAXT): unk0 frame count, unk4 current frame (a
 * texture index), unk8 the texture slot it replaces, unkC the saved slot.
 */
int ModelLod_GetTexAnimCount(ModelLod *l)
{
    return l->mesh->anim_count;
}
HOOK(0x4D0D90, ModelLod_GetTexAnimCount);

int ModelLod_GetTexAnimFrames(ModelLod *l, int i)
{
    return l->mesh->anims[i].unk0;
}
HOOK(0x4D0DA0, ModelLod_GetTexAnimFrames);

void ModelLod_SetTexAnimFrame(ModelLod *l, int i, int frame)
{
    l->mesh->anims[i].unk4 = frame;
}
HOOK(0x4D0DC0, ModelLod_SetTexAnimFrame);

void ModelLod_ApplyTexAnims(ModelLod *l)
{
    int n = l->mesh->anim_count;
    MeshAnim *a;

    if (n <= 0)
        return;
    for (a = l->mesh->anims; n; n--, a++) {
        a->unkC = (int)l->textures[a->unk8];
        l->textures[a->unk8] = l->textures[a->unk4];
    }
}
HOOK(0x4D0DE0, ModelLod_ApplyTexAnims);

void ModelLod_RestoreTexAnims(ModelLod *l)
{
    int n = l->mesh->anim_count;
    MeshAnim *a;

    if (n <= 0)
        return;
    for (a = l->mesh->anims; n; n--, a++)
        l->textures[a->unk8] = (void *)a->unkC;
}
HOOK(0x4D0E20, ModelLod_RestoreTexAnims);

int ModelLod_GetTexAnimStateSize(ModelLod *l)
{
    return l->mesh->anim_count * 4;
}
HOOK(0x4D0E50, ModelLod_GetTexAnimStateSize);

void ModelLod_SaveTexAnimState(ModelLod *l, int *out)
{
    int i;

    if (!out)
        return;
    for (i = 0; i < l->mesh->anim_count; i++)
        *out++ = l->mesh->anims[i].unk4;
}
HOOK(0x4D0E60, ModelLod_SaveTexAnimState);

void ModelLod_LoadTexAnimState(ModelLod *l, const int *in)
{
    int i;

    if (!in)
        return;
    for (i = 0; i < l->mesh->anim_count; i++)
        l->mesh->anims[i].unk4 = *in++;
}
HOOK(0x4D0EA0, ModelLod_LoadTexAnimState);

void Mesh3D_SetLodScale(float scale)
{
    gLodScale = scale;
}
HOOK(0x4D0EE0, Mesh3D_SetLodScale);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54835C) float gLodBlend INIT(1.0f);
PAD(igd, 54835C, 548360, 0x4) INIT({0xCD, 0xCC, 0x4C, 0x3F});
DATA(54835C) uint8_t gMesh3DEvent0 INIT(255);
DATA(54835C) uint8_t gMesh3DEvent3 INIT(255);
DATA(54835C) uint8_t gMesh3DEvent2 INIT(255);
DATA(54835C) uint8_t gMesh3DEvent1 INIT(255);
DATA(54835C) uint8_t gMesh3DEvent4 INIT(255);
PAD(igd, 54835C, 548369, 0x3);
DATA(54835C) int gMesh3DEvent5 INIT(255);
DATA(54835C) float gLodScale INIT(1.0f);
BSS(A4EE78) int gExcludeObjectMethod;
PAD(igb, A4EE78, A4EE7C, 0x4);
BSS(A4EE78) float gLodRigidDetail;
BSS(A4EE78) float gLodBoneDetail;
BSS(A4EE78) float gLodSplineDetail;
BSS(A4EE78) float gLodRigidCull;
BSS(A4EE78) float gLodBoneCull;
BSS(A4EE78) float gLodSplineCull;
BSS(A4EE78) float gLodRange;
BSS(A4EE78) int gExclSceneVersion;
BSS(A4EE78) int gExclSelectCount;
BSS(B46CE0) int gDrawSplineMeshMethod;
BSS(B46D14) int gDrawRigidMeshMethod;
BSS(B8170C) int gRenderModeUVFromGenericUVMethod;
ALIAS(gRenderModeUVFromGenericUV, gRenderModeUVFromGenericUVMethod, 0);
BSS(B817A0) int gGenericUVFromRenderModeUVMethod;
ALIAS(gGenericUVFromRenderModeUV, gGenericUVFromRenderModeUVMethod, 0);
STORE(igb, B817C0, 4) float store_B817C0[16][3];
ALIAS(gMesh3DFade, store_B817C0, 0);
BSS(B817C0) int gDrawBoneMeshMethod;
BSS(B818DC) int gDrawLightmapMeshMethod;
BSS(BCABA0) Scene gScene;
ALIAS(gRenderQueueCount, gScene, 64);
ALIAS(gSceneArea, gScene, 0);
ALIAS(gSceneH, gScene, 36);
ALIAS(gSceneHalfH, gScene, 44);
ALIAS(gSceneHalfW, gScene, 40);
ALIAS(gSceneMaxX, gScene, 56);
ALIAS(gSceneMaxY, gScene, 60);
ALIAS(gSceneMinX, gScene, 48);
ALIAS(gSceneMinY, gScene, 52);
ALIAS(gSceneRect, gScene, 24);
ALIAS(gSceneW, gScene, 32);
ALIAS(gSceneX, gScene, 24);
ALIAS(gSceneY, gScene, 28);
ALIAS(gScreenH, gScene, 36);
ALIAS(gScreenW, gScene, 32);
ALIAS(gViewCX, gScene, 40);
ALIAS(gViewCY, gScene, 44);
ALIAS(gViewH, gScene, 36);
ALIAS(gViewW, gScene, 32);
ALIAS(gViewX, gScene, 24);
ALIAS(gViewY, gScene, 28);
