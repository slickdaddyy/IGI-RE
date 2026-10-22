/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "door/door.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CC0, void *, Task_FindAncestor, (void *task, uint16_t type));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x47E6F0, uint8_t, sub_47E6F0, (void));
ORIG(0x4871C0, void, sub_4871C0, (char *p, char *q));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4DCCB0, void, Sector_AddObject, (void *sector, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, sub_4DCDD0, (void *obj));
ORIG(0x4DCF00, void, sub_4DCF00, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E0830, uint16_t, SectorInteriorObject_GetType, (void));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
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
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

#define gWorld VAR(void *, 0xA44344)
#define gSwitchParams VAR(void *, 0x57C1B4)
#define gSwitchType VAR(uint16_t, 0x57C1B8)

#define Terminal_MapHandler ((void *)0x443DC0)
#define sub_4F01B0 ((void *)0x4F01B0)
#define TASK_NOTHING ((void *)0x4D3710)

#define FIELD(name) ((void *)offsetof(Switch, name))

void Switch_CreateHandler(Switch *s);
void Switch_RunHandler(Switch *s);
void Switch_DeleteHandler(Switch *s);
void Switch_GetParams(void *task, void **out);
void Switch_ParamsChanged(Switch *s);
void Switch_CheckUse(Switch *s, UseQuery *q);
void sub_445820(void *task, char **arg);
void Switch_Destroy(Switch *s);
void Switch_AddSymbols(Switch *s);
void Switch_RemoveSymbols(Switch *s);
void Switch_MoveHandler(Switch *s, MoveMsg *msg);

/* the script pressed the switch */
void Switch_Press(Switch *s)
{
    char last = s->isPressed;

    s->isPressed = 1;
    s->isLastPressed = last;
    s->pressTick = gTick;
}
HOOK(0x444BC0, Switch_Press);

void Switch_Open(void)
{
    int i;

    gSwitchType = TaskType_New("Switch", sizeof(Switch), SectorInteriorObject_GetType(), 0);
    TaskType_SetHandler(gSwitchType, 0, Switch_CreateHandler);
    TaskType_SetHandler2(gSwitchType, 1, Switch_RunHandler);
    TaskType_SetHandler2(gSwitchType, 2, Switch_DeleteHandler);
    TaskType_SetHandler(gSwitchType, Param_GetTableEvent(), Switch_GetParams);
    TaskType_SetHandler(gSwitchType, Param_GetChangedEvent(), Switch_ParamsChanged);
    TaskType_SetHandler(gSwitchType, Game_GetEvent2(), Switch_CheckUse);
    TaskType_SetHandler(gSwitchType, Game_GetEvent3(), sub_4F01B0);
    TaskType_SetHandler(gSwitchType, Game_GetEvent4(), TASK_NOTHING);
    TaskType_SetHandler(gSwitchType, Game_GetEvent6(), sub_445820);
    TaskType_SetHandler(gSwitchType, Game_GetEvent9(), Switch_Destroy);
    TaskType_SetHandler2(gSwitchType, Level_GetEventDA8(), Switch_AddSymbols);
    TaskType_SetHandler2(gSwitchType, Level_GetEventDAC(), Switch_RemoveSymbols);
    TaskType_SetHandler(gSwitchType, RigidDynCubeObj_GetEvent4(), Switch_MoveHandler);
    TaskType_SetHandler(gSwitchType, ComputerObject_GetEvent0(), Terminal_MapHandler);
    TaskType_SetHandler(gSwitchType, sub_47E6F0(), NULL);
    gSwitchParams = ParamTable_New(10);
    ParamTable_Add(gSwitchParams, NULL, FIELD(place), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSwitchParams, NULL, FIELD(matrix), Param_Real32x9(), 7, NULL);
    ParamTable_Add(gSwitchParams, NULL, FIELD(on), Param_VarString(), 7, NULL);
    ParamTable_Add(gSwitchParams, NULL, FIELD(isOn), Param_Bool8(), 7, NULL);
    for (i = 0; i < 5; i++)
        ParamTable_Add(gSwitchParams, NULL, FIELD(models[i]), Param_String16(), 7, NULL);
    ParamTable_Add(gSwitchParams, NULL, FIELD(destroyable), Param_Bool8(), 7, NULL);
}
HOOK(0x444BF0, Switch_Open);

void Switch_CreateHandler(Switch *s)
{
    static const Matrix id = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};
    Vec3d zero = {0, 0, 0};
    float scale, radius;
    int i;

    QTaskList_Add(Game_GetTaskList0(), s);
    RigidDynCubeObj_Init(gWorld, s, NULL);
    scale = max_scale(&s->o);
    radius = VirModel_GetRadius(s->o.model) * scale;
    DynCube_Add(gWorld, s, &zero, radius, 1);
    Sector_AddObject(s->o.sector, s, DynCubeObj_GetPosition(s));
    s->o.matrix = id;
    s->matrix = id;
    for (i = 0; i < 5; i++)
        s->models[i][0] = 0;
    s->isOn = 1;
    s->isDestroyed = 0;
    s->isPressed = 0;
    s->isLastOn = 1;
    s->isLastDestroyed = 0;
    s->isLastPressed = 0;
    s->pressTick = 0;
    s->registered = 0;
    s->destroyable = 1;
    VarString_Init(&s->on);
}
HOOK(0x444F20, Switch_CreateHandler);

/* follows its script expression and shows the model for its state */
void Switch_RunHandler(Switch *s)
{
    BROADCAST(&s->o.task, 1);
    if (VarString_HasExpr(&s->on))
        s->isOn = (char)round_ll(VarString_EvalReal(&s->on, s));
    if (gTick > s->pressTick + 2) {
        char last = s->isPressed;

        s->isPressed = 0;
        s->isLastPressed = last;
    }
    if (s->isOn != s->isLastOn || s->isPressed != s->isLastPressed || s->isDestroyed != s->isLastDestroyed) {
        int i;

        if (s->isDestroyed) {
            i = 4;
        } else if (!s->isOn) {
            i = (s->isPressed != 0) + 2;
        } else if (s->isPressed) {
            i = 1;
        } else {
            Task *snd = Sound_Play(s, "switch");

            if (snd)
                Sound_SetPos(snd, &s->o.pos);
            i = 0;
        }
        RigidDynCubeObj_SetModel(s, VirModel_Find(s->models[i]));
        s->isLastOn = s->isOn;
        s->isLastDestroyed = s->isDestroyed;
    }
}
HOOK(0x4450F0, Switch_RunHandler);

void Switch_DeleteHandler(Switch *s)
{
    QTaskList_Remove(Game_GetTaskList0(), s);
    if (Task_GetId(s) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), s->o.task.type);

        if (fn)
            fn(s);
    }
    VarString_Free(&s->on);
    sub_4DCDD0(s);
    DynCube_Remove(s->o.sector, s);
}
HOOK(0x445270, Switch_DeleteHandler);

void Switch_GetParams(void *task, void **out)
{
    *out = gSwitchParams;
}
HOOK(0x4452E0, Switch_GetParams);

/* remembers where the switch sits on the object it is fixed to */
void Switch_UpdateLocal(Switch *s)
{
    Obj *p = Task_FindAncestor(s, RigidDynCubeObj_GetType());

    if (p)
        to_local(&s->offset, &s->local, &s->place, &s->matrix, p);
    else
        clear_local(&s->offset, &s->local);
}
HOOK(0x4453E0, Switch_UpdateLocal);

void Switch_ParamsChanged(Switch *s)
{
    float scale, radius;

    RigidDynCubeObj_SetModel(s, VirModel_Find(s->models[0]));
    scale = max_scale(&s->o);
    radius = VirModel_GetRadius(s->o.model) * scale;
    DynCube_Move(s->o.sector, s, &s->place, radius);
    sub_4DCF00(s->o.sector, s, DynCubeObj_GetPosition(s));
    s->isLastOn = s->isOn;
    s->o.matrix = s->matrix;
    VarString_Bind(&s->on, s);
    Switch_UpdateLocal(s);
}
HOOK(0x4452F0, Switch_ParamsChanged);

void Switch_CheckUse(Switch *s, UseQuery *q)
{
    if (!s->isDestroyed)
        check_use(&s->o, q);
}
HOOK(0x445730, Switch_CheckUse);

void sub_445820(void *task, char **arg)
{
    sub_4871C0(*arg, *arg + 0x44C);
}
HOOK(0x445820, sub_445820);

void Switch_Destroy(Switch *s)
{
    if (s->destroyable)
        s->isDestroyed = 1;
}
HOOK(0x445840, Switch_Destroy);

void Switch_AddSymbols(Switch *s)
{
    char name[64], buf[128];

    if (s->registered)
        return;
    s->registered = 1;
    Task_GetName(name, s);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &s->isOn);
    crt_sprintf(buf, "%s.isLastOn", name);
    Script_RegisterBool8(buf, &s->isLastOn);
    crt_sprintf(buf, "%s.isPressed", name);
    Script_RegisterBool8(buf, &s->isPressed);
    crt_sprintf(buf, "%s.isLastPressed", name);
    Script_RegisterBool8(buf, &s->isLastPressed);
    crt_sprintf(buf, "%s.isDestroyed", name);
    Script_RegisterBool8(buf, &s->isDestroyed);
    crt_sprintf(buf, "%s.isLastDestroyed", name);
    Script_RegisterBool8(buf, &s->isLastDestroyed);
}
HOOK(0x445860, Switch_AddSymbols);

void Switch_RemoveSymbols(Switch *s)
{
    char name[64], buf[128];

    if (!s->registered)
        return;
    Task_GetName(name, s);
    s->registered = 0;
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isDestroyed", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isPressed", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastDestroyed", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastPressed", name);
    Script_RemoveSymbol(buf);
    s->registered = 0;
}
HOOK(0x445980, Switch_RemoveSymbols);

/* RigidDynCubeObj event 4: moves along with the object it is fixed to */
void Switch_MoveHandler(Switch *s, MoveMsg *msg)
{
    float scale, radius;
    MoveMsg out;

    place_from(&s->place, msg->matrix, &s->offset, msg->pos);
    scale = max_scale(&s->o);
    radius = VirModel_GetRadius(s->o.model) * scale;
    DynCube_Move(s->o.sector, s, &s->place, radius);
    mat_mul(&s->matrix, msg->matrix, &s->local, ORDER(SUM01_2, SUM02_1, SUM02_1));
    s->o.matrix = s->matrix;
    out.pos = &s->place;
    out.matrix = &s->matrix;
    BROADCAST(&s->o.task, RigidDynCubeObj_GetEvent4(), &out);
}
HOOK(0x445A80, Switch_MoveHandler);

void Switch_Close(void)
{
    QTaskList_Free(gSwitchParams);
    TaskType_Free(gSwitchType);
}
HOOK(0x445DC0, Switch_Close);
