#ifndef EDITOBJ_EDITOBJ_H
#define EDITOBJ_EDITOBJ_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"

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

/* "RigidDynCubeObj" (0xF0 bytes), the fields used here */
typedef struct Obj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    void *world;
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0xF0 - 0xA4];
} Obj;

/* filled by the slot 7 (GetInfo) handlers */
typedef struct ObjInfo {
    Vec3d pos;
    int unk18[3];
    Matrix matrix;
    float radius;
    char *name;
} ObjInfo;

/* sent to children with the MagicObj move event */
typedef struct MoveArg {
    Vec3d *pos;
    Matrix *matrix;
} MoveArg;

typedef struct RangeI {
    int min, max;
} RangeI;

_Static_assert(sizeof(Obj) == 0xF0, "Obj");
_Static_assert(offsetof(ObjInfo, radius) == 0x4C, "ObjInfo");

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void *gCurrentWorld;

/* calls handler slot of each child of t (not deeper) with the given
   arguments; slot is evaluated for each child like in the original */
#define CALL_CHILDREN(t, slot, ...)                                     \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = c_->next && c_->next->next ? c_->next : NULL; \
            fn_ = gTaskHandlers[slot][c_->type];                        \
            if (fn_)                                                    \
                ((void (*)())fn_)(c_, ##__VA_ARGS__);                   \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode, like the original */
static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* the largest of the three scale factors */
static inline float max_scale(const Obj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
