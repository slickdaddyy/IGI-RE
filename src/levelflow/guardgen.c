#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "levelflow/levelflow.h"

ORIG(0x401E30, void, Task_BroadcastChildren, (void *task, uint8_t slot, void *arg));
ORIG(0x463170, char, Human_IsDead, (Task *human));
ORIG(0x463190, char, sub_463190, (Task *human));
ORIG(0x4AF8F0, void, List_AddTail, (void *list, void *node));
ORIG(0x4AF9B0, int, List_Count, (void *list));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4F1090, void, Level_SetTaskId, (int id, Task *task));
/* runs operation op of parameter value type `type` on a field */
ORIG(0x4F1AF0, void, ParamType_Call, (void *field, int type, uint8_t op, void *arg));
ORIG(0x4F1B90, uint8_t, ParamOp_Size, (void));
ORIG(0x4F1BA0, uint8_t, ParamOp_Copy, (void));

/* a saved copy of one task of the guard: its type, level id and the
   values of all its parameters */
typedef struct GuardTemplate {
    struct GuardTemplate *next;
    struct GuardTemplate *prev;
    List children;
    struct GuardTemplate *parent;
    int unk18;
    uint16_t type;
    char unk1E[2];
    int id;
    ParamTable *params;
    void **values;
} GuardTemplate;

/* respawns its first child (a guard and the tasks below it) each time
   the guard is dead, up to maxSpawns times, while isOn */
typedef struct GuardGenerator {
    Task task;
    VarString on;
    char isOn;
    char registered;
    char isSpawned;
    char unk73;
    int nSpawns;
    int maxSpawns;
    GuardTemplate *tree;
} GuardGenerator;

_Static_assert(sizeof(GuardTemplate) == 0x2C, "GuardTemplate");
_Static_assert(sizeof(GuardGenerator) == 0x80, "GuardGenerator");

#define gGuardGeneratorType VAR(uint16_t, 0x541094)
#define gGuardGeneratorParams VAR(ParamTable *, 0x5C8A88)

void GuardGenerator_Create(GuardGenerator *g);
void GuardGenerator_Run(GuardGenerator *g);
void GuardGenerator_Delete(GuardGenerator *g);
void GuardGenerator_GetParams(void *task, ParamTable **out);
void GuardGenerator_Changed(GuardGenerator *g);
void GuardGenerator_Activate(GuardGenerator *g);
void GuardGenerator_Deactivate(GuardGenerator *g);
void GuardGenerator_WorldAdd(Task *t, void *arg);
void GuardGenerator_WorldRemove(Task *t, void *arg);
void GuardGenerator_ComputerEvent(GuardGenerator *g, void *arg);

void GuardGenerator_Init(void)
{
    gGuardGeneratorType = TaskType_New("GuardGenerator", sizeof(GuardGenerator), Task_GetRootType(), 0);
    TaskType_SetHandler(gGuardGeneratorType, 0, GuardGenerator_Create);
    TaskType_SetHandler2(gGuardGeneratorType, 1, GuardGenerator_Run);
    TaskType_SetHandler2(gGuardGeneratorType, 2, GuardGenerator_Delete);
    TaskType_SetHandler(gGuardGeneratorType, Param_GetTableEvent(), GuardGenerator_GetParams);
    TaskType_SetHandler(gGuardGeneratorType, Param_GetChangedEvent(), GuardGenerator_Changed);
    TaskType_SetHandler2(gGuardGeneratorType, Level_GetEventDA8(), GuardGenerator_Activate);
    TaskType_SetHandler2(gGuardGeneratorType, Level_GetEventDAC(), GuardGenerator_Deactivate);
    TaskType_SetHandler(gGuardGeneratorType, World_GetEvent1(), GuardGenerator_WorldRemove);
    TaskType_SetHandler(gGuardGeneratorType, World_GetEvent0(), GuardGenerator_WorldAdd);
    TaskType_SetHandler(gGuardGeneratorType, RigidDynCubeObj_GetEvent2(), HANDLER_4E0890);
    TaskType_SetHandler(gGuardGeneratorType, ComputerObject_GetEvent0(), GuardGenerator_ComputerEvent);
    TaskType_SetHandler(gGuardGeneratorType, RigidDynCubeObj_GetEvent4(), ConditionalContainer_ForwardEvent4);
    gGuardGeneratorParams = ParamTable_New(2);
    ParamTable_Add(gGuardGeneratorParams, NULL, FIELD(GuardGenerator, on), Param_VarString(), 7, NULL);
    ParamTable_Add(gGuardGeneratorParams, NULL, FIELD(GuardGenerator, maxSpawns), Param_Int32(), 7, NULL);
}
HOOK(0x487BE0, GuardGenerator_Init);

void GuardGenerator_WorldAdd(Task *t, void *arg)
{
    EACH_CHILD(t, World_GetEvent0(), arg);
}
HOOK(0x487D90, GuardGenerator_WorldAdd);

void GuardGenerator_WorldRemove(Task *t, void *arg)
{
    EACH_CHILD(t, World_GetEvent1(), arg);
}
HOOK(0x487E20, GuardGenerator_WorldRemove);

void GuardGenerator_Create(GuardGenerator *g)
{
    VarString_Init(&g->on);
    g->isOn = 0;
    g->registered = 0;
    g->nSpawns = 0;
    g->maxSpawns = 0;
    g->isSpawned = 0;
    g->tree = NULL;
}
HOOK(0x487EB0, GuardGenerator_Create);

void GuardGenerator_Spawn(GuardGenerator *g);

void GuardGenerator_Run(GuardGenerator *g)
{
    Task *guard;

    if (VarString_HasExpr(&g->on))
        g->isOn = (char)round_int(VarString_EvalReal(&g->on, g));
    if (!g->isOn) {
        if (!g->task.head->next || !g->task.head)
            g->isSpawned = 0;
    } else if (g->nSpawns < g->maxSpawns) {
        guard = g->task.head;
        if (!guard->next || !guard) {
            GuardGenerator_Spawn(g);
            g->isSpawned = 1;
            g->nSpawns++;
        } else if (Human_IsDead(guard) && !sub_463190(guard)) {
            Task_Delete(FIRST_CHILD(&g->task));
            GuardGenerator_Spawn(g);
            g->isSpawned = 1;
            g->nSpawns++;
        }
    }
    EACH_CHILD(&g->task, 1);
}
HOOK(0x487EE0, GuardGenerator_Run);

void GuardTemplate_Spawn(GuardTemplate *t, Task *parent);

void GuardGenerator_Spawn(GuardGenerator *g)
{
    char changed;

    if (!g->tree)
        return;
    Mem_PushMode(1);
    GuardTemplate_Spawn(g->tree, &g->task);
    changed = 1;
    Task_BroadcastChildren(g, Param_GetChangedEvent(), &changed);
    Mem_PopMode();
}
HOOK(0x488000, GuardGenerator_Spawn);

void GuardTemplate_Spawn(GuardTemplate *t, Task *parent)
{
    Task *task = Task_New(parent, t->type, NULL);
    GuardTemplate *c, *n;
    int i;

    if (t->id > 0)
        Level_SetTaskId(t->id, task);
    for (i = 0; i < t->params->count; i++) {
        ParamEntry *e = &t->params->entries[i];
        void *field = (char *)task + e->offset;

        ParamType_Call(t->values[i], e->type, ParamOp_Copy(), &field);
    }
    if (List_Count(&t->children)) {
        c = t->children.head;
        for (n = c->next; n; c = n, n = n->next)
            GuardTemplate_Spawn(c, task);
    }
}
HOOK(0x488040, GuardTemplate_Spawn);

void GuardGenerator_FreeTree(GuardGenerator *g);

void GuardGenerator_Delete(GuardGenerator *g)
{
    GuardGenerator_FreeTree(g);
    if (Task_GetId(g) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), g->task.type);

        if (fn)
            fn(g);
    }
    VarString_Free(&g->on);
}
HOOK(0x4880F0, GuardGenerator_Delete);

void GuardTemplate_Free(GuardTemplate *t);

void GuardGenerator_FreeTree(GuardGenerator *g)
{
    if (g->tree)
        GuardTemplate_Free(g->tree);
}
HOOK(0x488140, GuardGenerator_FreeTree);

void GuardTemplate_Free(GuardTemplate *t)
{
    GuardTemplate *c = t->children.head, *n;
    int i;

    if (c->next && c) {
        do {
            n = c->next && c->next->next ? c->next : NULL;
            GuardTemplate_Free(c);
            c = n;
        } while (c);
    }
    if (t->params) {
        for (i = 0; i < t->params->count; i++) {
            Mem_Free(t->values[i]);
            t->values[i] = NULL;
        }
        Mem_Free(t->values);
        t->values = NULL;
    }
    Mem_Free(t);
}
HOOK(0x488160, GuardTemplate_Free);

void GuardGenerator_GetParams(void *task, ParamTable **out)
{
    *out = gGuardGeneratorParams;
}
HOOK(0x4881F0, GuardGenerator_GetParams);

void GuardGenerator_Snapshot(GuardGenerator *g);

void GuardGenerator_Changed(GuardGenerator *g)
{
    VarString_Bind(&g->on, g);
    GuardGenerator_Snapshot(g);
}
HOOK(0x488200, GuardGenerator_Changed);

GuardTemplate *GuardTemplate_New(GuardTemplate *parent, Task *task);

/* saves the first child and the tasks below it as templates, then deletes
   the child; it only walks two levels down */
void GuardGenerator_Snapshot(GuardGenerator *g)
{
    Task *cur = g->task.head;
    GuardTemplate *parent = NULL;

    if (!cur->next || !cur)
        goto done;
    for (;;) {
        ParamTable *params = NULL;
        TaskFn fn = HANDLER(Param_GetTableEvent(), cur->type);

        if (fn)
            fn(cur, &params);
        if (params) {
            GuardTemplate *t = GuardTemplate_New(parent, cur);
            int i;

            if (!parent)
                g->tree = t;
            t->params = params;
            t->values = Mem_Alloc(params->count * 4, 4);
            for (i = 0; i < t->params->count; i++) {
                ParamEntry *e = &t->params->entries[i];
                char *field = (char *)cur + e->offset;
                int size;
                void *dst;

                ParamType_Call(field, e->type, ParamOp_Size(), &size);
                if (size) {
                    t->values[i] = Mem_Alloc(size, 4);
                    dst = t->values[i];
                    ParamType_Call(field, e->type, ParamOp_Copy(), &dst);
                }
            }
            if (cur->head->next && cur->head) {
                cur = cur->head;
                parent = t;
                continue;
            }
        }
        if (cur->next && cur->next->next) {
            cur = cur->next;
            continue;
        }
        cur = cur->parent;
        if (cur->parent == &g->task)
            break;
        parent = parent->parent;
    }
done:
    Task_Delete(FIRST_CHILD(&g->task));
}
HOOK(0x488220, GuardGenerator_Snapshot);

GuardTemplate *GuardTemplate_New(GuardTemplate *parent, Task *task)
{
    GuardTemplate *t = Mem_Alloc(sizeof(GuardTemplate), 4);

    t->values = NULL;
    t->params = NULL;
    t->id = Task_GetId(task);
    t->type = task->type;
    t->parent = parent;
    t->prev = NULL;
    t->next = NULL;
    t->children.head = &t->children.tail;
    t->children.tailpred = &t->children.head;
    t->children.tail = NULL;
    if (parent)
        List_AddTail(&parent->children, t);
    return t;
}
HOOK(0x4883C0, GuardTemplate_New);

void GuardGenerator_Activate(GuardGenerator *g)
{
    char name[64];
    char buf[128];

    if (g->registered)
        return;
    g->registered = 1;
    Task_GetName(name, g);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &g->isOn);
    crt_sprintf(buf, "%s.isSpawned", name);
    Script_RegisterBool8(buf, &g->isSpawned);
    crt_sprintf(buf, "%s.nSpawns", name);
    Script_RegisterInt2(buf, &g->nSpawns);
}
HOOK(0x488430, GuardGenerator_Activate);

void GuardGenerator_Deactivate(GuardGenerator *g)
{
    char name[64];
    char buf[128];

    if (!g->registered)
        return;
    Task_GetName(name, g);
    g->registered = 0;
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isSpawned", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nSpawns", name);
    Script_RemoveSymbol(buf);
    g->registered = 0;
}
HOOK(0x4884D0, GuardGenerator_Deactivate);

void GuardGenerator_ComputerEvent(GuardGenerator *g, void *arg)
{
    if (!g->isSpawned)
        return;
    EACH_CHILD(&g->task, ComputerObject_GetEvent0(), arg);
}
HOOK(0x488560, GuardGenerator_ComputerEvent);

void GuardGenerator_Close(void)
{
    TaskType_Free(gGuardGeneratorType);
    QTaskList_Free(gGuardGeneratorParams);
}
HOOK(0x4885F0, GuardGenerator_Close);

uint16_t GuardGenerator_GetType(void)
{
    return gGuardGeneratorType;
}
HOOK(0x488610, GuardGenerator_GetType);
