#ifndef SHADOW_SHADOW_H
#define SHADOW_SHADOW_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"

typedef void (*TaskFn)();

/* "RigidDynCubeObj" (0xF0 bytes), the fields used here */
typedef struct Obj {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x28];
    int inSector;       /* 0x60, set by Sector_AddObject */
    int unk64;
    void *world;        /* 0x68 */
    void **model;       /* 0x6C, VirModel */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x44];
    Task *parts;        /* 0xE8, MagicObjContainer */
    int unkEC;
} Obj;

/* create argument of the RigidDynCubeObj kinds made from DefineMagicObj */
typedef struct MagicArg {
    const char *model;
    void *world;
    Vec3d pos;          /* 0x08 */
    Vec3d offset;       /* 0x20, from the owner */
    Matrix matrix;      /* 0x38 */
    Matrix local;       /* 0x60 */
    void *owner;        /* 0x88 */
    void *data;         /* 0x8C, the DefineMagicObj data block */
} MagicArg;

/* "Sector" (0x148 bytes): a magic object that owns other objects */
typedef struct Sector {
    Obj o;
    Vec3d offset;       /* 0xF0 */
    Matrix local;       /* 0x108 */
    Pool links;         /* 0x130, pointers into gSectorLinks */
} Sector;

/* "SectorObject" (0x1A0 bytes), a building made of sectors */
typedef struct SectorObject {
    Sector s;
    Task *sectors;      /* 0x148, MagicObjContainer of child sectors */
    char unk14C[0x40];
    int built;          /* 0x18C */
    float center[3];    /* 0x190 */
    float radius;       /* 0x19C */
} SectorObject;

/* one entry of the link pool at 0xA5E318 */
typedef struct SectorLink {
    void **item;        /* entry in the sector's links pool */
    Sector *sector;
    int index;
} SectorLink;

/* "Shadow" (0x124 bytes), a blob shadow quad under its owner */
typedef struct ShadowPoly {
    float *verts[3];
    float *colors[3];
    float uv[6];
    int unk30[6];
    int texture;
    int unk4C;
} ShadowPoly;

typedef struct Shadow {
    Task task;
    void *world;        /* 0x20 */
    Vec3d *pos;         /* 0x24 */
    int *visible;       /* 0x28 */
    int *enabled;       /* 0x2C */
    float *height;      /* 0x30 */
    void *dir;          /* 0x34 */
    float maxHeight;    /* 0x38 */
    float size;         /* 0x3C */
    float verts[4][3];  /* 0x40 */
    int texture;        /* 0x70 */
    Task *polys;        /* 0x74, the PolyList */
    ShadowPoly poly[2]; /* 0x78 */
    float color[3];     /* 0x118 */
} Shadow;

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

extern uint16_t gSectorType;
extern uint16_t gSectorObjectType;
extern uint16_t gSectorInteriorObjectType;
extern Pool gSectorLinks;
extern Pool gVisSectors;
extern void *gCurrentWorld;

/* exe constants, read in place */
extern float kZero;
extern float kOne;
extern double kOneD;
extern float kHalf;

/* Calls the slot handler of each child of t, keeping the next child on
   gTaskStack so handlers may delete tasks. slot is evaluated after the
   push, like the original. */
#define EACH_CHILD(t, slot, ...)                                        \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = HANDLER(slot, c_->type);                              \
            if (fn_)                                                    \
                fn_(c_, ##__VA_ARGS__);                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* the same without the NULL check on the handler */
#define EACH_CHILD_NC(t, slot, ...)                                     \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            HANDLER(slot, c_->type)(c_, ##__VA_ARGS__);                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* next sibling, or NULL at the end of the list */
static inline Task *next_task(Task *t)
{
    Task *n = t->next;

    return n && n->next ? n : NULL;
}

/* first child, or NULL for an empty list */
static inline Task *first_child(Task *t)
{
    Task *c = t->head;

    return c->next ? c : NULL;
}

static inline void pool_release(Pool *p, void *item)
{
    int index = *(int *)((char *)item + p->size);
    char *last;
    int last_index, tmp;

    p->used--;
    if (index == p->used)
        return;
    last = p->data + p->free[p->used] * p->stride;
    last_index = *(int *)(last + p->size);
    tmp = p->free[index];
    p->free[index] = p->free[last_index];
    p->free[last_index] = tmp;
    *(int *)(last + p->size) = index;
}

static inline float obj_scale(const Obj *o)
{
    float m = !(o->scale[2] >= o->scale[1]) ? o->scale[1] : o->scale[2];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
