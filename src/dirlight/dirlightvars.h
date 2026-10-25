#ifndef DIRLIGHT_DIRLIGHTVARS_H
#define DIRLIGHT_DIRLIGHTVARS_H

#include <stdint.h>

#include "types.h"

extern int gLightmapFlaggedTexelCount;
extern Pool gLightmapInfoPool;
extern int *gLightmapBuffer;
extern int gLightmapTexelCount;
extern int gLightmapBuilt;
extern void *gDirlightParams;
extern uint16_t gDirlightType;
extern void *gDirlightKeyframeParams;
extern uint16_t gDirlightKeyframeType;

#endif
