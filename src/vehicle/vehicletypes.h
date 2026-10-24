#ifndef VEHICLE_VEHICLETYPES_H
#define VEHICLE_VEHICLETYPES_H

#include "types.h"

/* creates a part of a vehicle or object at owner's place */
typedef struct PartCreateArg {
    const char *model;
    void *world;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    void *owner;
    void *def;
} PartCreateArg;
_Static_assert(sizeof(PartCreateArg) == 0x90, "PartCreateArg");

#endif
