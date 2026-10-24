#ifndef GRAPHNODE_H
#define GRAPHNODE_H

#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "magicobj/magicobjtypes.h"

typedef void (*TaskHandler)(void *task, void *arg);

extern TaskHandler gTaskHandlers[];
#define TASK_HANDLER(slot, type) gTaskHandlers[(slot) * 0x180 + (type)]
#define TASK_NO_TYPE 0x181

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
