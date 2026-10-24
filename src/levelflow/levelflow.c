#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "levelflow/levelflow.h"

ORIG(0x402870, int, Flow_GetFps, (void));
ORIG(0x406220, int *, Profile_GetActive, (void));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x416D90, void, Game_ResetState, (void));
ORIG(0x416F40, void, Game_ClearPlayers, (void));
ORIG(0x418B00, void *, MenuManager_New, (void *unk44, const char *path, int a, int b, int c));
ORIG(0x418D70, void, MenuManager_Free, (void *menu));
ORIG(0x48F2D0, int, AppContext_IsUSA, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4CEBA0, void *, VirModel_Add, (const char *name));
ORIG(0x4F1160, char, Level_IsRunning, (void));
ORIG(0x4F1CB0, void *, Param_Range0100, (void));

/* the level script's root: completion and failure conditions, tick
   counter and the script view of the number keys */
typedef struct LevelFlow {
    Task task;
    Vec3d pos;
    Matrix matrix;
    char isComplete;
    char isFailed;
    char unk62[2];
    int eDifficulty;
    int nTick;
    float startHour;
    char unk70;
    char unk71[3];
    float timeLimit;
    char registered;
    char unk79[3];
    VarString complete;
    VarString failed;
    char wasFailed;
    char unk11D[3];
    void *menu;
    int unk124;
} LevelFlow;

_Static_assert(offsetof(LevelFlow, complete) == 0x7C, "LevelFlow");
_Static_assert(sizeof(LevelFlow) == 0x128, "LevelFlow");

extern LevelFlow *gLevelFlow;
extern uint16_t gLevelFlowType;
extern ParamTable *gLevelFlowParams;
extern void *gLevelFlowModel;
extern char gKeyPressed[];
extern uint8_t gKeys[];
extern float kSecondsPerHour;
extern float kMinusFps;

void LevelFlow_Create(LevelFlow *lf);
void LevelFlow_Run(LevelFlow *lf);
void LevelFlow_Delete(LevelFlow *lf);
void LevelFlow_GetParams(void *task, ParamTable **out);
void LevelFlow_Changed(LevelFlow *lf);
void LevelFlow_Activate(LevelFlow *lf);
void LevelFlow_Deactivate(LevelFlow *lf);
ScriptValue *LevelFlow_GetBreakCutSceneKey(ScriptValue *ret);
ScriptValue *LevelFlow_LevelFailed(ScriptValue *ret);
ScriptValue *LevelFlow_IsCountryUSA(ScriptValue *ret);

void LevelFlow_Init(void)
{
    void *range;

    gLevelFlowModel = VirModel_Add("306_01_1");
    gLevelFlowType = TaskType_New("LevelFlow", sizeof(LevelFlow), EditorRigidObj_GetType(), 0);
    TaskType_SetHandler(gLevelFlowType, 0, LevelFlow_Create);
    TaskType_SetHandler2(gLevelFlowType, 1, LevelFlow_Run);
    TaskType_SetHandler2(gLevelFlowType, 2, LevelFlow_Delete);
    TaskType_SetHandler(gLevelFlowType, Param_GetTableEvent(), LevelFlow_GetParams);
    TaskType_SetHandler(gLevelFlowType, Param_GetChangedEvent(), LevelFlow_Changed);
    TaskType_SetHandler2(gLevelFlowType, Level_GetEventDA8(), LevelFlow_Activate);
    TaskType_SetHandler2(gLevelFlowType, Level_GetEventDAC(), LevelFlow_Deactivate);
    TaskType_SetHandler(gLevelFlowType, 7, NULL);
    gLevelFlowParams = ParamTable_New(7);
    ParamTable_Add(gLevelFlowParams, NULL, FIELD(LevelFlow, pos), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gLevelFlowParams, NULL, FIELD(LevelFlow, matrix), Param_Real32x9(), 7, NULL);
    range = Param_Range0100();
    ParamTable_Add(gLevelFlowParams, NULL, FIELD(LevelFlow, startHour), Param_Real32(), 7, range);
    ParamTable_Add(gLevelFlowParams, NULL, FIELD(LevelFlow, complete), Param_VarString(), 7, NULL);
    ParamTable_Add(gLevelFlowParams, NULL, FIELD(LevelFlow, failed), Param_VarString(), 7, NULL);
    ParamTable_Add(gLevelFlowParams, NULL, FIELD(LevelFlow, unk70), Param_Bool8(), 7, NULL);
    ParamTable_Add(gLevelFlowParams, NULL, FIELD(LevelFlow, timeLimit), Param_Real32(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_LEVELFLOW", gLevelFlowType);
    Script_RegisterFunction("LevelFlow_GetBreakCutSceneKey", LevelFlow_GetBreakCutSceneKey, NULL, 0);
    Script_RegisterFunction("LevelFlow_LevelFailed", LevelFlow_LevelFailed, NULL, 0);
    Script_RegisterFunction("LevelFlow_IsCountryUSA", LevelFlow_IsCountryUSA, NULL, 0);
}
HOOK(0x488BB0, LevelFlow_Init);

/* 1 while Escape is held */
ScriptValue *LevelFlow_GetBreakCutSceneKey(ScriptValue *ret)
{
    int key = gKeys[1] & 0x80;

    ret->i = key;
    ret->d = key;
    ret->s = gScriptNumberType;
    return ret;
}
HOOK(0x488E00, LevelFlow_GetBreakCutSceneKey);

ScriptValue *LevelFlow_LevelFailed(ScriptValue *ret)
{
    int failed = gLevelFlow->isFailed;

    ret->i = failed;
    ret->d = failed;
    ret->s = gScriptNumberType;
    return ret;
}
HOOK(0x488E50, LevelFlow_LevelFailed);

ScriptValue *LevelFlow_IsCountryUSA(ScriptValue *ret)
{
    ret->i = AppContext_IsUSA();
    ret->d = AppContext_IsUSA();
    ret->s = gScriptNumberType;
    return ret;
}
HOOK(0x488E90, LevelFlow_IsCountryUSA);

void LevelFlow_Create(LevelFlow *lf)
{
    int *profile;

    gLevelFlow = lf;
    Game_ResetState();
    VarString_Init(&lf->complete);
    VarString_Init(&lf->failed);
    lf->pos.x = 0;
    lf->pos.y = 0;
    lf->pos.z = 0;
    lf->matrix.m[0] = 1.0f;
    lf->matrix.m[1] = 0.0f;
    lf->matrix.m[2] = 0.0f;
    lf->matrix.m[3] = 0.0f;
    lf->matrix.m[4] = 1.0f;
    lf->matrix.m[5] = 0.0f;
    lf->matrix.m[6] = 0.0f;
    lf->matrix.m[7] = 0.0f;
    lf->matrix.m[8] = 1.0f;
    lf->matrix.count = 0;
    lf->unk70 = 0;
    lf->timeLimit = 0.0f;
    lf->registered = 0;
    lf->isComplete = 0;
    lf->isFailed = 0;
    lf->nTick = 0;
    lf->wasFailed = 0;
    profile = Profile_GetActive();
    if (!profile) {
        Warning_Show("LevelFlow: No player profile found");
        lf->eDifficulty = 1;
    } else {
        lf->eDifficulty = *profile;
    }
    lf->menu = MenuManager_New(*(void **)(Game_Get() + 0x44), "LOCAL:menusystem/ingamemenu.qsc", 0, 0, 0);
}
HOOK(0x488ED0, LevelFlow_Create);

void LevelFlow_Activate(LevelFlow *lf)
{
    static const char *const keys[10] = {
        "Key0.isPressed", "Key1.isPressed", "Key2.isPressed", "Key3.isPressed", "Key4.isPressed",
        "Key5.isPressed", "Key6.isPressed", "Key7.isPressed", "Key8.isPressed", "Key9.isPressed",
    };
    char name[64];
    char buf[128];
    int i;

    if (lf->registered)
        return;
    lf->registered = 1;
    Task_GetName(name, lf);
    crt_sprintf(buf, "%s.isComplete", name);
    Script_RegisterBool8(buf, &lf->isComplete);
    crt_sprintf(buf, "%s.isFailed", name);
    Script_RegisterBool8(buf, &lf->isFailed);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RegisterInt(buf, &lf->nTick);
    crt_sprintf(buf, "%s.eDifficulty", name);
    Script_RegisterInt(buf, &lf->eDifficulty);
    for (i = 0; i < 10; i++)
        Script_RegisterBool8(keys[i], &gKeyPressed[i]);
}
HOOK(0x488F90, LevelFlow_Activate);

void LevelFlow_Deactivate(LevelFlow *lf)
{
    static const char *const keys[10] = {
        "Key0.isPressed", "Key1.isPressed", "Key2.isPressed", "Key3.isPressed", "Key4.isPressed",
        "Key5.isPressed", "Key6.isPressed", "Key7.isPressed", "Key8.isPressed", "Key9.isPressed",
    };
    char name[64];
    char buf[128];
    int i;

    Task_GetName(name, lf);
    lf->registered = 0;
    crt_sprintf(buf, "%s.isComplete", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isFailed", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.eDifficulty", name);
    Script_RemoveSymbol(buf);
    for (i = 0; i < 10; i++)
        Script_RemoveSymbol(keys[i]);
    lf->registered = 0;
}
HOOK(0x4890F0, LevelFlow_Deactivate);

void LevelFlow_Delete(LevelFlow *lf)
{
    gLevelFlow = NULL;
    Game_ClearPlayers();
    if (Task_GetId(lf) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), lf->task.type);

        if (fn)
            fn(lf);
    }
    VarString_Free(&lf->complete);
    VarString_Free(&lf->failed);
    MenuManager_Free(lf->menu);
    lf->menu = NULL;
}
HOOK(0x489200, LevelFlow_Delete);

void LevelFlow_GetParams(void *task, ParamTable **out)
{
    *out = gLevelFlowParams;
}
HOOK(0x489280, LevelFlow_GetParams);

void LevelFlow_Run(LevelFlow *lf)
{
    int i;

    if (!Level_IsRunning())
        return;
    lf->wasFailed = lf->isFailed;
    EACH_CHILD(&lf->task, 1);
    if (VarString_HasExpr(&lf->complete))
        lf->isComplete = (char)round_int(VarString_EvalReal(&lf->complete, lf));
    if (VarString_HasExpr(&lf->failed))
        lf->isFailed = (char)round_int(VarString_EvalReal(&lf->failed, lf));
    if (lf->isFailed)
        lf->isComplete = 0;
    lf->nTick++;
    /* DirectInput codes: 0x0B is the 0 key, 0x02..0x0A are 1..9 */
    gKeyPressed[0] = gKeys[0x0B] & 0x80;
    for (i = 1; i < 10; i++)
        gKeyPressed[i] = gKeys[i + 1] & 0x80;
}
HOOK(0x489290, LevelFlow_Run);

void LevelFlow_Changed(LevelFlow *lf)
{
    VarString_Bind(&lf->complete, lf);
    VarString_Bind(&lf->failed, lf);
}
HOOK(0x489400, LevelFlow_Changed);

void LevelFlow_Close(void)
{
    QTaskList_Free(gLevelFlowParams);
    TaskType_Free(gLevelFlowType);
}
HOOK(0x489430, LevelFlow_Close);

char LevelFlow_IsComplete(void)
{
    return gLevelFlow->isComplete;
}
HOOK(0x489450, LevelFlow_IsComplete);

char LevelFlow_IsFailed(void)
{
    if (!gLevelFlow)
        return 0;
    return gLevelFlow->isFailed;
}
HOOK(0x489460, LevelFlow_IsFailed);

/* level time in hours: ticks at the timer rate plus the start hour */
long double LevelFlow_GetHour(void)
{
    float tick = gLevelFlow->nTick;
    int freq = Flow_GetFps();

    return tick / ((long double)freq * kSecondsPerHour) + gLevelFlow->startHour;
}
HOOK(0x489470, LevelFlow_GetHour);

/* frames left of the time limit (seconds), never below 0 */
int LevelFlow_GetTicksLeft(void)
{
    int left = -round_int((long double)gLevelFlow->timeLimit * kMinusFps) - gLevelFlow->nTick;

    return left > 0 ? left : 0;
}
HOOK(0x4894B0, LevelFlow_GetTicksLeft);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5339B8) float kMinusFps INIT(-30.0f);
ALIAS(kFuseFrames, kMinusFps, 0);
DATA(53422C) float kSecondsPerHour INIT(3600.0f);
BSS(5C8AA4) ParamTable *gLevelFlowParams;
STORE(igb, 5C8AA4, 1) char store_5C8AA8[12];
ALIAS(gKeyPressed, store_5C8AA8, 0);
BSS(5C8AA4) void *gLevelFlowModel;
BSS(5C8AA4) uint16_t gLevelFlowType;
PAD(igb, 5C8AA4, 5C8ABA, 0x2);
