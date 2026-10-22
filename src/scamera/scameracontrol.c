#include "hook.h"
#include "scamera/scamera.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, uint8_t, Level_GetEventDA8, (void));
ORIG(0x4F1220, uint8_t, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *table, int extra));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

#define gSCameraControlParams VAR(void *, 0x57C170)
#define gSCameraControlType VAR(uint16_t, 0x57C174)

void SCameraControl_Create(SCameraControl *c, void *arg);
void SCameraControl_Run(SCameraControl *c);
void SCameraControl_Delete(SCameraControl *c);
void SCameraControl_GetParams(Task *t, void **out);
void SCameraControl_ParamsChanged(SCameraControl *c, void *arg);
void SCameraControl_AddSymbols(SCameraControl *c);
void SCameraControl_RemoveSymbols(SCameraControl *c);

void SCameraControl_Open(void)
{
    gSCameraControlType = TaskType_New("SCameraControl", sizeof(SCameraControl), Terminal_GetType(), 0);
    TaskType_SetHandler(gSCameraControlType, 0, SCameraControl_Create);
    TaskType_SetHandler2(gSCameraControlType, 1, SCameraControl_Run);
    TaskType_SetHandler2(gSCameraControlType, 2, SCameraControl_Delete);
    TaskType_SetHandler(gSCameraControlType, Param_GetTableEvent(), SCameraControl_GetParams);
    TaskType_SetHandler(gSCameraControlType, Param_GetChangedEvent(), SCameraControl_ParamsChanged);
    TaskType_SetHandler2(gSCameraControlType, Level_GetEventDA8(), SCameraControl_AddSymbols);
    TaskType_SetHandler2(gSCameraControlType, Level_GetEventDAC(), SCameraControl_RemoveSymbols);
    gSCameraControlParams = ParamTable_Extend(Terminal_GetParams(), 1);
    ParamTable_Add(gSCameraControlParams, NULL, (void *)offsetof(SCameraControl, script), Param_VarString(), 7,
                   NULL);
}
HOOK(0x440C20, SCameraControl_Open);

void SCameraControl_Create(SCameraControl *c, void *arg)
{
    HANDLER(0, Terminal_GetType())(c, arg);
    c->detect = 0;
    c->registered = 0;
    VarString_Init(&c->script);
}
HOOK(0x440D30, SCameraControl_Create);

void SCameraControl_Run(SCameraControl *c)
{
    HANDLER(1, Terminal_GetType())(c);
    if (c->t.exploded)
        c->detect = 0;
    else if (VarString_HasExpr(&c->script))
        c->detect = round_int(VarString_EvalReal(&c->script, c));
}
HOOK(0x440D70, SCameraControl_Run);

void SCameraControl_Delete(SCameraControl *c)
{
    HANDLER(2, Terminal_GetType())(c);
    if (Task_GetId(c) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), c->t.o.task.type);

        if (fn)
            fn(c);
    }
    VarString_Free(&c->script);
}
HOOK(0x440DE0, SCameraControl_Delete);

void SCameraControl_GetParams(Task *t, void **out)
{
    *out = gSCameraControlParams;
}
HOOK(0x440E40, SCameraControl_GetParams);

void SCameraControl_ParamsChanged(SCameraControl *c, void *arg)
{
    uint8_t slot = Param_GetChangedEvent();

    HANDLER(slot, Terminal_GetType())(c, arg);
    VarString_Bind(&c->script, c);
}
HOOK(0x440E50, SCameraControl_ParamsChanged);

void SCameraControl_AddSymbols(SCameraControl *c)
{
    char name[64], buf[128];
    uint16_t type;

    if (c->registered)
        return;
    c->registered = 1;
    type = Terminal_GetType();
    HANDLER(Level_GetEventDA8(), type)(c);
    Task_GetName(name, c);
    crt_sprintf(buf, "%s.isDetection", name);
    Script_RegisterBool8(buf, &c->detect);
}
HOOK(0x440EA0, SCameraControl_AddSymbols);

void SCameraControl_RemoveSymbols(SCameraControl *c)
{
    char name[64], buf[128];
    uint16_t type;

    if (!c->registered)
        return;
    type = Terminal_GetType();
    HANDLER(Level_GetEventDAC(), type)(c);
    Task_GetName(name, c);
    c->registered = 0;
    crt_sprintf(buf, "%s.isDetection", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x440F30, SCameraControl_RemoveSymbols);

void SCameraControl_Close(void)
{
    QTaskList_Free(gSCameraControlParams);
    TaskType_Free(gSCameraControlType);
}
HOOK(0x440FB0, SCameraControl_Close);
