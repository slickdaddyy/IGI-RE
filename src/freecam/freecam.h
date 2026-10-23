#ifndef FREECAM_H
#define FREECAM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"

typedef struct FcNode {
    struct FcNode *next;
    struct FcNode *prev;
} FcNode;

typedef struct FcList {
    FcNode *head;
    FcNode *tail;
    FcNode *tailpred;
} FcList;

typedef struct Task {
    FcNode node;
    FcList children;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

/* fixed size item pool; slots[] maps a slot number to an item number */
typedef struct Pool {
    char *data;
    int *slots;
    int used;
    int capacity;
    int stride;
    int index_offset;
} Pool;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef void (*TaskHandler)();

#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define TASK_HANDLER(slot, type) (((TaskHandler *)0xA96AE0)[(slot) * 0x180 + (type)])
#define gWorld VAR(void *, 0xA44344)
#define gDynCubeObjType VAR(uint16_t, 0x54875C)
#define gCamPos VAR(Vec3d, 0xBCAB08)

/* exe constants, read in place so the x87 code sees the same values */
#define kZero VAR(float, 0x5333EC)
#define kOneD VAR(double, 0x5333F8)

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
