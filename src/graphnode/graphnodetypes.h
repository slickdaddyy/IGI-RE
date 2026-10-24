#ifndef GRAPHNODE_GRAPHNODETYPES_H
#define GRAPHNODE_GRAPHNODETYPES_H

#include "types.h"

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

#endif
