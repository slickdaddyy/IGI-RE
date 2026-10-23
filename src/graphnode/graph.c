#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "graphnode/graphnode.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, void *node));
ORIG(0x4AF960, void, List_Remove, (void *node));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B3320, char, Vec3d_IsZero, (const Vec3d *v));

/* a TaskRef to a graph, on gGraphs */
typedef struct GraphRef {
    Node node;
    Task **ref;
} GraphRef;

/* bytes per node of each graph task type */
typedef struct NodeSize {
    uint16_t type;
    int size;
} NodeSize;

#define NODESIZE_MAX 4

#define gGraphParams VAR(void *, 0xA76D10)
#define gGraphType VAR(uint16_t, 0x54E364)
#define gGraphLoadEvent VAR(uint8_t, 0x54E36B)
#define gGraphSaveEvent VAR(uint8_t, 0x54E36C)
#define gGraphNodeLoadEvent VAR(uint8_t, 0x54E36D)
#define gGraphEvent36E VAR(uint8_t, 0x54E36E)
#define gGraphNodeAddEvent VAR(uint8_t, 0x54E36F)
#define gGraphEvent371 VAR(uint8_t, 0x54E371)
#define gGraphAllocEvent VAR(uint8_t, 0x54E369)
#define gGraphs VAR(List, 0xA77248)
#define gNodeSizes ((NodeSize *)0xA77228)

static const double kZero = 0.0;
static const double kInside = 4096.0;
static const double kInsideNeg = -4096.0;

void Graph_GetWorldPos(Vec3d *out, Graph *g);
void Graph_SetWorldPos(Graph *g, const Vec3d *pos);

void Graph_Create(Graph *g)
{
    GraphRef *r;
    uint16_t type;

    g->pos.x = g->pos.y = g->pos.z = 0;
    g->id = -1;
    g->unk54 = 0;
    g->capacity = 0;
    g->nodeCount = 0;
    g->linkCount = 0;
    g->unk65[0] = 0;
    g->unk65[3] = 0;
    g->unk69[0] = 0;
    g->unk65[1] = 0;
    g->unk65[2] = 0;
    g->nodes = NULL;
    r = Mem_Alloc(sizeof(GraphRef), 4);
    r->ref = TaskRef_Add(g);
    r->node.prev = NULL;
    r->node.next = NULL;
    List_AddTail(&gGraphs, r);
    type = g->task.parent->type;
    g->pos.x = g->pos.y = g->pos.z = 0;
    g->attached = 0;
    if (TaskType_IsA(type, RigidDynCubeObj_GetType()))
        g->attached = 1;
    g->links.tailpred = (Node *)&g->links;
    g->links.tail = NULL;
    g->links.head = (Node *)&g->links.tail;
}
HOOK(0x4F90B0, Graph_Create);

void Graph_Delete(Graph *g)
{
    if (g->nodes) {
        Mem_Free(g->nodes);
        g->nodes = NULL;
    }
}
HOOK(0x4F9160, Graph_Delete);

/* keeps the world position when the parent becomes or stops being a
   RigidDynCubeObj, then refreshes the id */
void Graph_ParamsChanged(Graph *g)
{
    uint16_t type = g->task.parent->type;
    Vec3d p;

    if (TaskType_IsA(type, RigidDynCubeObj_GetType())) {
        if (!g->attached) {
            Graph_GetWorldPos(&p, g);
            g->attached = 1;
            Graph_SetWorldPos(g, &p);
        }
    } else if (g->attached) {
        Graph_GetWorldPos(&p, g);
        g->attached = 0;
        Graph_SetWorldPos(g, &p);
    }
    g->id = Task_GetId(g);
}
HOOK(0x4F9180, Graph_ParamsChanged);

void *Graph_GetParamTable(void)
{
    return gGraphParams;
}
HOOK(0x4F9200, Graph_GetParamTable);

uint16_t Graph_GetType(void)
{
    return gGraphType;
}
HOOK(0x4F9210, Graph_GetType);

uint8_t Graph_GetLoadEvent(void)
{
    return gGraphLoadEvent;
}
HOOK(0x4F9220, Graph_GetLoadEvent);

uint8_t Graph_GetSaveEvent(void)
{
    return gGraphSaveEvent;
}
HOOK(0x4F9230, Graph_GetSaveEvent);

uint8_t Graph_GetNodeLoadEvent(void)
{
    return gGraphNodeLoadEvent;
}
HOOK(0x4F9240, Graph_GetNodeLoadEvent);

uint8_t sub_4F9250(void)
{
    return gGraphEvent36E;
}
HOOK(0x4F9250, sub_4F9250);

uint8_t Graph_GetNodeAddEvent(void)
{
    return gGraphNodeAddEvent;
}
HOOK(0x4F9260, Graph_GetNodeAddEvent);

uint8_t sub_4F9270(void)
{
    return gGraphEvent371;
}
HOOK(0x4F9270, sub_4F9270);

uint8_t Graph_GetAllocEvent(void)
{
    return gGraphAllocEvent;
}
HOOK(0x4F9280, Graph_GetAllocEvent);

Graph *Graph_Find(int id)
{
    Node *n, *next;

    if (id == -1)
        return NULL;
    for (n = gGraphs.head; (next = n->next); n = next) {
        GraphRef *r = (GraphRef *)n;

        if (r->ref) {
            Graph *g = (Graph *)*r->ref;

            if (g->id == -1)
                g->id = Task_GetId(g);
            if (id == g->id)
                return g;
        }
    }
    return NULL;
}
HOOK(0x4F9290, Graph_Find);

void Graph_GetWorldPos(Vec3d *out, Graph *g)
{
    if (g->attached) {
        RigidDynCubeObj *p = (RigidDynCubeObj *)g->task.parent;
        const float *m = p->matrix.m;
        Vec3d r;

        *out = g->pos;
        r.x = m[2] * out->z + m[1] * out->y + m[0] * out->x;
        r.y = m[5] * out->z + m[4] * out->y + m[3] * out->x;
        r.z = m[8] * out->z + m[7] * out->y + m[6] * out->x;
        *out = r;
        out->x = p->pos.x + out->x;
        out->y = p->pos.y + out->y;
        out->z = p->pos.z + out->z;
    } else {
        *out = g->pos;
    }
}
HOOK(0x4F92E0, Graph_GetWorldPos);

void Graph_SetWorldPos(Graph *g, const Vec3d *pos)
{
    if (g->attached) {
        RigidDynCubeObj *p = (RigidDynCubeObj *)g->task.parent;
        const float *m = p->matrix.m;
        long double dx = pos->x - p->pos.x;
        long double dy = pos->y - p->pos.y;
        long double dz = pos->z - p->pos.z;
        Vec3d r;

        r.x = m[6] * dz + m[3] * dy + m[0] * dx;
        r.y = m[7] * dz + m[4] * dy + m[1] * dx;
        r.z = m[8] * dz + m[5] * dy + m[2] * dx;
        g->pos = r;
    } else {
        g->pos = *pos;
    }
}
HOOK(0x4F93B0, Graph_SetWorldPos);

void Graph_SetNodeSize(uint16_t type, int size)
{
    int i, free = -1;

    for (i = 0; i < NODESIZE_MAX; i++) {
        if (free == -1 && gNodeSizes[i].type == TASK_NO_TYPE)
            free = i;
        if (gNodeSizes[i].type == type) {
            gNodeSizes[i].size = size;
            return;
        }
    }
    if (free == -1) {
        Error_Show("NO FREE NODESIZE ELEMENTS");
        for (;;)
            ;
    }
    gNodeSizes[free].type = type;
    gNodeSizes[free].size = size;
}
HOOK(0x4F9460, Graph_SetNodeSize);

int Graph_GetNodeSize(Graph *g)
{
    int i;

    for (i = 0; i < NODESIZE_MAX; i++) {
        if (g->task.type == gNodeSizes[i].type)
            return gNodeSizes[i].size;
    }
    Error_Show("Undefined GraphNodeDataSize");
    for (;;)
        ;
}
HOOK(0x4F94D0, Graph_GetNodeSize);

void Graph_Grow(Graph *g, int id);

GraphNode *Graph_AddNode(Graph *g, int id, const Vec3d *pos, float gamma, float radius, void *material,
                         const char *criteria)
{
    GraphNode *n;
    TaskHandler fn;

    if (id >= 1000)
        return NULL;
    if (id >= g->capacity)
        Graph_Grow(g, id);
    if (Graph_GetNode(g, id)) {
        Error_Show("GRAPHNODE ID ALREADY USED");
        for (;;)
            ;
    }
    n = (GraphNode *)(g->nodes + Graph_GetNodeSize(g) * id);
    n->diameter = radius + radius;
    n->radius = radius;
    n->gamma = gamma;
    n->id = id;
    n->unk2C = -1;
    strcpy(n->criteria, criteria);
    n->pos = *pos;
    n->graph = g;
    n->flags = Script_EvalExpr(n->criteria);
    n->material = material;
    memset(n->links, 0, sizeof(n->links));
    fn = TASK_HANDLER(gGraphNodeAddEvent, g->task.type);
    if (fn)
        fn(g, n);
    g->nodeCount++;
    return n;
}
HOOK(0x4F9510, Graph_AddNode);

/* room for id, in steps of 100 nodes */
void Graph_Grow(Graph *g, int id)
{
    char *old = g->nodes;
    int cap = g->capacity + ((id - g->capacity) / 100 + 1) * 100;
    int size = Graph_GetNodeSize(g);
    TaskHandler fn;

    g->nodes = Mem_Alloc(size * cap, 4);
    Graph_ClearNodes(g, cap);
    if (old) {
        memcpy(g->nodes, old, g->capacity * size);
        Mem_Free(old);
    }
    g->capacity = cap;
    fn = TASK_HANDLER(Graph_GetAllocEvent(), g->task.type);
    if (fn)
        fn(g, NULL);
}
HOOK(0x4F9620, Graph_Grow);

void Graph_ClearNodes(Graph *g, int count)
{
    int size, off;

    if (!g->nodes)
        return;
    size = Graph_GetNodeSize(g);
    for (off = 0; count > 0; count--, off += size)
        *(int *)(g->nodes + off) = -1;
}
HOOK(0x4F96E0, Graph_ClearNodes);

GraphNode *Graph_GetNode(Graph *g, int id)
{
    GraphNode *n;

    if (!g->nodes || id >= g->capacity || id < 0 || id == -1)
        return NULL;
    n = (GraphNode *)(g->nodes + Graph_GetNodeSize(g) * id);
    return n->id != -1 ? n : NULL;
}
HOOK(0x4F9720, Graph_GetNode);

void Graph_GetNodePos(Vec3d *out, GraphNode *node)
{
    Graph *g = node->graph;

    if (g->attached) {
        RigidDynCubeObj *p = (RigidDynCubeObj *)g->task.parent;
        const float *m = p->matrix.m;
        long double z;
        Vec3d r;

        *out = node->pos;
        out->x = node->graph->pos.x + out->x;
        out->y = node->graph->pos.y + out->y;
        z = node->graph->pos.z + out->z;
        out->z = z;
        r.x = m[2] * z + m[1] * out->y + m[0] * out->x;
        r.y = m[5] * z + m[4] * out->y + m[3] * out->x;
        r.z = m[8] * z + m[7] * out->y + m[6] * out->x;
        *out = r;
        out->x = p->pos.x + out->x;
        out->y = p->pos.y + out->y;
        out->z = p->pos.z + out->z;
    } else {
        *out = node->pos;
        out->x = node->graph->pos.x + out->x;
        out->y = node->graph->pos.y + out->y;
        out->z = node->graph->pos.z + out->z;
    }
}
HOOK(0x4F9770, Graph_GetNodePos);

/* the node's rotation (or none) times the parent's when attached */
void Graph_GetNodeMatrix(Matrix *out, GraphNode *node, char rotate)
{
    Graph *g;

    if (rotate) {
        Matrix_SetRotZ(out, node->gamma);
    } else {
        int i;

        for (i = 0; i < 9; i++)
            out->m[i] = i % 4 == 0 ? 1.0f : 0.0f;
        out->unk24 = 0;
    }
    g = node->graph;
    if (g->attached) {
        RigidDynCubeObj *pt = (RigidDynCubeObj *)g->task.parent;
        const float *p = pt->matrix.m;
        float *a = out->m;
        Matrix r;

        r.m[0] = a[0] * p[0] + a[2] * p[6] + p[3] * a[1];
        r.m[1] = a[0] * p[1] + p[7] * a[2] + p[4] * a[1];
        r.m[2] = a[2] * p[8] + p[2] * a[0] + p[5] * a[1];
        r.m[3] = p[6] * a[5] + a[4] * p[3] + a[3] * p[0];
        r.m[4] = p[7] * a[5] + p[4] * a[4] + a[3] * p[1];
        r.m[5] = a[4] * p[5] + p[2] * a[3] + p[8] * a[5];
        r.m[6] = a[6] * p[0] + a[8] * p[6] + p[3] * a[7];
        r.m[7] = a[6] * p[1] + p[7] * a[8] + p[4] * a[7];
        r.m[8] = a[8] * p[8] + p[2] * a[6] + p[5] * a[7];
        r.unk24 = pt->matrix.unk24 + out->unk24 + 1;
        *out = r;
    }
}
HOOK(0x4F98A0, Graph_GetNodeMatrix);

/* pos is inside the node's square (half size (radius + tol) * 4096); with
   checkZ also within 4096 in height */
char Graph_IsInNode(const Vec3d *pos, GraphNode *node, double tol, char checkZ)
{
    Vec3d c, d;
    Matrix m;
    long double dz, a, r, lo, hi;
    double b, lod, hid;

    Graph_GetNodePos(&c, node);
    d.x = pos->x - c.x;
    d.y = pos->y - c.y;
    dz = pos->z - c.z;
    if (checkZ) {
        if (dz < kZero)
            dz = -dz;
        if (dz > kInside)
            return 0;
    }
    d.z = 0;
    if (Vec3d_IsZero(&d))
        return 0;
    Graph_GetNodeMatrix(&m, node, 1);
    a = m.m[6] * d.z + m.m[3] * d.y + m.m[0] * d.x;
    b = m.m[7] * d.z + m.m[4] * d.y + m.m[1] * d.x;
    r = node->radius + tol;
    lod = r * kInsideNeg;
    lo = lod;
    if (!(a >= lo))
        return 0;
    hid = r * kInside;
    hi = hid;
    if (a > hi)
        return 0;
    if (!(b >= lo))
        return 0;
    if (b > hi)
        return 0;
    return 1;
}
HOOK(0x4F9A40, Graph_IsInNode);

void GraphNode_AddLink(GraphNode *node, GraphLink *link);
void GraphNode_RemoveLink(GraphNode *node, GraphLink *link);

GraphLink *Graph_AddLink(Graph *g, int a, int b, int cost)
{
    GraphLink *l;

    Mem_PushMode(1);
    l = Mem_Alloc(sizeof(GraphLink), 4);
    Mem_PopMode();
    l->a = a;
    l->b = b;
    l->cost = cost;
    l->graph = g;
    l->node.prev = NULL;
    l->node.next = NULL;
    List_AddTail(&g->links, l);
    GraphNode_AddLink(Graph_GetNode(g, l->a), l);
    GraphNode_AddLink(Graph_GetNode(g, l->b), l);
    g->linkCount++;
    return l;
}
HOOK(0x4F9B80, Graph_AddLink);

void GraphNode_AddLink(GraphNode *node, GraphLink *link)
{
    int i;

    for (i = 0; i < GRAPHNODE_LINKS; i++) {
        if (!node->links[i]) {
            node->links[i] = link;
            return;
        }
    }
    Warning_Show("GRAPHNODE - LINKLIMIT REACHED");
}
HOOK(0x4F9C10, GraphNode_AddLink);

void GraphLink_Free(GraphLink *l)
{
    GraphNode_RemoveLink(Graph_GetNode(l->graph, l->a), l);
    GraphNode_RemoveLink(Graph_GetNode(l->graph, l->b), l);
    l->graph->linkCount--;
    List_Remove(l);
    Mem_Free(l);
}
HOOK(0x4F9C50, GraphLink_Free);

void GraphNode_RemoveLink(GraphNode *node, GraphLink *link)
{
    int i;

    for (i = 0; i < GRAPHNODE_LINKS; i++) {
        if (node->links[i] == link) {
            node->links[i] = NULL;
            return;
        }
    }
}
HOOK(0x4F9CA0, GraphNode_RemoveLink);
