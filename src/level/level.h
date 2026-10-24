#ifndef LEVEL_LEVEL_H
#define LEVEL_LEVEL_H

#include <stdint.h>

#include "igi.h"
#include "types.h"

/* the first 0x20 bytes of every task */
typedef struct Task {
    Node node;
    List children;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

/* the "Level" task, root of everything a level script creates */
typedef struct Level {
    Task task;
    int unk20;
    int time;
    int tick;
    int fps;
    int msPerTick;
    int unk34;
} Level;

typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *s;
    int unk14;
} ScriptValue;

typedef struct ScriptArgs {
    int count;
    void *vm;
    void **argv;
} ScriptArgs;

/* where a parameter parser is in the arguments of a script Task_New call */
typedef struct ParamCursor {
    int index;
    ScriptArgs *args;
    const char *file;
    int line;
} ParamCursor;

/* output of the parameter format operation, appended to buf */
typedef struct ParamText {
    int unk0;
    char *buf;
} ParamText;

typedef struct ParamEntry {
    int offset;
    int type;
    void *extra;
    char flags;
    char unk0D[3];
} ParamEntry;

typedef struct ParamTable {
    int count;
    int max;
    ParamEntry *entries;
} ParamTable;

typedef struct VarString {
    char *text;
} VarString;

/* a text note attached to a task, kept in a hash of lists by task address */
typedef struct TaskNote {
    Node node;
    Task **ref;
    char text[0x41];
} TaskNote;

typedef void (*TaskFn)(Task *task);

extern void * gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;

#define LEVEL_MAX_IDS 0x1000
#define LEVEL_NOTE_LISTS 251

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
extern uint8_t gLevelEvent0;
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
