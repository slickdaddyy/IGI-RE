#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "rotor/rotor.h"
#include "missile/missiletypes.h"
#include "task/taskvars.h"
#include "missile/missilevars.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));

extern Vec3d gTrainRelPos;
extern Matrix gTrainMatrix;

void Train_GetFrame(Train *t, void **out)
{
    Matrix tr;
    int i, j;

    ((void (*)(void *))HANDLER(1, t->v.task.type))(t);
    gTrainRelPos.x = t->v.pos.x - t->v.lastPos.x;
    gTrainRelPos.y = t->v.pos.y - t->v.lastPos.y;
    gTrainRelPos.z = t->v.pos.z - t->v.lastPos.z;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            tr.m[i * 3 + j] = t->v.lastMatrix.m[j * 3 + i];
    tr.count = t->v.lastMatrix.count;
    mat_mul(&gTrainMatrix, &t->v.matrix, &tr, 0, 1, 0);
    out[0] = &t->v.pos;
    out[2] = &gTrainRelPos;
    out[1] = &gTrainMatrix;
}
HOOK(0x42B930, Train_GetFrame);

void Train_OnWorldEvent0(Train *t, void *arg)
{
    uint8_t slot = World_GetEvent0();
    uint16_t type = Vehicle_GetType();

    gTaskHandlers[slot][type](t, arg);
}
HOOK(0x42BB60, Train_OnWorldEvent0);

void Train_OnWorldEvent1(Train *t, void *arg)
{
    uint8_t slot = World_GetEvent1();
    uint16_t type = Vehicle_GetType();

    gTaskHandlers[slot][type](t, arg);
    if (t->loop_sound) {
        Task_Delete(t->loop_sound);
        t->loop_sound = NULL;
    }
}
HOOK(0x42BBA0, Train_OnWorldEvent1);

void Train_Close(void)
{
    TaskType_Free(gTrainType);
    QTaskList_Free(gTrainParams);
}
HOOK(0x42BC00, Train_Close);

uint16_t Train_GetType(void)
{
    return gTrainType;
}
HOOK(0x42BC20, Train_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BCA0) Vec3d gTrainRelPos;
BSS(57BCC0) Matrix gTrainMatrix;
