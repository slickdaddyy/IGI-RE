#ifndef PARAM_PARAMTYPES_H
#define PARAM_PARAMTYPES_H

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

#endif
