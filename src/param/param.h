#ifndef PARAM_PARAM_H
#define PARAM_PARAM_H

#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "script/scripttypes.h"
#include "level/leveltypes.h"
#include "task/taskvars.h"

#define PARAM_MAX_TYPES 64
#define PARAM_MAX_OPS 16

typedef void (*ParamOpFn)(void *field, void *arg);

typedef struct ParamType {
    const char *name;
    int parent;
    ParamOpFn ops[PARAM_MAX_OPS];
    int used;
} ParamType;

/* script constants whose names match one of the regex patterns */
typedef struct ParamEnum {
    List patterns;
    void (*forEach)(void *ctx);
} ParamEnum;

typedef struct ParamEnumPattern {
    Node node;
    char prog[0x2800];
    char data[0x2000];
    const char *pattern;
} ParamEnumPattern;

typedef struct ParamEnumValue {
    int type;
    const char *name;
    int *value;
} ParamEnumValue;

typedef struct ParamEnumVisitor {
    int unk0;
    void (*add)(struct ParamEnumVisitor *v, ParamEnumValue *value);
} ParamEnumVisitor;

/* what a weapon's state event handler points to */
typedef struct WeaponState {
    char selected;
    char unk01[3];
    int slot;
    char unk08;
} WeaponState;

typedef void (*TaskFn)(Task *task);
typedef void (*TaskArgFn)(Task *task, void *arg);

#define HANDLER(slot, task) (gTaskHandlers[slot][(task)->type])

extern Task **gLevelTaskIds[];
extern List gTaskNotes[LEVEL_NOTE_LISTS];
extern Level *gLevel;
extern char gLevelPaused;
extern char gLevelForceRunning;

extern uint8_t gLevelDebugInfoEvent;
extern uint8_t gLevelNameEvent;
extern int gLevelActivateEvent;
extern int gLevelDeactivateEvent;
extern int gLevelUnusedNewTaskEvent;

extern const char gEmptyString[];

#endif
