#ifndef SECTOR_SECTORVARS_H
#define SECTOR_SECTORVARS_H

#include <stdint.h>

#include "transctx/transctxtypes.h"
#include "types.h"

extern uint16_t gSectorType;
extern uint16_t gSectorObjectType;
extern uint16_t gSectorInteriorObjectType;
extern Pool gVisSectors;
extern float gQCubeOutsideMin[];
extern float gQCubeOutsideMax[];
extern Scene gScene;

#endif
