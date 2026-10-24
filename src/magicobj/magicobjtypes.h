#ifndef MAGICOBJ_MAGICOBJTYPES_H
#define MAGICOBJ_MAGICOBJTYPES_H

#include "types.h"
#include "task/tasktypes.h"

/* "RigidDynCubeObj", a DynCubeObj with a model, orientation and scale */
typedef struct RigidDynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0x10];
    int unk48;
    char unk4C[0x14];
    int inSector;
    int unk64;
    void *world;
    void **model;       /* VirModel, one entry per LOD */
    Matrix matrix;
    float scale[3];
    int lod;            /* -1: none */
    Vec3f rgb[2];
    int unkC0[5];
    int unkD4;
    int unkD8;
    void *lightmapRefs;
    int unkE0;
    int unkE4;
    Task *attached;     /* a MagicObjContainer for the objects on it */
    int hidden;
} RigidDynCubeObj;
_Static_assert(offsetof(RigidDynCubeObj, scale) == 0x98, "RigidDynCubeObj");
_Static_assert(sizeof(RigidDynCubeObj) == 0xF0, "RigidDynCubeObj");

#endif
