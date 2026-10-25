/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "flatwater/flatwater.h"
#include "math/mathvars.h"

/* "SmoothQTask": a script value that follows its ideal value (an expression
   or a fixed number) at a limited speed, snapped to a step and clamped.
   Scripts see it as "<name>.vRealValue". */

ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

typedef struct SmoothQTask {
    Task task;
    Vec3d pos;          /* 0x20 */
    float m[9];         /* 0x38 */
    int unk5C;
    double ideal;       /* 0x60 */
    double value;       /* 0x68 */
    double idealConst;  /* 0x70, used when there is no expression */
    double start;       /* 0x78 */
    double upSpeed;     /* 0x80 */
    double downSpeed;   /* 0x88 */
    double min;         /* 0x90 */
    double max;         /* 0x98 */
    double step;        /* 0xA0, 0 for none */
    VarString expr;     /* 0xA8 */
    char registered;    /* 0xF8 */
    char padF9[7];
} SmoothQTask;

_Static_assert(sizeof(SmoothQTask) == 0x100, "SmoothQTask");

extern uint16_t gSmoothType;
extern void *gSmoothParams;

void SmoothQTask_Create(SmoothQTask *t);
void SmoothQTask_Run(SmoothQTask *t);
void SmoothQTask_Delete(SmoothQTask *t);
void SmoothQTask_GetParams(void *task, void **out);
void SmoothQTask_ParamsChanged(SmoothQTask *t, char *arg);
void SmoothQTask_Activate(SmoothQTask *t);
void SmoothQTask_Deactivate(SmoothQTask *t);

void SmoothQTask_Open(void)
{
    SmoothQTask t;

    gSmoothType = TaskType_New("SmoothQTask", sizeof(SmoothQTask), EditorRigidObj_GetType(), 0);
    TaskType_SetHandler(gSmoothType, 0, SmoothQTask_Create);
    TaskType_SetHandler2(gSmoothType, 1, SmoothQTask_Run);
    TaskType_SetHandler2(gSmoothType, 2, SmoothQTask_Delete);
    TaskType_SetHandler(gSmoothType, Param_GetTableEvent(), SmoothQTask_GetParams);
    TaskType_SetHandler(gSmoothType, Param_GetChangedEvent(), SmoothQTask_ParamsChanged);
    TaskType_SetHandler2(gSmoothType, Level_GetActivateEvent(), SmoothQTask_Activate);
    TaskType_SetHandler2(gSmoothType, Level_GetDeactivateEvent(), SmoothQTask_Deactivate);
    gSmoothParams = ParamTable_New(10);
    ParamTable_Add(gSmoothParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.m, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.idealConst, Param_Real64(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.start, Param_Real64(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.upSpeed, Param_Real64(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.downSpeed, Param_Real64(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.min, Param_Real64(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.max, Param_Real64(), 7, NULL);
    ParamTable_Add(gSmoothParams, &t, &t.step, Param_Real64(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_SMOOTHQTASK", gSmoothType);
}
HOOK(0x51EEE0, SmoothQTask_Open);

void SmoothQTask_Create(SmoothQTask *t)
{
    VarString_Init(&t->expr);
    memset(&t->pos, 0, sizeof(t->pos));
    memset(t->m, 0, sizeof(t->m));
    t->m[0] = 1.0f;
    t->m[4] = 1.0f;
    t->m[8] = 1.0f;
    t->unk5C = 0;
    memset(&t->ideal, 0, 8 * 7);
    t->registered = 0;
    t->max = 1.0;
    t->step = 1.0;
}
HOOK(0x51F140, SmoothQTask_Create);

void SmoothQTask_Activate(SmoothQTask *t)
{
    char name[64];
    char buf[128];

    if (t->registered)
        return;
    t->registered = 1;
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.vRealValue", name);
    Script_RegisterReal(buf, &t->value);
    crt_sprintf(buf, "%s.vIdealValue", name);
    Script_RegisterReal(buf, &t->value);
}
HOOK(0x51F1F0, SmoothQTask_Activate);

void SmoothQTask_Deactivate(SmoothQTask *t)
{
    char name[64];
    char buf[128];

    Task_GetName(name, t);
    t->registered = 0;
    crt_sprintf(buf, "%s.vRealValue", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.vIdealValue", name);
    Script_RemoveSymbol(buf);
    t->registered = 0;
}
HOOK(0x51F270, SmoothQTask_Deactivate);

void SmoothQTask_Delete(SmoothQTask *t)
{
    if (Task_GetId(t) >= 0) {
        TaskFn1 fn = TASK_HANDLER(Level_GetDeactivateEvent(), t->task.type);

        if (fn)
            fn(t);
    }
    VarString_Free(&t->expr);
}
HOOK(0x51F2E0, SmoothQTask_Delete);

void SmoothQTask_GetParams(void *task, void **out)
{
    *out = gSmoothParams;
}
HOOK(0x51F330, SmoothQTask_GetParams);

void SmoothQTask_Run(SmoothQTask *t)
{
    long double x;

    BROADCAST(t->task.head, 1);
    if (VarString_HasExpr(&t->expr))
        t->ideal = VarString_EvalReal(&t->expr, t);
    else
        memcpy(&t->ideal, &t->idealConst, sizeof(double));
    if (!(t->value >= t->ideal)) {
        x = t->upSpeed + t->value;
        t->value = x;
        if (x > t->ideal)
            memcpy(&t->value, &t->ideal, sizeof(double));
    } else if (t->value > t->ideal) {
        x = t->value - t->downSpeed;
        t->value = x;
        if (!(x >= t->ideal))
            memcpy(&t->value, &t->ideal, sizeof(double));
    }
    if (fne(t->step, kZero64))
        t->value = (long double)x87_fistp(t->value / t->step) * t->step;
    if (t->min > t->value) {
        t->value = t->min;
        return;
    }
    if (!(t->max >= t->value))
        t->value = t->max;
}
HOOK(0x51F340, SmoothQTask_Run);

void SmoothQTask_ParamsChanged(SmoothQTask *t, char *arg)
{
    if (!arg || !*arg)
        BROADCAST(t->task.head, Param_GetChangedEvent(), NULL);
    VarString_Bind(&t->expr, t);
    memcpy(&t->value, &t->start, sizeof(double));
}
HOOK(0x51F4B0, SmoothQTask_ParamsChanged);

void SmoothQTask_Close(void)
{
    QTaskList_Free(gSmoothParams);
    TaskType_Free(gSmoothType);
}
HOOK(0x51F560, SmoothQTask_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A837A0) uint16_t gSmoothType;
PAD(igb, A837A0, A837A2, 0x2);
BSS(A837A0) void *gSmoothParams;
