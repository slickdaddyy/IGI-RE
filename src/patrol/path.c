/* PatrolPath tasks hold a list of PatrolPathCommand children; a PatrolRoute
   walks through them for an AI. */
#include "hook.h"
#include "patrol/patrol.h"

typedef struct PatrolCommand {
    Task task;
    int type;
    int arg;
} PatrolCommand;

typedef struct PatrolPath {
    Task task;
    Node node;
    int id;
    int count;
    char listed;
    char unk31[3];
} PatrolPath;

/* where an AI is in a patrol path; 7 words */
typedef struct PatrolRoute {
    PatrolPath *path;
    PatrolCommand *cur;
    PatrolCommand *end;
    PatrolCommand *move;
    PatrolCommand *prevMove;
    char unk14;
    char unk15;
    char unk16[2];
    int unk18;
} PatrolRoute;

typedef struct PatrolCommandInfo {
    const char *help;
    const char *name;
} PatrolCommandInfo;

enum {
    PATROL_NONE = -1,
    PATROL_ANIMATION,
    PATROL_DELAY,
    PATROL_WALKTO,
    PATROL_RUNTO,
    PATROL_CROUCH,
    PATROL_LOOKATNODE,
    PATROL_END,
    PATROL_QUIT,
    PATROL_SETSPEED,
    PATROL_COUNT,
};

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1780, void, Debug_Log, (void *task, const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));

extern uint16_t gPatrolCommandType;
extern const PatrolCommandInfo gPatrolCommandInfo[];
extern uint16_t gPatrolPathType;
extern const char *gPatrolCommandNames[];
extern void *gPatrolCommandParams;
extern List gPatrolPaths;
extern void *gPatrolPathParams;

void PatrolPathCommand_Create(PatrolCommand *c);
void PatrolPathCommand_GetParams(PatrolCommand *c, void **out);
void PatrolPathCommand_Changed(PatrolCommand *c);
void PatrolPath_Create(PatrolPath *p);
void PatrolPath_Delete(PatrolPath *p);
void PatrolPath_GetParams(PatrolPath *p, void **out);
void PatrolPath_Changed(PatrolPath *p);

void PatrolPathCommand_Open(void)
{
    PatrolCommand tmp;

    gPatrolCommandType = TaskType_New("PatrolPathCommand", sizeof(PatrolCommand), Task_GetRootType(), 0);
    TaskType_SetHandler(gPatrolCommandType, 0, PatrolPathCommand_Create);
    TaskType_SetHandler(gPatrolCommandType, Param_GetTableEvent(), PatrolPathCommand_GetParams);
    TaskType_SetHandler(gPatrolCommandType, Param_GetChangedEvent(), PatrolPathCommand_Changed);
    gPatrolCommandParams = ParamTable_New(2);
    ParamTable_Add(gPatrolCommandParams, &tmp, &tmp.type, Param_Int32(), 7, NULL);
    ParamTable_Add(gPatrolCommandParams, &tmp, &tmp.arg, Param_Int32(), 7, NULL);
}
HOOK(0x4533B0, PatrolPathCommand_Open);

void PatrolPathCommand_Create(PatrolCommand *c)
{
    c->type = PATROL_NONE;
    c->arg = -1;
}
HOOK(0x453470, PatrolPathCommand_Create);

void PatrolPathCommand_GetParams(PatrolCommand *c, void **out)
{
    *out = gPatrolCommandParams;
}
HOOK(0x453480, PatrolPathCommand_GetParams);

void PatrolPathCommand_SetName(PatrolCommand *c);

void PatrolPathCommand_Changed(PatrolCommand *c)
{
    if (c->type < PATROL_NONE || c->type >= PATROL_COUNT)
        c->type = PATROL_NONE;
    PatrolPathCommand_SetName(c);
}
HOOK(0x453490, PatrolPathCommand_Changed);

void PatrolPathCommand_SetName(PatrolCommand *c)
{
    char buf[1024];
    int t = c->type;

    if (t == PATROL_NONE || t == PATROL_END || t == PATROL_QUIT || t == PATROL_CROUCH)
        crt_sprintf(buf, gPatrolCommandInfo[t].help);
    else
        crt_sprintf(buf, gPatrolCommandInfo[t].name, c->arg);
    Debug_Log(c, buf);
}
HOOK(0x4534B0, PatrolPathCommand_SetName);

void PatrolPathCommand_Close(void)
{
    TaskType_Free(gPatrolCommandType);
    QTaskList_Free(gPatrolCommandParams);
}
HOOK(0x453520, PatrolPathCommand_Close);

int PatrolPathCommand_GetType(PatrolCommand *c)
{
    return c->type;
}
HOOK(0x453540, PatrolPathCommand_GetType);

void PatrolPath_Open(void)
{
    int i;

    gPatrolPaths.head = (Node *)&gPatrolPaths.tail;
    gPatrolPaths.tailpred = (Node *)&gPatrolPaths.head;
    gPatrolPaths.tail = NULL;
    gPatrolPathType = TaskType_New("PatrolPath", sizeof(PatrolPath), Task_GetRootType(), 0);
    TaskType_SetHandler(gPatrolPathType, 0, PatrolPath_Create);
    TaskType_SetHandler2(gPatrolPathType, 2, PatrolPath_Delete);
    TaskType_SetHandler(gPatrolPathType, Param_GetTableEvent(), PatrolPath_GetParams);
    TaskType_SetHandler(gPatrolPathType, Param_GetChangedEvent(), PatrolPath_Changed);
    gPatrolPathParams = NULL;
    for (i = 0; i < PATROL_COUNT + 1; i++)
        Script_RegisterConstant(gPatrolCommandNames[i], i - 1);
}
HOOK(0x453550, PatrolPath_Open);

void PatrolPath_Create(PatrolPath *p)
{
    p->id = -1;
    p->listed = 0;
    p->node.prev = NULL;
    p->node.next = NULL;
}
HOOK(0x453630, PatrolPath_Create);

void PatrolPath_Delete(PatrolPath *p)
{
    if (p->listed)
        List_Remove(&p->node);
}
HOOK(0x453650, PatrolPath_Delete);

void PatrolPath_GetParams(PatrolPath *p, void **out)
{
    *out = gPatrolPathParams;
}
HOOK(0x453670, PatrolPath_GetParams);

int PatrolPath_CountCommands(PatrolPath *p);

void PatrolPath_Changed(PatrolPath *p)
{
    int id = Task_GetId(p);

    p->count = PatrolPath_CountCommands(p);
    if (id == -1)
        return;
    if (p->listed) {
        List_Remove(&p->node);
        p->node.next = NULL;
        p->node.prev = NULL;
    }
    p->id = id;
    List_AddTail(&gPatrolPaths, &p->node);
    p->listed = 1;
}
HOOK(0x453680, PatrolPath_Changed);

int PatrolPath_CountCommands(PatrolPath *p)
{
    Task *c;
    int n = 0;

    for (c = p->task.head; c->next; c = c->next)
        n++;
    return n;
}
HOOK(0x4536E0, PatrolPath_CountCommands);

void PatrolPath_Close(void)
{
    TaskType_Free(gPatrolPathType);
}
HOOK(0x453710, PatrolPath_Close);

PatrolCommand *PatrolRoute_Current(PatrolRoute *r)
{
    return r->cur;
}
HOOK(0x453720, PatrolRoute_Current);

/* the command after the current one; past the last one it goes back to
   the END command, or to the first command */
PatrolCommand *PatrolRoute_Next(PatrolRoute *r)
{
    Task *n = r->cur->task.next;
    Task *first;

    if (n && n->next)
        return (PatrolCommand *)n;
    if (r->end)
        return r->end;
    first = r->path->task.head;
    return first->next ? (PatrolCommand *)first : NULL;
}
HOOK(0x453730, PatrolRoute_Next);

PatrolCommand *PatrolRoute_Advance(PatrolRoute *r)
{
    PatrolCommand *c = PatrolRoute_Next(r);
    PatrolCommand *n;
    int type;

    if (!c)
        return NULL;
    n = c;
    type = PatrolPathCommand_GetType(c);
    if (type == PATROL_END) {
        if (c == r->end)
            return NULL;
        r->cur = c;
        r->end = c;
        n = PatrolRoute_Next(r);
        if (n == c || !n)
            return NULL;
        type = PatrolPathCommand_GetType(n);
    }
    if (type == PATROL_WALKTO || type == PATROL_RUNTO) {
        r->prevMove = r->move;
        r->move = n;
    } else if (type == PATROL_END) {
        Error_Show("END command following END command");
        for (;;);
    }
    r->cur = n;
    return n;
}
HOOK(0x453760, PatrolRoute_Advance);

PatrolPath *PatrolPath_Find(int id);
PatrolCommand *PatrolRoute_Seek(PatrolRoute *r, int n);

void PatrolRoute_Init(PatrolRoute *r, int id, int start)
{
    r->path = PatrolPath_Find(id);
    r->end = NULL;
    r->prevMove = NULL;
    r->unk15 = 1;
    r->unk14 = 0;
    r->unk18 = -1;
    r->cur = PatrolRoute_Seek(r, start);
    if (PatrolPathCommand_GetType(r->cur) == PATROL_END)
        r->cur = PatrolRoute_Next(r);
}
HOOK(0x4537E0, PatrolRoute_Init);

PatrolPath *PatrolPath_FromNode(Node *n);

PatrolPath *PatrolPath_Find(int id)
{
    Node *n;

    for (n = gPatrolPaths.head; n->next; n = n->next) {
        PatrolPath *p = PatrolPath_FromNode(n);

        if (p->id == id)
            return p;
    }
    return NULL;
}
HOOK(0x453840, PatrolPath_Find);

PatrolPath *PatrolPath_FromNode(Node *n)
{
    return (PatrolPath *)((char *)n - offsetof(PatrolPath, node));
}
HOOK(0x453870, PatrolPath_FromNode);

/* skips n commands, remembering the last move and END commands passed */
PatrolCommand *PatrolRoute_Seek(PatrolRoute *r, int n)
{
    Task *c = r->path->task.head;
    int i;

    if (!c->next)
        c = NULL;
    for (i = 0; c && i < n; i++) {
        int type = PatrolPathCommand_GetType((PatrolCommand *)c);

        if (type == PATROL_WALKTO || type == PATROL_RUNTO)
            r->move = (PatrolCommand *)c;
        else if (type == PATROL_END)
            r->end = (PatrolCommand *)c;
        c = c->next;
        if (c && !c->next)
            c = NULL;
    }
    return (PatrolCommand *)c;
}
HOOK(0x453880, PatrolRoute_Seek);
