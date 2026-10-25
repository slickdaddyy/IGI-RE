#ifndef LEVEL_LEVEL_H
#define LEVEL_LEVEL_H

#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "script/scripttypes.h"
#include "physobj/physobjtypes.h"
#include "level/leveltypes.h"

typedef void (*TaskFn)(Task *task);

extern void * gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;

extern Task **gLevelTaskIds[];
extern List gTaskNotes[];
extern Level *gLevel;
extern Task *gLevelFirstTask;
extern Task *gLevelDynamic;
extern Task *gLevelStatic;
extern Task *gLevelIdle;
extern Task *gLevelCameraBase;
extern char gLevelInsertHead;
extern char gLevelPaused;
extern void *gLevelModels;
extern const char *gLevelScriptFile;
extern int gLevelScriptLine;
extern char gLevelTaskNewEnabled;

extern uint16_t gLevelType;
extern uint16_t gStaticType;
extern uint16_t gDynamicType;
extern uint16_t gIdleType;
extern uint16_t gCameraBaseType;
extern uint8_t gLevelEventDA4;
extern uint8_t gLevelNameEvent;
extern int gLevelEventDA8;
extern int gLevelEventDAC;
extern int gLevelEventDB0;

/* parameter type ids */
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

Task *Task_FindById(int id);
int Task_GetId(Task *task);
void Level_SetTaskId(int id, Task *task);
void TaskNote_Free(TaskNote *note);
void ScriptInit(const char *file, char taskNewEnabled, int line, char insertHead);
void Level_Load(const char *path, int fps);

#endif
