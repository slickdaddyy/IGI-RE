#ifndef QASM_QASMTYPES_H
#define QASM_QASMTYPES_H

#include "task/tasktypes.h"

typedef struct QTaskList {
    int count;
    int max;
    Task *items[];
} QTaskList;

#endif
