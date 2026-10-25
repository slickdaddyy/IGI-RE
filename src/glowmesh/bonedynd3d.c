/* Direct3D side of BoneDynCubeObj: drawing and near plane depth. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "glowmesh/glowmesh.h"
#include "bonemesh/bonemeshtypes.h"
#include "glowmesh/glowmeshtypes.h"
#include "haze/hazetypes.h"
#include "magicobj/magicobjtypes.h"
#include "model/modeltypes.h"
#include "task/taskvars.h"
#include "camfeed/camfeedvars.h"
#include "bonemesh/bonemeshvars.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4CFC40, int, Mesh3D_GetDrawBoneMethod, (void));
ORIG(0x4D03D0, int, Mesh3D_Exclude, (float *center, float radius, void *out));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D7550, void, BoneDynCubeObj_Open, (void));
ORIG(0x4D95E0, void, BoneDynCubeObj_Close, (void));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (int **list, int *count));
ORIG(0x4DAA20, int, Light_GetActive, (int *count));

typedef void (*TaskHandler)(Task *task, void *arg);

extern Task *gTaskStack[];
extern int gTaskStackTop;

#define HEADER(obj) (((ModelLod *)*(obj)->model)->mesh->header)

void BoneDynCubeObj_NearZHandler(BoneDynCubeObj *obj, NearZArg *arg);

/* draws the object, then tells its children where it is */
void BoneDynCubeObj_EnterHandler(BoneDynCubeObj *obj)
{
    MeshDrawArg d;
    Frame a;
    Task *c;

    d.obj = obj;
    d.lights = (int *)(Light_GetActive(&d.lightCount));
    Haze_GetVisible(&d.haze, &d.hazeCount);
    gRenderModeCurrent.methods[Mesh3D_GetDrawBoneMethod()](&d);
    memcpy(&a.pos, &obj->pos, sizeof(a.pos));
    memcpy(&a.matrix, &obj->matrix, sizeof(a.matrix));
    c = (Task *)obj->task.head;
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

    if (obj->layer != 1 || !obj->model)
        return;
    h = HEADER(obj);
    px = obj->pos.x - gTransContext.pos.x;
    py = obj->pos.y - gTransContext.pos.y;
    pz = obj->pos.z - gTransContext.pos.z;
    x0 = obj->matrix.m[2] * h->center.z + obj->matrix.m[1] * h->center.y + obj->matrix.m[0] * h->center.x;
    y0 = obj->matrix.m[5] * h->center.z + obj->matrix.m[4] * h->center.y + obj->matrix.m[3] * h->center.x;
    z0 = obj->matrix.m[8] * h->center.z + obj->matrix.m[7] * h->center.y + obj->matrix.m[6] * h->center.x;
    wx = x0 + px;
    wy = y0 + py;
    wz = z0 + pz;
    v[0] = gTransContext.matrix.m[2] * wz + gTransContext.matrix.m[1] * wy + gTransContext.matrix.m[0] * wx;
    v[1] = gTransContext.matrix.m[5] * wz + gTransContext.matrix.m[4] * wy + gTransContext.matrix.m[3] * wx;
    v[2] = gTransContext.matrix.m[8] * wz + gTransContext.matrix.m[7] * wy + gTransContext.matrix.m[6] * wx;
    s = !(obj->scale[1] >= obj->scale[0]) ? obj->scale[0] : obj->scale[1];
    if (s > obj->scale[2])
        s = !(obj->scale[1] >= obj->scale[0]) ? obj->scale[0] : obj->scale[1];
    else
        s = obj->scale[2];
    radius = s * HEADER(obj)->sphere;
    if (Mesh3D_Exclude(v, radius, NULL))
        return;
    n = v[2] - h->sphere;
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
