#ifndef GRAPHNODE_H
#define GRAPHNODE_H

#include <stdint.h>

#include "igi.h"
#include "magicobj/magicobjtypes.h"
#include "graphnode/graphnodetypes.h"
#include "task/taskvars.h"

typedef void (*TaskHandler)(void *task, void *arg);

#define TASK_HANDLER(slot, type) gTaskHandlers[(slot)][(type)]
#define TASK_NO_TYPE 0x181

#define GRAPHNODE_LINKS 64

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
