#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "fxobj/fxobj.h"
#include "plane/planetypes.h"
#include "plane/planevars.h"

ORIG(0x498960, void *, Rain_New, (void *world, void *texture, int unk, float a, float b));
ORIG(0x498A70, void, Rain_Free, (void *rain, void *world));
ORIG(0x499CF0, void, Rain_Update, (void *rain));
ORIG(0x499F90, void, Rain_SetFalling, (void *rain, int count));
ORIG(0x499FA0, void, Rain_SetAlpha, (void *rain, float level));

/* weather particles around the camera ("RainEffect", 0x88 bytes) */
typedef struct RainEffect {
    Task task;
    void *rain;
    char unk24;
    float unk28;
    float unk2C;
    float level;
    float lastLevel;
    VarString expr;
} RainEffect;

_Static_assert(sizeof(RainEffect) == 0x88, "RainEffect");

extern char *gRainTexture;
extern uint16_t gRainEffectType;
extern void *gRainEffectParams;
extern RangeF gRainLevelRange;
extern RangeF gRainSizeRange;

static const float kRainScale = 4096.0f;

void Rudder_ParseDef(void *t, ParseArg *a)
{
    int i = a->index;
    RudderDef *d = a->out;

    (void)t;
    d->kind = Script_GetArgInt(a->args, i);
    d->angle = Script_GetArgReal(a->args, i + 1);
}
HOOK(0x436740, Rudder_ParseDef);

void Rudder_Close(void)
{
    TaskType_Free(gRudderType);
}
HOOK(0x436770, Rudder_Close);

void RainEffect_Create(RainEffect *t);
void RainEffect_Run(RainEffect *t);
void RainEffect_Delete(RainEffect *t);
void RainEffect_GetParams(void *t, void **out);
void RainEffect_Changed(RainEffect *t);

void RainEffect_Init(void)
{
    RainEffect r;

    gRainSizeRange.min = 5.0f;
    gRainSizeRange.max = 150.0f;
    gRainLevelRange.min = 0.0f;
    gRainLevelRange.max = 1.0f;
    gRainEffectType = TaskType_New("RainEffect", sizeof(RainEffect), Task_GetRootType(), 0);
    TaskType_SetHandler(gRainEffectType, 0, RainEffect_Create);
    TaskType_SetHandler2(gRainEffectType, 2, RainEffect_Delete);
    TaskType_SetHandler(gRainEffectType, Param_GetTableEvent(), RainEffect_GetParams);
    TaskType_SetHandler(gRainEffectType, Param_GetChangedEvent(), RainEffect_Changed);
    TaskType_SetHandler2(gRainEffectType, 1, RainEffect_Run);
    TaskType_SetHandler2(gRainEffectType, Level_GetActivateEvent(), TASK_NOTHING);
    TaskType_SetHandler2(gRainEffectType, Level_GetDeactivateEvent(), TASK_NOTHING);
    gRainEffectParams = ParamTable_New(5);
    ParamTable_Add(gRainEffectParams, &r, &r.unk24, Param_Bool8(), 7, NULL);
    ParamTable_Add(gRainEffectParams, &r, &r.unk28, Param_Real32(), 7, &gRainSizeRange);
    ParamTable_Add(gRainEffectParams, &r, &r.unk2C, Param_Real32(), 7, &gRainSizeRange);
    ParamTable_Add(gRainEffectParams, &r, &r.expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gRainEffectParams, &r, &r.level, Param_Real32(), 7, &gRainLevelRange);
    gRainTexture = Game_AddTexture("snow.tex");
}
HOOK(0x436780, RainEffect_Init);

void RainEffect_Create(RainEffect *t)
{
    t->rain = NULL;
    t->unk24 = 1;
    t->unk28 = 50.0f;
    t->unk2C = 20.0f;
    t->level = 0.3125f;
    t->lastLevel = 0.3125f;
    VarString_Init(&t->expr);
}
HOOK(0x436950, RainEffect_Create);

void RainEffect_Run(RainEffect *t)
{
    if (!t->rain)
        return;
    if (VarString_HasExpr(&t->expr))
        Rain_SetFalling(t->rain, round_int(VarString_EvalReal(&t->expr, t)));
    Rain_Update(t->rain);
}
HOOK(0x436990, RainEffect_Run);

void RainEffect_Delete(RainEffect *t)
{
    void *world = Game_GetWorld();

    Rain_Free(t->rain, world);
    if (Task_GetId(t) >= 0) {
        TaskFn fn = HANDLER(Level_GetDeactivateEvent(), t->task.type);

        if (fn)
            ((void (*)(void *))fn)(t);
    }
    VarString_Free(&t->expr);
}
HOOK(0x4369E0, RainEffect_Delete);

void RainEffect_GetParams(void *t, void **out)
{
    (void)t;
    *out = gRainEffectParams;
}
HOOK(0x436A40, RainEffect_GetParams);

/* rebuilds the rain object unless only the level changed */
void RainEffect_Changed(RainEffect *t)
{
    if (EQ_OR_NAN(t->level, t->lastLevel) && t->rain) {
        Rain_Free(t->rain, Game_GetWorld());
        t->rain = NULL;
    }
    if (!t->rain) {
        float b = t->unk2C * kRainScale;
        float a = t->unk28 * kRainScale;

        t->rain = Rain_New(Game_GetWorld(), gRainTexture + 0x18, t->unk24, a, b);
    }
    Rain_SetAlpha(t->rain, t->level);
    t->lastLevel = t->level;
    VarString_Bind(&t->expr, t);
}
HOOK(0x436A50, RainEffect_Changed);

void RainEffect_Close(void)
{
    QTaskList_Free(gRainEffectParams);
    TaskType_Free(gRainEffectType);
}
HOOK(0x436AF0, RainEffect_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BD80) char *gRainTexture;
BSS(57BD80) uint16_t gRainEffectType;
PAD(igb, 57BD80, 57BD86, 0x2);
BSS(57BD80) void *gRainEffectParams;
PAD(igb, 57BD80, 57BD8C, 0x4);
BSS(57BD80) RangeF gRainLevelRange;
BSS(57BD80) RangeF gRainSizeRange;
