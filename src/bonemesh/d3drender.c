/* Direct3D mesh methods: bone meshes, mesh loading, sphere culling and
   the rigid object handlers. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "bonemesh/bonemesh.h"
#include "magicobj/magicobjtypes.h"

ORIG(0x491D40, long double, Display_GetAspect, (void));
ORIG(0x4950F0, void, Direct3D_MakeMatrix, (void *out, const Matrix *rot, const Vec3f *pos));
ORIG(0x497E70, void, TransContext_Select, (TransCtx *ctx));
ORIG(0x49B2D0, void, Fog_SetRange, (Vec3d *pos, float radius));
ORIG(0x49B430, void, MeshBatch_Add, (int kind, MeshHeader *h, void *data));
ORIG(0x49B700, int, BoneMesh_Skin, (MeshRender *r, VirModel *model, void *anim, int lod));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B7450, void, PtrList_Clear, (void *list, int index));
ORIG(0x4B7470, void *, Chunk_Next, (void *c));
ORIG(0x4B7490, void *, Chunk_FirstChild, (void *c));
ORIG(0x4B74D0, void *, Chunk_Data, (void *c, void *parent));
ORIG(0x4B7590, void *, Chunk_Find, (void *c, unsigned int id));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4C3450, void, RigidDynCubeObj_Open, (void));
ORIG(0x4C4820, void, RigidDynCubeObj_Close, (void));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4CFC30, int, Mesh3D_GetDrawRigidMethod, (void));
ORIG(0x4CFF70, int, Mesh3D_GetBoneMeshLOD, (void *obj));
ORIG(0x4D03D0, int, Mesh3D_Exclude, (Vec3f *center, float radius, void *out));
ORIG(0x4D06C0, int, Mesh3D_ExcludeBone, (void *obj, int *clip));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D97E0, uint8_t, QObj_GetEvent, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (int *a, int *b));
ORIG(0x4DAA20, int, Light_GetActive, (int *a));

#define ID_RD3D 0x52443344
#define ID_VRTX 0x56525458
#define ID_REND 0x52454E44
#define ID_LTMP 0x4C544D50
#define ID_CMSH 0x434D5348
#define ID_CVTX 0x43565458
#define ID_CFCE 0x43464345
#define ID_CMAT 0x434D4154
#define ID_CSPH 0x43535048

/* an animated object drawn with DrawBoneMesh (a human) */
typedef struct BoneObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    char anim[0xC];
    VirModel *model;
    char unk78[0x5C];
    int layer;
} BoneObj;

_Static_assert(offsetof(BoneObj, layer) == 0xD4, "BoneObj");

/* what a rigid object passes to its children */
typedef struct PlaceArg {
    Vec3d pos;
    Matrix matrix;
} PlaceArg;

typedef struct SphereArg {
    Vec3d pos;
    float radius;
} SphereArg;

/* a view space vertex with its screen position and outcode */
typedef struct ClipVertex {
    float x, y, z;
    float sx, sy, rhw;
    int code;
} ClipVertex;

/* argument of the 0x4D14D0 event: the nearest view depth so far */
typedef struct NearZArg {
    int unk0;
    int unk4;
    float minZ;
} NearZArg;

/* one draw call of a lightmapped mesh (format 3) */
typedef struct LightmapGroup {
    char unk0[0xC];
    uint16_t indexCount;
    uint16_t next;
    char unk10[4];
    uint16_t start;
    char unk16[0xA];
    uint16_t indices[];
} LightmapGroup;

_Static_assert(sizeof(LightmapGroup) == 0x20, "LightmapGroup");

/* argument of the ExcludeObject method: a view space sphere */
typedef struct ExcludeArg {
    Vec3f *center;
    float radius;
    int *inside;
    int outside;
} ExcludeArg;

extern float gLayerFov[];
extern int gBoneClipDepth;
extern ClipVertex gRigidClipVerts[];
extern ClipVertex gLightmapClipVerts[];
extern float gNearPlane;
extern void *gVertexBuffer;
extern int gClipOnIndex;
extern int gClipOffIndex;
extern float gNearZ;
extern float gFarZ;
extern int gStateA0;
extern int gStateA1;
extern int gStateB0;
extern int gStateB1;
extern int gStateC0;
extern int gStateC1;
extern int gStateC2;
extern int gStateC3;

/* view frustum planes as x = k * z (0xB46CE4 ...), and their scales */
extern float gFrustumL;
extern float gFrustumLScale;
extern float gFrustumR;
extern float gFrustumRScale;
extern float gFrustumB;
extern float gFrustumBScale;
extern float gFrustumT;
extern float gFrustumTScale;

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);

void RigidDynCubeObj_GetSphereHandler(RigidDynCubeObj *obj, SphereArg *out);
void RigidDynCubeObj_NearZHandler(RigidDynCubeObj *obj, NearZArg *arg);

/* Calls the handler of slot_fn() on each child of t (not deeper), keeping
   the next child on gTaskStack so handlers may delete tasks. */
#define EACH_CHILD(t, slot_fn, arg)                                     \
    do {                                                                \
        Task *c_ = (Task *)(t)->children.head;                          \
        if (!c_->node.next || !c_)                                      \
            break;                                                      \
        do {                                                            \
            TaskHandler fn_;                                            \
            gTaskStack[gTaskStackTop++] =                               \
                c_->node.next && c_->node.next->next ? (Task *)c_->node.next : NULL; \
            fn_ = gTaskHandlers[slot_fn()][c_->type];                   \
            if (fn_)                                                    \
                fn_(c_, arg);                                           \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)
F32(kInsideMargin, 0x43CCCCCD); /* 409.6 */

void Direct3DRender_DrawBoneMesh(MeshDrawArg *arg)
{
    BoneObj *obj = arg->obj;
    VirModel *model = obj->model;
    TransCtx saved, view;
    ModelLod *lod;
    MeshHeader *h;
    MeshRender *r;
    MeshGroup *g;
    Matrix rot;
    Vec3f pos;
    char d3dm[64];
    float fovH;
    int clip, n, base;

    if (!model || Mesh3D_ExcludeBone(obj, &clip))
        return;
    n = Mesh3D_GetBoneMeshLOD(obj);
    if (obj->layer == 0)
        n = 0;
    else if (n == -1)
        return;
    if (obj->layer == 0) {
        saved = gTransCtx;
        view = saved;
        view.fov = gLayerFov[0];
        fovH = gSceneArea.h * gLayerFov[0];
        view.fovY = fovH / (Display_GetAspect() * gSceneArea.w);
        view.scaleX = gSceneArea.halfW / view.fov;
        view.layer = 0;
        view.scaleY = Display_GetAspect() * view.scaleX;
        TransContext_Select(&view);
    }
    lod = model->lods[n];
    h = (MeshHeader *)lod->mesh->header;
    pos.x = (float)(obj->pos.x - gTransCtx.pos[0]);
    pos.y = (float)(obj->pos.y - gTransCtx.pos[1]);
    pos.x = gNearPlaneScale * pos.x;
    pos.y = gNearPlaneScale * pos.y;
    pos.z = gNearPlaneScale * (obj->pos.z - gTransCtx.pos[2]);
    rot.m[0] = gNearPlaneScale;
    rot.m[1] = kZero.f;
    rot.m[2] = kZero.f;
    rot.m[3] = kZero.f;
    rot.m[4] = gNearPlaneScale;
    rot.m[5] = kZero.f;
    rot.m[6] = kZero.f;
    rot.m[7] = kZero.f;
    rot.m[8] = gNearPlaneScale;
    rot.count = 0;
    Direct3D_MakeMatrix(d3dm, &rot, &pos);
    DEV_SetTransform(gDevice, 1, d3dm);
    Fog_SetRange(&obj->pos, h->radius);
    if (h->format == 1) {
        r = h->render;
        g = r->unk1C;
        base = BoneMesh_Skin(r, model, obj->anim, n);
        STATE_SELECT(gStateListA, gStateA0);
        STATE_SELECT(gStateListB, gStateB0);
        STATE_SELECT(gStateListC, gStateC0);
        STATE_SELECT(gStateListD, clip ? gClipOffIndex : gClipOnIndex);
        for (;;) {
            if (g->texture == 0xFFFF) {
                DEV_SetTextureStageState(gDevice, 0, 1, 3);
                DEV_SetTexture(gDevice, 0, NULL);
                DEV_DrawIndexedPrimitiveVB(gDevice, r->primType, gVertexBuffer, g->start + base, g->vertexCount,
                                           g->indices, g->indexCount, 0);
                DEV_SetTextureStageState(gDevice, 0, 1, 4);
            } else {
                DEV_SetTexture(gDevice, 0, ((void **)lod->textures[g->texture])[8]);
                DEV_DrawIndexedPrimitiveVB(gDevice, r->primType, gVertexBuffer, g->start + base, g->vertexCount,
                                           g->indices, g->indexCount, 0);
            }
            if (g->next == 0xFFFF)
                break;
            g = (MeshGroup *)((char *)g + sizeof(MeshGroup) + g->next);
        }
    }
    if (obj->layer == 0)
        TransContext_Select(&saved);
}
HOOK(0x49F700, Direct3DRender_DrawBoneMesh);

/* GenericUVFromRenderModeUV and RenderModeUVFromGenericUV: the same UVs */
void Direct3DRender_CopyUV(int *uv)
{
    uv[4] = uv[2];
    uv[5] = uv[3];
}
HOOK(0x49FAF0, Direct3DRender_CopyUV);

static void *find_data(void *res, unsigned int id)
{
    return Chunk_Data(Chunk_Find(Chunk_FirstChild(res), id), res);
}

void Direct3DRender_LoadModel(MeshLoadArg *arg)
{
    MeshRender *r;
    void *vtx, *fce, *mat, *sph, *c;
    int i;

    switch (arg->header->format) {
    case 0:
        arg->header->render = find_data(arg->res, ID_RD3D);
        r = arg->header->render;
        r->unk10 = find_data(arg->res, ID_VRTX);
        r->unk14 = find_data(arg->res, ID_REND);
        MeshBatch_Add(0, arg->header, r->unkC);
        break;
    case 1:
        arg->header->render = find_data(arg->res, ID_RD3D);
        r = arg->header->render;
        r->unk18 = find_data(arg->res, ID_VRTX);
        r->unk1C = find_data(arg->res, ID_REND);
        break;
    case 3:
        arg->header->render = find_data(arg->res, ID_RD3D);
        r = arg->header->render;
        r->unk14 = find_data(arg->res, ID_VRTX);
        r->unk1C = find_data(arg->res, ID_REND);
        r->unk18 = find_data(arg->res, ID_LTMP);
        MeshBatch_Add(1, arg->header, r->unk10);
        break;
    default:
        Error_Show("Mesh format not supported");
        for (;;)
            ;
    }
    if (arg->header->unk28) {
        Error_Show("Mesh format not supported");
        for (;;)
            ;
    }
    vtx = Chunk_FirstChild(arg->res);
    fce = Chunk_FirstChild(arg->res);
    mat = Chunk_FirstChild(arg->res);
    sph = Chunk_FirstChild(arg->res);
    c = Chunk_Find(Chunk_FirstChild(arg->res), ID_CMSH);
    arg->header->coll = NULL;
    if (!c)
        return;
    arg->header->coll = Chunk_Data(c, arg->res);
    for (i = 0; i < 2; i++) {
        vtx = Chunk_Find(Chunk_Next(vtx), ID_CVTX);
        fce = Chunk_Find(Chunk_Next(fce), ID_CFCE);
        mat = Chunk_Find(Chunk_Next(mat), ID_CMAT);
        sph = Chunk_Find(Chunk_Next(sph), ID_CSPH);
        arg->header->coll->part[i].verts = Chunk_Data(vtx, arg->res);
        arg->header->coll->part[i].faces = Chunk_Data(fce, arg->res);
        arg->header->coll->part[i].mats = Chunk_Data(mat, arg->res);
        arg->header->coll->part[i].sphere = Chunk_Data(sph, arg->res);
    }
}
HOOK(0x49FB10, Direct3DRender_LoadModel);

void Direct3DRender_UnloadModel(MeshLoadArg *arg)
{
    MeshHeader *h = arg->header;

    switch (h->format) {
    case 0:
        memset((char *)h->render + 0x18, 0, 6 * 4);
        break;
    case 1:
        break;
    case 3:
        memset((char *)h->render + 0x20, 0, 6 * 4);
        break;
    default:
        Error_Show("Mesh format not supported");
        for (;;)
            ;
    }
}
HOOK(0x49FE60, Direct3DRender_UnloadModel);

/* outside: the sphere misses the view frustum. inside (when asked): it is
   well inside the side planes and between the near and far planes. */
void Direct3DRender_ExcludeObject(ExcludeArg *arg)
{
    Vec3f *c = arg->center;
    float r = arg->radius;
    float nr = -r;
    float nearZ = gNearZ, farZ = gFarZ;
    long double left, right;
    float rightF, bottom, top;
    char behind, outL, outR, outB, outT;
    char inL, inR, inB, inT, inZ;

    behind = c->z < nr;
    if (r + c->z < nearZ || c->z - r > farZ) {
        if (arg->inside)
            *arg->inside = 0;
        arg->outside = 1;
        return;
    }
    left = (c->x - gFrustumL * c->z) * gFrustumLScale;
    right = (c->x - gFrustumR * c->z) * gFrustumRScale;
    rightF = right;
    bottom = (c->y - gFrustumB * c->z) * gFrustumBScale;
    top = (c->y - gFrustumT * c->z) * gFrustumTScale;
    outL = left < nr;
    outR = right > r;
    outB = bottom < nr;
    outT = top > r;
    if (arg->inside) {
        inL = left > r;
        inR = rightF < nr;
        inB = bottom > r;
        inT = top < nr;
        inZ = r + kInsideMargin.f < c->z;
        *arg->inside = inL && inR && inB && inT && inZ;
        if (c->z - r <= nearZ || !(r + c->z < farZ))
            *arg->inside &= 0;
        else
            *arg->inside &= 1;
    }
    arg->outside = outL || outR || outB || outT || behind;
}
HOOK(0x49FEB0, Direct3DRender_ExcludeObject);

void Direct3DRender_Shutdown(void)
{
    PtrList_Clear(gStateListB, gStateB1);
    PtrList_Clear(gStateListC, gStateC3);
    PtrList_Clear(gStateListC, gStateC2);
    PtrList_Clear(gStateListA, gStateA1);
    PtrList_Clear(gStateListC, gStateC1);
    PtrList_Clear(gStateListA, gStateA0);
    PtrList_Clear(gStateListB, gStateB0);
    PtrList_Clear(gStateListC, gStateC0);
}
HOOK(0x4A0110, Direct3DRender_Shutdown);

/* draws the object, then tells its children (and those of the attached
   task) where it is */
void RigidDynCubeObj_EnterHandler(RigidDynCubeObj *obj)
{
    MeshDrawArg d;
    PlaceArg a;

    d.obj = obj;
    d.unk4 = Light_GetActive(&d.unk8);
    Haze_GetVisible(&d.unkC, &d.unk10);
    gRenderFuncs[Mesh3D_GetDrawRigidMethod()](&d);
    a.pos = obj->pos;
    a.matrix = obj->matrix;
    EACH_CHILD(&obj->task, RigidDynCubeObj_GetEvent2, &a);
    if (obj->attached) {
        a.pos = obj->pos;
        a.matrix = obj->matrix;
        EACH_CHILD(obj->attached, RigidDynCubeObj_GetEvent2, &a);
    }
}
HOOK(0x4A01B0, RigidDynCubeObj_EnterHandler);

/* screen outcode: 1 left, 2 top, 4 right, 8 bottom, 0x10 in front of the near plane */
static int outcode(float sx, float sy)
{
    return (sy > gSceneArea.maxY) << 3 | (sx > gSceneArea.maxX) << 2 | (sy < gSceneArea.minY) << 1 |
           (sx < gSceneArea.minX);
}

/* the other vertices of the triangle: o is the one outside, p and q follow it */
static void pick_one(ClipVertex *v[3], int ma, int mb, ClipVertex **o, ClipVertex **p, ClipVertex **q)
{
    int i = ma ? 0 : mb ? 1 : 2;

    *o = v[i];
    *p = v[(i + 1) % 3];
    *q = v[(i + 2) % 3];
}

/* the vertex inside and the two outside */
static void pick_two(ClipVertex *v[3], int ma, int mb, ClipVertex **in, ClipVertex **o1, ClipVertex **o2)
{
    int i = !ma ? 0 : !mb ? 1 : 2;

    *in = v[i];
    *o1 = v[(i + 1) % 3];
    *o2 = v[(i + 2) % 3];
}

/* Clips a view space triangle against the near plane and the screen and
   lowers *minZ to the nearest depth of what is left. Vertices cut off by a
   plane move onto it; a triangle with one vertex outside becomes two. */
void Direct3DRender_ClipTriangleDepth(ClipVertex a, ClipVertex b, ClipVertex c, float *minZ, float nearZ)
{
    ClipVertex *v[3] = {&a, &b, &c};
    ClipVertex *in, *o, *p, *q, tmp;
    long double t, d, z, r, sx, sy, za;
    float tf;
    int any, all, ma, mb, mc;

    gBoneClipDepth++;
    any = a.code | b.code | c.code;
    all = a.code & b.code & c.code;
    if (all)
        goto done;

    if (any & 0x10) {
        ma = a.code & 0x10;
        mb = b.code & 0x10;
        mc = c.code & 0x10;
        if (ma + mb + mc == 0x20) {
            pick_two(v, ma, mb, &in, &o, &p);
            t = (nearZ - in->z) / (o->z - in->z);
            o->x = (o->x - in->x) * t + in->x;
            o->y = (o->y - in->y) * t + in->y;
            z = (o->z - in->z) * t + in->z;
            o->z = z;
            r = kOne.f / z;
            o->rhw = r;
            o->sx = r * gTransCtx.scaleX * o->x + gTransCtx.centerX;
            o->sy = o->rhw * o->y * gTransCtx.scaleY + gTransCtx.centerY;
            o->code = outcode(o->sx, o->sy);
            t = (nearZ - in->z) / (p->z - in->z);
            p->x = (p->x - in->x) * t + in->x;
            p->y = (p->y - in->y) * t + in->y;
            z = (p->z - in->z) * t + in->z;
            p->z = z;
            r = kOne.f / z;
            p->rhw = r;
            p->sx = r * p->x * gTransCtx.scaleX + gTransCtx.centerX;
            p->sy = gTransCtx.scaleY * p->rhw * p->y + gTransCtx.centerY;
            p->code = outcode(p->sx, p->sy);
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (nearZ - p->z) / (o->z - p->z);
            tmp = *o;
            o->x = (o->x - p->x) * t + p->x;
            o->y = (o->y - p->y) * t + p->y;
            z = (o->z - p->z) * t + p->z;
            o->z = z;
            r = kOne.f / z;
            o->rhw = r;
            o->sx = r * o->x * gTransCtx.scaleX + gTransCtx.centerX;
            o->sy = gTransCtx.scaleY * o->y * o->rhw + gTransCtx.centerY;
            o->code = outcode(o->sx, o->sy);
            d = tmp.z - q->z;
            t = (nearZ - q->z) / d;
            tmp.x = (tmp.x - q->x) * t + q->x;
            tmp.y = (tmp.y - q->y) * t + q->y;
            tmp.z = d * t + q->z;
            r = kOne.f / tmp.z;
            tmp.rhw = r;
            sx = r * tmp.x * gTransCtx.scaleX + gTransCtx.centerX;
            tmp.sx = sx;
            sy = tmp.rhw * tmp.y * gTransCtx.scaleY + gTransCtx.centerY;
            tmp.sy = sy;
            tmp.code = (tmp.sy > gSceneArea.maxY) << 3 | (sx > gSceneArea.maxX) << 2 |
                       (sy < gSceneArea.minY) << 1 | (sx < gSceneArea.minX);
            Direct3DRender_ClipTriangleDepth(*q, tmp, *o, minZ, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 1) {
        ma = a.code & 1;
        mb = b.code & 1;
        mc = c.code & 1;
        if (ma + mb + mc == 2) {
            pick_two(v, ma, mb, &in, &o, &p);
            d = o->sx - in->sx;
            tf = (gSceneArea.minX - in->sx) / d;
            o->sx = d * tf + in->sx;
            o->sy = (o->sy - in->sy) * tf + in->sy;
            o->rhw = (o->rhw - in->rhw) * tf + in->rhw;
            o->code = outcode(o->sx, o->sy) & ~1;
            d = p->sx - in->sx;
            tf = (gSceneArea.minX - in->sx) / d;
            p->sx = d * tf + in->sx;
            p->sy = (p->sy - in->sy) * tf + in->sy;
            p->rhw = (p->rhw - in->rhw) * tf + in->rhw;
            p->code = outcode(p->sx, p->sy) & ~1;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            d = o->sx - p->sx;
            tf = (gSceneArea.minX - p->sx) / d;
            tmp = *o;
            o->sx = d * tf + p->sx;
            o->sy = (o->sy - p->sy) * tf + p->sy;
            o->rhw = (o->rhw - p->rhw) * tf + p->rhw;
            o->code = outcode(o->sx, o->sy) & ~1;
            d = tmp.sx - q->sx;
            tf = (gSceneArea.minX - q->sx) / d;
            sx = d * tf + q->sx;
            tmp.sx = sx;
            sy = (tmp.sy - q->sy) * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (tmp.sy > gSceneArea.maxY) << 3 | (sx > gSceneArea.maxX) << 2 | (sy < gSceneArea.minY) << 1;
            Direct3DRender_ClipTriangleDepth(*q, tmp, *o, minZ, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 2) {
        ma = a.code & 2;
        mb = b.code & 2;
        mc = c.code & 2;
        if (ma + mb + mc == 4) {
            pick_two(v, ma, mb, &in, &o, &p);
            t = (gSceneArea.minY - in->sy) / (o->sy - in->sy);
            o->sx = (o->sx - in->sx) * t + in->sx;
            o->sy = (o->sy - in->sy) * t + in->sy;
            o->rhw = (o->rhw - in->rhw) * t + in->rhw;
            o->code = outcode(o->sx, o->sy) & ~3;
            t = (gSceneArea.minY - in->sy) / (p->sy - in->sy);
            p->sx = (p->sx - in->sx) * t + in->sx;
            p->sy = (p->sy - in->sy) * t + in->sy;
            p->rhw = (p->rhw - in->rhw) * t + in->rhw;
            p->code = outcode(p->sx, p->sy) & ~3;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (gSceneArea.minY - p->sy) / (o->sy - p->sy);
            tmp = *o;
            o->sx = (o->sx - p->sx) * t + p->sx;
            o->sy = (o->sy - p->sy) * t + p->sy;
            o->rhw = (o->rhw - p->rhw) * t + p->rhw;
            o->code = outcode(o->sx, o->sy) & ~3;
            d = tmp.sy - q->sy;
            tf = (gSceneArea.minY - q->sy) / d;
            tmp.sx = (tmp.sx - q->sx) * tf + q->sx;
            sy = d * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (sy > gSceneArea.maxY) << 3 | (tmp.sx > gSceneArea.maxX) << 2;
            Direct3DRender_ClipTriangleDepth(*q, tmp, *o, minZ, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 4) {
        ma = a.code & 4;
        mb = b.code & 4;
        mc = c.code & 4;
        if (ma + mb + mc == 8) {
            pick_two(v, ma, mb, &in, &o, &p);
            d = o->sx - in->sx;
            tf = (gSceneArea.maxX - in->sx) / d;
            o->sx = d * tf + in->sx;
            o->sy = (o->sy - in->sy) * tf + in->sy;
            o->rhw = (o->rhw - in->rhw) * tf + in->rhw;
            o->code = (o->sy > gSceneArea.maxY) << 3;
            d = p->sx - in->sx;
            tf = (gSceneArea.maxX - in->sx) / d;
            p->sx = d * tf + in->sx;
            p->sy = (p->sy - in->sy) * tf + in->sy;
            p->rhw = (p->rhw - in->rhw) * tf + in->rhw;
            p->code = (p->sy > gSceneArea.maxY) << 3;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            d = o->sx - p->sx;
            tf = (gSceneArea.maxX - p->sx) / d;
            tmp = *o;
            o->sx = d * tf + p->sx;
            o->sy = (o->sy - p->sy) * tf + p->sy;
            o->rhw = (o->rhw - p->rhw) * tf + p->rhw;
            o->code = (o->sy > gSceneArea.maxY) << 3;
            d = tmp.sx - q->sx;
            tf = (gSceneArea.maxX - q->sx) / d;
            tmp.sx = d * tf + q->sx;
            sy = (tmp.sy - q->sy) * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (sy > gSceneArea.maxY) << 3;
            Direct3DRender_ClipTriangleDepth(*q, tmp, *o, minZ, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 8) {
        ma = a.code & 8;
        mb = b.code & 8;
        mc = c.code & 8;
        if (ma + mb + mc == 0x10) {
            pick_two(v, ma, mb, &in, &o, &p);
            t = (gSceneArea.maxY - in->sy) / (o->sy - in->sy);
            o->sx = (o->sx - in->sx) * t + in->sx;
            o->sy = (o->sy - in->sy) * t + in->sy;
            o->code = 0;
            o->rhw = (o->rhw - in->rhw) * t + in->rhw;
            t = (gSceneArea.maxY - in->sy) / (p->sy - in->sy);
            p->sx = (p->sx - in->sx) * t + in->sx;
            p->sy = (p->sy - in->sy) * t + in->sy;
            p->code = 0;
            p->rhw = (p->rhw - in->rhw) * t + in->rhw;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (gSceneArea.maxY - p->sy) / (o->sy - p->sy);
            tmp = *o;
            tmp.code = 0;
            o->sx = (o->sx - p->sx) * t + p->sx;
            o->sy = (o->sy - p->sy) * t + p->sy;
            o->code = 0;
            o->rhw = (o->rhw - p->rhw) * t + p->rhw;
            d = tmp.sy - q->sy;
            tf = (gSceneArea.maxY - q->sy) / d;
            tmp.sx = (tmp.sx - q->sx) * tf + q->sx;
            tmp.sy = d * tf + q->sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            Direct3DRender_ClipTriangleDepth(*q, tmp, *o, minZ, nearZ);
        }
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    za = kOne.f / a.rhw;
    b.rhw = kOne.f / b.rhw;
    c.rhw = kOne.f / c.rhw;
    if (za < *minZ)
        *minZ = za;
    if (b.rhw < *minZ)
        *minZ = b.rhw;
    if (c.rhw < *minZ)
        *minZ = c.rhw;
done:
    gBoneClipDepth--;
}
HOOK(0x4A0340, Direct3DRender_ClipTriangleDepth);

void RigidDynCubeObj_OpenD3D(void)
{
    uint8_t ev;

    RigidDynCubeObj_Open();
    ev = QCube_GetNearClipEvent();
    TaskType_SetHandler(RigidDynCubeObj_GetType(), ev, RigidDynCubeObj_NearZHandler);
    ev = QObj_GetEvent();
    TaskType_SetHandler(RigidDynCubeObj_GetType(), ev, RigidDynCubeObj_GetSphereHandler);
}
HOOK(0x4A1470, RigidDynCubeObj_OpenD3D);

/* bounding sphere of the model in world space */
void RigidDynCubeObj_GetSphereHandler(RigidDynCubeObj *obj, SphereArg *out)
{
    VirModel *m = (VirModel *)obj->model;
    MeshHeader *h;
    const Matrix *r = &obj->matrix;
    long double x;
    float y, z;

    if (!m)
        return;
    if (m->lod_count == 0) {
        Error_Show("model %s not converted", m->name);
        for (;;)
            ;
    }
    h = (MeshHeader *)m->lods[0]->mesh->header;
    x = r->m[1] * h->center.y + r->m[0] * h->center.x + r->m[2] * h->center.z;
    y = r->m[4] * h->center.y + r->m[3] * h->center.x + r->m[5] * h->center.z;
    z = r->m[7] * h->center.y + r->m[6] * h->center.x + r->m[8] * h->center.z;
    out->pos.x = x + obj->pos.x;
    out->pos.y = y + obj->pos.y;
    out->pos.z = z + obj->pos.z;
    out->radius = h->radius;
}
HOOK(0x4A14B0, RigidDynCubeObj_GetSphereHandler);

/* view space vertex and outcode, as the clipper wants them */
static int project(ClipVertex *out, float x, float y, long double z, float nearZ, float *sx, float *sy, float *rhw,
                   int lightmap)
{
    long double r, px, py;
    int code;

    out->x = x;
    out->y = y;
    out->z = z;
    if (z < nearZ) {
        code = 0x10;
    } else {
        r = kOne.f / out->z;
        *rhw = r;
        px = r * x * gTransCtx.scaleX + gTransCtx.centerX;
        *sx = px;
        if (lightmap)
            py = *rhw * y * gTransCtx.scaleY + gTransCtx.centerY;
        else
            py = gTransCtx.scaleY * *rhw * y + gTransCtx.centerY;
        *sy = py;
        code = (*sy > gSceneArea.maxY) << 3 | (px > gSceneArea.maxX) << 2 | (py < gSceneArea.minY) << 1 |
               (px < gSceneArea.minX);
    }
    out->sx = *sx;
    out->sy = *sy;
    out->rhw = *rhw;
    out->code = code;
    return code;
}

/* clips one triangle of the table when it is partly visible and may be
   nearer than *minZ */
static void clip_face(ClipVertex *a, ClipVertex *b, ClipVertex *c, float *minZ, float nearZ)
{
    if (!(a->code | b->code | c->code) || (a->code & b->code & c->code))
        return;
    if (a->z < *minZ || b->z < *minZ || c->z < *minZ)
        Direct3DRender_ClipTriangleDepth(*a, *b, *c, minZ, nearZ);
}

/* Lowers arg->minZ to the nearest view depth of the visible part of the
   mesh, then asks the attached task's children. For a vertex behind the
   near plane the screen position is the previous vertex's (stale stack for
   the first). */
void RigidDynCubeObj_NearZHandler(RigidDynCubeObj *obj, NearZArg *arg)
{
    float nearZ = gNearPlane;
    MeshHeader *h;
    MeshRender *r;
    Matrix rot;
    Vec3f pos, o;
    float y, z, s, radius, minZ, tx, ty, tz;
    float sx = 0, sy = 0, rhw = 0;
    long double x, wx, wy, wz, near, vz;
    ClipVertex *v;
    char *g;
    uint16_t *idx;
    const float *p;
    int i, j;

    if (obj->hidden != 1 || !obj->model || !((VirModel *)obj->model)->lods[0])
        return;
    h = (MeshHeader *)((VirModel *)obj->model)->lods[0]->mesh->header;
    pos.x = (float)(obj->pos.x - gTransCtx.pos[0]);
    pos.y = (float)(obj->pos.y - gTransCtx.pos[1]);
    pos.z = (float)(obj->pos.z - gTransCtx.pos[2]);
    x = h->center.z * obj->matrix.m[2] + h->center.y * obj->matrix.m[1] + obj->matrix.m[0] * h->center.x;
    y = obj->matrix.m[5] * h->center.z + obj->matrix.m[3] * h->center.x + h->center.y * obj->matrix.m[4];
    z = h->center.y * obj->matrix.m[7] + obj->matrix.m[6] * h->center.x + h->center.z * obj->matrix.m[8];
    wx = x + pos.x;
    wy = y + pos.y;
    wz = z + pos.z;
    pos.x = gTransCtx.m[2] * wz + gTransCtx.m[1] * wy + gTransCtx.m[0] * wx;
    pos.y = gTransCtx.m[5] * wz + gTransCtx.m[4] * wy + gTransCtx.m[3] * wx;
    pos.z = gTransCtx.m[8] * wz + gTransCtx.m[7] * wy + gTransCtx.m[6] * wx;
    s = obj->scale[1] < obj->scale[0] ? obj->scale[0] : obj->scale[1];
    if (s > obj->scale[2])
        s = obj->scale[1] < obj->scale[0] ? obj->scale[0] : obj->scale[1];
    else
        s = obj->scale[2];
    radius = s * h->radius;
    if (Mesh3D_Exclude(&pos, radius, NULL))
        return;
    near = pos.z - radius;
    if (near > arg->minZ)
        return;
    if (h->format == 0 || h->format == 3) {
        wx = obj->pos.x - gTransCtx.pos[0];
        wy = obj->pos.y - gTransCtx.pos[1];
        wz = obj->pos.z - gTransCtx.pos[2];
        o.x = gTransCtx.m[2] * wz + gTransCtx.m[1] * wy + gTransCtx.m[0] * wx;
        o.y = gTransCtx.m[5] * wz + gTransCtx.m[4] * wy + gTransCtx.m[3] * wx;
        o.z = wz * gTransCtx.m[8] + wy * gTransCtx.m[7] + wx * gTransCtx.m[6];
        r = h->render;
        minZ = arg->minZ;
        rot.m[0] = gTransCtx.m[1] * obj->matrix.m[3] + gTransCtx.m[0] * obj->matrix.m[0] +
                   gTransCtx.m[2] * obj->matrix.m[6];
        rot.m[1] = gTransCtx.m[1] * obj->matrix.m[4] + gTransCtx.m[2] * obj->matrix.m[7] +
                   gTransCtx.m[0] * obj->matrix.m[1];
        rot.m[2] = gTransCtx.m[1] * obj->matrix.m[5] + gTransCtx.m[2] * obj->matrix.m[8] +
                   gTransCtx.m[0] * obj->matrix.m[2];
        rot.m[3] = gTransCtx.m[4] * obj->matrix.m[3] + gTransCtx.m[3] * obj->matrix.m[0] +
                   gTransCtx.m[5] * obj->matrix.m[6];
        rot.m[4] = gTransCtx.m[4] * obj->matrix.m[4] + gTransCtx.m[5] * obj->matrix.m[7] +
                   gTransCtx.m[3] * obj->matrix.m[1];
        rot.m[5] = gTransCtx.m[4] * obj->matrix.m[5] + gTransCtx.m[5] * obj->matrix.m[8] +
                   gTransCtx.m[3] * obj->matrix.m[2];
        rot.m[6] = gTransCtx.m[7] * obj->matrix.m[3] + gTransCtx.m[6] * obj->matrix.m[0] +
                   gTransCtx.m[8] * obj->matrix.m[6];
        rot.m[7] = gTransCtx.m[7] * obj->matrix.m[4] + gTransCtx.m[8] * obj->matrix.m[7] +
                   gTransCtx.m[6] * obj->matrix.m[1];
        rot.m[8] = gTransCtx.m[7] * obj->matrix.m[5] + gTransCtx.m[8] * obj->matrix.m[8] +
                   gTransCtx.m[6] * obj->matrix.m[2];
        rot.count = obj->matrix.count + gTransCtx.unk24 + 1;
        if (h->format == 0) {
            for (i = 0; i < 9; i++)
                rot.m[i] = rot.m[i] * obj->scale[i % 3];
            p = r->unk10;
            for (i = 0; i < (int)r->unkC; i++, p += 8) {
                tx = rot.m[0] * p[0] + rot.m[2] * p[2] + rot.m[1] * p[1];
                ty = rot.m[3] * p[0] + rot.m[5] * p[2] + rot.m[4] * p[1];
                tz = rot.m[6] * p[0] + rot.m[8] * p[2] + rot.m[7] * p[1];
                vz = tz + o.z;
                v = &gRigidClipVerts[i];
                if (!project(v, tx + o.x, ty + o.y, vz, nearZ, &sx, &sy, &rhw, 0) && v->z < minZ)
                    minZ = v->z;
            }
            if (near < arg->minZ) {
                for (g = r->unk14; g; g += sizeof(MeshGroup) + ((MeshGroup *)g)->next) {
                    idx = ((MeshGroup *)g)->indices;
                    for (j = 0; j < ((MeshGroup *)g)->indexCount / 3; j++, idx += 3)
                        clip_face(&gRigidClipVerts[idx[0] + ((MeshGroup *)g)->start],
                                  &gRigidClipVerts[idx[1] + ((MeshGroup *)g)->start],
                                  &gRigidClipVerts[idx[2] + ((MeshGroup *)g)->start], &minZ, nearZ);
                    if (((MeshGroup *)g)->next == 0xFFFF)
                        break;
                }
            }
        } else {
            p = r->unk14;
            for (i = 0; i < (int)r->unk10; i++, p += 10) {
                tx = rot.m[2] * p[2] + rot.m[1] * p[1] + rot.m[0] * p[0];
                ty = rot.m[5] * p[2] + rot.m[4] * p[1] + rot.m[3] * p[0];
                tz = rot.m[8] * p[2] + rot.m[7] * p[1] + rot.m[6] * p[0];
                vz = tz + o.z;
                v = &gLightmapClipVerts[i];
                if (!project(v, tx + o.x, ty + o.y, vz, nearZ, &sx, &sy, &rhw, 1) && v->z < minZ)
                    minZ = v->z;
            }
            if (near < arg->minZ) {
                for (g = r->unk1C; g; g += sizeof(LightmapGroup) + ((LightmapGroup *)g)->next) {
                    idx = ((LightmapGroup *)g)->indices;
                    for (j = 0; j < ((LightmapGroup *)g)->indexCount / 3; j++, idx += 3)
                        clip_face(&gLightmapClipVerts[idx[0] + ((LightmapGroup *)g)->start],
                                  &gLightmapClipVerts[idx[1] + ((LightmapGroup *)g)->start],
                                  &gLightmapClipVerts[idx[2] + ((LightmapGroup *)g)->start], &minZ, nearZ);
                    if (((LightmapGroup *)g)->next == 0xFFFF)
                        break;
                }
            }
        }
        arg->minZ = minZ;
    }
    if (obj->attached)
        EACH_CHILD(obj->attached, QCube_GetNearClipEvent, arg);
}
HOOK(0x4A1570, RigidDynCubeObj_NearZHandler);

void RigidDynCubeObj_CloseD3D(void)
{
    RigidDynCubeObj_Close();
}
HOOK(0x4A23D0, RigidDynCubeObj_CloseD3D);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(7205B8) int gStateB1;
BSS(7205B8) int gStateC2;
ALIAS(gAlphaBlendIdx, gStateB1, 0);
ALIAS(gLitAlphaIdx, gStateC2, 0);
BSS(7234B8) int gStateC0;
ALIAS(gLitIdx, gStateC0, 0);
BSS(723504) int gStateA1;
ALIAS(gLmStageIdx, gStateA1, 0);
BSS(7AC09C) int gStateB0;
ALIAS(gOpaqueBlendIdx, gStateB0, 0);
BSS(7AF1E0) int gBoneClipDepth;
PAD(igb, 7AF1E0, 7AF1E4, 0x4);
PAD(igb, 7AF1E0, 7AF1E8, 0xFDE80);
ALIAS(gRigidClipVerts, pad_7AF1E8, 0);
PAD(igb, 7AF1E0, 8AD068, 0x88B88);
ALIAS(gLightmapClipVerts, pad_8AD068, 0);
BSS(B46CE4) float gFrustumL;
BSS(B46CE4) float gFrustumTScale;
ALIAS(gExclBottomCos, gFrustumTScale, 0);
ALIAS(gExclLeft, gFrustumL, 0);
BSS(B46D18) float gFrustumBScale;
BSS(B46D18) float gFrustumR;
BSS(B46D18) float gFrustumRScale;
ALIAS(gExclRight, gFrustumR, 0);
ALIAS(gExclRightCos, gFrustumRScale, 0);
ALIAS(gExclTopCos, gFrustumBScale, 0);
BSS(B816FC) float gFrustumB;
ALIAS(gExclTop, gFrustumB, 0);
BSS(B8179C) float gFrustumT;
ALIAS(gExclBottom, gFrustumT, 0);
BSS(B817A4) float gFrustumLScale;
ALIAS(gExclLeftCos, gFrustumLScale, 0);
BSS(BCAD7C) StateList *gStateListB;
ALIAS(gD3DListB, gStateListB, 0);
ALIAS(gD3DStatesB, gStateListB, 0);
ALIAS(gStateCache1, gStateListB, 0);
BSS(BCADAC) StateList *gStateListC;
ALIAS(gD3DListC, gStateListC, 0);
ALIAS(gD3DStatesC, gStateListC, 0);
ALIAS(gStateCache2, gStateListC, 0);
BSS(BCADC4) StateList *gStateListD;
ALIAS(gD3DStateList, gStateListD, 0);
ALIAS(gD3DStatesD, gStateListD, 0);
ALIAS(gStateCache3, gStateListD, 0);
BSS(BCADCC) float gFarZ;
ALIAS(gD3DUnkBCADCC, gFarZ, 0);
ALIAS(gFarPlane, gFarZ, 0);
BSS(BCADDC) float gNearZ;
ALIAS(gD3DUnkBCADDC, gNearZ, 0);
BSS(BCADE8) StateList *gStateListA;
ALIAS(gD3DListA, gStateListA, 0);
ALIAS(gD3DStatesA, gStateListA, 0);
ALIAS(gStateCache0, gStateListA, 0);
