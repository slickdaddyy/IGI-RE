#include <string.h>

#include "igi.h"
#include "hook.h"
#include "task/task.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4AF990, void, List_Insert, (Node *after, Node *node));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int stride, int count, int unk));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));

extern TaskType gTaskTypes[];
extern TaskHandler gTaskHandlers[][TASK_MAX_TYPES];
extern char gTaskHandlerSet[][TASK_MAX_TYPES];
extern TaskEvent gTaskEvents[];
extern uint8_t gTaskEventSlot[];
extern uint8_t gTaskSlotEvent[];
extern uint16_t gTaskRootType;
extern int gTaskUnk54F92C;
extern int gTaskCount;
extern uint32_t gTaskGlobalFlags;
extern int gTaskUnk567C60;
extern int gTaskSetDepth;
extern int gTaskSetDepth2;
extern int gTaskHandlerChanges;
extern Pool gTaskRefs;
extern Task *gTaskStack[];
extern int gTaskStackTop;

typedef void (*TaskDeleteHandler)(Task *task);

#define POOL_ITEM(p, i) ((p)->data + (p)->slots[i] * (p)->stride)
#define POOL_INDEX(p, item) (*(int *)((char *)(item) + (p)->index_offset))

static void *pool_take(Pool *p)
{
    char *item = POOL_ITEM(p, p->used);

    POOL_INDEX(p, item) = p->used;
    p->used++;
    return item;
}

static void pool_release(Pool *p, void *item)
{
    int index = POOL_INDEX(p, item);
    char *last;
    int last_index, tmp;

    p->used--;
    if (index == p->used)
        return;
    last = POOL_ITEM(p, p->used);
    last_index = POOL_INDEX(p, last);
    tmp = p->slots[index];
    p->slots[index] = p->slots[last_index];
    p->slots[last_index] = tmp;
    POOL_INDEX(p, last) = index;
}

/* next node in a list, or NULL at the end */
static Task *next_task(Node *n)
{
    return n->next && n->next->next ? (Task *)n->next : NULL;
}

void Task_InitSystem(void)
{
    int i;

    memset(gTaskStack, 0, 64 * sizeof(Task *));
    gTaskStackTop = 0;
    gTaskUnk54F92C = 0;
    TaskEvent_ResetAll();
    TaskType_ResetAll();
    memset(gTaskEventSlot, 0xFF, 16);
    memset(gTaskSlotEvent, 0xFF, TASK_MAX_SLOTS);
    gTaskRootType = TaskType_New("QTask", sizeof(Task), TASK_NO_TYPE, 0);
    TaskEvent_Set(Task_GetRootType(), 0);
    for (i = 1; i < 4; i++)
        TaskEvent_Set2(Task_GetRootType(), i);
    memset(gTaskHandlers, 0, sizeof(TaskHandler) * TASK_MAX_SLOTS * TASK_MAX_TYPES);
    Pool_Init(&gTaskRefs, sizeof(Task *), 0x17B0, 0);
    Script_RegisterConstant("QTASK_TYPE_UNDEFINED", TASK_NO_TYPE);
}
HOOK(0x4010D0, Task_InitSystem);

void TaskType_ResetAll(void)
{
    int i;

    for (i = 0; i < TASK_MAX_TYPES; i++) {
        gTaskTypes[i].used = 0;
        gTaskTypes[i].name = NULL;
    }
}
HOOK(0x4011B0, TaskType_ResetAll);

void TaskEvent_ResetAll(void)
{
    int i;

    for (i = 0; i < TASK_MAX_SLOTS; i++)
        gTaskEvents[i].used = 0;
}
HOOK(0x4011D0, TaskEvent_ResetAll);

void Task_ShutdownSystem(void)
{
    Pool_Free(&gTaskRefs);
    TaskEvent_Free(0);
    TaskEvent_Free(1);
    TaskEvent_Free(2);
    TaskEvent_Free(3);
    TaskType_Free(gTaskRootType);
    TaskType_WarnLeaks();
    TaskEvent_WarnLeaks();
}
HOOK(0x4011F0, Task_ShutdownSystem);

void TaskType_WarnLeaks(void)
{
    int i;

    for (i = 0; i < TASK_MAX_TYPES; i++)
        if (gTaskTypes[i].used)
            Warning_Show("QTasktype '%s' not deallocated", TaskType_GetName(i));
}
HOOK(0x401230, TaskType_WarnLeaks);

void TaskEvent_WarnLeaks(void)
{
    int i;

    for (i = 0; i < TASK_MAX_SLOTS; i++)
        if (gTaskEvents[i].used)
            Warning_Show("QTask event not deallocated");
}
HOOK(0x401270, TaskEvent_WarnLeaks);

Task *Task_New(Task *parent, uint16_t type, void *arg)
{
    Task *t = Task_Alloc(parent, type);

    if (parent)
        List_AddTail(&parent->children, &t->node);
    if (gTaskHandlers[TASK_SLOT_CREATE][t->type])
        gTaskHandlers[TASK_SLOT_CREATE][t->type](t, arg);
    return t;
}
HOOK(0x4012A0, Task_New);

Task *Task_Alloc(Task *parent, uint16_t type)
{
    TaskType *tt = &gTaskTypes[type];
    Task *t;
    char pooled;

    gTaskCount++;
    if (!tt->pooled || tt->pool.used >= tt->pool.capacity) {
        pooled = 0;
        Mem_PushMode(1);
        t = Mem_Alloc(tt->size, 4);
        Mem_PopMode();
    } else {
        pooled = 1;
        t = pool_take(&tt->pool);
    }
    t->node.prev = NULL;
    t->node.next = NULL;
    t->parent = parent;
    t->pooled = pooled;
    t->type = type;
    t->refs = 0;
    t->children.head = (Node *)&t->children.tail;
    t->children.tail = NULL;
    t->children.tailpred = (Node *)&t->children.head;
    return t;
}
HOOK(0x4012F0, Task_Alloc);

Task *Task_NewHead(Task *parent, uint16_t type, void *arg)
{
    Task *t = Task_Alloc(parent, type);

    if (parent)
        List_AddHead(&parent->children, &t->node);
    if (gTaskHandlers[TASK_SLOT_CREATE][t->type])
        gTaskHandlers[TASK_SLOT_CREATE][t->type](t, arg);
    return t;
}
HOOK(0x4013B0, Task_NewHead);

void TaskType_SetHandler(uint16_t type, uint8_t slot, TaskHandler fn)
{
    int i;

    gTaskSetDepth++;
    gTaskHandlerChanges++;
    gTaskHandlers[slot][type] = fn;
    gTaskHandlerSet[slot][type] = gTaskSetDepth == 1;
    /* pass it down to subtypes that did not set their own */
    for (i = 0; i < TASK_MAX_TYPES; i++)
        if (gTaskTypes[i].used && gTaskTypes[i].parent == type && !gTaskHandlerSet[slot][i])
            TaskType_SetHandler(i, slot, fn);
    TaskType_UpdateMasks();
    gTaskSetDepth--;
}
HOOK(0x401400, TaskType_SetHandler);

void TaskType_UpdateMasks(void)
{
    int i;

    for (i = 0; i < TASK_MAX_TYPES; i++)
        if (gTaskTypes[i].used)
            gTaskTypes[i].mask = TaskType_GetMask(i);
}
HOOK(0x4014B0, TaskType_UpdateMasks);

uint16_t TaskType_GetMask(uint16_t type)
{
    uint16_t mask = 0;
    int bit;

    for (bit = 0; bit < 16; bit++)
        if (gTaskEventSlot[bit] != 0xFF)
            mask |= (gTaskHandlers[gTaskEventSlot[bit]][type] != NULL) << bit;
    return mask | 0x8000;
}
HOOK(0x4014E0, TaskType_GetMask);

void TaskType_SetHandler2(uint16_t type, int slot, TaskHandler fn)
{
    int i;

    gTaskSetDepth2++;
    gTaskHandlerChanges++;
    gTaskHandlers[slot][type] = fn;
    gTaskHandlerSet[slot][type] = gTaskSetDepth2 == 1;
    for (i = 0; i < TASK_MAX_TYPES; i++)
        if (gTaskTypes[i].used && gTaskTypes[i].parent == type && !gTaskHandlerSet[slot][i])
            TaskType_SetHandler2(i, slot, fn);
    TaskType_UpdateMasks();
    gTaskSetDepth2--;
}
HOOK(0x401530, TaskType_SetHandler2);

void Task_Unlink(Task *task)
{
    if (task->parent)
        List_Remove(&task->node);
}
HOOK(0x4015D0, Task_Unlink);

void Task_Delete(Task *task)
{
    TaskType *tt;
    Node *child, *prev;
    int i, n;

    gTaskCount--;
    /* a broadcast walking over this task moves on to the next one */
    for (i = 0; i < gTaskStackTop; i++) {
        if (gTaskStack[i] == task) {
            gTaskStack[i] = next_task(&task->node);
            break;
        }
    }
    if (gTaskHandlers[TASK_SLOT_PREDELETE][task->type])
        ((TaskDeleteHandler)gTaskHandlers[TASK_SLOT_PREDELETE][task->type])(task);
    child = task->children.tailpred;
    for (prev = child->prev; prev; prev = prev->prev) {
        Task_Delete((Task *)child);
        child = prev;
    }
    if (gTaskHandlers[TASK_SLOT_DELETE][task->type])
        ((TaskDeleteHandler)gTaskHandlers[TASK_SLOT_DELETE][task->type])(task);
    n = gTaskRefs.used;
    if (task->refs) {
        for (i = 0; i < n; i++)
            if (*(Task **)POOL_ITEM(&gTaskRefs, i) == task)
                *(Task **)POOL_ITEM(&gTaskRefs, i) = NULL;
    }
    Task_Unlink(task);
    tt = &gTaskTypes[task->type];
    if (!task->pooled)
        Mem_Free(task);
    else
        pool_release(&tt->pool, task);
}
HOOK(0x4015F0, Task_Delete);

void Task_DeleteChildren(Task *task)
{
    Node *n = task->children.head;
    Task *next;

    if (!n->next)
        return;
    while (n) {
        next = next_task(n);
        Task_Delete((Task *)n);
        n = (Node *)next;
    }
}
HOOK(0x401780, Task_DeleteChildren);

static int event_alloc(uint16_t type, char flag)
{
    int i;

    for (i = 0x20; i < TASK_MAX_SLOTS; i++) {
        if (!gTaskEvents[i].used) {
            gTaskEvents[i].used = 1;
            gTaskEvents[i].type = type;
            gTaskEvents[i].flag = flag;
            return i;
        }
    }
    Error_Show("Unable to allocate event");
    for (;;)
        ;
}

int TaskEvent_Alloc(uint16_t type)
{
    return event_alloc(type, 1);
}
HOOK(0x4017C0, TaskEvent_Alloc);

int TaskEvent_Alloc2(uint16_t type)
{
    return event_alloc(type, 0);
}
HOOK(0x401810, TaskEvent_Alloc2);

void TaskEvent_Set(uint16_t type, uint8_t index)
{
    gTaskEvents[index].used = 1;
    gTaskEvents[index].type = type;
    gTaskEvents[index].flag = 1;
}
HOOK(0x401860, TaskEvent_Set);

void TaskEvent_Set2(uint16_t type, int index)
{
    gTaskEvents[index].used = 1;
    gTaskEvents[index].type = type;
    gTaskEvents[index].flag = 0;
}
HOOK(0x401890, TaskEvent_Set2);

void TaskEvent_Free(uint8_t index)
{
    gTaskEvents[index].used = 0;
}
HOOK(0x4018C0, TaskEvent_Free);

void TaskEvent_Free2(uint8_t index)
{
    TaskEvent_Free(index);
}
HOOK(0x4018E0, TaskEvent_Free2);

uint16_t Task_GetRootType(void)
{
    return gTaskRootType;
}
HOOK(0x4018F0, Task_GetRootType);

uint16_t TaskType_New(const char *name, int size, uint16_t parent, int pool_count)
{
    TaskType *tt;
    int i, slot;

    for (i = 0; gTaskTypes[i].used; i++) {
        if (i + 1 >= TASK_MAX_TYPES) {
            Error_Show("Unable to allocate task type");
            for (;;)
                ;
        }
    }
    tt = &gTaskTypes[i];
    tt->used = 1;
    tt->size = size;
    tt->name = name;
    tt->parent = parent;
    tt->flags = parent == TASK_NO_TYPE ? 0x20000000 : gTaskTypes[parent].flags;
    if (pool_count) {
        Pool_Init(&tt->pool, size, pool_count, 0);
        tt->pooled = 1;
    } else {
        tt->pooled = 0;
    }
    for (slot = 0; slot < TASK_MAX_SLOTS; slot++)
        gTaskHandlers[slot][i] = parent == TASK_NO_TYPE ? NULL : gTaskHandlers[slot][parent];
    TaskType_UpdateMasks();
    return i;
}
HOOK(0x401900, TaskType_New);

void TaskType_Free(uint16_t type)
{
    if (gTaskTypes[type].pooled)
        Pool_Free(&gTaskTypes[type].pool);
    gTaskTypes[type].used = 0;
}
HOOK(0x401A20, TaskType_Free);

int Task_GetCount(void)
{
    return gTaskCount;
}
HOOK(0x401A60, Task_GetCount);

uint16_t TaskType_Find(const char *name)
{
    int i;

    for (i = 0; i < TASK_MAX_TYPES; i++)
        if (gTaskTypes[i].used && strcmp(TaskType_GetName(i), name) == 0)
            return i;
    Error_Show("QTask type '%s' not defined", name);
    for (;;)
        ;
}
HOOK(0x401A70, TaskType_Find);

Task **TaskRef_Add(Task *task)
{
    Task **ref = pool_take(&gTaskRefs);

    *ref = task;
    task->refs++;
    return ref;
}
HOOK(0x401AE0, TaskRef_Add);

void TaskRef_Remove(Task **ref)
{
    if (*ref)
        (*ref)->refs--;
    pool_release(&gTaskRefs, ref);
}
HOOK(0x401B20, TaskRef_Remove);

const char *TaskType_GetName(uint16_t type)
{
    return gTaskTypes[type].name;
}
HOOK(0x401B90, TaskType_GetName);

void Task_MoveAfter(Task *task, Task *after)
{
    Task *parent = after->parent;

    Task_Unlink(task);
    List_Insert(&after->node, &task->node);
    task->parent = parent;
}
HOOK(0x401BB0, Task_MoveAfter);

Task *Task_FindChild(Task *task, uint16_t type)
{
    Node *n;

    for (n = task->children.head; n->next; n = n->next)
        if (((Task *)n)->type == type)
            return (Task *)n;
    return NULL;
}
HOOK(0x401BE0, Task_FindChild);

Task *Task_FindLastChild(Task *task, uint16_t type)
{
    Node *n;

    for (n = task->children.tailpred; n->prev; n = n->prev)
        if (((Task *)n)->type == type)
            return (Task *)n;
    return NULL;
}
HOOK(0x401C10, Task_FindLastChild);

Task *Task_FindNext(Task *task, uint16_t type)
{
    Node *n;

    for (n = task->node.next; n && n->next; n = n->next)
        if (((Task *)n)->type == type)
            return (Task *)n;
    return NULL;
}
HOOK(0x401C40, Task_FindNext);

Task *Task_FindPrev(Task *task, uint16_t type)
{
    Node *n;

    for (n = task->node.prev; n && n->prev; n = n->prev)
        if (((Task *)n)->type == type)
            return (Task *)n;
    return NULL;
}
HOOK(0x401C80, Task_FindPrev);

Task *Task_FindAncestor(Task *task, uint16_t type)
{
    Task *p;

    for (p = task->parent; p && !TaskType_IsA(p->type, type); p = p->parent)
        ;
    return p;
}
HOOK(0x401CC0, Task_FindAncestor);

char TaskType_IsA(uint16_t type, uint16_t base)
{
    do {
        if (type == base)
            return 1;
        type = gTaskTypes[type].parent;
    } while (type != TASK_NO_TYPE);
    return 0;
}
HOOK(0x401CF0, TaskType_IsA);

void TaskEvent_MapSlot(uint8_t slot, int bit)
{
    gTaskEventSlot[bit] = slot;
    gTaskSlotEvent[slot] = bit;
    TaskType_UpdateMasks();
}
HOOK(0x401D30, TaskEvent_MapSlot);

void TaskEvent_UnmapSlot(uint8_t slot)
{
    gTaskEventSlot[(signed char)gTaskSlotEvent[slot]] = 0xFF;
    gTaskSlotEvent[slot] = 0xFF;
    TaskType_UpdateMasks();
}
HOOK(0x401D50, TaskEvent_UnmapSlot);

void Task_Broadcast(Task *task, uint8_t slot, void *arg)
{
    Node *n;

    gTaskStack[gTaskStackTop++] = task;
    if (gTaskHandlers[slot][task->type])
        gTaskHandlers[slot][task->type](task, arg);
    /* skip the children if the handler deleted this task */
    if (task == gTaskStack[gTaskStackTop - 1] && task->children.head->next) {
        n = task->children.head;
        while (n) {
            gTaskStack[gTaskStackTop++] = next_task(n);
            Task_Broadcast((Task *)n, slot, arg);
            n = (Node *)gTaskStack[--gTaskStackTop];
        }
    }
    gTaskStackTop--;
}
HOOK(0x401D80, Task_Broadcast);

void Task_BroadcastChildren(Task *task, uint8_t slot, void *arg)
{
    Node *n;

    for (n = task->children.head; n && n->next; n = n->next)
        Task_Broadcast((Task *)n, slot, arg);
}
HOOK(0x401E30, Task_BroadcastChildren);

void Task_BroadcastType(Task *task, uint8_t slot, uint16_t type, void *arg)
{
    Node *n;

    gTaskStack[gTaskStackTop++] = task;
    if (task->type == type && gTaskHandlers[slot][task->type])
        gTaskHandlers[slot][task->type](task, arg);
    if (task == gTaskStack[gTaskStackTop - 1] && task->children.head->next) {
        n = task->children.head;
        while (n) {
            gTaskStack[gTaskStackTop++] = next_task(n);
            Task_BroadcastType((Task *)n, slot, type, arg);
            n = (Node *)gTaskStack[--gTaskStackTop];
        }
    }
    gTaskStackTop--;
}
HOOK(0x401E70, Task_BroadcastType);

void Task_SetUnk567C60(int value)
{
    gTaskUnk567C60 = value;
}
HOOK(0x401F30, Task_SetUnk567C60);

void Task_ClearPair(int *a, int *b)
{
    *a = 0;
    *b = 0;
}
HOOK(0x401F40, Task_ClearPair);

void Task_SetGlobalFlags(uint32_t flags)
{
    gTaskGlobalFlags |= flags;
}
HOOK(0x401F60, Task_SetGlobalFlags);

void Task_ClearGlobalFlags(uint32_t flags)
{
    gTaskGlobalFlags &= ~flags;
}
HOOK(0x401F80, Task_ClearGlobalFlags);

void TaskType_SetFlags(uint16_t type, uint32_t flags)
{
    gTaskTypes[type].flags = flags | 0x20000000;
}
HOOK(0x401FA0, TaskType_SetFlags);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 544954, 544954, 0x1);
DATA(544954) char str_544955[1] INIT("");
PAD(igd, 544954, 544956, 0x2);
DATA(54F928) uint16_t gTaskRootType;
PAD(igd, 54F928, 54F92A, 0x2);
DATA(54F928) int gTaskUnk54F92C;
STORE(igd, 54F928, 2) __typeof__(gTaskEvents[0]) store_54F930[254];
ALIAS(gTaskEvents, store_54F930, 0);
STORE(igd, 54F928, 1) __typeof__(gTaskEventSlot[0]) store_54FF24[16];
ALIAS(gTaskEventSlot, store_54FF24, 0);
PAD(igd, 54F928, 54FF34, 0xCC);
ALIAS(gTaskHandlerSet, pad_54FF34, 0);
BSS(567C38) Pool gTaskRefs;
PAD(igb, 567C38, 567C50, 0x8);
BSS(567C38) int gTaskCount;
BSS(567C38) uint32_t gTaskGlobalFlags;
BSS(567C38) int gTaskUnk567C60;
PAD(igb, 567C38, 567C64, 0x8);
BSS(567C38) int gTaskSetDepth;
BSS(567C38) int gTaskSetDepth2;
STORE(igb, A969E0, 1) __typeof__(gTaskSlotEvent[0]) store_A969E0[256];
ALIAS(gTaskSlotEvent, store_A969E0, 0);
STORE(igb, A969E0, 4) __typeof__(gTaskHandlers[0]) store_A96AE0[1];
ALIAS(gTaskHandlers, store_A96AE0, 0);
ALIAS(gTaskHandlers1, store_A96AE0, 0);
ALIAS(gWalkHandlers, store_A96AE0, 0);
STORE(igb, AF5EE0, 4) __typeof__(gTaskTypes[0]) store_AF5EE0[384];
ALIAS(gTaskTypes, store_AF5EE0, 0);
STORE(igb, AF5EE0, 4) __typeof__(gTaskStack[0]) store_AFA6E0[64];
ALIAS(gTaskStack, store_AFA6E0, 0);
BSS(AF5EE0) int gTaskStackTop;
