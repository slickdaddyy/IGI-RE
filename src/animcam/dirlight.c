/* The last handlers of the Dirlight module (directional light with
   DirlightKeyframe children). */
#pragma GCC optimize("excess-precision=standard")

#include <float.h>

#include "hook.h"
#include "animcam/animcam.h"

typedef struct Pool {
    char *data;
    int *used;          /* slot index of each live item */
    int count;
    int max;
    int stride;
    int size;
} Pool;

/* "Dirlight" (0x64 bytes) */
typedef struct Dirlight {
    Task task;
    char unk20[0x34];
    float minTime;      /* 0x54 */
    float maxTime;      /* 0x58 */
    float time;         /* 0x5C */
    char unk60[4];
} Dirlight;

/* "DirlightKeyframe" (0x54 bytes) */
typedef struct DirlightKeyframe {
    Task task;
    char unk20[0x30];
    float time;         /* 0x50 */
} DirlightKeyframe;

_Static_assert(sizeof(Dirlight) == 0x64, "Dirlight");
_Static_assert(sizeof(DirlightKeyframe) == 0x54, "DirlightKeyframe");

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x5269C0, void, sub_5269C0, (Dirlight *d));

#define gDirlightParams VAR(void *, 0xA83D30)
#define gDirlightType VAR(uint16_t, 0xA83D34)
#define gDirlightKeyframeParams VAR(void *, 0xA83D48)
#define gDirlightKeyframeType VAR(uint16_t, 0xA83D4C)
#define gDirlights VAR(Pool, 0xBA1E00)

void DirlightKeyframe_GetParams(void *task, void **out)
{
    *out = gDirlightKeyframeParams;
}
HOOK(0x527460, DirlightKeyframe_GetParams);

void DirlightKeyframe_Delete(DirlightKeyframe *k)
{
    ((Dirlight *)k->task.parent)->time = -FLT_MAX;
}
HOOK(0x527470, DirlightKeyframe_Delete);

void Dirlight_Close(void)
{
    Pool_Free(&gDirlights);
    QTaskList_Free(gDirlightKeyframeParams);
    QTaskList_Free(gDirlightParams);
    TaskType_Free(gDirlightKeyframeType);
    TaskType_Free(gDirlightType);
}
HOOK(0x527480, Dirlight_Close);

uint16_t Dirlight_GetType(void)
{
    return gDirlightType;
}
HOOK(0x5274C0, Dirlight_GetType);

void Dirlight_SetTime(float time)
{
    int i;

    for (i = 0; i < gDirlights.count; i++) {
        Dirlight *d = *(Dirlight **)(gDirlights.data + gDirlights.used[i] * gDirlights.stride);

        if (time < d->time || time > d->time) {
            d->time = time;
            sub_5269C0(d);
        }
    }
}
HOOK(0x5274D0, Dirlight_SetTime);
