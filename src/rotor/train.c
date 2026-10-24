#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "rotor/rotor.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));

/* "Train" (0x8E0 bytes) */
typedef struct Train {
    RigidObj obj;
    char unkF0[0x694 - 0xF0];
    Matrix unk694;
    char unk6BC[4];
    Vec3d unk6C0;
    char unk6D8[0x88C - 0x6D8];
    Task *unk88C;
} Train;

_Static_assert(offsetof(Train, unk88C) == 0x88C, "Train");

extern Vec3d gTrainRelPos;
extern uint16_t gTrainType;
extern void *gTrainParams;
extern Matrix gTrainMatrix;

void Train_GetFrame(Train *t, void **out)
{
    Matrix tr;
    int i, j;

    ((void (*)(void *))HANDLER(1, t->obj.task.type))(t);
    gTrainRelPos.x = t->obj.pos.x - t->unk6C0.x;
    gTrainRelPos.y = t->obj.pos.y - t->unk6C0.y;
    gTrainRelPos.z = t->obj.pos.z - t->unk6C0.z;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            tr.m[i * 3 + j] = t->unk694.m[j * 3 + i];
    tr.count = t->unk694.count;
    mat_mul(&gTrainMatrix, &t->obj.matrix, &tr, 0, 1, 0);
    out[0] = &t->obj.pos;
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
    if (t->unk88C) {
        Task_Delete(t->unk88C);
        t->unk88C = NULL;
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
