#ifndef PATROL_H
#define PATROL_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "task/taskvars.h"

typedef void (*TaskHandler)(void *task, void *arg);

#define TASK_HANDLER(slot, type) (((TaskHandler *)gTaskHandlers)[(slot) * 0x180 + (type)])

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
