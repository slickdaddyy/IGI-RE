#ifndef COMPRUN_COMPRUN_H
#define COMPRUN_COMPRUN_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

FN_EXTERN(Task_Nothing);

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)(void *task, void *arg);

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

/* 3x3 rotation, row major, and a counter the engine bumps on changes */
typedef struct Matrix {
    float m[9];
    int stamp;
} Matrix;

/* a script string parameter that may hold an expression */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

/* "RigidDynCubeObj" (0xF0 bytes), a moving object with a model */
typedef struct RigidObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    void *world;
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0x44];
    Task *child; /* 0xE8 */
    int unkEC;
} RigidObj;

/* what a picker passes to the touch handlers */
typedef struct Toucher {
    Task *task;
    int unk4;
    Vec3d pos;
} Toucher;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(sizeof(RigidObj) == 0xF0, "RigidObj");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)

#define gScriptEmptyString ((const char *)0x567C74)
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
static inline float max_scale(const RigidObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
