#ifndef SECTOR_SECTOR_H
#define SECTOR_SECTOR_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"

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

typedef void (*TaskFn)();

/* a portal of a sector mesh (0x14 bytes) */
typedef struct Portal {
    int firstVert;
    int vertCount;
    int firstTri;
    int triCount;
    int id;             /* the same id on both sides */
} Portal;

/* the mesh data of a LOD, the fields used here */
typedef struct SectorMesh {
    char unk0[0x20];
    int format;         /* 3 = lightmapped */
    char unk24[0x2C];
    Vec3f center;       /* 0x50 */
    float radius;       /* 0x5C */
    char unk60[0x18];
    float radius2;      /* 0x78 */
    char unk7C[8];
    short portalCount;  /* 0x84 */
    char unk86[0xA];
    int (*tris)[3];     /* 0x90 */
    Vec3f *verts;       /* 0x94 */
    Portal *portals;    /* 0x98 */
} SectorMesh;

typedef struct VirModel {
    void *lod[15];
    short lodCount;     /* 0x3C */
} VirModel;

static inline SectorMesh *lod_mesh(VirModel *m, int i)
{
    return *(SectorMesh **)(*(char **)((char *)m->lod[i] + 0xC) + 0xC);
}

/* "RigidDynCubeObj" (0xF0 bytes), the fields used here */
typedef struct Obj {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x28];
    int inSector;       /* 0x60 */
    int unk64;
    int world;          /* 0x68 */
    VirModel *model;    /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x34];
    int unkD8;
    void *lightmapRefs; /* 0xDC */
    char unkE0[8];
    Task *attached;     /* 0xE8 */
    char unkEC[4];
} Obj;

/* "Sector" (0x148 bytes): a room; objects inside are kept in its pool */
typedef struct Sector {
    Obj o;
    char unkF0[0x40];
    Pool objects;       /* 0x130, slots hold Obj pointers */
} Sector;

/* "SectorObject" (0x1A0 bytes): a building made of sectors */
typedef struct SectorObject {
    Sector s;
    Task *sectors;      /* 0x148 */
    char portalOpen[0x44];  /* 0x14C, by portal id */
    Vec3f offset;       /* 0x190 */
    int unk19C;
} SectorObject;

_Static_assert(sizeof(Obj) == 0xF0, "Obj");
_Static_assert(sizeof(Sector) == 0x148, "Sector");
_Static_assert(sizeof(SectorObject) == 0x1A0, "SectorObject");
_Static_assert(offsetof(SectorMesh, portals) == 0x98, "SectorMesh");

extern TaskFn gTaskHandlers[][0x180];
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

extern const float kZero;
extern const float kOne;

ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, unsigned int align));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));

/* the pool alloc and release the engine inlines everywhere */
static inline void *pool_take(Pool *p)
{
    char *s = p->data + p->free[p->used] * p->stride;

    *(int *)(s + p->size) = p->used;
    p->used++;
    return s;
}

static inline void pool_release(Pool *p, void *slot)
{
    int i = *(int *)((char *)slot + p->size);
    char *q;
    int j, t;

    p->used--;
    if (i == p->used)
        return;
    q = p->data + p->free[p->used] * p->stride;
    t = p->free[i];
    j = *(int *)(q + p->size);
    p->free[i] = p->free[j];
    p->free[j] = t;
    *(int *)(q + p->size) = i;
}

static inline void *pool_get(Pool *p, int i)
{
    return p->data + p->free[i] * p->stride;
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

int *Light_GetActive(int *count);

/* the first child of t, or NULL */
static inline Task *first_child(Task *t)
{
    return t->head->next ? t->head : NULL;
}

#endif
