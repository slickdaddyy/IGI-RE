#ifndef FREECAM_H
#define FREECAM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "task/tasktypes.h"
#include "transctx/transctxtypes.h"
#include "task/taskvars.h"

typedef void (*TaskHandler)();

extern Task *gTaskStack[];
extern int gTaskStackTop;
#define TASK_HANDLER(slot, type) (((TaskHandler *)gTaskHandlers)[(slot) * 0x180 + (type)])
extern struct Terrain *gCurrentWorld;
extern uint16_t gDynCubeObjType;

static inline Task *fc_next_sibling(Task *t)
{
    return t->node.next && t->node.next->next ? (Task *)t->node.next : NULL;
}

/* calls the handler of each task in a child list, keeping the next one on
   gTaskStack so handlers may delete tasks; the slot is read after the push */
#define FC_BROADCAST(first, SLOT, ...)                                  \
    do {                                                                \
        Task *c_ = (first);                                             \
        if (!c_->node.next || !c_)                                      \
            break;                                                      \
        do {                                                            \
            TaskHandler fn_;                                            \
            gTaskStack[gTaskStackTop++] = fc_next_sibling(c_);          \
            fn_ = TASK_HANDLER(SLOT, c_->type);                         \
            if (fn_)                                                    \
                fn_(c_, ##__VA_ARGS__);                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* x87 "not equal" as tested with C3: false when unordered */
static inline int fc_fne(long double a, long double b)
{
    return a < b || a > b;
}

/* fistp with the current rounding mode, low 32 bits */
static inline uint32_t fc_fistp(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (uint32_t)r;
}

#endif
