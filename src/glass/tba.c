#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "glass/glass.h"
#include "hook.h"

ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x487180, char *, sub_487180, (void));
ORIG(0x4871B0, char **, sub_4871B0, (void));
ORIG(0x4871C0, void, sub_4871C0, (char *user, char *icon));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4F0F00, void *, Param_Enum, (void));
ORIG(0x4F0FB0, void *, Param_Int16, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x414E20, void, sub_414E20, (Vec3d *out, void *obj, int point));

/* "GenericTBA" (0x2A8), an ExplodeObject the player can use, like a Terminal */
typedef struct GenericTBA {
    ExplodeObj e;
    char unk238;        /* runs the ExplodeObject code when set */
    char in_use;        /* 0x239 */
    char last_in_use;   /* 0x23A */
    char unk23B;
    int use_tick;       /* 0x23C */
    char on;            /* 0x240 */
    char finished;      /* 0x241 */
    char finished_tick; /* 0x242 */
    char unk243;
    int finish_tick;    /* 0x244 */
    char registered;    /* 0x248 */
    char unk249;
    int16_t anim;       /* 0x24A, the user's animation */
    float use_time;     /* 0x24C, seconds */
    char *icon_name;    /* 0x250 */
    char *icon;         /* 0x254, entry of the icon table */
    VarString script;   /* 0x258 */
} GenericTBA;

_Static_assert(offsetof(GenericTBA, script) == 0x258, "GenericTBA");
_Static_assert(sizeof(GenericTBA) == 0x2A8, "GenericTBA");

#define gTBAType VAR(uint16_t, 0x53C7DC)
#define gTBAIcons VAR(ParamEnum, 0x57C1F0)
#define gTBAParams VAR(void *, 0x57C1FC)

/* handlers shared with the Terminal and others */
#define Terminal_CheckUse ((void *)0x443C80)
#define Terminal_MapHandler ((void *)0x443DC0)
#define sub_51BAA0 ((void *)0x51BAA0)

void GenericTBA_Create(GenericTBA *t, void *arg);
void GenericTBA_Run(GenericTBA *t);
void GenericTBA_Delete(GenericTBA *t);
void GenericTBA_GetParamsHandler(Task *t, void **out);
void GenericTBA_ParamsChanged(GenericTBA *t, void *arg);
void GenericTBA_AddSymbols(GenericTBA *t);
void GenericTBA_RemoveSymbols(GenericTBA *t);
void GenericTBA_Use(GenericTBA *t);
void GenericTBA_DrawIcon(GenericTBA *t, char **arg);

#define FIELD(name) ((void *)offsetof(GenericTBA, name))

void GenericTBA_Open(void)
{
    gTBAIcons.count = 10;
    gTBAIcons.names = sub_4871B0();
    gTBAType = TaskType_New("GenericTBA", sizeof(GenericTBA), ExplodeObject_GetType(), 0);
    TaskType_SetHandler(gTBAType, 0, GenericTBA_Create);
    TaskType_SetHandler2(gTBAType, 1, GenericTBA_Run);
    TaskType_SetHandler2(gTBAType, 2, GenericTBA_Delete);
    TaskType_SetHandler(gTBAType, Param_GetTableEvent(), GenericTBA_GetParamsHandler);
    TaskType_SetHandler(gTBAType, Param_GetChangedEvent(), GenericTBA_ParamsChanged);
    TaskType_SetHandler2(gTBAType, Level_GetEventDA8(), GenericTBA_AddSymbols);
    TaskType_SetHandler2(gTBAType, Level_GetEventDAC(), GenericTBA_RemoveSymbols);
    TaskType_SetHandler(gTBAType, Game_GetEvent2(), Terminal_CheckUse);
    TaskType_SetHandler(gTBAType, Game_GetEvent3(), sub_51BAA0);
    TaskType_SetHandler(gTBAType, Game_GetEvent4(), GenericTBA_Use);
    TaskType_SetHandler(gTBAType, Game_GetEvent6(), GenericTBA_DrawIcon);
    TaskType_SetHandler(gTBAType, ComputerObject_GetEvent0(), Terminal_MapHandler);
    gTBAParams = ParamTable_Extend(ExplodeObject_GetParams(), 5);
    ParamTable_Add(gTBAParams, NULL, FIELD(unk238), Param_Bool8(), 7, NULL);
    ParamTable_Add(gTBAParams, NULL, FIELD(script), Param_VarString(), 7, NULL);
    ParamTable_Add(gTBAParams, NULL, FIELD(anim), Param_Int16(), 7, NULL);
    ParamTable_Add(gTBAParams, NULL, FIELD(use_time), Param_Real32(), 7, NULL);
    ParamTable_Add(gTBAParams, NULL, FIELD(icon_name), Param_Enum(), 7, &gTBAIcons);
}
HOOK(0x447DB0, GenericTBA_Open);

void GenericTBA_Create(GenericTBA *t, void *arg)
{
    QTaskList_Add(Game_GetTaskList0(), t);
    HANDLER(0, ExplodeObject_GetType())(t, arg);
    ExplodeObject_Setup(t, 1.0f, 0.5f, 0.5f, 1.0f, 0.0f, 5, 4, "explo_02_m");
    t->unk238 = 0;
    t->use_time = 4.0f;
    t->anim = -1;
    t->icon_name = NULL;
    t->in_use = 0;
    t->last_in_use = 0;
    t->use_tick = 0;
    t->on = 1;
    t->finished = 0;
    t->finished_tick = 0;
    t->finish_tick = 0;
    t->registered = 0;
    VarString_Init(&t->script);
}
HOOK(0x447FF0, GenericTBA_Create);

void GenericTBA_Run(GenericTBA *t)
{
    if (t->unk238)
        HANDLER(1, ExplodeObject_GetType())(t);
    if (t->e.exploded)
        t->on = 0;
    else if (VarString_HasExpr(&t->script))
        t->on = round_int(VarString_EvalReal(&t->script, t));
    if (gTick > t->use_tick + 2)
        t->in_use = 0;
    t->last_in_use = t->in_use;
    if (gTick > t->finish_tick + 2)
        t->finished_tick = 0;
}
HOOK(0x4480B0, GenericTBA_Run);

void GenericTBA_Delete(GenericTBA *t)
{
    if (t->icon_name) {
        Mem_Free(t->icon_name);
        t->icon_name = NULL;
    }
    QTaskList_Remove(Game_GetTaskList0(), t);
    HANDLER(2, ExplodeObject_GetType())(t);
    if (Task_GetId(t) >= 0) {
        TaskFn fn = gTaskHandlers[Level_GetEventDAC()][t->e.o.task.type];

        if (fn)
            fn(t);
    }
    VarString_Free(&t->script);
}
HOOK(0x448160, GenericTBA_Delete);

void GenericTBA_GetParamsHandler(Task *t, void **out)
{
    *out = gTBAParams;
}
HOOK(0x4481F0, GenericTBA_GetParamsHandler);

/* looks the icon name up in the 10 entry icon table */
void GenericTBA_ParamsChanged(GenericTBA *t, void *arg)
{
    char *icons = sub_487180();
    uint8_t slot = Param_GetChangedEvent();
    int i;

    HANDLER(slot, ExplodeObject_GetType())(t, arg);
    t->icon = NULL;
    if (t->icon_name) {
        char **names = sub_4871B0();

        for (i = 0; i < 10; i++) {
            if (strcmp(t->icon_name, names[i]) == 0) {
                t->icon = icons + 0x3EC + i * 0x18;
                break;
            }
        }
    }
    VarString_Bind(&t->script, t);
}
HOOK(0x448200, GenericTBA_ParamsChanged);

void GenericTBA_AddSymbols(GenericTBA *t)
{
    char name[64], buf[128];
    uint16_t type;

    if (t->registered)
        return;
    t->registered = 1;
    type = ExplodeObject_GetType();
    gTaskHandlers[Level_GetEventDA8()][type](t);
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RegisterBool8(buf, &t->in_use);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RegisterBool8(buf, &t->last_in_use);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &t->on);
    crt_sprintf(buf, "%s.isFinished", name);
    Script_RegisterBool8(buf, &t->finished);
    crt_sprintf(buf, "%s.isFinishedThisTick", name);
    Script_RegisterBool8(buf, &t->finished_tick);
}
HOOK(0x4482D0, GenericTBA_AddSymbols);

void GenericTBA_RemoveSymbols(GenericTBA *t)
{
    char name[64], buf[128];
    uint16_t type;

    if (!t->registered)
        return;
    type = ExplodeObject_GetType();
    gTaskHandlers[Level_GetEventDAC()][type](t);
    Task_GetName(name, t);
    t->registered = 0;
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isFinished", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isFinishedThisTick", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x4483F0, GenericTBA_RemoveSymbols);

void GenericTBA_Use(GenericTBA *t)
{
    t->in_use = 1;
    t->use_tick = gTick;
}
HOOK(0x4484F0, GenericTBA_Use);

void GenericTBA_DrawIcon(GenericTBA *t, char **arg)
{
    sub_4871C0(*arg, t->icon);
}
HOOK(0x448510, GenericTBA_DrawIcon);

void GenericTBA_Close(void)
{
    QTaskList_Free(gTBAParams);
    TaskType_Free(gTBAType);
}
HOOK(0x448530, GenericTBA_Close);

uint16_t GenericTBA_GetType(void)
{
    return gTBAType;
}
HOOK(0x448550, GenericTBA_GetType);

void *GenericTBA_GetParams(void)
{
    return gTBAParams;
}
HOOK(0x448560, GenericTBA_GetParams);

/* the world position of model point 1 and the orientation, for the user */
void GenericTBA_GetUsePos(GenericTBA *t, Vec3d *pos, Matrix *m)
{
    *m = t->e.o.matrix;
    sub_414E20(pos, t, 1);
    pos->x = t->e.o.pos.x + pos->x;
    pos->y = t->e.o.pos.y + pos->y;
    pos->z = t->e.o.pos.z + pos->z;
}
HOOK(0x448570, GenericTBA_GetUsePos);

float GenericTBA_GetUseTime(GenericTBA *t)
{
    return t->use_time;
}
HOOK(0x4485C0, GenericTBA_GetUseTime);

int16_t GenericTBA_GetAnim(GenericTBA *t)
{
    return t->anim;
}
HOOK(0x4485D0, GenericTBA_GetAnim);
