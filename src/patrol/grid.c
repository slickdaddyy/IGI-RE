/* AI helpers: node grids, the HumanMonitor target cache, claimed tasks. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "patrol/patrol.h"

typedef struct Matrix {
    float m[9];
    int count;
} Matrix;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct GraphNode {
    int id;
    char unk4[0x24];
    float radius;
} GraphNode;

typedef struct Graph {
    char unk0[0x50];
    int id;
} Graph;

/* AI brain of a soldier, only the fields used here */
typedef struct SoldierAI {
    char unk0[0x3AA8];
    void *unk3AA8;
    char unk3AAC[0xC0];
    Graph *graph;
} SoldierAI;

_Static_assert(offsetof(SoldierAI, graph) == 0x3B6C, "SoldierAI.graph");

typedef struct ViewObj {
    char unk0[0x50];
    Matrix matrix;
} ViewObj;

/* 20 x 20 counters spread around a graph node so AIs pick different spots */
typedef struct NodeGrid {
    Node node;
    int graph;
    int nodeId;
    int refs;
    int w;
    int h;
    int unk1C;
    int cells[20][20];
} NodeGrid;

_Static_assert(sizeof(NodeGrid) == 0x660, "NodeGrid");

typedef struct GridStamp {
    int dx, dy, weight;
} GridStamp;

/* nearest graph node of one target, refreshed once per tick */
typedef struct MonitorTarget {
    Node node;
    int nodeId;
    int tick;
    void *target;
} MonitorTarget;

typedef struct MonitorGraph {
    Node node;
    Graph *graph;
    List targets;
} MonitorGraph;

typedef struct Claim {
    Node node;
    void *task;
} Claim;

ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x4148F0, long double, Vec3d_PitchAngle, (Vec3d v));
ORIG(0x451380, void, HumanAI_GetTargetPos, (Vec3d *out, SoldierAI *ai));
ORIG(0x4513E0, void *, HumanAI_GetTarget, (SoldierAI *ai));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4F9720, GraphNode *, Graph_GetNode, (Graph *graph, int id));
ORIG(0x4F9770, void, Graph_GetNodePos, (Vec3d *out, GraphNode *node));
ORIG(0x4F98A0, void, Graph_GetNodeMatrix, (Matrix *out, GraphNode *node, int unk));
ORIG(0x5096F0, void, AIGraph_FindNearest, (GraphNode **out, Graph *graph, const Vec3d *pos));

extern const char *gAITypeNames[];
extern const GridStamp gGridStamp[];
extern List gGridsUsed;
extern List gGridsFree;
extern List gMonitorGraphsUsed;
extern List gMonitorGraphsFree;
extern List gMonitorTargetsFree;
extern List gClaimsUsed;
extern List gClaimsFree;

static const float kFrames = 30.0f;
static const double kAhead = 0x1.199999999999ap+0;
static const double kLift = 0x1.64ce9ed57275ep+4;
static const float kHalfF = 0.5f;
static const double kHalf = 0.5;
static const double kCellInv = 0x1p-13;
static const double kCell = 8192.0;

int AI_HasTarget(SoldierAI *ai)
{
    return ai->unk3AA8 != NULL;
}
HOOK(0x4528F0, AI_HasTarget);

const char *AIType_GetName(int type)
{
    return gAITypeNames[type];
}
HOOK(0x452910, AIType_GetName);

void *MoveItem_GetPos(char *p)
{
    return p + 8;
}
HOOK(0x452920, MoveItem_GetPos);

void AI_AimThrow(Matrix *outMatrix, float *outVec, ViewObj *obj, const double *from, float scale)
{
    const float *m = obj->matrix.m;
    const float *r;
    Vec3d pos, v;
    Matrix rot;
    long double dx, dy, dz, px;
    double py, pz;
    float s = scale * kFrames;
    float pitch;

    HumanAI_GetTargetPos(&pos, (SoldierAI *)obj);
    dx = pos.x - from[0];
    dy = pos.y - from[1];
    dz = pos.z - from[2];
    pos.x = dx;
    pos.y = dy;
    pos.z = dz;
    v.x = m[0] * dx + m[3] * dy + m[6] * dz;
    v.y = m[4] * dy + m[7] * dz + m[1] * dx;
    v.z = m[5] * dy + m[8] * dz + m[2] * dx;
    pitch = Vec3d_PitchAngle(v);
    Matrix_SetRotX(&rot, pitch);
    r = rot.m;
    outMatrix->m[0] = r[0] * m[0] + r[6] * m[2] + r[3] * m[1];
    outMatrix->m[1] = r[1] * m[0] + r[7] * m[2] + r[4] * m[1];
    outMatrix->m[2] = r[2] * m[0] + r[8] * m[2] + r[5] * m[1];
    outMatrix->m[3] = r[6] * m[5] + r[3] * m[4] + r[0] * m[3];
    outMatrix->m[4] = r[7] * m[5] + r[4] * m[4] + r[1] * m[3];
    outMatrix->m[5] = r[8] * m[5] + r[5] * m[4] + r[2] * m[3];
    outMatrix->m[6] = r[6] * m[8] + r[3] * m[7] + r[0] * m[6];
    outMatrix->m[7] = r[7] * m[8] + r[4] * m[7] + r[1] * m[6];
    outMatrix->m[8] = r[8] * m[8] + r[5] * m[7] + r[2] * m[6];
    outMatrix->count = obj->matrix.count + 1 + rot.count;
    px = pos.x * kAhead;
    py = pos.y * kAhead;
    pz = pos.z * kAhead;
    outVec[0] = px / s;
    outVec[1] = py / s;
    outVec[2] = pz / s + s * kLift;
}
HOOK(0x452930, AI_AimThrow);

void NodeGrid_Stamp(NodeGrid *g, int x, int y, char add)
{
    const GridStamp *s;

    for (s = gGridStamp; s < gGridStamp + 25; s++) {
        int cx = s->dx + x, cy = s->dy + y;

        if (cx < 0 || cx >= g->w || cy < 0 || cy >= g->h)
            continue;
        if (add)
            g->cells[cx][cy] += s->weight;
        else
            g->cells[cx][cy] -= s->weight;
    }
}
HOOK(0x452B80, NodeGrid_Stamp);

NodeGrid *NodeGrid_Find(int graph, int nodeId)
{
    NodeGrid *g;

    for (g = (NodeGrid *)gGridsUsed.head; g->node.next; g = (NodeGrid *)g->node.next)
        if (g->graph == graph && g->nodeId == nodeId)
            return g;
    return NULL;
}
HOOK(0x452BE0, NodeGrid_Find);

void NodeGrid_Release(NodeGrid *g, int x, int y)
{
    g->refs--;
    NodeGrid_Stamp(g, x, y, 0);
    if (g->refs == 0) {
        List_Remove(&g->node);
        g->node.prev = NULL;
        g->node.next = NULL;
        List_AddTail(&gGridsFree, &g->node);
    }
}
HOOK(0x452C10, NodeGrid_Release);

/* takes a free grid for the node, sized by the node radius */
static NodeGrid *NodeGrid_New(Graph *graph, int nodeId, GraphNode *node, int getNodeFirst)
{
    NodeGrid *g = (NodeGrid *)gGridsFree.head;

    if (!g->node.next)
        return NULL;
    List_Remove(&g->node);
    g->node.prev = NULL;
    g->node.next = NULL;
    List_AddTail(&gGridsUsed, &g->node);
    if (!getNodeFirst)
        node = Graph_GetNode(graph, nodeId);
    g->refs = 0;
    g->graph = graph->id;
    g->nodeId = nodeId;
    g->w = x87_fistp(node->radius * kHalfF);
    g->h = x87_fistp(node->radius * kHalfF);
    return g;
}

NodeGrid *NodeGrid_Acquire(Graph *graph, int nodeId, int x, int y)
{
    NodeGrid *g = NodeGrid_Find(graph->id, nodeId);

    if (!g) {
        g = NodeGrid_New(graph, nodeId, NULL, 0);
        if (!g)
            return NULL;
    }
    g->refs++;
    NodeGrid_Stamp(g, x, y, 1);
    return g;
}
HOOK(0x452C60, NodeGrid_Acquire);

static double Grid_Clamp(double v, int max)
{
    if (!(v >= 0.0))
        return 0.0;
    if (v > (long double)max)
        return max;
    return v;
}

NodeGrid *NodeGrid_Place(int *outX, int *outY, double *outPos, Graph *graph, int nodeId, const double *pos)
{
    GraphNode *node = Graph_GetNode(graph, nodeId);
    NodeGrid *g = NodeGrid_Find(graph->id, nodeId);
    Vec3d np;
    Matrix mat;
    const float *m = mat.m;
    double d[3], lz, gx, gy;
    long double lx, ly;
    int ix, iy;

    if (!g) {
        g = NodeGrid_New(graph, nodeId, node, 1);
        if (!g)
            return NULL;
    }
    g->refs++;
    Graph_GetNodePos(&np, node);
    d[0] = pos[0] - np.x;
    d[1] = pos[1] - np.y;
    d[2] = pos[2] - np.z;
    Graph_GetNodeMatrix(&mat, node, 1);
    lx = (double)(m[6] * d[2] + m[3] * d[1] + m[0] * d[0]) * kCellInv;
    ly = (double)(m[7] * d[2] + m[4] * d[1] + m[1] * d[0]);
    lz = (double)(m[8] * d[2] + m[5] * d[1] + m[2] * d[0]) * kCellInv;
    ly = (double)(ly * kCellInv);
    gx = g->w * kHalf + lx;
    gy = g->h * kHalf + ly;
    gx = Grid_Clamp(gx, g->w);
    gy = Grid_Clamp(gy, g->h);
    iy = x87_fistp(gy);
    ix = x87_fistp(gx);
    NodeGrid_Stamp(g, ix, iy, 1);
    if (outX)
        *outX = ix;
    if (outY)
        *outY = iy;
    if (outPos) {
        long double a = (gx - g->w * kHalf) * kCell;
        long double b = (double)(gy - g->h * kHalf) * kCell;
        double x = m[2] * lz + m[1] * b + m[0] * a;
        double y = m[5] * lz + m[4] * b + m[3] * a;
        long double z = m[8] * lz + m[7] * b + m[6] * a;

        outPos[0] = x + np.x;
        outPos[1] = y + np.y;
        outPos[2] = z + np.z;
    }
    return g;
}
HOOK(0x452D20, NodeGrid_Place);

void NodeGrid_GetSize(int *w, int *h, Graph *graph, int nodeId)
{
    GraphNode *node;

    *w = 0;
    *h = 0;
    node = Graph_GetNode(graph, nodeId);
    *w = x87_fistp(node->radius * kHalfF);
    *h = x87_fistp(node->radius * kHalfF);
}
HOOK(0x453040, NodeGrid_GetSize);

/* nearest node to the target's position, NULL when there is none */
static GraphNode *Monitor_Refresh(MonitorTarget *e, MonitorGraph *mg, SoldierAI *ai)
{
    GraphNode *found = NULL;
    Vec3d pos;

    e->tick = Flow_GetFrames();
    HumanAI_GetTargetPos(&pos, ai);
    AIGraph_FindNearest(&found, mg->graph, &pos);
    if (found) {
        e->nodeId = found->id;
        return found;
    }
    e->nodeId = -1;
    return NULL;
}

GraphNode *HumanMonitor_Lookup(SoldierAI *ai, MonitorGraph *mg)
{
    void *target = HumanAI_GetTarget(ai);
    MonitorTarget *e, *next;

    if (!target) {
        Warning_Show("HumanMonitor - no target");
        return NULL;
    }
    for (e = (MonitorTarget *)mg->targets.head; e->node.next; e = (MonitorTarget *)e->node.next) {
        if (e->target != target)
            continue;
        if (e->tick != Flow_GetFrames())
            return Monitor_Refresh(e, mg, ai);
        if (e->nodeId == -1)
            return NULL;
        return Graph_GetNode(mg->graph, e->nodeId);
    }
    /* every free entry is tried until one finds a node */
    e = (MonitorTarget *)gMonitorTargetsFree.head;
    for (next = (MonitorTarget *)e->node.next; next; e = next, next = (MonitorTarget *)next->node.next) {
        GraphNode *found;

        List_Remove(&e->node);
        List_AddTail(&mg->targets, &e->node);
        e->target = target;
        found = Monitor_Refresh(e, mg, ai);
        if (found)
            return found;
    }
    return NULL;
}
HOOK(0x4530A0, HumanMonitor_Lookup);

GraphNode *HumanMonitor_FindTargetNode(SoldierAI *ai)
{
    MonitorGraph *mg;

    for (mg = (MonitorGraph *)gMonitorGraphsUsed.head; mg->node.next; mg = (MonitorGraph *)mg->node.next)
        if (mg->graph == ai->graph)
            return HumanMonitor_Lookup(ai, mg);
    mg = (MonitorGraph *)gMonitorGraphsFree.head;
    if (!mg->node.next)
        return NULL;
    List_Remove(&mg->node);
    List_AddTail(&gMonitorGraphsUsed, &mg->node);
    mg->graph = ai->graph;
    return HumanMonitor_Lookup(ai, mg);
}
HOOK(0x453200, HumanMonitor_FindTargetNode);

void HumanMonitor_Reset(void)
{
    Node *mg, *nextMg, *e, *nextE;

    mg = gMonitorGraphsUsed.head;
    for (nextMg = mg->next; nextMg; mg = nextMg, nextMg = nextMg->next) {
        e = ((MonitorGraph *)mg)->targets.head;
        for (nextE = e->next; nextE; e = nextE, nextE = nextE->next) {
            List_Remove(e);
            List_AddTail(&gMonitorTargetsFree, e);
        }
        List_Remove(mg);
        List_AddTail(&gMonitorGraphsFree, mg);
    }
}
HOOK(0x453270, HumanMonitor_Reset);

char AIClaim_IsClaimed(void *task)
{
    Claim *c;

    for (c = (Claim *)gClaimsUsed.head; c->node.next; c = (Claim *)c->node.next)
        if (c->task == task)
            return 1;
    return 0;
}
HOOK(0x4532D0, AIClaim_IsClaimed);

Claim *AIClaim_Set(void *task, char claim)
{
    Claim *c;

    if (claim) {
        c = (Claim *)gClaimsFree.head;
        if (!c->node.next)
            return NULL;
        List_Remove(&c->node);
        c->node.prev = NULL;
        c->node.next = NULL;
        List_AddTail(&gClaimsUsed, &c->node);
        c->task = task;
        return c;
    }
    for (c = (Claim *)gClaimsUsed.head; c->node.next; c = (Claim *)c->node.next) {
        if (c->task == task) {
            List_Remove(&c->node);
            c->node.prev = NULL;
            c->node.next = NULL;
            List_AddTail(&gClaimsFree, &c->node);
            break;
        }
    }
    return NULL;
}
HOOK(0x453300, AIClaim_Set);

void AIClaim_Release(Claim *c)
{
    AIClaim_Set(c->task, 0);
}
HOOK(0x453390, AIClaim_Release);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 535348, 535348, 0x1) INIT({0x74});
DATA(535348) char str_535349[7] INIT("eFileA");
STORE(igd, 53CA98, 4) __typeof__(gGridStamp[0]) store_53CA98[25] INIT({{.dx = -2, .dy = -2, .weight = 25}, {.dx = -1, .dy = -2, .weight = 25}, {.dy = -2, .weight = 25}, {.dx = 1, .dy = -2, .weight = 25}, {.dx = 2, .dy = -2, .weight = 25}, {.dx = -2, .dy = -1, .weight = 25}, {.dx = -1, .dy = -1, .weight = 50}, {.dy = -1, .weight = 50}, {.dx = 1, .dy = -1, .weight = 50}, {.dx = 2, .dy = -1, .weight = 25}, {.dx = -2, .weight = 25}, {.dx = -1, .weight = 50}, {.weight = 100}, {.dx = 1, .weight = 50}, {.dx = 2, .weight = 25}, {.dx = -2, .dy = 1, .weight = 25}, {.dx = -1, .dy = 1, .weight = 50}, {.dy = 1, .weight = 50}, {.dx = 1, .dy = 1, .weight = 50}, {.dx = 2, .dy = 1, .weight = 25}, {.dx = -2, .dy = 2, .weight = 25}, {.dx = -1, .dy = 2, .weight = 25}, {.dy = 2, .weight = 25}, {.dx = 1, .dy = 2, .weight = 25}, {.dx = 2, .dy = 2, .weight = 25}});
ALIAS(gGridStamp, store_53CA98, 0);
DATA(544F48) char str_544F48[1] INIT("");
PAD(igd, 544F48, 544F49, 0x3);
PAD(igd, 545244, 545244, 0x1);
DATA(545244) char str_545245[1] INIT("");
PAD(igd, 545244, 545246, 0x2);
BSS(57C748) List gMonitorGraphsFree;
BSS(57D758) List gClaimsFree;
PAD(igb, 57D758, 57D764, 0x4);
BSS(57D758) List gGridsUsed;
BSS(57E1B8) List gMonitorTargetsFree;
BSS(58A9E8) List gGridsFree;
BSS(58AA18) List gMonitorGraphsUsed;
BSS(5BDA28) List gClaimsUsed;
