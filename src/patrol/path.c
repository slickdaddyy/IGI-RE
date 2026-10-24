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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(str_53E020);
REF_EXTERN(str_53E03C);
REF_EXTERN(str_53E06C);
REF_EXTERN(str_53E09C);
REF_EXTERN(str_53E0E0);
REF_EXTERN(str_53E0F4);
REF_EXTERN(str_53E120);
REF_EXTERN(str_53E13C);
REF_EXTERN(str_53E150);
REF_EXTERN(str_53E17C);
REF_EXTERN(str_53E190);
REF_EXTERN(str_53E1C0);
REF_EXTERN(str_53E1EC);
REF_EXTERN(str_53E220);
REF_EXTERN(str_53E240);
REF_EXTERN(str_53E2C8);
REF_EXTERN(str_53E2E4);
REF_EXTERN(str_53E2FC);
REF_EXTERN(str_53E314);
REF_EXTERN(str_53E334);
REF_EXTERN(str_53E350);
REF_EXTERN(str_53E368);
REF_EXTERN(str_53E384);
REF_EXTERN(str_53E39C);
REF_EXTERN(str_53E3B8);
DATA(53DFCC) uint16_t gPatrolCommandType INIT(385);
PAD(igd, 53DFCC, 53DFCE, 0x2);
STORE(igd, 53DFD8, 4) __typeof__(gPatrolCommandInfo[0]) store_53DFD8[9] INIT({{.help = (__typeof__(store_53DFD8[0].help))ref_str_53E240, .name = (__typeof__(store_53DFD8[0].name))ref_str_53E220}, {.help = (__typeof__(store_53DFD8[1].help))ref_str_53E1EC, .name = (__typeof__(store_53DFD8[1].name))ref_str_53E1C0}, {.help = (__typeof__(store_53DFD8[2].help))ref_str_53E190, .name = (__typeof__(store_53DFD8[2].name))ref_str_53E17C}, {.help = (__typeof__(store_53DFD8[3].help))ref_str_53E150, .name = (__typeof__(store_53DFD8[3].name))ref_str_53E13C}, {.help = (__typeof__(store_53DFD8[4].help))ref_str_53E120, .name = (__typeof__(store_53DFD8[4].name))ref_str_53E120}, {.help = (__typeof__(store_53DFD8[5].help))ref_str_53E0F4, .name = (__typeof__(store_53DFD8[5].name))ref_str_53E0E0}, {.help = (__typeof__(store_53DFD8[6].help))ref_str_53E09C, .name = (__typeof__(store_53DFD8[6].name))ref_str_53E09C}, {.help = (__typeof__(store_53DFD8[7].help))ref_str_53E06C, .name = (__typeof__(store_53DFD8[7].name))ref_str_53E06C}, {.help = (__typeof__(store_53DFD8[8].help))ref_str_53E03C, .name = (__typeof__(store_53DFD8[8].name))ref_str_53E020}});
ALIAS(gPatrolCommandInfo, store_53DFD8, 0);
DATA(53E020) char str_53E020[28] INIT("Set travel speed to %d km/h");
DATA(53E020) char str_53E03C[45] INIT("Set travel speed. Parameter is speed in km/h");
PAD(igd, 53E020, 53E069, 0x3);
DATA(53E020) char str_53E06C[47] INIT("Quit script, stops script. Takes no parameters");
PAD(igd, 53E020, 53E09B, 0x1);
DATA(53E020) char str_53E09C[67] INIT("End script, only runs commands after this one. Takes no parameters");
PAD(igd, 53E020, 53E0DF, 0x1);
DATA(53E020) char str_53E0E0[20] INIT("Looks at node id %d");
DATA(53E020) char str_53E0F4[44] INIT("Look at specific node, parameter is node id");
DATA(53E020) char str_53E120[28] INIT("Crouch. Takes no parameters");
DATA(53E020) char str_53E13C[19] INIT("Runs to node id %d");
PAD(igd, 53E020, 53E14F, 0x1);
DATA(53E020) char str_53E150[44] INIT("Runs to specific node, parameter is node id");
DATA(53E020) char str_53E17C[20] INIT("Walks to node id %d");
DATA(53E020) char str_53E190[47] INIT("Walks to a specific node, parameter is node id");
PAD(igd, 53E020, 53E1BF, 0x1);
DATA(53E020) char str_53E1C0[41] INIT("Delays the script execution for %d ticks");
PAD(igd, 53E020, 53E1E9, 0x3);
DATA(53E020) char str_53E1EC[51] INIT("Delays the script execution, parameter is in ticks");
PAD(igd, 53E020, 53E21F, 0x1);
DATA(53E020) char str_53E220[30] INIT("Plays predefined animation %d");
PAD(igd, 53E020, 53E23E, 0x2);
DATA(53E020) char str_53E240[52] INIT("Play animation, parameter is predefine animation id");
DATA(53E020) char str_53E274[19] INIT("No command entered");
PAD(igd, 53E020, 53E287, 0x15) INIT({0x00, 0x50, 0x61, 0x74, 0x72, 0x6F, 0x6C, 0x50, 0x61, 0x74, 0x68, 0x43, 0x6F, 0x6D, 0x6D, 0x61, 0x6E, 0x64});
DATA(53E020) uint16_t gPatrolPathType INIT(385);
PAD(igd, 53E020, 53E29E, 0x2);
STORE(igd, 53E020, 4) const char *store_53E2A0[10] INIT({(__typeof__(store_53E2A0[0]))ref_str_53E3B8, (__typeof__(store_53E2A0[1]))ref_str_53E39C, (__typeof__(store_53E2A0[2]))ref_str_53E384, (__typeof__(store_53E2A0[3]))ref_str_53E368, (__typeof__(store_53E2A0[4]))ref_str_53E350, (__typeof__(store_53E2A0[5]))ref_str_53E334, (__typeof__(store_53E2A0[6]))ref_str_53E314, (__typeof__(store_53E2A0[7]))ref_str_53E2FC, (__typeof__(store_53E2A0[8]))ref_str_53E2E4, (__typeof__(store_53E2A0[9]))ref_str_53E2C8});
ALIAS(gPatrolCommandNames, store_53E2A0, 0);
DATA(53E020) char str_53E2C8[27] INIT("PATROLPATHCOMMAND_SETSPEED");
PAD(igd, 53E020, 53E2E3, 0x1);
DATA(53E020) char str_53E2E4[23] INIT("PATROLPATHCOMMAND_QUIT");
PAD(igd, 53E020, 53E2FB, 0x1);
DATA(53E020) char str_53E2FC[22] INIT("PATROLPATHCOMMAND_END");
PAD(igd, 53E020, 53E312, 0x2);
DATA(53E020) char str_53E314[29] INIT("PATROLPATHCOMMAND_LOOKATNODE");
PAD(igd, 53E020, 53E331, 0x3);
DATA(53E020) char str_53E334[25] INIT("PATROLPATHCOMMAND_CROUCH");
PAD(igd, 53E020, 53E34D, 0x3);
DATA(53E020) char str_53E350[24] INIT("PATROLPATHCOMMAND_RUNTO");
DATA(53E020) char str_53E368[25] INIT("PATROLPATHCOMMAND_WALKTO");
PAD(igd, 53E020, 53E381, 0x3);
DATA(53E020) char str_53E384[24] INIT("PATROLPATHCOMMAND_DELAY");
DATA(53E020) char str_53E39C[28] INIT("PATROLPATHCOMMAND_ANIMATION");
DATA(53E020) char str_53E3B8[23] INIT("PATROLPATHCOMMAND_NONE");
PAD(igd, 53E020, 53E3CF, 0x1);
BSS(5BDA68) void *gPatrolCommandParams;
PAD(igb, 5BDA68, 5BDA6C, 0x4);
BSS(5BDA68) List gPatrolPaths;
BSS(5BDA68) void *gPatrolPathParams;
