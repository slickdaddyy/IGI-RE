#ifndef EDITCAM_EDITCAM_H
#define EDITCAM_EDITCAM_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24;
} Matrix;

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

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define gCurrentWorld VAR(Terrain *, 0xA44344)
#define gDynCubeObjType VAR(uint16_t, 0x54875C)

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

/* the hit record of Coll_DropPoints */
typedef struct CollHit {
    Task *task;
    int unk4;
    Vec3d pos;
    char unk20[0x38];
    int found;
    char unk5C[0x14];
} CollHit;

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

/* a task kept in the dynamic cube trees */
typedef struct DynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    Terrain *world;
    int unk6C;
} DynCubeObj;

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
_Static_assert(sizeof(DynCubeObj) == 0x70, "DynCubeObj");

#endif
