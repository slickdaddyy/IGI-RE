#ifndef PATROL_H
#define PATROL_H

#include <stddef.h>
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
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    struct Task *tail;
    struct Task *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    uint16_t pooled;
} Task;

typedef void (*TaskHandler)(void *task, void *arg);

#define TASK_HANDLER(slot, type) (((TaskHandler *)0xA96AE0)[(slot) * 0x180 + (type)])

ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, Node *node));
ORIG(0x4AF960, void, List_Remove, (Node *node));

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

#endif
