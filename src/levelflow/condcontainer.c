#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "levelflow/levelflow.h"

/* the script handlers of "ConditionalContainer"; the type itself is made
   at 0x4872C0 */
typedef struct ConditionalContainer {
    Task task;
    VarString unk20;
    VarString unk70;
    VarString unkC0;
    char isRun;
    char registered;
    char inWorld;
    char unk113;
} ConditionalContainer;

_Static_assert(sizeof(ConditionalContainer) == 0x114, "ConditionalContainer");

extern uint16_t gConditionalContainerType;
extern void *gConditionalContainerParams;

void ConditionalContainer_Activate(ConditionalContainer *c)
{
    char name[64];
    char buf[128];

    if (c->registered)
        return;
    c->registered = 1;
    Task_GetName(name, c);
    crt_sprintf(buf, "%s.isRun", name);
    Script_RegisterBool8(buf, &c->isRun);
}
HOOK(0x487920, ConditionalContainer_Activate);

void ConditionalContainer_Deactivate(ConditionalContainer *c)
{
    char name[64];
    char buf[128];

    if (!c->registered)
        return;
    Task_GetName(name, c);
    c->registered = 0;
    crt_sprintf(buf, "%s.isRun", name);
    Script_RemoveSymbol(buf);
    c->registered = 0;
}
HOOK(0x487980, ConditionalContainer_Deactivate);

void ConditionalContainer_ForwardEvent4(Task *t, void *arg)
{
    EACH_CHILD(t, RigidDynCubeObj_GetEvent4(), arg);
}
HOOK(0x4879E0, ConditionalContainer_ForwardEvent4);

/* puts the children into the world while isRun is set */
void ConditionalContainer_WorldAdd(ConditionalContainer *c, void *arg)
{
    if (!c->isRun || c->inWorld)
        return;
    EACH_CHILD(&c->task, World_GetEvent0(), arg);
    c->inWorld = 1;
}
HOOK(0x487A70, ConditionalContainer_WorldAdd);

void ConditionalContainer_WorldRemove(ConditionalContainer *c, void *arg)
{
    if (!c->inWorld)
        return;
    EACH_CHILD(&c->task, World_GetEvent1(), arg);
    c->inWorld = 0;
}
HOOK(0x487B20, ConditionalContainer_WorldRemove);

void ConditionalContainer_Close(void)
{
    TaskType_Free(gConditionalContainerType);
    QTaskList_Free(gConditionalContainerParams);
}
HOOK(0x487BC0, ConditionalContainer_Close);
