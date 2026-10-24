#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "scamera/scamera.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int point));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x416AD0, void, UseQuery_Consider, (void *user, void *obj, Vec3d *pos));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x467DF0, void, ComputerObject_DrawSprite, (char *arg));
ORIG(0x467F30, void, ComputerObject_DrawMarker, (char *arg));
ORIG(0x4680B0, void, ComputerObject_DrawText, (char *arg));
ORIG(0x481340, uint16_t, ExplodeObject_GetType, (void));
ORIG(0x481350, void *, ExplodeObject_GetParams, (void));
ORIG(0x481360, void, ExplodeObject_Setup, (void *o, float a, float b, float c, float d, int e, int f, int g,
                                          const char *sound));
ORIG(0x4871C0, void, StatusScreen_DrawActionIcon, (char *p, char *q));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4F0F50, void *, Param_Real32, (void));
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

extern uint16_t gTerminalType;
extern void *gTerminalParams;

extern double kZeroD;
extern double kUseRange; /* 3072 */

void Terminal_Create(Terminal *t, void *arg);
void Terminal_Run(Terminal *t);
void Terminal_Delete(Terminal *t);
void Terminal_GetParamsHandler(Task *t, void **out);
void Terminal_ParamsChanged(Terminal *t, void *arg);
void Terminal_AddSymbols(Terminal *t);
void Terminal_RemoveSymbols(Terminal *t);
void Terminal_CheckUse(Terminal *t, char *user);
void Terminal_GetUseKind(Terminal *t, int *out);
void Terminal_Use(Terminal *t);
void Terminal_DrawIcon(Terminal *t, char **arg);

void Terminal_Open(void)
{
    gTerminalType = TaskType_New("Terminal", sizeof(Terminal), ExplodeObject_GetType(), 0);
    TaskType_SetHandler(gTerminalType, 0, Terminal_Create);
    TaskType_SetHandler2(gTerminalType, 1, Terminal_Run);
    TaskType_SetHandler2(gTerminalType, 2, Terminal_Delete);
    TaskType_SetHandler(gTerminalType, Param_GetTableEvent(), Terminal_GetParamsHandler);
    TaskType_SetHandler(gTerminalType, Param_GetChangedEvent(), Terminal_ParamsChanged);
    TaskType_SetHandler2(gTerminalType, Level_GetEventDA8(), Terminal_AddSymbols);
    TaskType_SetHandler2(gTerminalType, Level_GetEventDAC(), Terminal_RemoveSymbols);
    TaskType_SetHandler(gTerminalType, Game_GetEvent2(), Terminal_CheckUse);
    TaskType_SetHandler(gTerminalType, Game_GetEvent3(), Terminal_GetUseKind);
    TaskType_SetHandler(gTerminalType, Game_GetEvent4(), Terminal_Use);
    TaskType_SetHandler(gTerminalType, Game_GetEvent6(), Terminal_DrawIcon);
    TaskType_SetHandler(gTerminalType, ComputerObject_GetEvent0(), Terminal_MapHandler);
    gTerminalParams = ParamTable_Extend(ExplodeObject_GetParams(), 2);
    ParamTable_Add(gTerminalParams, NULL, (void *)offsetof(Terminal, script), Param_VarString(), 7, NULL);
    ParamTable_Add(gTerminalParams, NULL, (void *)offsetof(Terminal, unk24C), Param_Real32(), 7, NULL);
}
HOOK(0x443680, Terminal_Open);

void Terminal_Create(Terminal *t, void *arg)
{
    QTaskList_Add(Game_GetTaskList0(), t);
    HANDLER(0, ExplodeObject_GetType())(t, arg);
    ExplodeObject_Setup(t, 1.0f, 0.5f, 0.5f, 1.0f, 0, 5, 4, "explo_02_m");
    t->unk24C = 4.0f;
    t->in_use = 0;
    t->last_in_use = 0;
    t->use_tick = 0;
    t->on = 1;
    t->hacked = 0;
    t->hacked_tick = 0;
    t->hack_tick = 0;
    t->registered = 0;
    VarString_Init(&t->script);
}
HOOK(0x443840, Terminal_Create);

void Terminal_Run(Terminal *t)
{
    HANDLER(1, ExplodeObject_GetType())(t);
    if (t->exploded)
        t->on = 0;
    else if (VarString_HasExpr(&t->script))
        t->on = round_int(VarString_EvalReal(&t->script, t));
    if (gTick > t->use_tick + 2)
        t->in_use = 0;
    t->last_in_use = t->in_use;
    if (gTick > t->hack_tick + 2)
        t->hacked_tick = 0;
}
HOOK(0x4438E0, Terminal_Run);

void Terminal_Delete(Terminal *t)
{
    QTaskList_Remove(Game_GetTaskList0(), t);
    HANDLER(2, ExplodeObject_GetType())(t);
    if (Task_GetId(t) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), t->o.task.type);

        if (fn)
            fn(t);
    }
    VarString_Free(&t->script);
}
HOOK(0x443990, Terminal_Delete);

void Terminal_GetParamsHandler(Task *t, void **out)
{
    *out = gTerminalParams;
}
HOOK(0x443A00, Terminal_GetParamsHandler);

void Terminal_ParamsChanged(Terminal *t, void *arg)
{
    uint8_t slot = Param_GetChangedEvent();

    HANDLER(slot, ExplodeObject_GetType())(t, arg);
    VarString_Bind(&t->script, t);
}
HOOK(0x443A10, Terminal_ParamsChanged);

void Terminal_AddSymbols(Terminal *t)
{
    char name[64], buf[128];
    uint16_t type;

    if (t->registered)
        return;
    t->registered = 1;
    type = ExplodeObject_GetType();
    HANDLER(Level_GetEventDA8(), type)(t);
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RegisterBool8(buf, &t->in_use);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RegisterBool8(buf, &t->last_in_use);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &t->on);
    crt_sprintf(buf, "%s.isHacked", name);
    Script_RegisterBool8(buf, &t->hacked);
    crt_sprintf(buf, "%s.isHackedThisTick", name);
    Script_RegisterBool8(buf, &t->hacked_tick);
}
HOOK(0x443A60, Terminal_AddSymbols);

void Terminal_RemoveSymbols(Terminal *t)
{
    char name[64], buf[128];
    uint16_t type;

    if (!t->registered)
        return;
    type = ExplodeObject_GetType();
    HANDLER(Level_GetEventDAC(), type)(t);
    Task_GetName(name, t);
    t->registered = 0;
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isHacked", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isHackedThisTick", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x443B80, Terminal_RemoveSymbols);

/* the user (position at +8) must stand in front of the screen and close enough */
void Terminal_CheckUse(Terminal *t, char *user)
{
    Vec3d *u = (Vec3d *)(user + 8);
    float *m = t->o.matrix.m;
    long double px, front, side;
    Vec3d p, d;

    if (!t->on)
        return;
    Model_GetPoint(&p, t, 1);
    px = p.x + t->o.pos.x;
    p.x = px;
    p.y = p.y + t->o.pos.y;
    p.z = p.z + t->o.pos.z;
    d.x = u->x - px;
    d.y = u->y - p.y;
    d.z = u->z - p.z;
    side = m[6] * d.z + m[3] * d.y + m[0] * d.x;
    front = m[7] * d.z + m[4] * d.y + m[1] * d.x;
    if (!(front > kZeroD))
        return;
    if (side < kZeroD)
        side = -side;
    if (side < kUseRange)
        UseQuery_Consider(user, t, &p);
}
HOOK(0x443C80, Terminal_CheckUse);

void Terminal_GetUseKind(Terminal *t, int *out)
{
    *out = 5;
}
HOOK(0x443D70, Terminal_GetUseKind);

void Terminal_Use(Terminal *t)
{
    t->in_use = 1;
    t->use_tick = gTick;
}
HOOK(0x443D80, Terminal_Use);

void Terminal_DrawIcon(Terminal *t, char **arg)
{
    StatusScreen_DrawActionIcon(*arg, *arg + 0x404);
}
HOOK(0x443DA0, Terminal_DrawIcon);

/* fills the position, orientation and radius of a map marker */
void Terminal_MapHandler(RigidDynCubeObj *o, char *arg)
{
    float scale;

    *(Vec3d **)(arg + 0xC) = &o->pos;
    *(Matrix **)(arg + 0x10) = &o->matrix;
    scale = sobj_scale(o);
    *(float *)(arg + 0x14) = VirModel_GetRadius(o->model) * scale;
    ComputerObject_DrawSprite(arg);
    ComputerObject_DrawMarker(arg);
    ComputerObject_DrawText(arg);
}
HOOK(0x443DC0, Terminal_MapHandler);

void Terminal_Close(void)
{
    QTaskList_Free(gTerminalParams);
    TaskType_Free(gTerminalType);
}
HOOK(0x443E60, Terminal_Close);

uint16_t Terminal_GetType(void)
{
    return gTerminalType;
}
HOOK(0x443E80, Terminal_GetType);

void *Terminal_GetParams(void)
{
    return gTerminalParams;
}
HOOK(0x443E90, Terminal_GetParams);

void Terminal_SetHacked(Terminal *t)
{
    t->hacked = 1;
    t->hacked_tick = 1;
    t->hack_tick = gTick;
}
HOOK(0x443EA0, Terminal_SetHacked);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533890) double kUseRange INIT(3072.0);
ALIAS(kUseWidth, kUseRange, 0);
DATA(53C680) uint16_t gTerminalType INIT(385);
PAD(igd, 53C680, 53C682, 0x2);
BSS(57C1A0) void *gTerminalParams;
