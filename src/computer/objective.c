#include "hook.h"
#include "computer/computer.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));

/* one line of the objectives list */
typedef struct Objective {
    char name[32];
    int id;
    VarString visible;
    VarString done;
    int isVisible;
    int isDone;
} Objective;

/* the objectives shown on the map computer, while cond is true */
typedef struct ComputerObjective {
    Task task;
    VarString cond;
    Objective obj[6];
} ComputerObjective;

_Static_assert(sizeof(Objective) == 0xCC, "Objective");
_Static_assert(sizeof(ComputerObjective) == 0x538, "ComputerObjective");

extern void *gObjectiveParams;
extern uint16_t gObjectiveType;
extern ComputerObjective *gObjective;

void ComputerObjective_Create(ComputerObjective *c);
void ComputerObjective_Run(ComputerObjective *c);
void ComputerObjective_Delete(ComputerObjective *c);
void ComputerObjective_GetParams(void *task, void **out);
void ComputerObjective_Changed(ComputerObjective *c);

void ComputerObjective_Open(void)
{
    ComputerObjective c;
    int i;

    gObjectiveType = TaskType_New("DefineComputerObjective", sizeof(ComputerObjective), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gObjectiveType, 0, ComputerObjective_Create);
    TaskType_SetHandler2(gObjectiveType, 1, ComputerObjective_Run);
    TaskType_SetHandler2(gObjectiveType, 2, ComputerObjective_Delete);
    TaskType_SetHandler(gObjectiveType, Param_GetTableEvent(), ComputerObjective_GetParams);
    TaskType_SetHandler(gObjectiveType, Param_GetChangedEvent(), ComputerObjective_Changed);
    gObjectiveParams = ParamTable_New(25);
    ParamTable_Add(gObjectiveParams, &c, &c.cond, Param_VarString(), 7, NULL);
    for (i = 0; i < 6; i++) {
        ParamTable_Add(gObjectiveParams, &c, c.obj[i].name, Param_String32(), 7, NULL);
        ParamTable_Add(gObjectiveParams, &c, &c.obj[i].id, Param_Int32(), 7, NULL);
        ParamTable_Add(gObjectiveParams, &c, &c.obj[i].visible, Param_VarString(), 7, NULL);
        ParamTable_Add(gObjectiveParams, &c, &c.obj[i].done, Param_VarString(), 7, NULL);
    }
}
HOOK(0x466F70, ComputerObjective_Open);

void ComputerObjective_Create(ComputerObjective *c)
{
    int i;

    VarString_Init(&c->cond);
    for (i = 0; i < 6; i++) {
        c->obj[i].name[0] = 0;
        c->obj[i].id = -1;
        VarString_Init(&c->obj[i].visible);
        VarString_Init(&c->obj[i].done);
        c->obj[i].isVisible = 0;
        c->obj[i].isDone = 0;
    }
}
HOOK(0x4673B0, ComputerObjective_Create);

static int eval_flag(VarString *v, void *task)
{
    if (!VarString_HasExpr(v))
        return 0;
    return (char)round_ll(VarString_EvalReal(v, task));
}

void ComputerObjective_Run(ComputerObjective *c)
{
    int i;

    if (VarString_HasExpr(&c->cond) && !(char)round_ll(VarString_EvalReal(&c->cond, c)))
        return;
    gObjective = c;
    for (i = 0; i < 6; i++) {
        c->obj[i].isVisible = eval_flag(&c->obj[i].visible, c);
        c->obj[i].isDone = eval_flag(&c->obj[i].done, c);
    }
}
HOOK(0x467410, ComputerObjective_Run);

void ComputerObjective_Delete(ComputerObjective *c)
{
    int i;

    VarString_Free(&c->cond);
    for (i = 0; i < 6; i++) {
        VarString_Free(&c->obj[i].visible);
        VarString_Free(&c->obj[i].done);
    }
    gObjective = NULL;
}
HOOK(0x4674D0, ComputerObjective_Delete);

void ComputerObjective_GetParams(void *task, void **out)
{
    *out = gObjectiveParams;
}
HOOK(0x467520, ComputerObjective_GetParams);

void ComputerObjective_Changed(ComputerObjective *c)
{
    int i;

    VarString_Bind(&c->cond, c);
    for (i = 0; i < 6; i++) {
        VarString_Bind(&c->obj[i].visible, c);
        VarString_Bind(&c->obj[i].done, c);
    }
}
HOOK(0x467530, ComputerObjective_Changed);

void ComputerObjective_Close(void)
{
    TaskType_Free(gObjectiveType);
    QTaskList_Free(gObjectiveParams);
}
HOOK(0x467570, ComputerObjective_Close);

Objective *ComputerObjective_Get(int i)
{
    if (gObjective)
        return &gObjective->obj[i];
    return NULL;
}
HOOK(0x467590, ComputerObjective_Get);
