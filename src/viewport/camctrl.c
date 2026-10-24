#include "hook.h"
#include "viewport/viewport.h"

/* "CameraControlTask": runs its children, then moves the sound listener to its camera. */

typedef struct CameraControl {
    Task task;
    Camera *camera;
    int unk24;
    Task *listener;
} CameraControl;

extern uint16_t gCameraControlType;
extern CameraControl *gCameraControlCurrent;

ORIG(0x4EBBA0, Task *, SoundSysMicrophone_New, (void *parent, void *arg));
ORIG(0x4EBBF0, void, SoundSysMicrophone_Set, (Task *listener, Camera *rot, double *pos));

void CameraControl_Run(CameraControl *cc);

void CameraControl_Init(void)
{
    gCameraControlType = TaskType_New("CameraControlTask", sizeof(CameraControl), Task_GetRootType(), 0);
    TaskType_SetHandler2(gCameraControlType, 1, CameraControl_Run);
    TaskType_SetHandler2(gCameraControlType, 2, TASK_NOTHING);
}
HOOK(0x4E7C50, CameraControl_Init);

void CameraControl_Run(CameraControl *cc)
{
    gCameraControlCurrent = cc;
    CALL_CHILDREN(&cc->task, 1, 1);
    gCameraControlCurrent = NULL;
    if (cc->listener)
        SoundSysMicrophone_Set(cc->listener, cc->camera, cc->camera->pos);
}
HOOK(0x4E7C90, CameraControl_Run);

void CameraControl_Shutdown(void)
{
    TaskType_Free(gCameraControlType);
}
HOOK(0x4E7D30, CameraControl_Shutdown);

CameraControl *CameraControl_New(void *parent, Camera *camera)
{
    CameraControl *cc = (CameraControl *)Task_New(parent, gCameraControlType, NULL);

    cc->camera = camera;
    cc->unk24 = 0;
    cc->listener = NULL;
    return cc;
}
HOOK(0x4E7D40, CameraControl_New);

Task *CameraControl_AddListener(void *parent, CameraControl *cc, void *arg)
{
    return cc->listener = SoundSysMicrophone_New(parent, arg);
}
HOOK(0x4E7D70, CameraControl_AddListener);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54D974) uint16_t gCameraControlType INIT(385);
PAD(igd, 54D974, 54D976, 0x2);
BSS(A5EBDC) CameraControl *gCameraControlCurrent;
