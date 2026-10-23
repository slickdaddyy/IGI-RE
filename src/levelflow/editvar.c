#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "levelflow/levelflow.h"

/* a script counter: each frame nValue goes up by the value of inc and
   down by the value of dec */
typedef struct EditVariable {
    Task task;
    Vec3d pos;
    VarString inc;
    VarString dec;
    int nValue;
    char registered;
    char unkDD[3];
} EditVariable;

_Static_assert(sizeof(EditVariable) == 0xE0, "EditVariable");

#define gEditVariableType VAR(uint16_t, 0x5410D0)
#define gEditVariableParams VAR(ParamTable *, 0x5C8A9C)

void EditVariable_Create(EditVariable *v);
void EditVariable_Run(EditVariable *v);
void EditVariable_Delete(EditVariable *v);
void EditVariable_GetParams(void *task, ParamTable **out);
void EditVariable_Changed(EditVariable *v);
void EditVariable_Activate(EditVariable *v);
void EditVariable_Deactivate(EditVariable *v);

void EditVariable_Init(void)
{
    gEditVariableType = TaskType_New("EditVariable", sizeof(EditVariable), Position_GetType(), 0);
    TaskType_SetHandler(gEditVariableType, 0, EditVariable_Create);
    TaskType_SetHandler2(gEditVariableType, 1, EditVariable_Run);
    TaskType_SetHandler2(gEditVariableType, 2, EditVariable_Delete);
    TaskType_SetHandler2(gEditVariableType, 3, TASK_NOTHING);
    TaskType_SetHandler(gEditVariableType, Param_GetTableEvent(), EditVariable_GetParams);
    TaskType_SetHandler(gEditVariableType, Param_GetChangedEvent(), EditVariable_Changed);
    TaskType_SetHandler2(gEditVariableType, Level_GetEventDA8(), EditVariable_Activate);
    TaskType_SetHandler2(gEditVariableType, Level_GetEventDAC(), EditVariable_Deactivate);
    gEditVariableParams = ParamTable_New(4);
    ParamTable_Add(gEditVariableParams, NULL, FIELD(EditVariable, pos), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gEditVariableParams, NULL, FIELD(EditVariable, nValue), Param_Int32(), 7, NULL);
    ParamTable_Add(gEditVariableParams, NULL, FIELD(EditVariable, inc), Param_VarString(), 7, NULL);
    ParamTable_Add(gEditVariableParams, NULL, FIELD(EditVariable, dec), Param_VarString(), 7, NULL);
}
HOOK(0x488800, EditVariable_Init);

void EditVariable_Create(EditVariable *v)
{
    VarString_Init(&v->inc);
    VarString_Init(&v->dec);
    v->nValue = 0;
    v->registered = 0;
}
HOOK(0x488980, EditVariable_Create);

void EditVariable_Run(EditVariable *v)
{
    if (VarString_HasExpr(&v->inc))
        v->nValue += round_int(VarString_EvalReal(&v->inc, v));
    if (VarString_HasExpr(&v->dec))
        v->nValue -= round_int(VarString_EvalReal(&v->dec, v));
}
HOOK(0x4889B0, EditVariable_Run);

void EditVariable_Delete(EditVariable *v)
{
    Task_GetId(v);
    if (Task_GetId(v) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), v->task.type);

        if (fn)
            fn(v);
    }
    VarString_Free(&v->inc);
    VarString_Free(&v->dec);
}
HOOK(0x488A30, EditVariable_Delete);

void EditVariable_GetParams(void *task, ParamTable **out)
{
    *out = gEditVariableParams;
}
HOOK(0x488A90, EditVariable_GetParams);

void EditVariable_Changed(EditVariable *v)
{
    VarString_Bind(&v->inc, v);
    VarString_Bind(&v->dec, v);
}
HOOK(0x488AA0, EditVariable_Changed);

void EditVariable_Activate(EditVariable *v)
{
    char name[64];
    char buf[128];

    if (v->registered)
        return;
    v->registered = 1;
    Task_GetName(name, v);
    crt_sprintf(buf, "%s.nValue", name);
    Script_RegisterInt2(buf, &v->nValue);
}
HOOK(0x488AD0, EditVariable_Activate);

void EditVariable_Deactivate(EditVariable *v)
{
    char name[64];
    char buf[128];

    if (!v->registered)
        return;
    Task_GetName(name, v);
    crt_sprintf(buf, "%s.nValue", name);
    Script_RemoveSymbol(buf);
    v->registered = 0;
}
HOOK(0x488B30, EditVariable_Deactivate);

void EditVariable_Close(void)
{
    QTaskList_Free(gEditVariableParams);
    TaskType_Free(gEditVariableType);
}
HOOK(0x488B90, EditVariable_Close);
