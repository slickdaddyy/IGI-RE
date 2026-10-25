#ifndef GRAPHTASK_GRAPHTASKVARS_H
#define GRAPHTASK_GRAPHTASKVARS_H

#include <stdint.h>

#include "types.h"

extern uint16_t gGraphType;
extern void *gGraphParams;
extern List gStreamHandlersFree;
extern List gGraphRefs;
extern List gStreamHandlers;

#endif
