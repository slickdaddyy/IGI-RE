#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

/* the parts of the Graph base task used here */
typedef struct Graph {
    char unk0[0x58];
    int capacity;       /* node ids are below this */
    char unk5C[0x20];
} Graph;

typedef struct GraphNode {
    int id;
    int unk4;
    Vec3d pos;
    float gamma;
    float radius;
    float diameter;
    int unk2C;
    int flags;          /* NODECRITERIA_* bits */
} GraphNode;

typedef struct GraphStream GraphStream;

typedef struct GraphStreamValue {
    GraphStream *stream;
    Graph *graph;
    int unk8;
    int unkC;
    int type;
} GraphStreamValue;

/* AIGraph: the graph task the AI walks on, with a route table that gives
   the next node and the distance from any node to any other. */
typedef struct AIGraph {
    Graph graph;
    double unk80;       /* params; 80 and 88 over 100 are scaled by 1/4096 */
    double unk88;
    double unk90;
    double unk98;
    double unkA0;
    double unkA8;
    int unkB0;
    int unkB4;
    struct GraphStep *routes;   /* capacity * capacity, indexed [to][from] */
    char unkBC;
} AIGraph;

typedef struct GraphStep {
    int next;           /* -1: no route */
    float dist;
} GraphStep;

_Static_assert(sizeof(AIGraph) == 0xC0, "AIGraph size");

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1240, int, Level_CountTaskIds, (void *task));
ORIG(0x4F1090, void, Level_SetTaskId, (int id, void *task));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *table, int extra));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F0F80, void *, Param_Real64, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F90B0, void, Graph_Create, (Graph *g, void *arg));
ORIG(0x4F9180, void, Graph_ParamsChanged, (Graph *g, void *arg));
ORIG(0x4F9200, void *, Graph_GetParamTable, (void));
ORIG(0x4F9210, uint16_t, Graph_GetType, (void));
ORIG(0x4F9220, uint8_t, Graph_GetLoadEvent, (void));
ORIG(0x4F9230, uint8_t, Graph_GetSaveEvent, (void));
ORIG(0x4F9240, uint8_t, Graph_GetNodeLoadEvent, (void));
ORIG(0x4F9250, uint8_t, sub_4F9250, (void));
ORIG(0x4F9260, uint8_t, Graph_GetNodeAddEvent, (void));
ORIG(0x4F9280, uint8_t, Graph_GetAllocEvent, (void));
ORIG(0x4F9460, void, Graph_SetNodeSize, (uint16_t type, int size));
ORIG(0x4F9720, GraphNode *, Graph_GetNode, (Graph *g, int id));
ORIG(0x4F9770, void, Graph_GetNodePos, (Vec3d *out, GraphNode *node));
ORIG(0x4F9CD0, void, GraphStream_ReadValue, (GraphStreamValue *v, void *out, int size));
ORIG(0x4F9DF0, void, GraphStream_AddHandler, (const char *name, int type, void (*fn)(GraphStreamValue *v)));
ORIG(0x4F9EB0, void, GraphStream_WriteValue, (GraphStream *s, const char *name, int type, const void *value, int size));
ORIG(0x4FA850, char, GraphStream_ReadInt, (GraphStream *s, int *out));
ORIG(0x4FA9D0, char, GraphStream_ReadBlob, (GraphStream *s, void *out, int size));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x5186C0, int, NodeCriteria_Register, (const char *name));
ORIG(0x518720, void, NodeCriteria_Unregister, (int mask));
ORIG(0x518550, void, sub_518550, (void));

#define gAIGraphType VAR(uint16_t, 0x54E83C)
#define gAIGraphEvent VAR(uint8_t, 0x54E83E)
/* min and max of two params */
#define gRangeA8 ((double *)0xA7B2A8)
#define gRange98 ((double *)0xA7B2B8)
#define gAIGraphParams VAR(void *, 0xA7B2CC)
#define gCriteriaDoor VAR(int, 0xA7B2D0)
#define gCriteriaView VAR(int, 0xA7B2D4)
#define gCriteriaStair VAR(int, 0xA7B2D8)
#define gTaskHandlers ((void (**)(void *))0xA96AE0)
#define TASK_HANDLER(slot, type) gTaskHandlers[(slot) * 0x180 + (type)]

#define ROUTE_TABLE_TYPE 10
#define AIGRAPH_NODE_SIZE 0x240

static const double kParamUnitLimit = 100.0;
static const double kParamUnitScale = 1.0 / 4096.0;
static const float kRadiusScale = 4096.0f;
static const double kNearMax = 40960000.0;
static const double kZero = 0.0;

void AIGraph_Shutdown(void);
void AIGraph_ReadRouteTable(GraphStreamValue *v);
void AIGraph_AllocRoutes(AIGraph *g);
void AIGraph_ClearRoutes(AIGraph *g);
GraphStep *AIGraph_GetRoute(AIGraph *g, int from, int to);
void AIGraph_OnAlloc(AIGraph *g);
void AIGraph_ParamsChanged(AIGraph *g, void *arg);
void AIGraph_GetParamTable(void *task, void **out);
void AIGraph_LoadRoutes(AIGraph *g, GraphStream **s);
void AIGraph_ReadNode(void *task, GraphStream **arg);
void AIGraph_SaveRoutes(AIGraph *g, GraphStream **s);
void AIGraph_Create(AIGraph *g, void *arg);
void AIGraph_Delete(AIGraph *g);
int AIGraph_GetViewCriteria(void);

void sub_508F90(void)
{
    AIGraph_Shutdown();
    sub_518550();
}
HOOK(0x508F90, sub_508F90);

void AIGraph_Init(void)
{
    AIGraph p;
    uint16_t type;

    type = TaskType_New("AIGraph", sizeof(AIGraph), Graph_GetType(), 0, 0);
    gAIGraphType = type;
    TaskType_SetHandler(type, 0, AIGraph_Create);
    TaskType_SetHandler2(gAIGraphType, 1, Task_Nothing);
    TaskType_SetHandler2(gAIGraphType, 2, AIGraph_Delete);
    TaskType_SetHandler(gAIGraphType, Graph_GetAllocEvent(), AIGraph_OnAlloc);
    TaskType_SetHandler(gAIGraphType, Graph_GetSaveEvent(), AIGraph_SaveRoutes);
    TaskType_SetHandler(gAIGraphType, sub_4F9250(), Task_Nothing);
    TaskType_SetHandler(gAIGraphType, Graph_GetNodeAddEvent(), Task_Nothing);
    TaskType_SetHandler(gAIGraphType, Graph_GetLoadEvent(), AIGraph_LoadRoutes);
    TaskType_SetHandler(gAIGraphType, Graph_GetNodeLoadEvent(), AIGraph_ReadNode);
    TaskType_SetHandler(gAIGraphType, Param_GetChangedEvent(), AIGraph_ParamsChanged);
    TaskType_SetHandler(gAIGraphType, Param_GetTableEvent(), AIGraph_GetParamTable);
    gRange98[1] = 20.0;
    gRange98[0] = 0.3;
    gRangeA8[1] = 1.0;
    gRangeA8[0] = 0.01;
    gAIGraphParams = ParamTable_Extend(Graph_GetParamTable(), 7);
    ParamTable_Add(gAIGraphParams, &p, &p.unk80, Param_Real64(), 7, NULL);
    ParamTable_Add(gAIGraphParams, &p, &p.unk88, Param_Real64(), 5, NULL);
    ParamTable_Add(gAIGraphParams, &p, &p.unk90, Param_Real64(), 7, NULL);
    ParamTable_Add(gAIGraphParams, &p, &p.unk98, Param_Real64(), 7, gRange98);
    ParamTable_Add(gAIGraphParams, &p, &p.unkA0, Param_Real64(), 7, NULL);
    ParamTable_Add(gAIGraphParams, &p, &p.unkBC, Param_Bool8(), 7, NULL);
    ParamTable_Add(gAIGraphParams, &p, &p.unkA8, Param_Real64(), 7, gRangeA8);
    Graph_SetNodeSize(gAIGraphType, AIGRAPH_NODE_SIZE);
    gCriteriaDoor = NodeCriteria_Register("NODECRITERIA_DOOR");
    gCriteriaView = NodeCriteria_Register("NODECRITERIA_VIEW");
    gCriteriaStair = NodeCriteria_Register("NODECRITERIA_STAIR");
    GraphStream_AddHandler("RouteTable", ROUTE_TABLE_TYPE, AIGraph_ReadRouteTable);
}
HOOK(0x508FA0, AIGraph_Init);

void AIGraph_ReadRouteTable(GraphStreamValue *v)
{
    AIGraph *g = (AIGraph *)v->graph;

    AIGraph_AllocRoutes(g);
    if (g->routes)
        GraphStream_ReadValue(v, g->routes, g->graph.capacity * g->graph.capacity * sizeof(GraphStep));
}
HOOK(0x5092A0, AIGraph_ReadRouteTable);

void AIGraph_AllocRoutes(AIGraph *g)
{
    int n;

    Mem_PushMode(1);
    if (g->routes) {
        Mem_Free(g->routes);
        g->routes = NULL;
    }
    n = g->graph.capacity;
    if (n > 1) {
        g->routes = Mem_Alloc(n * n * sizeof(GraphStep), 4);
        AIGraph_ClearRoutes(g);
    }
    Mem_PopMode();
}
HOOK(0x5092E0, AIGraph_AllocRoutes);

void AIGraph_ClearRoutes(AIGraph *g)
{
    int i, j;

    for (i = 0; i < g->graph.capacity; i++) {
        for (j = 0; j < g->graph.capacity; j++) {
            GraphStep *s = AIGraph_GetRoute(g, i, j);

            s->next = -1;
            s->dist = -1.0f;
        }
    }
}
HOOK(0x509340, AIGraph_ClearRoutes);

GraphStep *AIGraph_GetRoute(AIGraph *g, int from, int to)
{
    return &g->routes[g->graph.capacity * to + from];
}
HOOK(0x509390, AIGraph_GetRoute);

void AIGraph_OnAlloc(AIGraph *g)
{
    AIGraph_AllocRoutes(g);
}
HOOK(0x5093B0, AIGraph_OnAlloc);

void AIGraph_ParamsChanged(AIGraph *g, void *arg)
{
    if (Task_GetId(g) == -1)
        Level_SetTaskId(Level_CountTaskIds(g), g);
    if (g->unk80 > kParamUnitLimit)
        g->unk80 = g->unk80 * kParamUnitScale;
    if (g->unk88 > kParamUnitLimit)
        g->unk88 = g->unk88 * kParamUnitScale;
    Graph_ParamsChanged(&g->graph, arg);
}
HOOK(0x5093C0, AIGraph_ParamsChanged);

void AIGraph_GetParamTable(void *task, void **out)
{
    *out = gAIGraphParams;
}
HOOK(0x509440, AIGraph_GetParamTable);

void AIGraph_LoadRoutes(AIGraph *g, GraphStream **s)
{
    GraphStream_ReadBlob(*s, g->routes, g->graph.capacity * g->graph.capacity * sizeof(GraphStep));
}
HOOK(0x509450, AIGraph_LoadRoutes);

/* skips the two link lists an old node record carries */
void AIGraph_ReadNode(void *task, GraphStream **arg)
{
    int count, unused, i;

    GraphStream_ReadInt(arg[1], &count);
    for (i = 0; i < count; i++) {
        GraphStream_ReadInt(arg[1], &unused);
        GraphStream_ReadInt(arg[1], &unused);
    }
    GraphStream_ReadInt(arg[1], &count);
    for (i = 0; i < count; i++) {
        GraphStream_ReadInt(arg[1], &unused);
        GraphStream_ReadInt(arg[1], &unused);
    }
}
HOOK(0x509480, AIGraph_ReadNode);

void AIGraph_SaveRoutes(AIGraph *g, GraphStream **s)
{
    if (!g->routes)
        AIGraph_AllocRoutes(g);
    GraphStream_WriteValue(*s, "RouteTable", ROUTE_TABLE_TYPE, g->routes,
                           g->graph.capacity * g->graph.capacity * sizeof(GraphStep));
}
HOOK(0x509520, AIGraph_SaveRoutes);

void AIGraph_Create(AIGraph *g, void *arg)
{
    Graph_Create(&g->graph, arg);
    g->unk80 = 1.0;
    g->unkA0 = 1.0;
    g->unk88 = 2.0;
    g->unk90 = 3.0;
    g->unk98 = 0.3;
    g->unkA8 = 0.05;
    g->unkBC = 0;
    g->unkB0 = -1;
    g->unkB4 = -1;
    g->routes = NULL;
}
HOOK(0x509570, AIGraph_Create);

void AIGraph_Delete(AIGraph *g)
{
    if (g->routes) {
        Mem_Free(g->routes);
        g->routes = NULL;
    }
    TASK_HANDLER(2, Graph_GetType())(g);
}
HOOK(0x509610, AIGraph_Delete);

void AIGraph_Shutdown(void)
{
    NodeCriteria_Unregister(gCriteriaStair | gCriteriaView | gCriteriaDoor);
    QTaskList_Free(gAIGraphParams);
    if (gAIGraphEvent != 0xFF)
        TaskEvent_Free(gAIGraphEvent);
    TaskType_Free(gAIGraphType);
}
HOOK(0x509650, AIGraph_Shutdown);

uint8_t AIGraph_GetEvent(void)
{
    if (gAIGraphEvent == 0xFF)
        gAIGraphEvent = TaskEvent_Alloc(0);
    return gAIGraphEvent;
}
HOOK(0x5096A0, AIGraph_GetEvent);

GraphStep *AIGraph_GetStep(AIGraph *g, int from, int to)
{
    GraphStep *s;

    if (!g->routes)
        return NULL;
    s = AIGraph_GetRoute(g, from, to);
    return s->next != -1 ? s : NULL;
}
HOOK(0x5096C0, AIGraph_GetStep);

/* Finds the node closest to pos, skipping view nodes. Returns 0 when there
   is none, 2 when pos is inside the closest node's radius, else 1. */
int AIGraph_FindNearest(GraphNode **out, AIGraph *g, const Vec3d *pos)
{
    GraphNode *best[3] = { NULL, NULL, NULL };
    double dist[3] = { kNearMax, kNearMax, kNearMax };
    Vec3d p;
    int i, k, j;

    *out = NULL;
    if (g->graph.capacity <= 0)
        return 0;
    for (i = 0; i < g->graph.capacity; i++) {
        GraphNode *node = Graph_GetNode(&g->graph, i);
        long double d;

        if (!node || (node->flags & AIGraph_GetViewCriteria()))
            continue;
        Graph_GetNodePos(&p, node);
        d = Vec3d_Distance(&p, pos) - node->radius * kRadiusScale;
        for (k = 0; k < 3; k++) {
            if (!best[k])
                break;
            if (!(d >= dist[k])) {
                /* a forward copy, so both later slots get slot k */
                for (j = k; j < 2; j++) {
                    best[j + 1] = best[j];
                    memcpy(&dist[j + 1], &dist[j], sizeof(double));
                }
                break;
            }
        }
        if (k < 3) {
            dist[k] = d;
            best[k] = node;
        }
    }
    if (!best[0])
        return 0;
    Graph_GetNodePos(&p, best[0]);
    *out = best[0];
    return !(dist[0] >= kZero) ? 2 : 1;
}
HOOK(0x5096F0, AIGraph_FindNearest);

int AIGraph_GetDoorCriteria(void)
{
    return gCriteriaDoor;
}
HOOK(0x509860, AIGraph_GetDoorCriteria);

int AIGraph_GetViewCriteria(void)
{
    return gCriteriaView;
}
HOOK(0x509870, AIGraph_GetViewCriteria);

int AIGraph_GetStairCriteria(void)
{
    return gCriteriaStair;
}
HOOK(0x509880, AIGraph_GetStairCriteria);

char GraphNode_HasCriteria(GraphNode *node, int mask)
{
    return (node->flags & mask) != 0;
}
HOOK(0x509890, GraphNode_HasCriteria);

uint16_t AIGraph_GetType(void)
{
    return gAIGraphType;
}
HOOK(0x5098A0, AIGraph_GetType);
