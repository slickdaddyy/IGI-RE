#ifndef CAMERA_CAMERA_H
#define CAMERA_CAMERA_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

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

/* 3x3 rotation, row major, plus a serial number */
typedef struct Matrix {
    float m[9];
    int serial;
} Matrix;

/* a hit of the collision queries, 0x70 bytes */
typedef struct CollHit {
    Task *task;
    int unk4;
    Vec3d pos;
    char unk20[0x18];
    float normal[3];
    int unk44;
    int unk48;
    float *material;
    int unk50;
    int unk54;
    int found;
    int unk5C;
    double dist;
    char unk68[8];
} CollHit;

/* the second word is the object the lines ignore */
typedef struct CollFilter {
    int unk0;
    Task *ignore;
} CollFilter;

typedef struct Explosion Explosion;

Explosion *Explosion_Spawn(Vec3d *pos, int count, int delay, float unk50, float radius, float unk58,
                           float burnSize, int unk68, int unk6C, Task **burn, const char *sprite, const char *sound,
                           int noiseType, int noiseUnk8, float noiseRadius, int unk84);

_Static_assert(sizeof(CollHit) == 0x70, "CollHit");
_Static_assert(offsetof(CollHit, found) == 0x58, "CollHit.found");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)

/* the next child of a broadcast, NULL after the last one */
#define NEXT_CHILD(c) ((c)->next && (c)->next->next ? (c)->next : NULL)

/* calls handler slot of each child of t (not deeper); slot is evaluated
   for each child after the push, like the original */
#define CALL_CHILDREN(t, slot, ...)                                     \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = NEXT_CHILD(c_);               \
            fn_ = gTaskHandlers[slot][c_->type];                        \
            if (fn_)                                                    \
                ((void (*)())fn_)(c_, ##__VA_ARGS__);                   \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode */
static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

static inline long double x87_tan(long double x)
{
    __asm__("fptan\n\tfstp %%st(0)" : "+t"(x));
    return x;
}

/* fcomp then "test ah, 0x40": equal, or unordered */
static inline int x87_eq(long double a, long double b)
{
    return !(a < b || a > b);
}

#endif
