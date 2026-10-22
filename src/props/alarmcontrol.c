#include "hook.h"
#include "props/props.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x443E80, uint16_t, Terminal_GetType, (void));
ORIG(0x443E90, void *, Terminal_GetParams, (void));
ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1080, int, Level_GetTick, (void));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *parent, int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

/* "AlarmControl" (0x350 bytes), a Terminal that raises the alarm. Two script
   expressions set isTrigger and isAlarm while the terminal works. */
typedef struct AlarmControl {
    Task task;
    char unk20[0x170];
    char unk190;            /* terminal: off */
    char unk191[0xAF];
    char unk240;            /* terminal: powered */
    char unk241[0x5F];
    char isTrigger;         /* 0x2A0 */
    char isAlarm;           /* 0x2A1 */
    char lastAlarm;         /* 0x2A2 */
    char pad2A3;
    int nTriggerLastTick;   /* 0x2A4 */
    char registered;        /* 0x2A8 */
    char pad2A9[3];
    VarString trigger;      /* 0x2AC */
    VarString alarm;        /* 0x2FC */
    char pad34C[4];
} AlarmControl;

_Static_assert(offsetof(AlarmControl, unk240) == 0x240, "unk240");
_Static_assert(offsetof(AlarmControl, trigger) == 0x2AC, "trigger");
_Static_assert(sizeof(AlarmControl) == 0x350, "AlarmControl");

#define gAlarmControlType VAR(uint16_t, 0x57C164)
#define gAlarmControlParams VAR(void *, 0x57C168)

void AlarmControl_Create(AlarmControl *a, void *arg);
void AlarmControl_Run(AlarmControl *a);
void AlarmControl_Delete(AlarmControl *a);
void AlarmControl_GetParams(void *task, void **out);
void AlarmControl_ParamsChanged(AlarmControl *a, void *arg);
void AlarmControl_Activate(AlarmControl *a);
void AlarmControl_Deactivate(AlarmControl *a);

void AlarmControl_Open(void)
{
    AlarmControl a;

    gAlarmControlType = TaskType_New("AlarmControl", sizeof(AlarmControl), Terminal_GetType(), 0);
    TaskType_SetHandler(gAlarmControlType, 0, AlarmControl_Create);
    TaskType_SetHandler2(gAlarmControlType, 1, AlarmControl_Run);
    TaskType_SetHandler2(gAlarmControlType, 2, AlarmControl_Delete);
    TaskType_SetHandler(gAlarmControlType, Param_GetTableEvent(), AlarmControl_GetParams);
    TaskType_SetHandler(gAlarmControlType, Param_GetChangedEvent(), AlarmControl_ParamsChanged);
    TaskType_SetHandler2(gAlarmControlType, Level_GetEventDA8(), AlarmControl_Activate);
    TaskType_SetHandler2(gAlarmControlType, Level_GetEventDAC(), AlarmControl_Deactivate);
    gAlarmControlParams = ParamTable_Extend(Terminal_GetParams(), 2);
    ParamTable_Add(gAlarmControlParams, &a, &a.trigger, Param_VarString(), 7, NULL);
    ParamTable_Add(gAlarmControlParams, &a, &a.alarm, Param_VarString(), 7, NULL);
}
HOOK(0x440710, AlarmControl_Open);

void AlarmControl_Create(AlarmControl *a, void *arg)
{
    HANDLER(0, Terminal_GetType())(a, arg);
    a->isTrigger = 0;
    a->isAlarm = 0;
    a->lastAlarm = 0;
    a->nTriggerLastTick = -10000;
    a->registered = 0;
    VarString_Init(&a->trigger);
    VarString_Init(&a->alarm);
}
HOOK(0x440840, AlarmControl_Create);

void AlarmControl_Run(AlarmControl *a)
{
    HANDLER(1, Terminal_GetType())(a);
    a->lastAlarm = a->isAlarm;
    if (a->unk190 || !a->unk240) {
        a->isTrigger = 0;
        a->isAlarm = 0;
        return;
    }
    if (VarString_HasExpr(&a->trigger))
        a->isTrigger = (char)round_ll(VarString_EvalReal(&a->trigger, a));
    if (a->isTrigger)
        a->nTriggerLastTick = Level_GetTick();
    if (VarString_HasExpr(&a->alarm))
        a->isAlarm = (char)round_ll(VarString_EvalReal(&a->alarm, a));
}
HOOK(0x4408A0, AlarmControl_Run);

void AlarmControl_Delete(AlarmControl *a)
{
    HANDLER(2, Terminal_GetType())(a);
    if (Task_GetId(a) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), a->task.type);

        if (fn)
            fn(a);
    }
    VarString_Free(&a->trigger);
    VarString_Free(&a->alarm);
}
HOOK(0x440970, AlarmControl_Delete);

void AlarmControl_GetParams(void *task, void **out)
{
    *out = gAlarmControlParams;
}
HOOK(0x4409E0, AlarmControl_GetParams);

void AlarmControl_ParamsChanged(AlarmControl *a, void *arg)
{
    int slot = Param_GetChangedEvent();

    HANDLER(slot, Terminal_GetType())(a, arg);
    VarString_Bind(&a->trigger, a);
    VarString_Bind(&a->alarm, a);
}
HOOK(0x4409F0, AlarmControl_ParamsChanged);

void AlarmControl_Activate(AlarmControl *a)
{
    char name[64], buf[128];
    uint16_t type;

    if (a->registered)
        return;
    a->registered = 1;
    type = Terminal_GetType();
    HANDLER(Level_GetEventDA8(), type)(a);
    Task_GetName(name, a);
    crt_sprintf(buf, "%s.isTrigger", name);
    Script_RegisterBool8(buf, &a->isTrigger);
    crt_sprintf(buf, "%s.isAlarm", name);
    Script_RegisterBool8(buf, &a->isAlarm);
    crt_sprintf(buf, "%s.nTriggerLastTick", name);
    Script_RegisterInt(buf, &a->nTriggerLastTick);
}
HOOK(0x440A40, AlarmControl_Activate);

void AlarmControl_Deactivate(AlarmControl *a)
{
    char name[64], buf[128];
    uint16_t type;

    if (!a->registered)
        return;
    type = Terminal_GetType();
    HANDLER(Level_GetEventDAC(), type)(a);
    Task_GetName(name, a);
    a->registered = 0;
    crt_sprintf(buf, "%s.isTrigger", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isAlarm", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nTriggerLastTick", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x440B20, AlarmControl_Deactivate);

void AlarmControl_Close(void)
{
    QTaskList_Free(gAlarmControlParams);
    TaskType_Free(gAlarmControlType);
}
HOOK(0x440BE0, AlarmControl_Close);

char AlarmControl_IsAlarm(AlarmControl *a)
{
    return a->isAlarm;
}
HOOK(0x440C00, AlarmControl_IsAlarm);
