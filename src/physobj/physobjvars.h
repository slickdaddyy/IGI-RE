#ifndef PHYSOBJ_PHYSOBJVARS_H
#define PHYSOBJ_PHYSOBJVARS_H

#include <stdint.h>

#include "level/leveltypes.h"
#include "types.h"

extern uint16_t gLevelType;
extern uint16_t gStaticType;
extern uint16_t gDynamicType;
extern uint16_t gIdleType;
extern int gLevelActivateEvent;
extern int gLevelDeactivateEvent;
extern int gLevelUnusedNewTaskEvent;
extern struct Task *gModelLODSettingsContainer;
extern int gParamReal32x3;
extern int gParamReal32x9;
extern int gParamDropDownCombo;
extern int gParamInt16;
extern int gParamInt32;
extern int gParamGraphEnvelope;
extern int gParamAngle;
extern int gParamObjectPos;
extern int gParamScriptFile;
extern int gParamBool8;
extern int gParamVarString;
extern int gParamGraph;
extern int gParamDegrees;
extern List gTaskNotes[LEVEL_NOTE_LISTS];
extern int gParamString256;
extern int gParamReal32;
extern int gParamReal64;
extern int gParamQuat32;
extern int gParamPitchGraph;
extern int gParamRGB;
extern int gParamString16;
extern int gParamString32;
extern int gParamLevelExpression;
extern int gParamReal64x3;
extern int gParamQTaskNote;
extern int gParamPushButton;
extern int gParamBitfield;
extern char gLevelPaused;
extern char gLevelForceRunning;

#endif
