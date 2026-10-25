#ifndef LEVEL_LEVEL_H
#define LEVEL_LEVEL_H

#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "script/scripttypes.h"
#include "physobj/physobjtypes.h"
#include "level/leveltypes.h"

typedef void (*TaskFn)(Task *task);

extern Task *gLevelFirstTask;
extern Task *gLevelDynamic;
extern Task *gLevelStatic;
extern Task *gLevelIdle;
extern Task *gLevelCameraBase;
extern char gLevelInsertHead;
extern void *gLevelModels;
extern const char *gLevelScriptFile;
extern int gLevelScriptLine;
extern char gLevelTaskNewEnabled;

Task *Task_FindById(int id);
int Task_GetId(Task *task);
void Level_SetTaskId(int id, Task *task);
void TaskNote_Free(TaskNote *note);
void ScriptInit(const char *file, char taskNewEnabled, int line, char insertHead);
void Level_Load(const char *path, int fps);

#endif
