#ifndef SOUND_SOUNDVARS_H
#define SOUND_SOUNDVARS_H

#include <stdint.h>

#include "sound/soundtypes.h"

extern uint16_t gDynCubeObjType;
extern int gCellMaskEnabled;
extern struct SphereNode *gSphereStack[];
extern float *gSphereVerts;
extern SegBuffer *gSphereNodes;
extern struct SphereMesh *gSphereMesh;
extern int gSphereDepth;
extern struct QTaskList *gSoundTasks;
extern struct QTaskList *gSoundDefs;

#endif
