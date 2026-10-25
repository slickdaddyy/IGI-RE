#ifndef TASK_TASKTYPES_H
#define TASK_TASKTYPES_H

#include <stdint.h>

#include "types.h"

#define TASK_MAX_TYPES 0x180

typedef struct TaskType {
    const char *name;
    int size;
    uint16_t parent;
    uint16_t unk0A;
    Pool pool;
    char pooled;
    char used;
    char unk26[2];
    uint16_t mask;
    uint16_t unk2A;
    uint32_t flags;
} TaskType;
_Static_assert(sizeof(TaskType) == 0x30, "TaskType");

/* the first 0x20 bytes of every task. node and children are what the list
   functions take; next ... tailpred are the same links typed as tasks */
typedef struct Task {
    union {
        Node node;
        struct {
            struct Task *next;
            struct Task *prev;
        };
    };
    union {
        List children;
        struct {
            struct Task *head;
            struct Task *tail;
            struct Task *tailpred;
        };
    };
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;
_Static_assert(sizeof(Task) == 0x20, "Task");

#endif
