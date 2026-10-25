#ifndef LITMAP_LITMAP_H
#define LITMAP_LITMAP_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "task/taskvars.h"

extern Task *gTaskStack[];
extern int gTaskStackTop;
#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])
extern struct Terrain *gCurrentWorld;

/* exe constants, read in place so the x87 code sees the same values */
extern const float kZero;
extern const float kOne;
extern const float kHalf;
extern const float kTwo;
extern const float kPi;
extern double kZeroD;

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));

static inline Task *next_sibling(Task *t)
{
    return t->next && t->next->next ? t->next : NULL;
}

/* calls the slot handler of each task in a child list, keeping the next one
   on gTaskStack so handlers may delete tasks */
#define BROADCAST(first, SLOT, ...)                                     \
    do {                                                                \
        Task *c_ = (first);                                             \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            void *fn_;                                                  \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = TASK_HANDLER(SLOT, c_->type);                         \
            if (fn_)                                                    \
                ((void (*)())fn_)(c_, ##__VA_ARGS__);                   \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* x87 "not equal" as tested with C3: false when unordered */
static inline int fne(long double a, long double b)
{
    return a < b || a > b;
}

/* fistp with the current rounding mode, low 32 bits */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

#endif
