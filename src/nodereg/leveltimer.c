/* LevelTimer: counts ticks while running. "isRun" and "nTick" are script
   variables; the run and reset expressions are checked every frame. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "nodereg/nodereg.h"
#include "param/paramtypes.h"

/* "LevelTimer" (0x110) */
typedef struct LevelTimer {
    Task task;
    Vec3d pos;
    Matrix matrix;
    char isRun;
    char unk61[3];
    int nTick;
    char registered;
    char unk69[3];
    VarString run;
    VarString reset;
} LevelTimer;

_Static_assert(sizeof(LevelTimer) == 0x110, "LevelTimer");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F70, int, Param_Real32x9, (void));
ORIG(0x4F0FC0, int, Param_VarString, (void));
ORIG(0x4F1000, int, Param_Bool8, (void));
ORIG(0x4F1010, int, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, int type, int flags, void *extra));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

extern uint16_t gLevelTimerType;
extern void *gLevelTimerParams;

void LevelTimer_Create(LevelTimer *t);
void LevelTimer_Register(LevelTimer *t);
void LevelTimer_Unregister(LevelTimer *t);
void LevelTimer_Delete(LevelTimer *t);
void LevelTimer_GetParamTable(LevelTimer *t, ParamTable *out);
void LevelTimer_Run(LevelTimer *t);
void LevelTimer_ParamsChanged(LevelTimer *t, const char *name);

void LevelTimer_Open(void)
{
    LevelTimer p;
    uint16_t t = TaskType_New("LevelTimer", sizeof(LevelTimer), EditorRigidObj_GetType(), 0, 0);

    gLevelTimerType = t;
    TaskType_SetHandler(t, 0, LevelTimer_Create);
    TaskType_SetHandler2(gLevelTimerType, 1, LevelTimer_Run);
    TaskType_SetHandler2(gLevelTimerType, 2, LevelTimer_Delete);
    TaskType_SetHandler(gLevelTimerType, Param_GetTableEvent(), LevelTimer_GetParamTable);
    TaskType_SetHandler(gLevelTimerType, Param_GetChangedEvent(), LevelTimer_ParamsChanged);
    TaskType_SetHandler2(gLevelTimerType, Level_GetEventDA8(), LevelTimer_Register);
    TaskType_SetHandler2(gLevelTimerType, Level_GetEventDAC(), LevelTimer_Unregister);
    gLevelTimerParams = ParamTable_New(5);
    ParamTable_Add(gLevelTimerParams, &p, &p.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gLevelTimerParams, &p, &p.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gLevelTimerParams, &p, &p.run, Param_VarString(), 7, NULL);
    ParamTable_Add(gLevelTimerParams, &p, &p.reset, Param_VarString(), 7, NULL);
    ParamTable_Add(gLevelTimerParams, &p, &p.isRun, Param_Bool8(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_LEVELTIMER", gLevelTimerType);
}
HOOK(0x51BAC0, LevelTimer_Open);

void LevelTimer_Create(LevelTimer *t)
{
    VarString_Init(&t->run);
    VarString_Init(&t->reset);
    t->pos.x = t->pos.y = t->pos.z = 0;
    t->matrix.m[0] = 1;
    t->matrix.m[1] = 0;
    t->matrix.m[2] = 0;
    t->matrix.m[3] = 0;
    t->matrix.m[4] = 1;
    t->matrix.m[5] = 0;
    t->matrix.m[6] = 0;
    t->matrix.m[7] = 0;
    t->matrix.m[8] = 1;
    t->matrix.count = 0;
    t->registered = 0;
    t->isRun = 1;
    t->nTick = 0;
}
HOOK(0x51BC60, LevelTimer_Create);

void LevelTimer_Register(LevelTimer *t)
{
    char name[64];
    char buf[128];

    if (t->registered)
        return;
    t->registered = 1;
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.isRun", name);
    Script_RegisterBool8(buf, &t->isRun);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RegisterInt(buf, &t->nTick);
}
HOOK(0x51BCC0, LevelTimer_Register);

void LevelTimer_Unregister(LevelTimer *t)
{
    char name[64];
    char buf[128];

    Task_GetName(name, t);
    t->registered = 0;
    crt_sprintf(buf, "%s.isRun", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RemoveSymbol(buf);
    t->registered = 0;
}
HOOK(0x51BD40, LevelTimer_Unregister);

void LevelTimer_Delete(LevelTimer *t)
{
    if (Task_GetId(t) >= 0) {
        TaskFn fn = gTaskHandlers[Level_GetEventDAC()][t->task.type];

        if (fn)
            ((void (*)(void *))fn)(t);
    }
    VarString_Free(&t->run);
    VarString_Free(&t->reset);
}
HOOK(0x51BDB0, LevelTimer_Delete);

void LevelTimer_GetParamTable(LevelTimer *t, ParamTable *out)
{
    *out = gLevelTimerParams;
}
HOOK(0x51BE00, LevelTimer_GetParamTable);

void LevelTimer_Run(LevelTimer *t)
{
    CALL_CHILDREN(&t->task, 1);
    if (VarString_HasExpr(&t->run))
        t->isRun = x87_fistp(VarString_EvalReal(&t->run, t));
    if (VarString_HasExpr(&t->reset)) {
        long double r = VarString_EvalReal(&t->reset, t);

        /* NaN counts as zero */
        if (r < 0 || r > 0)
            t->nTick = 0;
    }
    if (t->isRun)
        t->nTick++;
}
HOOK(0x51BE10, LevelTimer_Run);

void LevelTimer_ParamsChanged(LevelTimer *t, const char *name)
{
    if (!name || !*name)
        CALL_CHILDREN(&t->task, Param_GetChangedEvent(), 0);
    VarString_Bind(&t->run, t);
    VarString_Bind(&t->reset, t);
}
HOOK(0x51BEE0, LevelTimer_ParamsChanged);

void LevelTimer_Close(void)
{
    QTaskList_Free(gLevelTimerParams);
    TaskType_Free(gLevelTimerType);
}
HOOK(0x51BF90, LevelTimer_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A83774) uint16_t gLevelTimerType;
PAD(igb, A83774, A83776, 0x2);
BSS(A83774) void *gLevelTimerParams;
