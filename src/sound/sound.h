#ifndef SOUND_SOUND_H
#define SOUND_SOUND_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "task/taskvars.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)(void *task, void *arg);

extern Task *gTaskStack[];
extern int gTaskStackTop;

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

/* calls the slot handler of each child of t (not deeper) with arg, keeping
   the next child on gTaskStack so handlers may delete tasks */
#define CALL_CHILDREN(t, slot, arg)                                     \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = gTaskHandlers[slot][c_->type];                        \
            if (fn_)                                                    \
                fn_(c_, arg);                                           \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x401810, int, TaskEvent_Alloc2, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018E0, void, TaskEvent_Free2, (int index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401F60, void, Task_SetGlobalFlags, (uint32_t flags));
ORIG(0x401F80, void, Task_ClearGlobalFlags, (uint32_t flags));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));

#endif
