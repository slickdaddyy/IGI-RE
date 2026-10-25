#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "litmap/litmap.h"

ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0F80, void *, Param_Real64, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B7FC0, void, Script_RegisterReal, (const char *name, double *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4EE940, char, ScreenGrab_IsActive, (void));

/* A script value that follows a target (an expression or a fixed value) at
   a rise and fall speed, with random noise on top, published as
   "<name>.vValue". */
typedef struct NoiseQTask {
    Task task;
    Vec3d pos;
    Matrix matrix;
    double target;
    double value;
    double current;
    double noise;
    int noiseTime;
    int unk84;
    double base;
    double start;
    double rise;
    double fall;
    double noiseMin;
    double noiseMax;
    int timeMin;
    int timeMax;
    double min;
    double max;
    double step;
    VarString expr;
    char registered;
    char unk129[7];
} NoiseQTask;

_Static_assert(offsetof(NoiseQTask, target) == 0x60, "target");
_Static_assert(offsetof(NoiseQTask, base) == 0x88, "base");
_Static_assert(offsetof(NoiseQTask, expr) == 0xD8, "expr");
_Static_assert(sizeof(NoiseQTask) == 0x130, "NoiseQTask");

extern uint16_t gNoiseQTaskType;
extern void *gNoiseQTaskParams;
extern char gNoiseRandom[];

void NoiseQTask_Create(NoiseQTask *n);
void NoiseQTask_Update(NoiseQTask *n);
void NoiseQTask_Delete(NoiseQTask *n);
void NoiseQTask_GetParams(NoiseQTask *n, void **out);
void NoiseQTask_Changed(NoiseQTask *n, const char *arg);
void NoiseQTask_Register(NoiseQTask *n);
void NoiseQTask_Unregister(NoiseQTask *n);

void NoiseQTask_Init(void)
{
    NoiseQTask t;
    uint16_t type;

    type = TaskType_New("NoiseQTask", sizeof(NoiseQTask), EditorRigidObj_GetType(), 0, 0);
    gNoiseQTaskType = type;
    TaskType_SetHandler(type, 0, NoiseQTask_Create);
    TaskType_SetHandler2(gNoiseQTaskType, 1, NoiseQTask_Update);
    TaskType_SetHandler2(gNoiseQTaskType, 2, NoiseQTask_Delete);
    TaskType_SetHandler(gNoiseQTaskType, Param_GetTableEvent(), NoiseQTask_GetParams);
    TaskType_SetHandler(gNoiseQTaskType, Param_GetChangedEvent(), NoiseQTask_Changed);
    TaskType_SetHandler2(gNoiseQTaskType, Level_GetActivateEvent(), NoiseQTask_Register);
    TaskType_SetHandler2(gNoiseQTaskType, Level_GetDeactivateEvent(), NoiseQTask_Unregister);
    gNoiseQTaskParams = ParamTable_New(20);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.base, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.start, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.rise, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.fall, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.noiseMin, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.noiseMax, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.timeMin, Param_Int32(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.timeMax, Param_Int32(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.min, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.max, Param_Real64(), 7, NULL);
    ParamTable_Add(gNoiseQTaskParams, &t, &t.step, Param_Real64(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_NOISEQTASK", gNoiseQTaskType);
    Random_Init(gNoiseRandom);
}
HOOK(0x51FEF0, NoiseQTask_Init);

void NoiseQTask_Create(NoiseQTask *n)
{
    int i;

    VarString_Init(&n->expr);
    for (i = 0; i < 9; i++)
        n->matrix.m[i] = 0;
    n->matrix.m[0] = n->matrix.m[4] = n->matrix.m[8] = 1.0f;
    n->matrix.count = 0;
    n->pos.x = n->pos.y = n->pos.z = 0;
    n->target = n->value = n->current = n->noise = 0;
    n->noiseTime = 0;
    n->base = n->start = n->rise = n->fall = 0;
    n->noiseMin = n->noiseMax = 0;
    n->timeMin = n->timeMax = 0;
    n->min = 0;
    n->max = 1.0;
    n->step = 1.0;
    n->registered = 0;
}
HOOK(0x520200, NoiseQTask_Create);

void NoiseQTask_Register(NoiseQTask *n)
{
    char name[64], symbol[128];

    if (n->registered)
        return;
    n->registered = 1;
    Task_GetName(name, n);
    crt_sprintf(symbol, "%s.vValue", name);
    Script_RegisterReal(symbol, &n->value);
}
HOOK(0x5202F0, NoiseQTask_Register);

void NoiseQTask_Unregister(NoiseQTask *n)
{
    char name[64], symbol[128];

    Task_GetName(name, n);
    n->registered = 0;
    crt_sprintf(symbol, "%s.vValue", name);
    Script_RemoveSymbol(symbol);
    n->registered = 0;
}
HOOK(0x520350, NoiseQTask_Unregister);

void NoiseQTask_Delete(NoiseQTask *n)
{
    if (Task_GetId(n) >= 0) {
        void (*fn)(void *) = TASK_HANDLER(Level_GetDeactivateEvent(), n->task.type);

        if (fn)
            fn(n);
    }
    VarString_Free(&n->expr);
}
HOOK(0x5203A0, NoiseQTask_Delete);

void NoiseQTask_GetParams(NoiseQTask *n, void **out)
{
    *out = gNoiseQTaskParams;
}
HOOK(0x5203F0, NoiseQTask_GetParams);

void NoiseQTask_Update(NoiseQTask *n)
{
    long double v;

    BROADCAST(n->task.head, 1);
    if (VarString_HasExpr(&n->expr))
        n->target = VarString_EvalReal(&n->expr, n);
    else
        n->target = n->base;
    if (n->current < n->target) {
        v = (long double)n->rise + n->current;
        n->current = v;
        if (v > n->target)
            n->current = n->target;
    } else if (n->current > n->target) {
        v = (long double)n->current - n->fall;
        n->current = v;
        if (v < n->target)
            n->current = n->target;
    }
    if (n->noiseTime > 0) {
        n->noiseTime--;
    } else {
        double range = n->noiseMax - n->noiseMin;
        int span = n->timeMax - n->timeMin;

        n->noiseTime = n->timeMin;
        if (span)
            n->noiseTime += Random_Int(gNoiseRandom, span + 1);
        n->noise = n->noiseMin;
        if (range > kZeroD)
            n->noise = Random_Float(gNoiseRandom) * range + n->noise;
    }
    n->value = n->current;
    if (ScreenGrab_IsActive())
        n->value = (long double)n->noise + n->value;
    if (fne(n->step, kZeroD))
        n->value = (long double)round_int((long double)n->value / n->step) * n->step;
    if (n->min > n->value)
        n->value = n->min;
    else if (n->max < n->value)
        n->value = n->max;
}
HOOK(0x520400, NoiseQTask_Update);

void NoiseQTask_Changed(NoiseQTask *n, const char *arg)
{
    if (!arg || !*arg)
        BROADCAST(n->task.head, Param_GetChangedEvent(), 0);
    VarString_Bind(&n->expr, n);
    if (n->noiseMax < n->noiseMin)
        n->noiseMax = n->noiseMin;
    n->current = n->start;
}
HOOK(0x520630, NoiseQTask_Changed);

void NoiseQTask_Shutdown(void)
{
    QTaskList_Free(gNoiseQTaskParams);
    TaskType_Free(gNoiseQTaskType);
}
HOOK(0x520710, NoiseQTask_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A83838) uint16_t gNoiseQTaskType;
PAD(igb, A83838, A8383A, 0x2);
BSS(A83838) void *gNoiseQTaskParams;
STORE(igb, A83838, 1) char store_A83840[16];
ALIAS(gNoiseRandom, store_A83840, 0);
