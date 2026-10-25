#ifndef GLASS_GLASSVARS_H
#define GLASS_GLASSVARS_H

#include <stdint.h>

#include "glass/glasstypes.h"

extern const float *gShatterColor;
extern float gShatterSpeed;
extern uint16_t gShatterType;
extern int gShatterMode;
extern GlassShatter *gShatterTask;
extern ShardPoly *gShatterPolys;
extern Shard *gShatterShards;

#endif
