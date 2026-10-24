/* BoneDynCubeObj: a dyncube object animated by a bone hierarchy. */
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "haze/haze.h"

FN_EXTERN(BoneDynCubeObj_CollLine);
FN_EXTERN(BoneDynCubeObj_EnterHandler);
FN_EXTERN(BoneDynCubeObj_LineHit);
FN_EXTERN(Vehicle_CollFilter);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4C7760, void, DynCubeObj_ClearRenderData, (void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA10, uint8_t, World_GetEvent0_, (void *unused));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D2D60, void, BoneObj_Create, (BoneObj *obj, int skel));
ORIG(0x4D2E40, void, BoneObj_Free, (BoneObj *obj));

#define BONE_ENTER_HANDLER (FN(0x5124C0, BoneDynCubeObj_EnterHandler))
#define BONE_HANDLER_4D7620 (FN(0x4D7620, BoneDynCubeObj_LineHit))
#define BONE_HANDLER_4D88D0 (FN(0x4D88D0, BoneDynCubeObj_CollLine))
#define BONE_HANDLER_4F4390 (FN(0x4F4390, Vehicle_CollFilter))

extern uint16_t gBoneType;
extern uint8_t gBoneEvent;
extern uint16_t gDynCubeObjType;

void BoneDynCubeObj_GetInfo(BoneDynCubeObj *o, ObjInfo *info);
void BoneDynCubeObj_ActivateHandler(BoneDynCubeObj *o, int *arg);
void BoneDynCubeObj_MoveHandler(BoneDynCubeObj *o, char *arg);

void BoneDynCubeObj_Open(void)
{
    gBoneType = TaskType_New("BoneDynCubeObj", sizeof(BoneDynCubeObj), gDynCubeObjType, 0, 0);
    gBoneEvent = TaskEvent_Alloc(gBoneType);
    TaskType_SetHandler(gBoneType, 4, BONE_ENTER_HANDLER);
    TaskType_SetHandler(gBoneType, 7, BoneDynCubeObj_GetInfo);
    TaskType_SetHandler(gBoneType, 9, BONE_HANDLER_4D7620);
    TaskType_SetHandler(gBoneType, 8, BONE_HANDLER_4D88D0);
    TaskType_SetHandler(gBoneType, gBoneEvent, BONE_HANDLER_4F4390);
    TaskType_SetHandler(gBoneType, World_GetEvent0_(BoneDynCubeObj_ActivateHandler), BoneDynCubeObj_ActivateHandler);
    TaskType_SetHandler(gBoneType, 6, BoneDynCubeObj_MoveHandler);
}
HOOK(0x4D7550, BoneDynCubeObj_Open);

void BoneDynCubeObj_GetInfo(BoneDynCubeObj *o, ObjInfo *info)
{
    info->pos = o->pos;
    info->unk18[0] = 0;
    info->unk18[1] = 0;
    info->unk18[2] = 0;
    info->matrix = o->matrix;
    memcpy(&info->radius, &o->radius, 4);
    if (info->name)
        memcpy(info->name, "Bone Object", 12);
}
HOOK(0x4D9480, BoneDynCubeObj_GetInfo);

void BoneDynCubeObj_ActivateHandler(BoneDynCubeObj *o, int *arg)
{
    HzNode *n;
    HzHandler h;

    if (arg[0] == 0)
        DynCube_Add((void *)arg[1], o, &o->pos, o->radius, arg[2]);
    n = o->task.head;
    if (!n->next)
        return;
    while (n) {
        gTaskStack[gTaskStackTop++] = n->next && n->next->next ? n->next : NULL;
        h = gTaskHandlers[World_GetEvent0()][((HzTask *)n)->type];
        if (h)
            h(n, arg);
        n = gTaskStack[--gTaskStackTop];
    }
}
HOOK(0x4D94F0, BoneDynCubeObj_ActivateHandler);

void BoneDynCubeObj_MoveHandler(BoneDynCubeObj *o, char *arg)
{
    DynCube_Move(gWorld, o, (Vec3d *)arg, o->radius);
    o->matrix = *(Matrix *)(arg + 0x18);
}
HOOK(0x4D95A0, BoneDynCubeObj_MoveHandler);

void BoneDynCubeObj_Close(void)
{
    TaskType_Free(gBoneType);
    gBoneType = 0x181;
    TaskEvent_Free(gBoneEvent);
    gBoneEvent = 0xFF;
}
HOOK(0x4D95E0, BoneDynCubeObj_Close);

uint16_t BoneDynCubeObj_GetType(void)
{
    return gBoneType;
}
HOOK(0x4D9610, BoneDynCubeObj_GetType);

void BoneDynCubeObj_Init(void *world, BoneDynCubeObj *o, short skel, void **model)
{
    (void)world;
    DynCubeObj_ClearRenderData(o);
    BoneObj_Create(&o->bones, skel);
    o->model = model;
    o->matrix = (Matrix){ { 1.0f, 0, 0, 0, 1.0f, 0, 0, 0, 1.0f }, 0 };
    o->radius = VirModel_GetRadius(model);
    o->scale[0] = 1.0f;
    o->scale[1] = 1.0f;
    o->scale[2] = 1.0f;
    o->unkD0 = 0;
    o->unkB0 = 1.0f;
    o->unkB4 = 0;
    memset(o->unkB8, 0, sizeof(o->unkB8));
    o->unkCC = 1;
    o->unkD4 = 1;
}
HOOK(0x4D9620, BoneDynCubeObj_Init);

void BoneDynCubeObj_Deinit(BoneDynCubeObj *o)
{
    BoneObj_Free(&o->bones);
}
HOOK(0x4D96F0, BoneDynCubeObj_Deinit);

void BoneDynCubeObj_SetModel(BoneDynCubeObj *o, void **model)
{
    if (!model) {
        Error_Show("VirModel is null");
        for (;;)
            ;
    }
    o->model = model;
    o->radius = VirModel_GetRadius(model);
}
HOOK(0x4D9700, BoneDynCubeObj_SetModel);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(548618) uint16_t gBoneType INIT(385);
DATA(548618) uint8_t gBoneEvent INIT(255);
PAD(igd, 548618, 54861B, 0x1);
