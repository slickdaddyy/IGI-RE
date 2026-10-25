#include "hook.h"
#include "graphnode/graphnode.h"

ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4F65E0, char *, RainObj_FindCell, (void *rain, const Vec3d *pos));

#define RAIN_MAX 8

extern void *gRainParams;
extern uint16_t gRainObjType;
extern struct RainObj *gRainObjs[];
extern void (*gRainCallback)(char *state);

static const float kOne = 1.0f;
static const float kZero = 0.0f;

void RainDynCubeObj_Shutdown(void)
{
    QTaskList_Free(gRainParams);
    TaskType_Free(gRainObjType);
    gRainObjType = TASK_NO_TYPE;
}
HOOK(0x4F80B0, RainDynCubeObj_Shutdown);

void RainDynCubeObj_SetCallback(void *fn)
{
    gRainCallback = fn;
}
HOOK(0x4F80E0, RainDynCubeObj_SetCallback);

/* 1 when a rain cell with drops covers pos */
long double RainDynCubeObj_IsRainingAt(const Vec3d *pos)
{
    int i;

    for (i = 0; i < RAIN_MAX; i++) {
        char *cell;

        if (gRainObjs[i] && (cell = RainObj_FindCell((char *)gRainObjs[i], pos)) && *(int *)(cell + 0x1C))
            return kOne;
    }
    return kZero;
}
HOOK(0x4F80F0, RainDynCubeObj_IsRainingAt);

/* the ground hit list and its length of every rain object */
void RainDynCubeObj_GetHits(void **hits, int *counts, int *n)
{
    int i;

    *n = 0;
    for (i = 0; i < RAIN_MAX; i++) {
        char *r = (char *)gRainObjs[i];

        if (r) {
            hits[*n] = r + 0x2D18;
            counts[*n] = *(int *)(r + 0x4288);
            (*n)++;
        }
    }
}
HOOK(0x4F8140, RainDynCubeObj_GetHits);
