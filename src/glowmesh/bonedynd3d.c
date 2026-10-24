/* Direct3D side of BoneDynCubeObj: drawing and near plane depth. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "glowmesh/glowmesh.h"
#include "types.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4CFC40, int, Mesh3D_GetDrawBoneMethod, (void));
ORIG(0x4D03D0, int, Mesh3D_Exclude, (float *center, float radius, void *out));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D7550, void, BoneDynCubeObj_Open, (void));
ORIG(0x4D95E0, void, BoneDynCubeObj_Close, (void));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (int *list, int *count));
ORIG(0x4DAA20, int, Light_GetActive, (int *count));

typedef struct Task {
    Node node;
    Node *children; /* list head */
    char unkC[0x10];
    uint16_t type;
} Task;

typedef void (*TaskHandler)(Task *task, void *arg);

typedef struct MeshHeader {
    char unk0[0x30];
    float center[3];
    float radius;
} MeshHeader;

typedef struct MeshLod {
    char unk0[0xC];
    MeshHeader *header;
} MeshLod;

typedef struct VirModel {
    char unk0[0xC];
    MeshLod *lod;
} VirModel;

typedef struct BoneDynCubeObj {
    Task task;
    double pos[3];
    char unk38[0x3C];
    struct VirModel **model;
    float matrix[10];
    float unkA0;
    float scale[3];
    char unkB0[0x24];
    int unkD4;
} BoneDynCubeObj;

typedef struct MeshDrawArg {
    BoneDynCubeObj *obj;
    int lights;
    int lightCount;
    int haze;
    int hazeCount;
} MeshDrawArg;

/* what children get told: where the object is */
typedef struct PlaceArg {
    double pos[3];
    float matrix[10];
} PlaceArg;

typedef struct NearZArg {
    int unk0;
    int unk4;
    float minZ;
} NearZArg;

_Static_assert(sizeof(Task) == 0x20, "Task");
_Static_assert(offsetof(BoneDynCubeObj, model) == 0x74, "model");
_Static_assert(offsetof(BoneDynCubeObj, scale) == 0xA4, "scale");
_Static_assert(offsetof(BoneDynCubeObj, unkD4) == 0xD4, "unkD4");

extern Task *gTaskStack[];
extern int gTaskStackTop;
extern TaskHandler gTaskHandlers[][0x180];

#define HEADER(obj) ((*(obj)->model)->lod->header)

void BoneDynCubeObj_NearZHandler(BoneDynCubeObj *obj, NearZArg *arg);

/* draws the object, then tells its children where it is */
void BoneDynCubeObj_EnterHandler(BoneDynCubeObj *obj)
{
    MeshDrawArg d;
    PlaceArg a;
    Task *c;

    d.obj = obj;
    d.lights = Light_GetActive(&d.lightCount);
    Haze_GetVisible(&d.haze, &d.hazeCount);
    gRenderFuncs[Mesh3D_GetDrawBoneMethod()](&d);
    memcpy(a.pos, obj->pos, sizeof(a.pos));
    memcpy(a.matrix, obj->matrix, sizeof(a.matrix));
    c = (Task *)obj->task.children;
    if (!c->node.next)
        return;
    while (c) {
        TaskHandler fn;

        gTaskStack[gTaskStackTop++] = c->node.next && c->node.next->next ? (Task *)c->node.next : NULL;
        fn = gTaskHandlers[RigidDynCubeObj_GetEvent2()][c->type];
        if (fn)
            fn(c, &a);
        c = gTaskStack[--gTaskStackTop];
    }
}
HOOK(0x5124C0, BoneDynCubeObj_EnterHandler);

void BoneDynCubeObj_OpenD3D(void)
{
    uint8_t ev;

    BoneDynCubeObj_Open();
    ev = QCube_GetNearClipEvent();
    TaskType_SetHandler(BoneDynCubeObj_GetType(), ev, BoneDynCubeObj_NearZHandler);
}
HOOK(0x5125A0, BoneDynCubeObj_OpenD3D);

/* Lowers arg->minZ to the near side of the bounding sphere (not scaled),
   but not nearer than the near plane. */
void BoneDynCubeObj_NearZHandler(BoneDynCubeObj *obj, NearZArg *arg)
{
    float nearZ = gNearPlane;
    MeshHeader *h;
    float px, py, pz, y0, z0, s, radius;
    float v[3];
    long double x0, wx, wy, wz, n;

    if (obj->unkD4 != 1 || !obj->model)
        return;
    h = HEADER(obj);
    px = obj->pos[0] - gViewPos[0];
    py = obj->pos[1] - gViewPos[1];
    pz = obj->pos[2] - gViewPos[2];
    x0 = obj->matrix[2] * h->center[2] + obj->matrix[1] * h->center[1] + obj->matrix[0] * h->center[0];
    y0 = obj->matrix[5] * h->center[2] + obj->matrix[4] * h->center[1] + obj->matrix[3] * h->center[0];
    z0 = obj->matrix[8] * h->center[2] + obj->matrix[7] * h->center[1] + obj->matrix[6] * h->center[0];
    wx = x0 + px;
    wy = y0 + py;
    wz = z0 + pz;
    v[0] = gViewMatrix[2] * wz + gViewMatrix[1] * wy + gViewMatrix[0] * wx;
    v[1] = gViewMatrix[5] * wz + gViewMatrix[4] * wy + gViewMatrix[3] * wx;
    v[2] = gViewMatrix[8] * wz + gViewMatrix[7] * wy + gViewMatrix[6] * wx;
    s = !(obj->scale[1] >= obj->scale[0]) ? obj->scale[0] : obj->scale[1];
    if (s > obj->scale[2])
        s = !(obj->scale[1] >= obj->scale[0]) ? obj->scale[0] : obj->scale[1];
    else
        s = obj->scale[2];
    radius = s * HEADER(obj)->radius;
    if (Mesh3D_Exclude(v, radius, NULL))
        return;
    n = v[2] - h->radius;
    if (n > arg->minZ)
        return;
    if (nearZ > n)
        n = nearZ;
    arg->minZ = n;
}
HOOK(0x5125C0, BoneDynCubeObj_NearZHandler);

void BoneDynCubeObj_CloseD3D(void)
{
    BoneDynCubeObj_Close();
}
HOOK(0x5127D0, BoneDynCubeObj_CloseD3D);
