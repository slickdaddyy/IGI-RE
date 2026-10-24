#pragma GCC optimize("excess-precision=standard")

#include "freecam/freecam.h"
#include "freecam/light.h"

/* FlashTask fades the colour and range of a point light through a list of
   keys; a key with frames -1 at the end makes it loop. */

typedef struct FlashKey {
    float color[3];
    int frames;
    float range;
} FlashKey;

typedef struct FlashTask {
    Task task;
    PointLightObj *light;
    void *world;
    Vec3d pos;
    FlashKey *keys;
    int keyCount;
    int key;
    int frame;
    char looped;
    char unk51[3];
    float color[3];
    float step[3];
    float cur[3];
    float rangeStep;
    float range;
    Task *owner;
    int unk84;
} FlashTask;

_Static_assert(sizeof(FlashTask) == 0x88, "FlashTask");

ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));

extern uint16_t gFlashTaskType;
extern char gFlashTaskReady;

void FlashTask_Run(FlashTask *t);
void FlashTask_Delete(FlashTask *t);

void FlashTask_Init(void)
{
    gFlashTaskType = TaskType_New("FlashTask", sizeof(FlashTask), Task_GetRootType(), 2, 0);
    TaskType_SetHandler2(gFlashTaskType, 2, FlashTask_Delete);
    TaskType_SetHandler2(gFlashTaskType, 1, FlashTask_Run);
    gFlashTaskReady = 1;
}
HOOK(0x530A80, FlashTask_Init);

static void flash_show(FlashTask *t)
{
    memcpy(t->light->color, t->color, 12);
    PointLightObj_SetRange(t->world, t->light, &t->pos, t->range);
}

void FlashTask_Run(FlashTask *t)
{
    FlashKey *k = &t->keys[t->key];

    if (t->frame < k[-1].frames && !t->looped) {
        t->frame++;
        t->cur[0] = t->step[0] + t->cur[0];
        t->cur[1] = t->step[1] + t->cur[1];
        t->color[1] = t->cur[1];
        t->cur[2] = t->step[2] + t->cur[2];
        t->color[2] = t->cur[2];
        t->color[0] = t->cur[0];
        t->range = t->rangeStep + t->range;
        flash_show(t);
    } else if (t->key < t->keyCount - 1) {
        t->step[0] = (k[1].color[0] - k->color[0]) / k->frames;
        t->step[1] = (k[1].color[1] - k->color[1]) / k->frames;
        t->step[2] = (k[1].color[2] - k->color[2]) / k->frames;
        t->rangeStep = (k[1].range - k->range) / k->frames;
        memcpy(t->cur, k->color, 12);
        memcpy(t->color, k->color, 12);
        t->key++;
        memcpy(&t->range, &k->range, 4);
        t->frame = 0;
        t->looped = 0;
        flash_show(t);
    } else if (k->frames == -1) {
        t->key = 0;
        t->frame = 0;
        t->looped = 1;
    } else {
        Task_Delete(t->owner);
    }
}
HOOK(0x530AD0, FlashTask_Run);

void FlashTask_Delete(FlashTask *t)
{
    PointLightObj_Delete(t->world, t->light);
}
HOOK(0x530C40, FlashTask_Delete);

void FlashTask_Shutdown(void)
{
    TaskType_Free(gFlashTaskType);
    gFlashTaskReady = 0;
}
HOOK(0x530C60, FlashTask_Shutdown);
