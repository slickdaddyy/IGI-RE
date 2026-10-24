#ifndef COLL_COLLTYPES_H
#define COLL_COLLTYPES_H

#include "types.h"
#include "task/tasktypes.h"

/* a hit of the collision queries, 0x70 bytes */
typedef struct CollHit {
    Task *task;
    int unk4;
    Vec3d pos;
    char unk20[0x18];
    Vec3f normal;
    int unk44;
    int unk48;
    struct GameMaterial *material;  /* front side */
    struct GameMaterial *back;      /* back side */
    int unk54;
    int found;
    int unk5C;
    double dist;
    char unk68[8];
} CollHit;
_Static_assert(sizeof(CollHit) == 0x70, "CollHit");

#endif
