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

typedef struct CollQuery {
    const Vec3d *points;
    int count;
    void *results;
    char active[128]; /* a handler clears an entry to take over that point */
    char unk8C;
    int unk90;
    int unk94;
} CollQuery;
_Static_assert(sizeof(CollQuery) == 0x98, "CollQuery");

/* a terrain box whose height map raises or lowers the ground */
typedef struct CollBump {
    int unk0;
    int texShift;
    int unk8;
    int sizeShift;
    float scale;
    int unk14;
    double x, y;
    const signed char *heights;
    int unk2C;
} CollBump;
_Static_assert(sizeof(CollBump) == 0x30, "CollBump");

/* a node of the collision tree in the .ctr file */
typedef struct CollNode {
    uint16_t child[8]; /* index into Terrain.ctr */
    uint8_t code[8];   /* orientation change when entering each child */
    uint8_t mask;      /* which children exist */
    uint8_t pad19[3];
    const short *leaf;
} CollNode;
_Static_assert(sizeof(CollNode) == 0x20, "CollNode");

#endif
