#ifndef PATROL_PATROLTYPES_H
#define PATROL_PATROLTYPES_H

#include "types.h"

/* nearest graph node of one target, refreshed once per tick */
typedef struct MonitorTarget {
    Node node;
    int nodeId;
    int tick;
    void *target;
} MonitorTarget;

typedef struct Claim {
    Node node;
    void *task;
} Claim;

typedef struct NodeGrid {
    Node node;
    int graphId;
    int nodeId;
    int refs;
    int w, h;
    int unk1C;
    int cells[20][20];
} NodeGrid;

typedef struct MonitorGraph {
    Node node;
    struct Graph *graph;
    List targets;
} MonitorGraph;

/* {path, cur, end, move, prevMove, ...}, see the patrol notes */
typedef struct PatrolRoute {
    struct PatrolPath *path;
    struct PatrolCommand *cur;
    struct PatrolCommand *end;
    struct PatrolCommand *move;
    struct PatrolCommand *prevMove;
    char crouch;
    char walk;
    char unk16[2];
    int timer;
} PatrolRoute;

#endif
