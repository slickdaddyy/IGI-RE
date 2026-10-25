#ifndef LOD_LODVARS_H
#define LOD_LODVARS_H

#include <stdint.h>

#include "boneobj/boneobjtypes.h"
#include "glowmesh/glowmeshtypes.h"
#include "lod/lodtypes.h"
#include "types.h"

extern float gLodBlend;
extern int gQCubeDrawOutside;
extern float gLodRigidDetail;
extern float gLodBoneDetail;
extern float gLodSplineDetail;
extern float gLodRigidCull;
extern float gLodBoneCull;
extern float gLodSplineCull;
extern float gLodRange;
extern int gQCubeInside;
extern struct DynCubeObj *gQCubeEnclosing;
extern struct Sector *gViewSector;
extern Skeleton gSkeletons[];
extern Pool gSourcePool;
extern Pool gOrientPool;
extern Pool gInstPool;
extern Pool gFadePool;
extern float gTicksPerFrame;
extern Pool gNodePool;
extern QCacheEntry gQCubeCache[];
extern int gQCubeSkipCells;
extern int gQCubeOrigin[];
extern uint8_t gQCubeChildOrder[][8];
extern int gQCubeCount;
extern QEntry *gQCubeOut;
extern int gTerrainMeshCount;
extern int gTerrainCellCount;
extern TerrainCell gTerrainCells[];
extern float gExclLeft;
extern float gExclBottomCos;
extern float gExclTopCos;
extern float gExclRight;
extern float gExclRightCos;
extern float gExclTop;
extern float gLayerFov[];
extern int gRenderModeUVFromGenericUVMethod;
extern float gExclBottom;
extern int gGenericUVFromRenderModeUVMethod;
extern float gExclLeftCos;
extern int gVisibleLightMaps[];
extern int gDisplayFrame;

#endif
