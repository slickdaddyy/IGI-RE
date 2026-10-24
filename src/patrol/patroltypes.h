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

#endif
