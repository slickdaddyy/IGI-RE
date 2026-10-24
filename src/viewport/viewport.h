#ifndef VIEWPORT_VIEWPORT_H
#define VIEWPORT_VIEWPORT_H

#include <stdint.h>

#include "igi.h"
#include "transctx/transctx.h"

FN_EXTERN(SubLevel_Run);
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

typedef void (*TaskFn)(Task *task);

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))
#define SUBLEVEL_RUN (FN(0x488700, SubLevel_Run))

/* next sibling to visit after c, NULL at the end of the list */
#define TASK_NEXT(c) ((c)->next && (c)->next->next ? (c)->next : NULL)

/* calls handler slot of each child of t (not deeper), keeping the next child
   on gTaskStack so handlers may delete tasks; check: skip missing handlers */
#define CALL_CHILDREN(t, slot, check)                       \
    do {                                                    \
        Task *c_ = (t)->head;                               \
        if (!c_->next || !c_)                               \
            break;                                          \
        do {                                                \
            TaskFn fn_;                                     \
            gTaskStack[gTaskStackTop++] = TASK_NEXT(c_);    \
            fn_ = gTaskHandlers[slot][c_->type];            \
            if (!(check) || fn_)                            \
                fn_(c_);                                    \
            c_ = gTaskStack[--gTaskStackTop];               \
        } while (c_);                                       \
    } while (0)

/* rotation (9 floats and a spare word), position and fov, what a view looks through */
typedef struct Camera {
    float m[9];
    int unk24;
    double pos[3];
    float fov;
    float squeeze; /* 0x44, vertical scale of the view around its centre */
} Camera;

/* "ViewportQTask", 200 bytes */
typedef struct Viewport {
    Task task;
    float x, y, w, h; /* 0x20, fractions of the display */
    float fovScale;
    float rect[4]; /* 0x34, pixels, set every frame */
    int unk44;
    int unk48;
    void *world; /* 0x4C */
    Camera *camera;
    Scene scene; /* 0x54 */
    int unkA0;
    int unkA4;
    int unkA8;
    int slot; /* 0xAC, from 0x5181F0 */
    uint8_t lists[9]; /* 0xB0, QCube_Render flags, list i on at [1 + i] */
    char padB9[3];
    int fog;   /* 0xBC, copied to gFogEnable */
    int world3d; /* 0xC0, QCube_Render draws the world */
    void *sortList; /* 0xC4 */
} Viewport;

_Static_assert(sizeof(Viewport) == 200, "Viewport");

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401810, int, TaskEvent_Alloc2, (uint16_t type));
ORIG(0x4018E0, void, TaskEvent_Free2, (int index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1780, void, Debug_Log, (void));

int Screen_GetDrawEvent(void);

#endif
