/* QObj, the base of everything the qcube renderer draws, and cameras. */
#include "hook.h"
#include "igi.h"
#include "haze/haze.h"

ORIG(0x401860, void, TaskEvent_Set, (uint16_t type, uint8_t index));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (int a, int b));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1800, QTaskList *, QTaskList_New, (int max));
ORIG(0x4C1830, void, QTaskList_Free, (QTaskList *list));

extern uint16_t gQObjType;
extern uint8_t gQObjEvent;
extern uint8_t gCameraEvent;
extern QTaskList *gCameraListeners;

void QObj_Open(void)
{
    TaskEvent_Set(0, 4);
    TaskEvent_Set(0, 5);
    TaskEvent_Set(0, 6);
    TaskEvent_Set(0, 7);
    gQObjEvent = TaskEvent_Alloc(0);
    gQObjType = TaskType_New("QObj", sizeof(QObj), Task_GetRootType(0, 0), 0, 0);
}
HOOK(0x4D9730, QObj_Open);

void QObj_Close(void)
{
    TaskType_Free(gQObjType);
    gQObjType = 0x181;
    TaskEvent_Free(7);
    TaskEvent_Free(4);
    TaskEvent_Free(5);
    TaskEvent_Free(6);
    TaskEvent_Free(gQObjEvent);
}
HOOK(0x4D9790, QObj_Close);

uint8_t QObj_GetEvent(void)
{
    return gQObjEvent;
}
HOOK(0x4D97E0, QObj_GetEvent);

void QObj_SetRenderData(QObj *o, void *data)
{
    o->renderData = data;
}
HOOK(0x4D97F0, QObj_SetRenderData);

void *QObj_GetRenderData(QObj *o)
{
    return o->renderData;
}
HOOK(0x4D9800, QObj_GetRenderData);

void Camera_Open(void)
{
    gCameraEvent = TaskEvent_Alloc(0);
    gCameraListeners = QTaskList_New(0x100);
}
HOOK(0x4D9810, Camera_Open);

void Camera_Close(void)
{
    TaskEvent_Free(gCameraEvent);
    QTaskList_Free(gCameraListeners);
}
HOOK(0x4D9830, Camera_Close);

uint8_t Camera_GetChangedEvent(void)
{
    return gCameraEvent;
}
HOOK(0x4D9850, Camera_GetChangedEvent);

QTaskList *Camera_GetListeners(void)
{
    return gCameraListeners;
}
HOOK(0x4D9860, Camera_GetListeners);

/* Takes the view when it is from a newer frame or has at least the current
   priority, then tells every listener. */
void Camera_Set(Camera *cam, const Vec3d *pos, const Matrix *axes, float fov, float aspect, float priority,
                int frame, void *owner, int unk58, int unk5C)
{
    int arg, i;

    cam->count++;
    if (frame > cam->frame) {
        cam->frame = frame;
        cam->priority = -1.0f;
    }
    if (!(priority >= cam->priority))
        return;
    cam->pos = *pos;
    cam->axes = *axes;
    cam->fov = fov;
    cam->aspect = aspect;
    cam->owner = owner;
    cam->unk58 = unk58;
    cam->unk5C = unk5C;
    cam->priority = priority;
    arg = unk5C;
    for (i = 0; i < Camera_GetListeners()->count; i++) {
        HzHandler h = gTaskHandlers[Camera_GetChangedEvent()][Camera_GetListeners()->items[i]->type];

        h(Camera_GetListeners()->items[i], &arg);
    }
}
HOOK(0x4D9870, Camera_Set);

void Camera_Init(Camera *cam)
{
    cam->frame = -1;
    cam->count = 0;
}
HOOK(0x4D9950, Camera_Init);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(548638) uint16_t gTaskType548638 INIT(385);
DATA(548638) uint8_t gQObjEvent INIT(255);
PAD(igd, 548638, 54863B, 0x9) INIT({0x00, 0x51, 0x4F, 0x62, 0x6A});
DATA(548638) uint8_t gCameraEvent INIT(255);
PAD(igd, 548638, 548645, 0x3);
ALIAS(gQObjType, gTaskType548638, 0);
BSS(A54690) QTaskList *gCameraListeners;
