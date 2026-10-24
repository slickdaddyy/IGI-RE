#ifndef COMPRUN_COMPRUN_H
#define COMPRUN_COMPRUN_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "magicobj/magicobjtypes.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)(void *task, void *arg);

/* what a picker passes to the touch handlers */
typedef struct Toucher {
    Task *task;
    int unk4;
    Vec3d pos;
} Toucher;

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;

extern const char gScriptEmptyString[];
#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

/* calls fn(child, arg) on each child of t (not deeper), with fn read from the
   slot given by ev, evaluated once per child. The next child is kept on
   gTaskStack so handlers may delete tasks. */
#define CALL_CHILDREN_EV(t, ev, arg)                                    \
    do {                                                                \
        Task *c_ = ((Task *)(t))->head;                                 \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = gTaskHandlers[(uint8_t)(ev)][c_->type];               \
            if (fn_)                                                    \
                fn_(c_, (void *)(arg));                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* the same for the run slot, called with the child only */
#define CALL_CHILDREN_RUN(t)                                            \
    do {                                                                \
        Task *c_ = ((Task *)(t))->head;                                 \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = gTaskHandlers[1][c_->type];                           \
            if (fn_)                                                    \
                ((void (*)(void *))fn_)(c_);                            \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

/* the largest of the three scale factors */
static inline float max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
