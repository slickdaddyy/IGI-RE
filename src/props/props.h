#ifndef PROPS_PROPS_H
#define PROPS_PROPS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "magicobj/magicobjtypes.h"
#include "graphtask/graphtasktypes.h"
#include "props/propstypes.h"
#include "task/taskvars.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)();

/* the collision filter message of 0x4FC0E0 */
typedef struct CollMsg {
    int unk0;
    void *obj;
} CollMsg;

extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

/* copies 4 bytes without going through the FPU, like the original */
#define MOV32(dst, src) memcpy(&(dst), &(src), 4)

/* calls the slot handler of each child of t, keeping the next child on
   gTaskStack so handlers may delete tasks; the slot is read after the push */
#define BROADCAST(t, SLOT, ...)                                         \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = HANDLER(SLOT, c_->type);                              \
            if (fn_)                                                    \
                fn_(c_, ##__VA_ARGS__);                                 \
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

/* the largest of the three scale factors, compared like the original */
static inline float max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

/* out = a * b. Each element sums three products p0..p2 (p_k = a[i][k] * b[k][j]);
   rows set in b_rows add (p1 + p2) + p0, the others (p0 + p2) + p1. */
static inline void mat_mul(Matrix *out, const Matrix *a, const Matrix *b, int b_rows)
{
    Matrix n;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            long double p0 = (long double)a->m[i * 3] * b->m[j];
            long double p1 = (long double)a->m[i * 3 + 1] * b->m[3 + j];
            long double p2 = (long double)a->m[i * 3 + 2] * b->m[6 + j];

            n.m[i * 3 + j] = b_rows & (1 << i) ? (p1 + p2) + p0 : (p0 + p2) + p1;
        }
    }
    n.count = a->count + b->count + 1;
    *out = n;
}

#endif
