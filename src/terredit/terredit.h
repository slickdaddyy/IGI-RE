#ifndef TERREDIT_TERREDIT_H
#define TERREDIT_TERREDIT_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "magicobj/magicobjtypes.h"

typedef void (*TaskFn)(void *task, void *arg);

typedef struct Terrain {
    char unk0[0xDE0];
    int offset[3];
} Terrain;

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern Terrain *gCurrentWorld;

/* the handler a task type has for a slot */
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* next sibling, or NULL at the end of the list */
static inline Task *next_sibling(Task *t)
{
    return t->next && t->next->next ? t->next : NULL;
}

/* calls the handler for the slot that GETSLOT() returns (called again for
   each child) on each child of t, keeping the next child on gTaskStack so
   handlers may delete tasks */
#define CALL_CHILDREN(t, GETSLOT, arg)                                  \
    do {                                                                \
        Task *c_ = ((Task *)(t))->head;                                 \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = HANDLER(GETSLOT, c_->type);                           \
            if (fn_)                                                    \
                fn_(c_, (arg));                                         \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

static inline long double x87_sin(long double x)
{
    __asm__("fsin" : "+t"(x));
    return x;
}

static inline long double x87_cos(long double x)
{
    __asm__("fcos" : "+t"(x));
    return x;
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

/* fchs, kept apart from the rounding of the result */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

static inline long double x87_abs(long double x)
{
    __asm__("fabs" : "+t"(x));
    return x;
}

/* atan2(y, x) as fpatan */
static inline long double x87_atan2(long double y, long double x)
{
    long double r;

    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

/* the unrotated part of an ObjInfo */
static inline void obj_info_identity(ObjInfo *info)
{
    info->velocity = (Vec3f){0};
    info->matrix.m[0] = 1.0f;
    info->matrix.m[1] = 0;
    info->matrix.m[2] = 0;
    info->matrix.m[3] = 0;
    info->matrix.m[4] = 1.0f;
    info->matrix.m[5] = 0;
    info->matrix.m[6] = 0;
    info->matrix.m[7] = 0;
    info->matrix.m[8] = 1.0f;
    info->matrix.count = 0;
}

#endif
