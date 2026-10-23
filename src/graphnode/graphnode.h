#ifndef GRAPHNODE_H
#define GRAPHNODE_H

#include <stdint.h>

#include "igi.h"

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24;
} Matrix;

typedef struct Node {
    struct Node *next;
    struct Node *prev;
} Node;

typedef struct List {
    Node *head;
    Node *tail;
    Node *tailpred;
} List;

typedef struct Task {
    Node node;
    List children;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskHandler)(void *task, void *arg);

#define gTaskHandlers ((TaskHandler *)0xA96AE0)
#define TASK_HANDLER(slot, type) gTaskHandlers[(slot) * 0x180 + (type)]
#define TASK_NO_TYPE 0x181

/* the parts of a RigidDynCubeObj used here */
typedef struct RigidDynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0x38];
    Matrix matrix;
    int unk98;
} RigidDynCubeObj;

/* Graph task: a node array indexed by node id plus a list of links */
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
    char unk65[4];
    char unk69[3];
    char *nodes;
    List links;
} Graph;

typedef struct GraphLink {
    Node node;
    int a;
    int b;
    int cost;
    Graph *graph;
} GraphLink;

#define GRAPHNODE_LINKS 64

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
    Graph *graph;
    void *material;
    GraphLink *links[GRAPHNODE_LINKS];
} GraphNode;

typedef struct GraphStream {
    int unk0;
    void *file;
} GraphStream;

/* passed to a GraphStream handler while a tagged graph file is read */
typedef struct GraphStreamValue {
    GraphStream *stream;
    Graph *graph;
    int unk8;
    int unkC;
    int type;
} GraphStreamValue;

typedef struct GraphStreamHandler {
    Node node;
    int hash;
    int type;
    void (*fn)(GraphStreamValue *v);
} GraphStreamHandler;

int Graph_GetNodeSize(Graph *g);
GraphNode *Graph_AddNode(Graph *g, int id, const Vec3d *pos, float gamma, float radius, void *material,
                         const char *criteria);
void Graph_ClearNodes(Graph *g, int count);
GraphNode *Graph_GetNode(Graph *g, int id);
GraphLink *Graph_AddLink(Graph *g, int a, int b, int cost);
uint8_t Graph_GetAllocEvent(void);
uint8_t Graph_GetLoadEvent(void);
uint8_t Graph_GetNodeLoadEvent(void);

char GraphStream_ReadByte(GraphStream *s, uint8_t *out);
char GraphStream_ReadInt(GraphStream *s, int *out);
char GraphStream_ReadFloat(GraphStream *s, float *out);
char GraphStream_ReadVec3d(GraphStream *s, Vec3d *out);
char GraphStream_ReadString(GraphStream *s, char *out, int max);
char GraphStream_WriteUByte(GraphStream *s, uint8_t v);
char GraphStream_WriteInt(GraphStream *s, int v);

#endif
