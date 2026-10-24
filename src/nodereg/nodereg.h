#ifndef NODEREG_NODEREG_H
#define NODEREG_NODEREG_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)(void *task, void *arg);

/* what the param table handlers get: the table is stored through out */
typedef void *ParamTable;

#define TASK_NO_TYPE 0x181
#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void *gCurrentWorld;

/* calls handler slot of each child of t (not deeper); slot is evaluated
   for each child like in the original */
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

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

void GraphNode_Open(void);
void GraphNode_Close(void);
void Water_Open(void);
void Water_Close(void);
void GFXCapsViewer_Open(void);
void GFXCapsViewer_Close(void);
void ShadowTask_Open(void);
void ShadowTask_Close(void);
void SplinePathNode_Open(void);
void SplinePathNode_Close(void);
void SplinePath_Open(void);
void SplinePath_Close(void);
void SplinePathGuide_Open(void);
void SplinePathGuide_Close(void);
void AnimSound_Open(void);
void AnimSound_Close(void);
void EditorMagicObj_Open(void);
void EditorMagicObj_Close(void);
void LevelTimer_Open(void);
void LevelTimer_Close(void);
void SoundManager_Open(void);

#endif
