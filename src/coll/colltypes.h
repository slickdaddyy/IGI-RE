#ifndef COLL_COLLTYPES_H
#define COLL_COLLTYPES_H

#include <stdint.h>

#include "types.h"
#include "task/tasktypes.h"

typedef struct CollFilter {
    int unk0;
    Task *task;
    Task *owner;
    Task *other;
    float unk10;
} CollFilter;

/* the filter of a collision query: the task to leave out */
typedef struct CollIgnore {
    int unk0;
    Task *task;
} CollIgnore;

/* a terrain box whose bit map marks material holes */
typedef struct CollHole {
    int material;
    int texShift;
    int unk8;
    int sizeShift;
    float scale;
    int unk14;
    double x, y;
    const uint8_t *bits;
    int unk2C;
} CollHole;
_Static_assert(sizeof(CollHole) == 0x30, "CollHole");

/* a hit of the collision queries, 0x70 bytes */
typedef struct CollHit {
    Task *task;
    int unk4;
    Vec3d pos;
    Vec3f local;        /* object space */
    Vec3f localNormal;
    Vec3f normal;
    int unk44;
    int tri;
    struct GameMaterial *material;  /* front side */
    struct GameMaterial *back;      /* back side */
    int close;          /* within 0.1 of an earlier hit */
    int found;
    int unk5C;
    double dist;
    float u, v;
} CollHit;
_Static_assert(sizeof(CollHit) == 0x70, "CollHit");

typedef struct CollLineQuery {
    const Vec3d *starts;
    const Vec3d *ends;
    int count;
    uint8_t slot;       /* 0xFF: none, else a handler that must set *slotArg */
    char *slotArg;
    int mesh;
    int allFaces;
    int magic;          /* 'ABCD' */
    CollHit *results;
} CollLineQuery;
_Static_assert(offsetof(CollLineQuery, results) == 0x20, "CollLineQuery");

#endif
