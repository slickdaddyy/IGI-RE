#ifndef VEHICLE_VEHICLETYPES_H
#define VEHICLE_VEHICLETYPES_H

#include "types.h"
#include "task/tasktypes.h"

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

/* node of a list of task references (vehicle seats, targets) */
typedef struct RefNode {
    struct RefNode *next;
    struct RefNode *prev;
    Task **ref;
    char unkC[0x1D];
    char active;
} RefNode;

#endif
