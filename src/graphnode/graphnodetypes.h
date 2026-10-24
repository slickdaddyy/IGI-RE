#ifndef GRAPHNODE_GRAPHNODETYPES_H
#define GRAPHNODE_GRAPHNODETYPES_H

#include "task/tasktypes.h"

typedef struct GraphStream {
    int unk0;
    void *file;
} GraphStream;

typedef struct GraphStreamValue {
    struct GraphStream *stream;
    struct Graph *graph;
    struct GraphNode *node;
    struct GraphLink *link;
    int type;
} GraphStreamValue;

/* a free field handler slot */
typedef struct GraphStreamHandler {
    Node node;
    int hash;
    int type;
    void (*fn)(GraphStreamValue *v);
} GraphStreamHandler;

typedef struct GraphRef {
    Node node;
    Task **ref;
} GraphRef;

typedef struct Graph {
    Task task;
    Vec3d pos;          /* relative to the parent when attached */
    char unk38[0x18];
    int id;
    int unk54;
    int capacity;
    int nodeCount;
    int linkCount;
    char attached;      /* parent is a RigidDynCubeObj */
    char unk65;         /* PushButton parameter */
    char unk66[2];
    char unk68;         /* two Bool8 parameters */
    char unk69;
    char unk6A[2];
    char *nodes;
    List links;
} Graph;
_Static_assert(sizeof(Graph) == 0x80, "Graph");

typedef struct GraphLink {
    Node node;
    int a;
    int b;
    int cost;
    struct Graph *graph;
} GraphLink;

typedef struct GraphNode {
    int id;             /* -1 for a free slot */
    int unk4;
    Vec3d pos;
    float gamma;
    float radius;
    float diameter;
    int unk2C;
    int flags;          /* the criteria text, evaluated */
    char criteria[0x100];
    struct Graph *graph;
    void *material;
    struct GraphLink *links[64];
} GraphNode;
_Static_assert(sizeof(GraphNode) == 0x240, "GraphNode");

#endif
