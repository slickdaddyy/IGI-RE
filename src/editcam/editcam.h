#ifndef EDITCAM_EDITCAM_H
#define EDITCAM_EDITCAM_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "coll/colltypes.h"
#include "terrain/terraintypes.h"

typedef void (*TaskFn)(void *task, void *arg);

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern uint16_t gDynCubeObjType;

/* the handler a task type has for a slot */
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* next sibling, or NULL at the end of the list */
static inline Task *next_sibling(Task *t)
{
    return t->next && t->next->next ? t->next : NULL;
}

/* calls the handler for slot on each child of t (not deeper), keeping the
   next child on gTaskStack so handlers may delete tasks. A macro: gcc drops
   calls through ORIG pointers passed to inline functions. */
#define CALL_CHILDREN(t, slot, arg)                                     \
    do {                                                                \
        Task *c_ = ((Task *)(t))->head;                                 \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = HANDLER(slot, c_->type);                              \
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

/* one .bit or .hmp table entry */
typedef struct TerrainMap {
    void *data;
    char flag;
    char unk5[3];
    int size;
} TerrainMap;

typedef struct Terrain {
    char unk0[0xDCC];
    TerrainMap *bit;
    TerrainMap *hmp;
    char unkDD4[0xC];
    int offset[3];
} Terrain;
extern Terrain *gCurrentWorld;

/* MenuItem: one selectable item of a menu panel */
typedef struct MenuItem {
    Task task;
    char unk20[0x18];
    int id;
    int link[4];        /* ids of the neighbours, see MenuItem_Move */
    char disabled;
    char pressed;
    char selected;
    char unk4F;
    TaskFn onEvent;
} MenuItem;

/* the argument of the select event */
typedef struct MenuItemEvent {
    int id;
    int unk4[5];
    unsigned int keys;
} MenuItemEvent;

_Static_assert(sizeof(MenuItem) == 0x54, "MenuItem");
_Static_assert(offsetof(Terrain, offset) == 0xDE0, "Terrain");

#endif
