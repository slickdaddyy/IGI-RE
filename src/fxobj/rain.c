#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "fxobj/fxobj.h"

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

typedef struct RudderDef {
    int kind;
    float angle;
} RudderDef;

#define gRudderType VAR(uint16_t, 0x57BD74)
#define gRainTexture VAR(char *, 0x57BD80)
#define gRainType VAR(uint16_t, 0x57BD84)
#define gRainParams VAR(void *, 0x57BD88)
#define gRainLevelRange ((void *)0x57BD90)
#define gRainSizeRange ((void *)0x57BD98)

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

    VAR(float, 0x57BD98) = 5.0f;
    VAR(float, 0x57BD9C) = 150.0f;
    VAR(float, 0x57BD90) = 0.0f;
    VAR(float, 0x57BD94) = 1.0f;
    gRainType = TaskType_New("RainEffect", sizeof(RainEffect), Task_GetRootType(), 0);
    TaskType_SetHandler(gRainType, 0, RainEffect_Create);
    TaskType_SetHandler2(gRainType, 2, RainEffect_Delete);
    TaskType_SetHandler(gRainType, Param_GetTableEvent(), RainEffect_GetParams);
    TaskType_SetHandler(gRainType, Param_GetChangedEvent(), RainEffect_Changed);
    TaskType_SetHandler2(gRainType, 1, RainEffect_Run);
    TaskType_SetHandler2(gRainType, Level_GetEventDA8(), TASK_NOTHING);
    TaskType_SetHandler2(gRainType, Level_GetEventDAC(), TASK_NOTHING);
    gRainParams = ParamTable_New(5);
    ParamTable_Add(gRainParams, &r, &r.unk24, Param_Bool8(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.unk28, Param_Real32(), 7, gRainSizeRange);
    ParamTable_Add(gRainParams, &r, &r.unk2C, Param_Real32(), 7, gRainSizeRange);
    ParamTable_Add(gRainParams, &r, &r.expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gRainParams, &r, &r.level, Param_Real32(), 7, gRainLevelRange);
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
        TaskFn fn = HANDLER(Level_GetEventDAC(), t->task.type);

        if (fn)
            ((void (*)(void *))fn)(t);
    }
    VarString_Free(&t->expr);
}
HOOK(0x4369E0, RainEffect_Delete);

void RainEffect_GetParams(void *t, void **out)
{
    (void)t;
    *out = gRainParams;
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
    QTaskList_Free(gRainParams);
    TaskType_Free(gRainType);
}
HOOK(0x436AF0, RainEffect_Close);
