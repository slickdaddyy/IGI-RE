/* Direct3D mesh methods: bone meshes, mesh loading, sphere culling and
   the rigid object handlers. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "bonemesh/bonemesh.h"
#include "magicobj/magicobjtypes.h"
#include "bonemesh/bonemeshtypes.h"
#include "haze/hazetypes.h"
#include "d3d/d3dtypes.h"
#include "task/taskvars.h"
#include "camfeed/camfeedvars.h"
#include "bonemesh/bonemeshvars.h"
#include "file/filevars.h"
#include "glowmesh/glowmeshvars.h"

ORIG(0x491D40, long double, Display_GetAspect, (void));
ORIG(0x4950F0, void, Direct3D_MakeMatrix, (void *out, const Matrix *rot, const Vec3f *pos));
ORIG(0x497E70, void, TransContext_Select, (TransContext *ctx));
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
ORIG(0x4DA180, void, Haze_GetVisible, (int **a, int *b));
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

typedef struct SphereArg {
    Vec3d pos;
    float radius;
} SphereArg;

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

extern float gLayerFov[];
extern int gBoneClipDepth;
extern ClipVertex gRigidClipVerts[];
extern ClipVertex gLightmapClipVerts[];
extern DynVB gVBNormal;
extern int gD3DClippingOnIndex;
extern int gD3DClippingOffIndex;
extern float gNearZ;
extern int gTexStageIdx;
extern int gLmStageIdx;
extern int gOpaqueBlendIdx;
extern int gAlphaBlendIdx;
extern int gLitIdx;
extern int gUnlitIdx;
extern int gLitAlphaIdx;
extern int gUnlitAlphaIdx;

/* view frustum planes as x = k * z (0xB46CE4 ...), and their scales */
extern float gExclLeft;
extern float gExclLeftCos;
extern float gExclRight;
extern float gExclRightCos;
extern float gExclTop;
extern float gExclTopCos;
extern float gExclBottom;
extern float gExclBottomCos;

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
    BoneDynCubeObj *obj = arg->obj;
    VirModel *model = (VirModel *)obj->model;
    TransContext saved, view;
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
        saved = gTransContext;
        view = saved;
        view.fov = gLayerFov[0];
        fovH = gScene.h * gLayerFov[0];
        view.fovY = fovH / (Display_GetAspect() * gScene.w);
        view.scaleX = gScene.halfW / view.fov;
        view.layer = 0;
        view.scaleY = Display_GetAspect() * view.scaleX;
        TransContext_Select(&view);
    }
    lod = model->lods[n];
    h = (MeshHeader *)lod->mesh->header;
    pos.x = (float)(obj->pos.x - gTransContext.pos.x);
    pos.y = (float)(obj->pos.y - gTransContext.pos.y);
    pos.x = gWorldScale * pos.x;
    pos.y = gWorldScale * pos.y;
    pos.z = gWorldScale * (obj->pos.z - gTransContext.pos.z);
    rot.m[0] = gWorldScale;
    rot.m[1] = kZero.f;
    rot.m[2] = kZero.f;
    rot.m[3] = kZero.f;
    rot.m[4] = gWorldScale;
    rot.m[5] = kZero.f;
    rot.m[6] = kZero.f;
    rot.m[7] = kZero.f;
    rot.m[8] = gWorldScale;
    rot.count = 0;
    Direct3D_MakeMatrix(d3dm, &rot, &pos);
    DEV_SetTransform(gD3DDevice, 1, d3dm);
    Fog_SetRange(&obj->pos, h->sphere);
    if (h->format == 1) {
        r = h->data;
        g = r->unk1C;
        base = BoneMesh_Skin(r, model, &obj->bones, n);
        STATE_SELECT(gD3DTextureStates, gTexStageIdx);
        STATE_SELECT(gD3DBlendStates, gOpaqueBlendIdx);
        STATE_SELECT(gD3DRenderStates, gLitIdx);
        STATE_SELECT(gD3DClipStates, clip ? gD3DClippingOffIndex : gD3DClippingOnIndex);
        for (;;) {
            if (g->texture == 0xFFFF) {
                DEV_SetTextureStageState(gD3DDevice, 0, 1, 3);
                DEV_SetTexture(gD3DDevice, 0, NULL);
                DEV_DrawIndexedPrimitiveVB(gD3DDevice, r->primType, gVBNormal.vb, g->base + base, g->vertexCount,
                                           g->indices, g->indexCount, 0);
                DEV_SetTextureStageState(gD3DDevice, 0, 1, 4);
            } else {
                DEV_SetTexture(gD3DDevice, 0, ((void **)lod->textures[g->texture])[8]);
                DEV_DrawIndexedPrimitiveVB(gD3DDevice, r->primType, gVBNormal.vb, g->base + base, g->vertexCount,
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
        arg->header->data = find_data(arg->res, ID_RD3D);
        r = arg->header->data;
        r->unk10 = find_data(arg->res, ID_VRTX);
        r->unk14 = find_data(arg->res, ID_REND);
        MeshBatch_Add(0, arg->header, r->unkC);
        break;
    case 1:
        arg->header->data = find_data(arg->res, ID_RD3D);
        r = arg->header->data;
        r->unk18 = find_data(arg->res, ID_VRTX);
        r->unk1C = find_data(arg->res, ID_REND);
        break;
    case 3:
        arg->header->data = find_data(arg->res, ID_RD3D);
        r = arg->header->data;
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
        memset((char *)h->data + 0x18, 0, 6 * 4);
        break;
    case 1:
        break;
    case 3:
        memset((char *)h->data + 0x20, 0, 6 * 4);
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
    left = (c->x - gExclLeft * c->z) * gExclLeftCos;
    right = (c->x - gExclRight * c->z) * gExclRightCos;
    rightF = right;
    bottom = (c->y - gExclTop * c->z) * gExclTopCos;
    top = (c->y - gExclBottom * c->z) * gExclBottomCos;
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
    PtrList_Clear(gD3DBlendStates, gAlphaBlendIdx);
    PtrList_Clear(gD3DRenderStates, gUnlitAlphaIdx);
    PtrList_Clear(gD3DRenderStates, gLitAlphaIdx);
    PtrList_Clear(gD3DTextureStates, gLmStageIdx);
    PtrList_Clear(gD3DRenderStates, gUnlitIdx);
    PtrList_Clear(gD3DTextureStates, gTexStageIdx);
    PtrList_Clear(gD3DBlendStates, gOpaqueBlendIdx);
    PtrList_Clear(gD3DRenderStates, gLitIdx);
}
HOOK(0x4A0110, Direct3DRender_Shutdown);

/* draws the object, then tells its children (and those of the attached
   task) where it is */
void RigidDynCubeObj_EnterHandler(RigidDynCubeObj *obj)
{
    MeshDrawArg d;
    Frame a;

    d.obj = (struct BoneDynCubeObj *)obj;
    d.lights = (int *)(Light_GetActive(&d.lightCount));
    Haze_GetVisible(&d.haze, &d.hazeCount);
    gRenderModeCurrent.methods[Mesh3D_GetDrawRigidMethod()](&d);
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
    return (sy > gScene.maxY) << 3 | (sx > gScene.maxX) << 2 | (sy < gScene.minY) << 1 |
           (sx < gScene.minX);
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
            o->sx = r * gTransContext.scaleX * o->x + gTransContext.centerX;
            o->sy = o->rhw * o->y * gTransContext.scaleY + gTransContext.centerY;
            o->code = outcode(o->sx, o->sy);
            t = (nearZ - in->z) / (p->z - in->z);
            p->x = (p->x - in->x) * t + in->x;
            p->y = (p->y - in->y) * t + in->y;
            z = (p->z - in->z) * t + in->z;
            p->z = z;
            r = kOne.f / z;
            p->rhw = r;
            p->sx = r * p->x * gTransContext.scaleX + gTransContext.centerX;
            p->sy = gTransContext.scaleY * p->rhw * p->y + gTransContext.centerY;
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
            o->sx = r * o->x * gTransContext.scaleX + gTransContext.centerX;
            o->sy = gTransContext.scaleY * o->y * o->rhw + gTransContext.centerY;
            o->code = outcode(o->sx, o->sy);
            d = tmp.z - q->z;
            t = (nearZ - q->z) / d;
            tmp.x = (tmp.x - q->x) * t + q->x;
            tmp.y = (tmp.y - q->y) * t + q->y;
            tmp.z = d * t + q->z;
            r = kOne.f / tmp.z;
            tmp.rhw = r;
            sx = r * tmp.x * gTransContext.scaleX + gTransContext.centerX;
            tmp.sx = sx;
            sy = tmp.rhw * tmp.y * gTransContext.scaleY + gTransContext.centerY;
            tmp.sy = sy;
            tmp.code = (tmp.sy > gScene.maxY) << 3 | (sx > gScene.maxX) << 2 |
                       (sy < gScene.minY) << 1 | (sx < gScene.minX);
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
            tf = (gScene.minX - in->sx) / d;
            o->sx = d * tf + in->sx;
            o->sy = (o->sy - in->sy) * tf + in->sy;
            o->rhw = (o->rhw - in->rhw) * tf + in->rhw;
            o->code = outcode(o->sx, o->sy) & ~1;
            d = p->sx - in->sx;
            tf = (gScene.minX - in->sx) / d;
            p->sx = d * tf + in->sx;
            p->sy = (p->sy - in->sy) * tf + in->sy;
            p->rhw = (p->rhw - in->rhw) * tf + in->rhw;
            p->code = outcode(p->sx, p->sy) & ~1;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            d = o->sx - p->sx;
            tf = (gScene.minX - p->sx) / d;
            tmp = *o;
            o->sx = d * tf + p->sx;
            o->sy = (o->sy - p->sy) * tf + p->sy;
            o->rhw = (o->rhw - p->rhw) * tf + p->rhw;
            o->code = outcode(o->sx, o->sy) & ~1;
            d = tmp.sx - q->sx;
            tf = (gScene.minX - q->sx) / d;
            sx = d * tf + q->sx;
            tmp.sx = sx;
            sy = (tmp.sy - q->sy) * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (tmp.sy > gScene.maxY) << 3 | (sx > gScene.maxX) << 2 | (sy < gScene.minY) << 1;
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
            t = (gScene.minY - in->sy) / (o->sy - in->sy);
            o->sx = (o->sx - in->sx) * t + in->sx;
            o->sy = (o->sy - in->sy) * t + in->sy;
            o->rhw = (o->rhw - in->rhw) * t + in->rhw;
            o->code = outcode(o->sx, o->sy) & ~3;
            t = (gScene.minY - in->sy) / (p->sy - in->sy);
            p->sx = (p->sx - in->sx) * t + in->sx;
            p->sy = (p->sy - in->sy) * t + in->sy;
            p->rhw = (p->rhw - in->rhw) * t + in->rhw;
            p->code = outcode(p->sx, p->sy) & ~3;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (gScene.minY - p->sy) / (o->sy - p->sy);
            tmp = *o;
            o->sx = (o->sx - p->sx) * t + p->sx;
            o->sy = (o->sy - p->sy) * t + p->sy;
            o->rhw = (o->rhw - p->rhw) * t + p->rhw;
            o->code = outcode(o->sx, o->sy) & ~3;
            d = tmp.sy - q->sy;
            tf = (gScene.minY - q->sy) / d;
            tmp.sx = (tmp.sx - q->sx) * tf + q->sx;
            sy = d * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (sy > gScene.maxY) << 3 | (tmp.sx > gScene.maxX) << 2;
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
            tf = (gScene.maxX - in->sx) / d;
            o->sx = d * tf + in->sx;
            o->sy = (o->sy - in->sy) * tf + in->sy;
            o->rhw = (o->rhw - in->rhw) * tf + in->rhw;
            o->code = (o->sy > gScene.maxY) << 3;
            d = p->sx - in->sx;
            tf = (gScene.maxX - in->sx) / d;
            p->sx = d * tf + in->sx;
            p->sy = (p->sy - in->sy) * tf + in->sy;
            p->rhw = (p->rhw - in->rhw) * tf + in->rhw;
            p->code = (p->sy > gScene.maxY) << 3;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            d = o->sx - p->sx;
            tf = (gScene.maxX - p->sx) / d;
            tmp = *o;
            o->sx = d * tf + p->sx;
            o->sy = (o->sy - p->sy) * tf + p->sy;
            o->rhw = (o->rhw - p->rhw) * tf + p->rhw;
            o->code = (o->sy > gScene.maxY) << 3;
            d = tmp.sx - q->sx;
            tf = (gScene.maxX - q->sx) / d;
            tmp.sx = d * tf + q->sx;
            sy = (tmp.sy - q->sy) * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (sy > gScene.maxY) << 3;
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
            t = (gScene.maxY - in->sy) / (o->sy - in->sy);
            o->sx = (o->sx - in->sx) * t + in->sx;
            o->sy = (o->sy - in->sy) * t + in->sy;
            o->code = 0;
            o->rhw = (o->rhw - in->rhw) * t + in->rhw;
            t = (gScene.maxY - in->sy) / (p->sy - in->sy);
            p->sx = (p->sx - in->sx) * t + in->sx;
            p->sy = (p->sy - in->sy) * t + in->sy;
            p->code = 0;
            p->rhw = (p->rhw - in->rhw) * t + in->rhw;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (gScene.maxY - p->sy) / (o->sy - p->sy);
            tmp = *o;
            tmp.code = 0;
            o->sx = (o->sx - p->sx) * t + p->sx;
            o->sy = (o->sy - p->sy) * t + p->sy;
            o->code = 0;
            o->rhw = (o->rhw - p->rhw) * t + p->rhw;
            d = tmp.sy - q->sy;
            tf = (gScene.maxY - q->sy) / d;
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
    out->radius = h->sphere;
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
        px = r * x * gTransContext.scaleX + gTransContext.centerX;
        *sx = px;
        if (lightmap)
            py = *rhw * y * gTransContext.scaleY + gTransContext.centerY;
        else
            py = gTransContext.scaleY * *rhw * y + gTransContext.centerY;
        *sy = py;
        code = (*sy > gScene.maxY) << 3 | (px > gScene.maxX) << 2 | (py < gScene.minY) << 1 |
               (px < gScene.minX);
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
    long double x, wx, wy, wz, zn, vz;
    ClipVertex *v;
    char *g;
    uint16_t *idx;
    const float *p;
    int i, j;

    if (obj->hidden != 1 || !obj->model || !((VirModel *)obj->model)->lods[0])
        return;
    h = (MeshHeader *)((VirModel *)obj->model)->lods[0]->mesh->header;
    pos.x = (float)(obj->pos.x - gTransContext.pos.x);
    pos.y = (float)(obj->pos.y - gTransContext.pos.y);
    pos.z = (float)(obj->pos.z - gTransContext.pos.z);
    x = h->center.z * obj->matrix.m[2] + h->center.y * obj->matrix.m[1] + obj->matrix.m[0] * h->center.x;
    y = obj->matrix.m[5] * h->center.z + obj->matrix.m[3] * h->center.x + h->center.y * obj->matrix.m[4];
    z = h->center.y * obj->matrix.m[7] + obj->matrix.m[6] * h->center.x + h->center.z * obj->matrix.m[8];
    wx = x + pos.x;
    wy = y + pos.y;
    wz = z + pos.z;
    pos.x = gTransContext.matrix.m[2] * wz + gTransContext.matrix.m[1] * wy + gTransContext.matrix.m[0] * wx;
    pos.y = gTransContext.matrix.m[5] * wz + gTransContext.matrix.m[4] * wy + gTransContext.matrix.m[3] * wx;
    pos.z = gTransContext.matrix.m[8] * wz + gTransContext.matrix.m[7] * wy + gTransContext.matrix.m[6] * wx;
    s = obj->scale[1] < obj->scale[0] ? obj->scale[0] : obj->scale[1];
    if (s > obj->scale[2])
        s = obj->scale[1] < obj->scale[0] ? obj->scale[0] : obj->scale[1];
    else
        s = obj->scale[2];
    radius = s * h->sphere;
    if (Mesh3D_Exclude(&pos, radius, NULL))
        return;
    zn = pos.z - radius;
    if (zn > arg->minZ)
        return;
    if (h->format == 0 || h->format == 3) {
        wx = obj->pos.x - gTransContext.pos.x;
        wy = obj->pos.y - gTransContext.pos.y;
        wz = obj->pos.z - gTransContext.pos.z;
        o.x = gTransContext.matrix.m[2] * wz + gTransContext.matrix.m[1] * wy + gTransContext.matrix.m[0] * wx;
        o.y = gTransContext.matrix.m[5] * wz + gTransContext.matrix.m[4] * wy + gTransContext.matrix.m[3] * wx;
        o.z = wz * gTransContext.matrix.m[8] + wy * gTransContext.matrix.m[7] + wx * gTransContext.matrix.m[6];
        r = h->data;
        minZ = arg->minZ;
        rot.m[0] = gTransContext.matrix.m[1] * obj->matrix.m[3] + gTransContext.matrix.m[0] * obj->matrix.m[0] +
                   gTransContext.matrix.m[2] * obj->matrix.m[6];
        rot.m[1] = gTransContext.matrix.m[1] * obj->matrix.m[4] + gTransContext.matrix.m[2] * obj->matrix.m[7] +
                   gTransContext.matrix.m[0] * obj->matrix.m[1];
        rot.m[2] = gTransContext.matrix.m[1] * obj->matrix.m[5] + gTransContext.matrix.m[2] * obj->matrix.m[8] +
                   gTransContext.matrix.m[0] * obj->matrix.m[2];
        rot.m[3] = gTransContext.matrix.m[4] * obj->matrix.m[3] + gTransContext.matrix.m[3] * obj->matrix.m[0] +
                   gTransContext.matrix.m[5] * obj->matrix.m[6];
        rot.m[4] = gTransContext.matrix.m[4] * obj->matrix.m[4] + gTransContext.matrix.m[5] * obj->matrix.m[7] +
                   gTransContext.matrix.m[3] * obj->matrix.m[1];
        rot.m[5] = gTransContext.matrix.m[4] * obj->matrix.m[5] + gTransContext.matrix.m[5] * obj->matrix.m[8] +
                   gTransContext.matrix.m[3] * obj->matrix.m[2];
        rot.m[6] = gTransContext.matrix.m[7] * obj->matrix.m[3] + gTransContext.matrix.m[6] * obj->matrix.m[0] +
                   gTransContext.matrix.m[8] * obj->matrix.m[6];
        rot.m[7] = gTransContext.matrix.m[7] * obj->matrix.m[4] + gTransContext.matrix.m[8] * obj->matrix.m[7] +
                   gTransContext.matrix.m[6] * obj->matrix.m[1];
        rot.m[8] = gTransContext.matrix.m[7] * obj->matrix.m[5] + gTransContext.matrix.m[8] * obj->matrix.m[8] +
                   gTransContext.matrix.m[6] * obj->matrix.m[2];
        rot.count = obj->matrix.count + gTransContext.matrix.count + 1;
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
            if (zn < arg->minZ) {
                for (g = r->unk14; g; g += sizeof(MeshGroup) + ((MeshGroup *)g)->next) {
                    idx = ((MeshGroup *)g)->indices;
                    for (j = 0; j < ((MeshGroup *)g)->indexCount / 3; j++, idx += 3)
                        clip_face(&gRigidClipVerts[idx[0] + ((MeshGroup *)g)->base],
                                  &gRigidClipVerts[idx[1] + ((MeshGroup *)g)->base],
                                  &gRigidClipVerts[idx[2] + ((MeshGroup *)g)->base], &minZ, nearZ);
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
            if (zn < arg->minZ) {
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
BSS(7AF1E0) int gBoneClipDepth;
PAD(igb, 7AF1E0, 7AF1E4, 0x4);
PAD(igb, 7AF1E0, 7AF1E8, 0xFDE80);
ALIAS(gRigidClipVerts, pad_7AF1E8, 0);
PAD(igb, 7AF1E0, 8AD068, 0x88B88);
ALIAS(gLightmapClipVerts, pad_8AD068, 0);
BSS(BCAAE0) TransContext gTransContext;
