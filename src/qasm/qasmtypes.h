#ifndef QASM_QASMTYPES_H
#define QASM_QASMTYPES_H

#include "task/tasktypes.h"

typedef struct QTaskList {
    int count;
    int max;
    Task *items[];
} QTaskList;

typedef struct QvmLabelRef {
    Node node;
    int offset;
    int base;
} QvmLabelRef;

typedef struct QvmFixup {
    Node node;
    int offset;
    int value;
} QvmFixup;

#endif
