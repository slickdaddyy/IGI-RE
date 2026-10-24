#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "sound/sound.h"
#include "magicobj/magicobjtypes.h"

FN_EXTERN(SplineObjSegment_Hit);

ORIG(0x49A010, void, SplineCache_Remove, (void *obj));
ORIG(0x49A050, float *, SplineMesh_GetVerts, (void *obj, int model, int part, int unk3, int unk4, int *count));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *tree, void *o, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *tree, void *o, Vec3d *pos, float radius, int unk));
ORIG(0x4C7560, void, DynCube_Remove, (void *tree, void *o));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4CFC50, int, Mesh3D_GetDrawSplineMethod, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (int **layers, int *count));
ORIG(0x4DAA20, int *, Light_GetActive, (int *count));
ORIG(0x4E5360, uint8_t, DynCubeObj_GetMaskVisibleEvent, (void));
ORIG(0x4E55B0, uint8_t, CellMask_GetDrawEvent, (void));
ORIG(0x4E5610, void, CellMask_FillTriangle, (void *mask, float ax, float ay, float bx, float by, float cx,
                                             float cy));
ORIG(0x4E4530, void, SplineObjSegment_Setup, (void *o, Vec3d *from, Vec3d *to, float *rotFrom, float *rotTo,
                                              char rigid, char reversed, int segments, int unk,
                                              const char *model));

#define SplineObjSegment_Hit (FN(0x4E16E0, SplineObjSegment_Hit))

typedef void (*RenderFn)(void *arg);
extern RenderFn gRenderFuncs[];
extern uint16_t gSplineObjSegmentType;

/* a collision mesh part */
typedef struct MeshPart {
    int triCount;
    int vertCount;
    int unk08[2];
    uint16_t (*tris)[4];
    int unk14[3];
} MeshPart;

typedef struct SplineBuffer {
    void *data;
    int count;
} SplineBuffer;

/* "SplineObjSegment" (0x5B8 bytes), a RigidDynCubeObj whose model is bent
   along a curve; see SplineMesh_GetVerts */
typedef struct SplineObjSegment {
    Task task;
    Vec3d pos;
    char unk38[0x34];
    char **models;
    float rot[10];
    char unk98[0xC];
    int lod;
    char unkA8[0x48];
    void *tree;
    float radius;
    float length;
    float start;
    float end;
    float segPos[21][3];
    float segRot[20][10];
    float lastRot[9];
    float segLength[21];        /* [0] is lastRot's spare tenth word */
    int unk598;
    int segments;
    SplineBuffer buffers[2];
    char **lastModel;
    char rigid;
    char reversed;
    char pad5B6[2];
} SplineObjSegment;

_Static_assert(offsetof(SplineObjSegment, lod) == 0xA4, "lod");
_Static_assert(offsetof(SplineObjSegment, tree) == 0xF0, "tree");
_Static_assert(offsetof(SplineObjSegment, segRot) == 0x200, "segRot");
_Static_assert(offsetof(SplineObjSegment, segLength) == 0x544, "segLength");
_Static_assert(offsetof(SplineObjSegment, buffers) == 0x5A0, "buffers");
_Static_assert(sizeof(SplineObjSegment) == 0x5B8, "SplineObjSegment");

/* the argument of the cell mask draw event */
typedef struct CellMaskDrawArg {
    void *mask;
    int size;
    double *center;
    float range;
} CellMaskDrawArg;

/* the argument of the spline draw method */
typedef struct SplineDrawArg {
    SplineObjSegment *obj;
    int *lights;
    int lightCount;
    int *layers;
    int layerCount;
} SplineDrawArg;

static const float kHalf = 0.5f;
static const double kHalfD = 0.5;

/* node of the bounding sphere tree built while setting up a segment */
typedef struct SphereNode {
    float x, y, z;
    float radius;
    uint16_t firstTri;
    uint16_t triCount;
    uint16_t child;
    uint16_t next;
} SphereNode;

typedef struct SphereMesh {
    char unk0[0x10];
    uint16_t (*tris)[4];
} SphereMesh;

extern float *gSphereVerts;
extern SphereNode **gSphereNodes;
extern SphereMesh *gSphereMesh;
extern SphereNode *gSphereStack[];
extern int gSphereDepth;

void SplineObjSegment_Delete(SplineObjSegment *o)
{
    SplineBuffer *b = o->buffers;
    int i;

    DynCube_Remove(o->tree, o);
    for (i = 2; i; i--, b++) {
        if (b->data) {
            Mem_Free(b->data);
            b->data = NULL;
        }
    }
}
HOOK(0x4E3F80, SplineObjSegment_Delete);

/* draws both collision meshes from above as 2D triangles */
void SplineObjSegment_DrawMask(SplineObjSegment *o, CellMaskDrawArg *view)
{
    int visible = 1;
    double px, py;
    TaskFn h;
    MeshPart *part;
    int i, j;

    if (!o->models)
        return;
    h = gTaskHandlers[DynCubeObj_GetMaskVisibleEvent()][o->task.type];
    if (h)
        h(o, &visible);
    if (!visible)
        return;
    px = o->pos.x;
    py = o->pos.y;
    part = *(MeshPart **)(*(char **)(*(char **)(o->models[o->lod] + 0xC) + 0xC) + 0x2C);
    for (j = 0; j < 2; j++, part++) {
        float *v = SplineMesh_GetVerts(o, o->lod, j, 1, 0, NULL);

        for (i = 0; i < part->triCount; i++) {
            long double k = (long double)view->size * kHalf / view->range;
            const float *a = v + part->tris[i][0] * 3;
            const float *b = v + part->tris[i][1] * 3;
            const float *c = v + part->tris[i][2] * 3;
            double *ctr = view->center;
            float ax = (float)((((long double)a[0] + px) - ctr[0] + view->range) * k);
            float ay = (float)((((long double)a[1] + py) - ctr[1] + view->range) * k);
            float bx = (float)((((long double)b[0] + px) - ctr[0] + view->range) * k);
            float by = (float)((((long double)b[1] + py) - ctr[1] + view->range) * k);
            float cx = (float)((((long double)c[0] + px) - ctr[0] + view->range) * k);
            float cy = (float)((((long double)c[1] + py) - ctr[1] + view->range) * k);

            CellMask_FillTriangle(view->mask, ax, ay, bx, by, cx, cy);
        }
    }
}
HOOK(0x4E3FC0, SplineObjSegment_DrawMask);

void SplineObjSegment_Render(SplineObjSegment *o);
void Task_YesHandler(void *o, char *out);

void SplineObjSegment_Init(void)
{
    gSplineObjSegmentType = TaskType_New("SplineObjSegment", sizeof(SplineObjSegment), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler2(gSplineObjSegmentType, 2, SplineObjSegment_Delete);
    TaskType_SetHandler(gSplineObjSegmentType, 4, SplineObjSegment_Render);
    TaskType_SetHandler(gSplineObjSegmentType, 10, SplineObjSegment_Hit);
    TaskType_SetHandler(gSplineObjSegmentType, 11, TASK_NOTHING);
    TaskType_SetHandler(gSplineObjSegmentType, 9, SplineObjSegment_Hit);
    TaskType_SetHandler(gSplineObjSegmentType, 12, Task_YesHandler);
    TaskType_SetHandler(gSplineObjSegmentType, CellMask_GetDrawEvent(), SplineObjSegment_DrawMask);
    Task_SetGlobalFlags(0x10000000);
    TaskType_SetFlags(gSplineObjSegmentType, 0x10000000);
}
HOOK(0x4E41B0, SplineObjSegment_Init);

void Task_YesHandler(void *o, char *out)
{
    (void)o;
    *out = 1;
}
HOOK(0x4E4280, Task_YesHandler);

/* queues the bent mesh with the active lights, then moves the children
   along with the identity rotation */
void SplineObjSegment_Render(SplineObjSegment *o)
{
    SplineDrawArg draw;
    Frame move;

    draw.obj = o;
    draw.lights = Light_GetActive(&draw.lightCount);
    Haze_GetVisible(&draw.layers, &draw.layerCount);
    gRenderFuncs[Mesh3D_GetDrawSplineMethod()](&draw);
    move.pos = o->pos;
    move.matrix.m[0] = 1.0f;
    move.matrix.m[1] = 0;
    move.matrix.m[2] = 0;
    move.matrix.m[3] = 0;
    move.matrix.m[4] = 1.0f;
    move.matrix.m[5] = 0;
    move.matrix.m[6] = 0;
    move.matrix.m[7] = 0;
    move.matrix.m[8] = 1.0f;
    move.matrix.count = 0;
    CALL_CHILDREN(&o->task, RigidDynCubeObj_GetEvent2(), &move);
}
HOOK(0x4E4290, SplineObjSegment_Render);

void SplineObjSegment_Shutdown(void)
{
    Task_ClearGlobalFlags(0x10000000);
    TaskType_Free(gSplineObjSegmentType);
}
HOOK(0x4E43B0, SplineObjSegment_Shutdown);

SplineObjSegment *SplineObjSegment_New(Task *parent, void *tree, Vec3d *from, Vec3d *to, float *rotFrom,
                                       float *rotTo, char rigid, char reversed, int segments, int unk,
                                       const char *model)
{
    SplineObjSegment *o = (SplineObjSegment *)Task_New(parent, gSplineObjSegmentType, NULL);
    Vec3d mid;
    float dist;
    int i;

    RigidDynCubeObj_Init(tree, o, VirModel_Find(model));
    o->rot[0] = 1.0f;
    o->rot[4] = 1.0f;
    o->rot[8] = 1.0f;
    o->rot[1] = 0;
    o->rot[2] = 0;
    o->rot[3] = 0;
    o->rot[5] = 0;
    o->rot[6] = 0;
    o->rot[7] = 0;
    o->rot[9] = 0;
    for (i = 0; i < 2; i++) {
        o->buffers[i].data = NULL;
        o->buffers[i].count = 0;
    }
    o->tree = tree;
    o->segments = segments;
    o->lastModel = NULL;
    if (rigid)
        o->segments = 1;
    o->reversed = 0;
    o->rigid = 0;
    mid.x = (from->x + to->x) * kHalfD;
    mid.y = (from->y + to->y) * kHalfD;
    mid.z = (from->z + to->z) * kHalfD;
    dist = Vec3d_Distance(from, to);
    DynCube_Add(tree, o, &mid, dist, 1);
    /* rewrites the same value, left from a debugging check */
    if (((uint32_t *)from)[0] == 0 && ((uint32_t *)from)[1] == 0xC113F2DE) {
        ((uint32_t *)from)[0] = 0;
        ((uint32_t *)from)[1] = 0xC113F2DE;
    }
    SplineObjSegment_Setup(o, from, to, rotFrom, rotTo, rigid, reversed, segments, unk, model);
    return o;
}
HOOK(0x4E43D0, SplineObjSegment_New);

/* the radius of each node on the stack grows to reach every vertex of the
   triangles of the node at the top and of all nodes below it */
void SplineObjSegment_GrowRadius(void)
{
    SphereNode *node = gSphereStack[gSphereDepth];
    int t, i, k;

    node->radius = 0;
    gSphereDepth++;
    if (node->child) {
        SphereNode *c = &(*gSphereNodes)[node->child];

        gSphereStack[gSphereDepth] = c;
        SplineObjSegment_GrowRadius();
        while (c->next) {
            c = &(*gSphereNodes)[c->next];
            gSphereStack[gSphereDepth] = c;
            SplineObjSegment_GrowRadius();
        }
    }
    for (t = node->firstTri; t < node->firstTri + node->triCount; t++) {
        uint16_t *tri = gSphereMesh->tris[t];
        const float *v[3];

        v[0] = gSphereVerts + tri[0] * 3;
        v[1] = gSphereVerts + tri[1] * 3;
        v[2] = gSphereVerts + tri[2] * 3;
        for (i = 0; i < gSphereDepth; i++) {
            SphereNode *n = gSphereStack[i];

            for (k = 0; k < 3; k++) {
                long double dz = (long double)v[k][2] - n->z;
                long double dy = (long double)v[k][1] - n->y;
                long double dx = (long double)v[k][0] - n->x;
                long double d = __builtin_sqrtl(dx * dx + dy * dy + dz * dz);

                n->radius = n->radius > d ? n->radius : d;
            }
        }
    }
    gSphereDepth--;
}
HOOK(0x4E51C0, SplineObjSegment_GrowRadius);

uint16_t SplineObjSegment_GetType(void)
{
    return gSplineObjSegmentType;
}
HOOK(0x4E5350, SplineObjSegment_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, A5E374, A5E374, 0x78);
ALIAS(gSphereStack, pad_A5E374, 0);
BSS(A5E374) float *gSphereVerts;
BSS(A5E374) SphereNode **gSphereNodes;
BSS(A5E374) uint16_t gSplineObjSegmentType;
PAD(igb, A5E374, A5E3F6, 0x2);
BSS(A5E374) SphereMesh *gSphereMesh;
PAD(igb, A5E374, A5E3FC, 0x4);
BSS(A5E374) int gSphereDepth;
