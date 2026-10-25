#ifndef NODEREG_NODEREGVARS_H
#define NODEREG_NODEREGVARS_H

#include <stdint.h>

#include "types.h"

extern uint16_t gSoundManagerType;
extern struct Terrain *gCurrentWorld;
extern void *gSoundManagerParams;
extern Vec3f gAmbient[5];
extern float gLightColor[];
extern float gWaterTint[];

#endif
