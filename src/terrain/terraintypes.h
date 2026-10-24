#ifndef TERRAIN_TERRAINTYPES_H
#define TERRAIN_TERRAINTYPES_H

#include <stdint.h>

#include "types.h"
#include "task/tasktypes.h"

/* A task that lives in a dyncube tree. */
typedef struct DynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0xC];
    float radius;
    int unk48;
    int unk4C;
    struct DynCubeNode *node;
    struct DynCubeObj *prev;
    struct DynCubeObj *next;
    uint16_t mask;             /* copy of the task type's event mask */
    uint16_t unk5E;
    int unk60;
} DynCubeObj;
_Static_assert(sizeof(DynCubeObj) == 0x68, "DynCubeObj");

#endif
