#ifndef PARAM_PARAM_H
#define PARAM_PARAM_H

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

typedef struct Task {
    Node node;
    List children;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef struct Level {
    Task task;
    int unk20;
    int time;
    int tick;
} Level;

/* a text note attached to a task, kept in a hash of lists by task address */
typedef struct TaskNote {
    Node node;
    Task **ref;
    char text[0x41];
} TaskNote;

/* A script expression kept as text. Bind expands "this." and task ids
   and keeps references to the tasks it names. */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    Task **refs[16];
    int refCount;
} VarString;

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

#define PARAM_MAX_TYPES 64
#define PARAM_MAX_OPS 16

typedef void (*ParamOpFn)(void *field, void *arg);

typedef struct ParamType {
    const char *name;
    int parent;
    ParamOpFn ops[PARAM_MAX_OPS];
    int used;
} ParamType;

typedef struct ScriptArgs ScriptArgs;

/* where a parameter parser is in the arguments of a script Task_New call */
typedef struct ParamCursor {
    int index;
    ScriptArgs *args;
    const char *file;
    int line;
} ParamCursor;

typedef struct ParamRange {
    float min, max;
} ParamRange;

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

typedef struct ScriptSymbol {
    int kind;
    int valueType;
    int unk08;
    int size;
    void *handler;
    const char *name;
    int data;
} ScriptSymbol;

/* what a weapon's state event handler points to */
typedef struct WeaponState {
    char selected;
    char unk01[3];
    int slot;
    char unk08;
} WeaponState;

typedef void (*TaskFn)(Task *task);
typedef void (*TaskArgFn)(Task *task, void *arg);

extern void * gTaskHandlers[][0x180];
#define HANDLER(slot, task) (gTaskHandlers[slot][(task)->type])

#define LEVEL_MAX_IDS 0x1000
extern Task **gLevelTaskIds[];
extern List gTaskNotes[];
extern Level *gLevel;
extern char gLevelPaused;
extern char gLevelA758BE;

extern uint8_t gLevelEventDA4;
extern uint8_t gLevelNameEvent;
extern int gLevelEventDA8;
extern int gLevelEventDAC;
extern int gLevelEventDB0;

extern char gScriptEmptyString[];

#endif
