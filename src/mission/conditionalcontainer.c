#include "hook.h"
#include "mission/mission.h"

/* "ConditionalContainer" (0x114 bytes): shows its children while cond is true */
typedef struct ConditionalContainer {
    Task task;
    VarString cond;
    VarString onTrue;
    VarString onFalse;
    char active;
    char unk111;
    char shown;
    char pad113;
} ConditionalContainer;

/* sent to the children when they are put into the world */
typedef struct WorldArg {
    int unk0;
    void *world;
    int unk8;
} WorldArg;

_Static_assert(offsetof(ConditionalContainer, active) == 0x110, "ConditionalContainer.active");
_Static_assert(sizeof(ConditionalContainer) == 0x114, "ConditionalContainer");

ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x505D40, uint16_t, sub_505D40, (void));

#define Handler4E0890 ((void *)0x4E0890)
#define ConditionalContainer_Activate ((void *)0x487920)
#define ConditionalContainer_Deactivate ((void *)0x487980)
#define ConditionalContainer_Move ((void *)0x4879E0)
#define ConditionalContainer_Removed ((void *)0x487B20)
#define ConditionalContainer_Added ((void *)0x487A70)

#define gConditionalContainerType VAR(uint16_t, 0x5C8A80)
#define gConditionalContainerParams VAR(void *, 0x5C8A7C)

void ConditionalContainer_Create(ConditionalContainer *c);
void ConditionalContainer_Run(ConditionalContainer *c);
void ConditionalContainer_Delete(ConditionalContainer *c);
void ConditionalContainer_GetParams(void *task, void **out);
void ConditionalContainer_Changed(ConditionalContainer *c);
void ConditionalContainer_ComputerEvent(ConditionalContainer *c, void *arg);
void ConditionalContainer_Show(ConditionalContainer *c);

void ConditionalContainer_Init(void)
{
    ConditionalContainer c;

    gConditionalContainerType = TaskType_New("ConditionalContainer", sizeof(ConditionalContainer), sub_505D40(), 0, 0);
    TaskType_SetHandler(gConditionalContainerType, 0, ConditionalContainer_Create);
    TaskType_SetHandler2(gConditionalContainerType, 1, ConditionalContainer_Run);
    TaskType_SetHandler2(gConditionalContainerType, 2, ConditionalContainer_Delete);
    TaskType_SetHandler(gConditionalContainerType, RigidDynCubeObj_GetEvent2(), Handler4E0890);
    TaskType_SetHandler(gConditionalContainerType, Param_GetTableEvent(), ConditionalContainer_GetParams);
    TaskType_SetHandler(gConditionalContainerType, Param_GetChangedEvent(), ConditionalContainer_Changed);
    TaskType_SetHandler(gConditionalContainerType, ComputerObject_GetEvent0(), ConditionalContainer_ComputerEvent);
    TaskType_SetHandler2(gConditionalContainerType, 3, ConditionalContainer_Show);
    TaskType_SetHandler2(gConditionalContainerType, Level_GetEventDA8(), ConditionalContainer_Activate);
    TaskType_SetHandler2(gConditionalContainerType, Level_GetEventDAC(), ConditionalContainer_Deactivate);
    TaskType_SetHandler(gConditionalContainerType, RigidDynCubeObj_GetEvent4(), ConditionalContainer_Move);
    TaskType_SetHandler(gConditionalContainerType, World_GetEvent1(), ConditionalContainer_Removed);
    TaskType_SetHandler(gConditionalContainerType, World_GetEvent0(), ConditionalContainer_Added);
    gConditionalContainerParams = ParamTable_New(3);
    ParamTable_Add(gConditionalContainerParams, &c, &c.cond, Param_VarString(), 7, NULL);
    ParamTable_Add(gConditionalContainerParams, &c, &c.onTrue, Param_VarString(), 7, NULL);
    ParamTable_Add(gConditionalContainerParams, &c, &c.onFalse, Param_VarString(), 7, NULL);
}
HOOK(0x4872C0, ConditionalContainer_Init);

void ConditionalContainer_Create(ConditionalContainer *c)
{
    VarString_Init(&c->cond);
    VarString_Init(&c->onTrue);
    VarString_Init(&c->onFalse);
    c->active = 0;
    c->shown = 1;
    c->unk111 = 0;
}
HOOK(0x4874A0, ConditionalContainer_Create);

void ConditionalContainer_Run(ConditionalContainer *c)
{
    char was = c->active;

    if (VarString_HasExpr(&c->cond))
        c->active = (char)round_ll(VarString_EvalReal(&c->cond, c));
    if (!c->active && !was) {
        if (c->shown) {
            void *world = Game_GetWorld();

            CALL_CHILDREN(c, World_GetEvent1(), &world);
            c->shown = 0;
        }
    } else {
        if (!c->shown) {
            WorldArg arg;

            arg.unk0 = 0;
            arg.world = Game_GetWorld();
            arg.unk8 = 2;
            CALL_CHILDREN(c, World_GetEvent0(), &arg);
            c->shown = 1;
        }
        CALL_CHILDREN(c, 1);
    }
    if (c->active) {
        if (!was && VarString_HasExpr(&c->onTrue))
            VarString_EvalReal(&c->onTrue, c);
    } else if (was && VarString_HasExpr(&c->onFalse)) {
        VarString_EvalReal(&c->onFalse, c);
    }
}
HOOK(0x4874E0, ConditionalContainer_Run);

void ConditionalContainer_Delete(ConditionalContainer *c)
{
    if (Task_GetId(c) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), c->task.type);

        if (fn)
            ((void (*)(void *))fn)(c);
    }
    VarString_Free(&c->cond);
    VarString_Free(&c->onTrue);
    VarString_Free(&c->onFalse);
}
HOOK(0x487730, ConditionalContainer_Delete);

void ConditionalContainer_GetParams(void *task, void **out)
{
    *out = gConditionalContainerParams;
}
HOOK(0x487790, ConditionalContainer_GetParams);

void ConditionalContainer_Changed(ConditionalContainer *c)
{
    VarString_Bind(&c->cond, c);
    VarString_Bind(&c->onTrue, c);
    VarString_Bind(&c->onFalse, c);
}
HOOK(0x4877A0, ConditionalContainer_Changed);

/* passed on to the children only while they are shown */
void ConditionalContainer_ComputerEvent(ConditionalContainer *c, void *arg)
{
    if (c->shown)
        CALL_CHILDREN(c, ComputerObject_GetEvent0(), arg);
}
HOOK(0x4877D0, ConditionalContainer_ComputerEvent);

void ConditionalContainer_Show(ConditionalContainer *c)
{
    WorldArg arg;

    if (c->shown)
        return;
    arg.unk0 = 0;
    arg.world = Game_GetWorld();
    arg.unk8 = 2;
    CALL_CHILDREN(c, World_GetEvent0(), &arg);
    c->shown = 1;
}
HOOK(0x487860, ConditionalContainer_Show);
