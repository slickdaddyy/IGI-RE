#ifndef PARAM_PARAMTYPES_H
#define PARAM_PARAMTYPES_H

#include "types.h"
#include "task/tasktypes.h"

/* A script expression kept as text. Bind expands "this." and task ids
   and keeps references to the tasks it names. */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    struct Task **refs[16];
    int refCount;
} VarString;
_Static_assert(sizeof(VarString) == 0x50, "VarString");

/* min and max of a script parameter */
typedef struct RangeF {
    float min, max;
} RangeF;

typedef struct RangeI {
    int min, max;
} RangeI;

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

/* a text note attached to a task, kept in a hash of lists by task address */
typedef struct TaskNote {
    Node node;
    Task **ref;
    char text[0x41];
} TaskNote;

/* argument of a script parse callback */
typedef struct ScriptArg {
    void *out;
    void *args;
    int index;
} ScriptArg;

#endif
