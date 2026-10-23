#ifndef LEVEL_LEVEL_H
#define LEVEL_LEVEL_H

#include <stdint.h>

#include "igi.h"

typedef struct Node {
    struct Node *next;
    struct Node *prev;
} Node;

typedef struct List {
    Node *head;
    Node *tail;
    Node *tailpred;
} List;

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

#define gTaskHandlers ((void *(*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)

#define LEVEL_MAX_IDS 0x1000
#define LEVEL_NOTE_LISTS 251

#define gLevelTaskIds ((Task ***)0xA71890)
#define gTaskNotes ((List *)0xA70CA8)
#define gLevel VAR(Level *, 0xA758A4)
#define gLevelFirstTask VAR(Task *, 0xA758A8)
#define gLevelDynamic VAR(Task *, 0xA758AC)
#define gLevelStatic VAR(Task *, 0xA758B0)
#define gLevelIdle VAR(Task *, 0xA758B4)
#define gLevelCameraBase VAR(Task *, 0xA758B8)
#define gLevelInsertHead VAR(char, 0xA758BC)
#define gLevelPaused VAR(char, 0xA758BD)
#define gLevelModels VAR(void *, 0xA70C94)
#define gLevelScriptFile VAR(const char *, 0xA70C98)
#define gLevelScriptLine VAR(int, 0xA70C74)
#define gLevelTaskNewEnabled VAR(char, 0x54DD16)

#define gLevelType VAR(uint16_t, 0x54DD0C)
#define gStaticType VAR(uint16_t, 0x54DD0E)
#define gDynamicType VAR(uint16_t, 0x54DD10)
#define gIdleType VAR(uint16_t, 0x54DD12)
#define gCameraBaseType VAR(uint16_t, 0x54DD14)
#define gLevelEvent0 VAR(uint8_t, 0x54DDA4)
#define gLevelEvent1 VAR(uint8_t, 0x54DDA5)
#define gLevelEventDA8 VAR(int, 0x54DDA8)
#define gLevelEventDAC VAR(int, 0x54DDAC)
#define gLevelEventDB0 VAR(int, 0x54DDB0)

/* parameter type ids */
#define gParamReal32x3 VAR(int, 0xA70C68)
#define gParamReal32x9 VAR(int, 0xA70C6C)
#define gParamDropDownCombo VAR(int, 0xA70C70)
#define gParamInt16 VAR(int, 0xA70C78)
#define gParamInt32 VAR(int, 0xA70C7C)
#define gParamGraphEnvelope VAR(int, 0xA70C80)
#define gParamAngle VAR(int, 0xA70C84)
#define gParamObjectPos VAR(int, 0xA70C88)
#define gParamScriptFile VAR(int, 0xA70C8C)
#define gParamBool8 VAR(int, 0xA70C90)
#define gParamVarString VAR(int, 0xA70C9C)
#define gParamGraph VAR(int, 0xA70CA0)
#define gParamDegrees VAR(int, 0xA70CA4)
#define gParamString256 VAR(int, 0xA7186C)
#define gParamReal32 VAR(int, 0xA71870)
#define gParamReal64 VAR(int, 0xA71874)
#define gParamQuat32 VAR(int, 0xA71878)
#define gParamPitchGraph VAR(int, 0xA7187C)
#define gParamRGB VAR(int, 0xA71880)
#define gParamString16 VAR(int, 0xA71884)
#define gParamString32 VAR(int, 0xA71888)
#define gParamLevelExpression VAR(int, 0xA7188C)
#define gParamReal64x3 VAR(int, 0xA75890)
#define gParamQTaskNote VAR(int, 0xA75894)
#define gParamPushButton VAR(int, 0xA75898)
#define gParamBitfield VAR(int, 0xA758A0)

Task *Task_FindById(int id);
int Task_GetId(Task *task);
void Level_SetTaskId(int id, Task *task);
void TaskNote_Free(TaskNote *note);
void ScriptInit(const char *file, char taskNewEnabled, int line, char insertHead);
void Level_Load(const char *path, int fps);

#endif
