/* The last handlers of the Dirlight module (directional light with
   DirlightKeyframe children). */
#pragma GCC optimize("excess-precision=standard")

#include <float.h>

#include "hook.h"
#include "animcam/animcam.h"
#include "dirlight/dirlighttypes.h"
#include "animcam/animcamvars.h"
#include "dirlight/dirlightvars.h"

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x5269C0, void, Dirlight_Update, (Dirlight *d));

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
    Pool_Free(&gDirlightPool);
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

    for (i = 0; i < gDirlightPool.used; i++) {
        Dirlight *d = *(Dirlight **)(gDirlightPool.data + gDirlightPool.free[i] * gDirlightPool.stride);

        if (time < d->time || time > d->time) {
            d->time = time;
            Dirlight_Update(d);
        }
    }
}
HOOK(0x5274D0, Dirlight_SetTime);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(BA1E00) Pool gDirlightPool;
