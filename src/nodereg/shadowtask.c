/* ShadowTask: a direction and a strength, edited as task parameters. */
#include "hook.h"
#include "nodereg/nodereg.h"

/* "ShadowTask" (0x30) */
typedef struct ShadowTask {
    Task task;
    Vec3f dir;
    float strength;
} ShadowTask;

_Static_assert(sizeof(ShadowTask) == 0x30, "ShadowTask");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F50, int, Param_Real32, (void));
ORIG(0x4F0F60, int, Param_Real32x3, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, int type, int flags, void *extra));

extern uint16_t gShadowTaskType;
extern void *gShadowTaskParams;

void ShadowTask_GetParamTable(ShadowTask *s, ParamTable *out);

void ShadowTask_Open(void)
{
    ShadowTask p;

    gShadowTaskType = TaskType_New("ShadowTask", sizeof(ShadowTask), Task_GetRootType(), 0, 0);
    TaskType_SetHandler(gShadowTaskType, Param_GetTableEvent(), ShadowTask_GetParamTable);
    TaskType_SetHandler(gShadowTaskType, Param_GetChangedEvent(), TASK_NOTHING);
    gShadowTaskParams = ParamTable_New(2);
    ParamTable_Add(gShadowTaskParams, &p, &p.dir, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gShadowTaskParams, &p, &p.strength, Param_Real32(), 7, NULL);
}
HOOK(0x519760, ShadowTask_Open);

void ShadowTask_GetParamTable(ShadowTask *s, ParamTable *out)
{
    *out = gShadowTaskParams;
}
HOOK(0x519810, ShadowTask_GetParamTable);

void ShadowTask_Close(void)
{
    QTaskList_Free(gShadowTaskParams);
    TaskType_Free(gShadowTaskType);
    gShadowTaskType = TASK_NO_TYPE;
}
HOOK(0x519820, ShadowTask_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54ED3C) uint16_t gShadowTaskType INIT(385);
PAD(igd, 54ED3C, 54ED3E, 0x2);
BSS(A83744) void *gShadowTaskParams;
