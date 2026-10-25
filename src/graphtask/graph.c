/* The "Graph" task type: AI path graphs loaded from MISSION:graphs/graph%d.dat */
#include "hook.h"
#include "igi.h"
#include "graphtask/graphtask.h"
#include "graphnode/graphnodetypes.h"
#include "param/paramtypes.h"
#include "graphtask/graphtaskvars.h"
#include "graphnode/graphnodevars.h"
#include "nodereg/noderegvars.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, Node *node));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B16D0, int, File_Read, (void *file, void *buf, int size));
ORIG(0x4B1730, int, File_Seek, (void *file, int offset, int whence));
ORIG(0x4B17C0, void, File_Flush, (void *file));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int index));
ORIG(0x4C1780, void, Debug_Log, (void));
ORIG(0x4C1830, void, QTaskList_Free, (void *table));
ORIG(0x4CE400, void *, Terrain_GetMaterial, (void *terrain, int id));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1020, void *, Param_Graph, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1B60, int, ParamOp_Parse, (void));
ORIG(0x4F1BC0, void, ParamType_SetOp, (void *type, int op, void *fn));
ORIG(0x4F90B0, void, Graph_Create, (Graph *g));
ORIG(0x4F9160, void, Graph_Delete, (Graph *g));
ORIG(0x4F9180, void, Graph_ParamsChanged, (Graph *g));
ORIG(0x4F9460, void, Graph_SetNodeSize, (uint16_t type, int size));
ORIG(0x4F94D0, int, Graph_GetNodeSize, (Graph *g));
ORIG(0x4F9510, GraphNode *, Graph_AddNode, (Graph *g, int id, const Vec3d *pos, float angle, float radius,
                                            void *material, const char *criteria, int unk));
ORIG(0x4F96E0, void, Graph_ClearNodes, (Graph *g, int count));
ORIG(0x4F9720, GraphNode *, Graph_GetNode, (Graph *g, int id));
ORIG(0x4F9B80, GraphLink *, Graph_AddLink, (Graph *g, int a, int b, int cost));
ORIG(0x4F9C50, void, GraphLink_Free, (GraphLink *link));
ORIG(0x4F9CD0, void, GraphStream_ReadValue, (GraphStreamValue *v, void *out, int size));
ORIG(0x4F9DF0, void, GraphStream_AddHandler, (const char *name, int type, void (*fn)(GraphStreamValue *v)));
ORIG(0x4F9FF0, void, GraphOpen, (Graph *g, const char *path));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

extern uint8_t gGraphEvents[];
extern int gGraphLinkCost;
extern GraphStreamHandler gGraphHandlerSlots[];
extern int gGraphLinkB;
extern int gGraphLinkA;
extern GraphNode gGraphNodeTemp;

/* the next byte of the file, which stays unread. When nothing is read
   the result is the low byte of the stream pointer. */
int GraphStream_PeekByte(GraphStream *s)
{
    uintptr_t b = (uintptr_t)s;

    File_Flush(s->file);
    File_Read(s->file, &b, 1);
    File_Flush(s->file);
    File_Seek(s->file, -1, 1);
    return b & 0xFF;
}
HOOK(0x4FAA10, GraphStream_PeekByte);

void Graph_PreDelete(Graph *g);
void Graph_GetParams(Graph *g, void **out);
void Graph_ParseParam(Graph *g, ParamCursor *cur);
void Graph_ReadMaxNodes(GraphStreamValue *v);
void Graph_ReadNodeID(GraphStreamValue *v);
void Graph_ReadNodePosition(GraphStreamValue *v);
void Graph_ReadNodeGamma(GraphStreamValue *v);
void Graph_ReadNodeRadius(GraphStreamValue *v);
void Graph_ReadNodeMaterial(GraphStreamValue *v);
void Graph_ReadNodeCriteria(GraphStreamValue *v);
void Graph_ReadLinkID1(GraphStreamValue *v);
void Graph_ReadLinkID2(GraphStreamValue *v);
void Graph_ReadLinkType(GraphStreamValue *v);

#define FIELD(name) ((void *)offsetof(Graph, name))

void Graph_Open(void)
{
    static const int order[10] = {0, 1, 2, 3, 4, 7, 9, 8, 5, 6};
    GraphStreamHandler *h;
    int i, op;

    gGraphType = TaskType_New("Graph", 0x80, EditorRigidObj_GetType(), 0, 0);
    for (i = 0; i < 10; i++)
        gGraphEvents[order[i]] = TaskEvent_Alloc(0);
    TaskType_SetHandler(gGraphType, 0, Graph_Create);
    TaskType_SetHandler2(gGraphType, 1, Debug_Log);
    TaskType_SetHandler2(gGraphType, 2, Graph_Delete);
    TaskType_SetHandler2(gGraphType, 3, Graph_PreDelete);
    TaskType_SetHandler(gGraphType, Param_GetChangedEvent(), Graph_ParamsChanged);
    TaskType_SetHandler(gGraphType, Param_GetTableEvent(), Graph_GetParams);
    op = ParamOp_Parse();
    ParamType_SetOp(Param_Graph(), op, Graph_ParseParam);
    gGraphParams = ParamTable_New(5);
    /* the original passes a dummy Graph on its stack, only the difference
       of the two pointers is kept */
    ParamTable_Add(gGraphParams, NULL, FIELD(pos), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gGraphParams, NULL, FIELD(unk65), Param_PushButton(), 7, NULL);
    ParamTable_Add(gGraphParams, NULL, FIELD(unk68), Param_Bool8(), 6, NULL);
    ParamTable_Add(gGraphParams, NULL, FIELD(unk69), Param_Bool8(), 6, NULL);
    ParamTable_Add(gGraphParams, NULL, NULL, Param_Graph(), 7, NULL);

    gGraphRefs.head = (Node *)&gGraphRefs.tail;
    gGraphRefs.tailpred = (Node *)&gGraphRefs;
    gGraphRefs.tail = NULL;
    for (i = 0; i < 4; i++) {
        gGraphNodeSizes[i].type = 0x181;
        gGraphNodeSizes[i].size = 0;
    }
    Graph_SetNodeSize(gGraphType, sizeof(GraphNode));

    gStreamHandlers.head = (Node *)&gStreamHandlers.tail;
    gStreamHandlers.tailpred = (Node *)&gStreamHandlers;
    gStreamHandlers.tail = NULL;
    gStreamHandlersFree.head = (Node *)&gStreamHandlersFree.tail;
    gStreamHandlersFree.tailpred = (Node *)&gStreamHandlersFree;
    gStreamHandlersFree.tail = NULL;
    for (h = gGraphHandlerSlots; h < gGraphHandlerSlots + 64; h++) {
        h->node.prev = NULL;
        h->node.next = NULL;
        List_AddTail(&gStreamHandlersFree, &h->node);
    }

    GraphStream_AddHandler("nMaxNodes", 5, Graph_ReadMaxNodes);
    GraphStream_AddHandler("nNodeID", 5, Graph_ReadNodeID);
    GraphStream_AddHandler("tNodePosition", 8, Graph_ReadNodePosition);
    GraphStream_AddHandler("vNodeGamma", 6, Graph_ReadNodeGamma);
    GraphStream_AddHandler("vNodeRadius", 6, Graph_ReadNodeRadius);
    GraphStream_AddHandler("nNodeMaterial", 5, Graph_ReadNodeMaterial);
    GraphStream_AddHandler("nNodeCriteria", 9, Graph_ReadNodeCriteria);
    GraphStream_AddHandler("nLinkID1", 5, Graph_ReadLinkID1);
    GraphStream_AddHandler("nLinkID2", 5, Graph_ReadLinkID2);
    GraphStream_AddHandler("eLinkType", 5, Graph_ReadLinkType);
}
HOOK(0x4FAA60, Graph_Open);

/* drops the graph's reference and frees the links of every node */
void Graph_PreDelete(Graph *g)
{
    GraphRef *r = (GraphRef *)gGraphRefs.head;
    Node *next;
    int i, j;

    for (next = r->node.next; next; next = next->next) {
        if (r->ref && *r->ref == (Task *)g) {
            TaskRef_Remove(r->ref);
            List_Remove(&r->node);
            Mem_Free(r);
            break;
        }
        r = (GraphRef *)next;
    }
    for (i = 0; i < g->capacity; i++) {
        GraphNode *n = Graph_GetNode(g, i);

        if (!n)
            continue;
        for (j = 0; j < 64; j++)
            if (n->links[j])
                GraphLink_Free(n->links[j]);
    }
}
HOOK(0x4FADB0, Graph_PreDelete);

void Graph_GetParams(Graph *g, void **out)
{
    *out = gGraphParams;
}
HOOK(0x4FAE40, Graph_GetParams);

/* the Graph parameter: three script ints, the second is nMaxNodes */
void Graph_ParseParam(Graph *g, ParamCursor *cur)
{
    char path[1024];
    int i = cur->index;

    g->unk68 = 0;
    g->unk69 = 0;
    Script_GetArgInt(cur->args, i);
    g->capacity = Script_GetArgInt(cur->args, i + 1);
    Script_GetArgInt(cur->args, i + 2);
    g->nodeCount = 0;
    g->linkCount = 0;
    crt_sprintf(path, "MISSION:graphs/graph%d.dat", Task_GetId(g));
    GraphOpen(g, path);
    cur->index = i + 3;
}
HOOK(0x4FAE50, Graph_ParseParam);

void Graph_ReadMaxNodes(GraphStreamValue *v)
{
    Graph *g = v->graph;
    int n = g->capacity;

    if (g->nodes) {
        Mem_Free(g->nodes);
        g->nodes = NULL;
    }
    GraphStream_ReadValue(v, &n, 0);
    g->nodes = Mem_Alloc(Graph_GetNodeSize(g) * n, 4);
    Graph_ClearNodes(g, n);
    g->capacity = n;
}
HOOK(0x4FAEE0, Graph_ReadMaxNodes);

void Graph_ReadNodeID(GraphStreamValue *v)
{
    int *p = (int *)&gGraphNodeTemp;
    int i;

    for (i = 0; i < 0x90; i++)
        p[i] = 0;
    v->node = NULL;
    GraphStream_ReadValue(v, &gGraphNodeTemp.id, 0);
}
HOOK(0x4FAF50, Graph_ReadNodeID);

void Graph_ReadNodePosition(GraphStreamValue *v)
{
    GraphStream_ReadValue(v, &gGraphNodeTemp.pos, 0);
}
HOOK(0x4FAF80, Graph_ReadNodePosition);

void Graph_ReadNodeGamma(GraphStreamValue *v)
{
    GraphStream_ReadValue(v, &gGraphNodeTemp.gamma, 0);
}
HOOK(0x4FAFA0, Graph_ReadNodeGamma);

void Graph_ReadNodeRadius(GraphStreamValue *v)
{
    GraphStream_ReadValue(v, &gGraphNodeTemp.radius, 0);
}
HOOK(0x4FAFC0, Graph_ReadNodeRadius);

void Graph_ReadNodeMaterial(GraphStreamValue *v)
{
    int id = -1;

    GraphStream_ReadValue(v, &id, 0);
    gGraphNodeTemp.material = id != -1 ? Terrain_GetMaterial(gCurrentWorld, id) : NULL;
}
HOOK(0x4FAFE0, Graph_ReadNodeMaterial);

/* the last field of a node: adds it */
void Graph_ReadNodeCriteria(GraphStreamValue *v)
{
    GraphNode *t = &gGraphNodeTemp;

    GraphStream_ReadValue(v, t->criteria, sizeof(t->criteria));
    v->node = Graph_AddNode(v->graph, t->id, &t->pos, t->gamma, t->radius, t->material, t->criteria, 0);
}
HOOK(0x4FB030, Graph_ReadNodeCriteria);

void Graph_ReadLinkID1(GraphStreamValue *v)
{
    gGraphLinkA = -1;
    gGraphLinkB = -1;
    v->link = NULL;
    GraphStream_ReadValue(v, &gGraphLinkA, 0);
}
HOOK(0x4FB080, Graph_ReadLinkID1);

void Graph_ReadLinkID2(GraphStreamValue *v)
{
    GraphStream_ReadValue(v, &gGraphLinkB, 0);
}
HOOK(0x4FB0B0, Graph_ReadLinkID2);

/* the last field of a link: adds it */
void Graph_ReadLinkType(GraphStreamValue *v)
{
    GraphStream_ReadValue(v, &gGraphLinkCost, 0);
    v->link = Graph_AddLink(v->graph, gGraphLinkA, gGraphLinkB, gGraphLinkCost);
}
HOOK(0x4FB0D0, Graph_ReadLinkType);

void Graph_Close(void)
{
    static const int order[10] = {0, 1, 2, 3, 4, 5, 6, 8, 7, 9};
    int i;

    QTaskList_Free(gGraphParams);
    for (i = 0; i < 10; i++)
        TaskEvent_Free(gGraphEvents[order[i]]);
    TaskType_Free(gGraphType);
}
HOOK(0x4FB110, Graph_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E364) uint16_t gGraphType INIT(385);
PAD(igd, 54E364, 54E366, 0x2) INIT({0x81, 0x01});
BSS(A76D10) void *gGraphParams;
PAD(igb, A76D10, A76D14, 0x4);
BSS(A76D10) List gStreamHandlersFree;
BSS(A76D10) int gGraphLinkCost;
STORE(igb, A76D10, 4) __typeof__(gGraphHandlerSlots[0]) store_A76D28[64];
ALIAS(gGraphHandlerSlots, store_A76D28, 0);
BSS(A77248) List gGraphRefs;
BSS(A77248) int gGraphLinkB;
BSS(A77248) int gGraphLinkA;
PAD(igb, A77248, A7725C, 0x4);
BSS(A77248) GraphNode gGraphNodeTemp;
BSS(A77248) List gStreamHandlers;
PAD(igb, A77248, A774AC, 0x4);
