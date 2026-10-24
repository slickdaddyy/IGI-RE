#include "hook.h"
#include "computer/computer.h"

FN_EXTERN(Task_Nothing);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));

#define HANDLER_4D3710 (FN(0x4D3710, Task_Nothing))

/* the area the map computer can show: a position and a size */
typedef struct ComputerLimit {
    Task task;
    Vec3d pos;
    float size;
    int unk3C;
} ComputerLimit;

_Static_assert(sizeof(ComputerLimit) == 0x40, "ComputerLimit");

extern void *gLimitParams;
extern uint16_t gLimitType;
extern ComputerLimit *gLimit;

void ComputerLimit_Create(ComputerLimit *l);
void ComputerLimit_Delete(void);
void ComputerLimit_GetParams(void *task, void **out);

void ComputerLimit_Open(void)
{
    ComputerLimit l;

    gLimitType = TaskType_New("DefineComputerLimit", sizeof(ComputerLimit), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gLimitType, 0, ComputerLimit_Create);
    TaskType_SetHandler2(gLimitType, 2, ComputerLimit_Delete);
    TaskType_SetHandler(gLimitType, Param_GetTableEvent(), ComputerLimit_GetParams);
    TaskType_SetHandler(gLimitType, Param_GetChangedEvent(), HANDLER_4D3710);
    gLimitParams = ParamTable_New(2);
    ParamTable_Add(gLimitParams, &l, &l.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gLimitParams, &l, &l.size, Param_Real32(), 7, NULL);
}
HOOK(0x466E40, ComputerLimit_Open);

void ComputerLimit_Create(ComputerLimit *l)
{
    gLimit = l;
}
HOOK(0x466F10, ComputerLimit_Create);

void ComputerLimit_Delete(void)
{
    gLimit = NULL;
}
HOOK(0x466F20, ComputerLimit_Delete);

void ComputerLimit_GetParams(void *task, void **out)
{
    *out = gLimitParams;
}
HOOK(0x466F30, ComputerLimit_GetParams);

void ComputerLimit_Close(void)
{
    TaskType_Free(gLimitType);
    QTaskList_Free(gLimitParams);
}
HOOK(0x466F40, ComputerLimit_Close);

Vec3d *ComputerLimit_Get(void)
{
    if (gLimit)
        return &gLimit->pos;
    return NULL;
}
HOOK(0x466F60, ComputerLimit_Get);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5BDC5C) void *gLimitParams;
BSS(5BDC5C) uint16_t gLimitType;
PAD(igb, 5BDC5C, 5BDC62, 0x6);
BSS(5BDC5C) ComputerLimit *gLimit;
