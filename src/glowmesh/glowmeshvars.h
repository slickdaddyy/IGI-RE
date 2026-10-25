#ifndef GLOWMESH_GLOWMESHVARS_H
#define GLOWMESH_GLOWMESHVARS_H

#include <stdint.h>

#include "glowmesh/glowmeshtypes.h"
#include "types.h"

extern float gTerrainFogScale;
extern float gTerrainFogColor[];
extern float gClipZBias;
extern Pool gTerrainChunkPool;
extern float gClipZScale;
extern Pool gGlowPool;
extern Pool gQCubePool0;
extern double gQCubeOriginD[];
extern Pool gQCubePool1;
extern TerrainMesh *gTerrainMeshes[];
extern int gQCubeMinSize;
extern Pool gQCubePool2;
extern float gClipRhwScale;
extern float gLayer1MaxZ;
extern float gLayer1MinZ;
extern float gFarZ;
extern int gTerrMeshTotal;
extern float gMorphScale;
extern uint8_t gTerrainTileMaps[][8][8];
extern int gMorphStart;

#endif
