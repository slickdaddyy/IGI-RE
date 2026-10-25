/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "door/door.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x4871C0, void, StatusScreen_DrawActionIcon, (char *p, char *q));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));
ORIG(0x4DCF00, void, Sector_MoveObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *obj));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));

extern uint16_t gDrawerType;
extern void *gCabinetParams;
extern uint16_t gCabinetType;

/* what a human asked a cabinet to do */
typedef struct CabinetUseArg {
    char unk0[0x24];
    int close;          /* 0x24 */
} CabinetUseArg;

void Cabinet_Run(Cabinet *c)
{
    BROADCAST(&c->o.task, 1);
    if (c->o.attached && c->moving)
        HANDLER(1, c->o.attached->type)(c->o.attached);
    if (gLevelFlow->nTick > c->useTick + 2)
        c->inUse = 0;
    c->lastInUse = c->inUse;
}
HOOK(0x444590, Cabinet_Run);

void Cabinet_Delete(Cabinet *c)
{
    QTaskList_Remove(Game_GetTaskList0(), c);
    if (Task_GetId(c) >= 0) {
        TaskFn fn = HANDLER(Level_GetDeactivateEvent(), c->o.task.type);

        if (fn)
            fn(c);
    }
    if (c->o.attached)
        Task_Delete(c->o.attached);
    Sector_RemoveObject(c);
    DynCube_Remove(c->o.world, c);
}
HOOK(0x444660, Cabinet_Delete);

void Cabinet_GetParams(void *task, void **out)
{
    *out = gCabinetParams;
}
HOOK(0x4446D0, Cabinet_GetParams);

/* also builds the drawers */
void Cabinet_ParamsChanged(Cabinet *c, char *arg)
{
    Task *t;
    float scale, radius;

    RigidDynCubeObj_SetModel(c, VirModel_Find(c->model));
    scale = max_scale(&c->o);
    radius = VirModel_GetRadius(c->o.model) * scale;
    DynCube_Move(c->o.world, c, &c->place, radius);
    Sector_MoveObject(c->o.world, c, DynCubeObj_GetPosition(c));
    Matrix_SetRotZ(&c->o.matrix, c->yaw);
    if (c->o.attached)
        Task_Delete(c->o.attached);
    t = MagicObjContainer_New(NULL, c);
    c->o.attached = t;
    RigidDynCubeObj_CreateMagicObjs(t, c, c);
    if (!arg || !*arg)
        BROADCAST(c->o.attached, Param_GetChangedEvent(), NULL);
}
HOOK(0x4446E0, Cabinet_ParamsChanged);

void Cabinet_CheckUse(Cabinet *c, UseQuery *q)
{
    check_use(&c->o, q);
}
HOOK(0x444860, Cabinet_CheckUse);

void Cabinet_Use(Cabinet *c, CabinetUseArg *arg)
{
    c->inUse = 1;
    c->useTick = gLevelFlow->nTick;
    if (arg->close == 0) {
        c->opening = 1;
        c->closing = 0;
        c->moving = 1;
    }
    if (arg->close == 1) {
        c->closing = 1;
        c->opening = 0;
        c->moving = 1;
    }
}
HOOK(0x444940, Cabinet_Use);

void Cabinet_DrawIcon(void *task, char **arg)
{
    StatusScreen_DrawActionIcon(*arg, *arg + 0x3EC);
}
HOOK(0x4449A0, Cabinet_DrawIcon);

void Cabinet_AddSymbols(Cabinet *c)
{
    char name[64], buf[128];

    if (c->registered)
        return;
    c->registered = 1;
    Task_GetName(name, c);
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RegisterBool8(buf, &c->inUse);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RegisterBool8(buf, &c->lastInUse);
    crt_sprintf(buf, "%s.isSearched", name);
    Script_RegisterBool8(buf, &c->searched);
}
HOOK(0x4449C0, Cabinet_AddSymbols);

void Cabinet_RemoveSymbols(Cabinet *c)
{
    char name[64], buf[128];

    if (!c->registered)
        return;
    Task_GetName(name, c);
    c->registered = 0;
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isSearched", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x444A70, Cabinet_RemoveSymbols);

void Cabinet_Close(void)
{
    QTaskList_Free(gCabinetParams);
    TaskType_Free(gDrawerType);
    TaskType_Free(gCabinetType);
}
HOOK(0x444B00, Cabinet_Close);

/* where a human uses obj (its model point 0) and how it is turned */
void Obj_GetUsePoint(RigidDynCubeObj *o, Vec3d *pos, Matrix *m)
{
    *m = o->matrix;
    Model_GetPoint(pos, o, 0);
    pos->x = o->pos.x + pos->x;
    pos->y = o->pos.y + pos->y;
    pos->z = o->pos.z + pos->z;
}
HOOK(0x444B30, Obj_GetUsePoint);

float Cabinet_GetSearchTime(Cabinet *c)
{
    return c->searchTime;
}
HOOK(0x444B80, Cabinet_GetSearchTime);

void Cabinet_SetSearched(Cabinet *c)
{
    c->searched = 1;
}
HOOK(0x444B90, Cabinet_SetSearched);

/* snaps the drawers shut */
void Cabinet_Reset(Cabinet *c)
{
    c->reset = 1;
    c->moving = 1;
}
HOOK(0x444BA0, Cabinet_Reset);
