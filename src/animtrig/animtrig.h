#ifndef ANIMTRIG_ANIMTRIG_H
#define ANIMTRIG_ANIMTRIG_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    struct Task *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)(void *task, void *arg);

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)

#define TASK_NOTHING ((void *)0x4D3710)

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

/* runs the children of t (slot 1, no argument) */
#define RUN_CHILDREN(t)                                                 \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            void (*fn_)(Task *);                                        \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = (void (*)(Task *))gTaskHandlers[1][c_->type];         \
            if (fn_)                                                    \
                fn_(c_);                                                \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* A script function result. Bytes 4 and 20 are never set. */
typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *s;
    int unk14;
} ScriptValue;

typedef struct ScriptArgs ScriptArgs;

#define gScriptEmptyString ((const char *)0x567C74)

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8930, void, Script_SetSymbolContext, (const char *name, void *context));
ORIG(0x4B8A20, int, Script_GetArgInt, (ScriptArgs *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (ScriptArgs *args, int i, char *buf, int size));
ORIG(0x4B8B00, int, Script_HasArg, (ScriptArgs *args, int i));

#endif
