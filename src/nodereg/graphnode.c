/* NodeCriteria flags for graph nodes, and the GraphNode editor task that
   places one node of its parent graph. */
#include "hook.h"
#include "nodereg/nodereg.h"

/* "GraphNode" (0x118), a child of a Graph */
typedef struct GraphNode {
    Task task;
    char unk20[0x18];
    int id;
    char textsLoaded;
    char unk3D[7];
    void *world;
    int unk48;
    void *texts[3];
    char unk58[0x118 - 0x58];
} GraphNode;

_Static_assert(sizeof(GraphNode) == 0x118, "GraphNode");

/* a script constant whose value is one bit of a node's criteria */
typedef struct NodeCriteria {
    char used;
    const char *name;
} NodeCriteria;

#define NODECRITERIA_MAX 32

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4B6F30, void, Text_Free, (void *text));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CEBA0, void *, VirModel_Add, (const char *name));
ORIG(0x4D3710, void, Task_Nothing, (void *task, void *arg));
ORIG(0x4F0FA0, int, Param_Int32, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, int type, int flags, void *extra));
ORIG(0x4F9270, uint8_t, Graph_GetEvent371, (void));
ORIG(0x4F9720, void *, Graph_GetNode, (void *graph, int id));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

extern uint16_t gGraphNodeType;
extern void *gGraphNodeModel;
extern NodeCriteria gNodeCriteria[];
extern void *gGraphNodeParams;

int NodeCriteria_Register(const char *name)
{
    int i;

    for (i = 0; i < NODECRITERIA_MAX; i++) {
        if (!gNodeCriteria[i].used) {
            int bit = 1 << i;

            gNodeCriteria[i].used = 1;
            gNodeCriteria[i].name = name;
            Script_RegisterConstant(name, bit);
            return bit;
        }
    }
    Error_Show("Unable to register NodeCriteria %s", name);
    for (;;)
        ;
}
HOOK(0x5186C0, NodeCriteria_Register);

void NodeCriteria_Unregister(int mask)
{
    int i;

    for (i = 0; i < NODECRITERIA_MAX; i++) {
        if (mask & (1 << i)) {
            Script_UnregisterSymbol(gNodeCriteria[i].name);
            gNodeCriteria[i].used = 0;
            gNodeCriteria[i].name = NULL;
        }
    }
}
HOOK(0x518720, NodeCriteria_Unregister);

void GraphNode_Create(GraphNode *n, int *id);
void GraphNode_Run(GraphNode *n);
void GraphNode_Delete(GraphNode *n);
void GraphNode_ParamsChanged(GraphNode *n);
void GraphNode_GetParamTable(GraphNode *n, ParamTable *out);

void GraphNode_Open(void)
{
    GraphNode p;
    uint16_t t = TaskType_New("GraphNode", sizeof(GraphNode), EditorRigidObj_GetType(), 0, 0);
    int i;

    gGraphNodeType = t;
    TaskType_SetHandler(t, 0, GraphNode_Create);
    TaskType_SetHandler2(gGraphNodeType, 1, GraphNode_Run);
    TaskType_SetHandler2(gGraphNodeType, 2, GraphNode_Delete);
    TaskType_SetHandler(gGraphNodeType, Param_GetChangedEvent(), GraphNode_ParamsChanged);
    TaskType_SetHandler(gGraphNodeType, Param_GetTableEvent(), GraphNode_GetParamTable);
    TaskType_SetHandler(gGraphNodeType, Graph_GetEvent371(), TASK_NOTHING);
    gGraphNodeModel = VirModel_Add("1703_01_1");
    for (i = 0; i < NODECRITERIA_MAX; i++) {
        gNodeCriteria[i].used = 0;
        gNodeCriteria[i].name = NULL;
    }
    gGraphNodeParams = ParamTable_New(1);
    ParamTable_Add(gGraphNodeParams, &p, &p.id, Param_Int32(), 2, NULL);
}
HOOK(0x518760, GraphNode_Open);

void GraphNode_Create(GraphNode *n, int *id)
{
    n->id = *id;
    n->world = gCurrentWorld;
    n->textsLoaded = 0;
}
HOOK(0x518870, GraphNode_Create);

void GraphNode_ParamsChanged(GraphNode *n)
{
    Task_Nothing(n, Graph_GetNode(n->task.parent, n->id));
}
HOOK(0x518890, GraphNode_ParamsChanged);

void GraphNode_GetParamTable(GraphNode *n, ParamTable *out)
{
    *out = gGraphNodeParams;
}
HOOK(0x5188B0, GraphNode_GetParamTable);

void GraphNode_Run(GraphNode *n)
{
    Task_Delete(n);
}
HOOK(0x5188C0, GraphNode_Run);

void GraphNode_Delete(GraphNode *n)
{
    int i;

    if (!n->textsLoaded)
        return;
    for (i = 0; i < 3; i++)
        if (n->texts[i])
            Text_Free(n->texts[i]);
}
HOOK(0x5188D0, GraphNode_Delete);

void GraphNode_Close(void)
{
    QTaskList_Free(gGraphNodeParams);
    gGraphNodeParams = NULL;
    TaskType_Free(gGraphNodeType);
}
HOOK(0x518900, GraphNode_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54EC9C) uint16_t gGraphNodeType INIT(385);
PAD(igd, 54EC9C, 54EC9E, 0x2);
BSS(A800F0) void *gGraphNodeModel;
PAD(igb, A800F0, A800F4, 0x4);
PAD(igb, A800F0, A800F8, 0x104);
ALIAS(gNodeCriteria, pad_A800F8, 0);
BSS(A800F0) void *gGraphNodeParams;
