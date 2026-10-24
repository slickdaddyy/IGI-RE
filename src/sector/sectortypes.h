#ifndef SECTOR_SECTORTYPES_H
#define SECTOR_SECTORTYPES_H

#include "magicobj/magicobjtypes.h"

typedef struct Light {
    Vec3f dir;
    Vec3f color;
    Vec3f ambient;
    char unk24;
    char on;
} Light;

typedef struct Sector {
    RigidDynCubeObj o;
    Vec3d offset;
    Matrix local;
    Pool objects;       /* slots hold RigidDynCubeObj pointers */
} Sector;
_Static_assert(sizeof(Sector) == 0x148, "Sector");

typedef struct SectorObject {
    Sector s;
    Task *sectors;      /* MagicObjContainer of child sectors */
    char portalOpen[0x40];  /* by portal id */
    int built;
    Vec3f center;
    float radius;
} SectorObject;
_Static_assert(sizeof(SectorObject) == 0x1A0, "SectorObject");

/* an object in a sector's pool */
typedef struct SectorLink {
    struct RigidDynCubeObj **slot;
    Sector *sector;
    SectorObject *owner;
} SectorLink;

#endif
