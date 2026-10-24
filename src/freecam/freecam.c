#include "freecam/freecam.h"

/* CamTask and FreeCamera: task types for scripted and free cameras. A
   FreeCamera sets the view every frame while it is active. */

typedef struct FreeCamera {
    Task task;
    float axes[10];
    Vec3d pos;
    float fov;
    char unk64[0x20];
    char active;
    char unk85[3];
    void *camera;
    int unk8C;
} FreeCamera;

_Static_assert(offsetof(FreeCamera, fov) == 0x60, "fov");
_Static_assert(sizeof(FreeCamera) == 0x90, "FreeCamera");

ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4E75A0, int, Screen_GetFrame, (void));
ORIG(0x4D9870, void, Camera_Set, (void *cam, const Vec3d *pos, const float *axes, float fov, float aspect,
                                  float priority, int frame, void *owner, int unk9, int unk10));

extern uint16_t gCamTaskType;
extern uint16_t gFreeCameraType;

void CamTask_Init(void)
{
    gCamTaskType = TaskType_New("CamTask", 0x88, Task_GetRootType(), 0, 0);
}
HOOK(0x530220, CamTask_Init);

void CamTask_Shutdown(void)
{
    TaskType_Free(gCamTaskType);
}
HOOK(0x530250, CamTask_Shutdown);

uint16_t CamTask_GetType(void)
{
    return gCamTaskType;
}
HOOK(0x530260, CamTask_GetType);

void FreeCamera_Run(FreeCamera *c);

void FreeCamera_Init(void)
{
    gFreeCameraType = TaskType_New("FreeCamera", sizeof(FreeCamera), Task_GetRootType(), 0, 0);
    TaskType_SetHandler2(gFreeCameraType, 1, FreeCamera_Run);
    TaskType_SetHandler2(gFreeCameraType, 2, Task_Nothing);
}
HOOK(0x5310E0, FreeCamera_Init);

void FreeCamera_Run(FreeCamera *c)
{
    if (c->active)
        Camera_Set(c->camera, &c->pos, c->axes, c->fov, 1.0f, 100.0f, Screen_GetFrame(), NULL, 0, 0);
}
HOOK(0x531130, FreeCamera_Run);

void FreeCamera_Shutdown(void)
{
    TaskType_Free(gFreeCameraType);
}
HOOK(0x531180, FreeCamera_Shutdown);
