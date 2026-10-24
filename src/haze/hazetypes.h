#ifndef HAZE_HAZETYPES_H
#define HAZE_HAZETYPES_H

#include "task/tasktypes.h"
#include "boneobj/boneobjtypes.h"

typedef struct BoneDynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    BoneObj bones;
    void **model;
    Matrix matrix;
    float radius;
    float scale[3];
    float unkB0;
    int unkB4;
    int unkB8[5];
    int unkCC;
    int unkD0;
    int layer;
} BoneDynCubeObj;
_Static_assert(sizeof(BoneDynCubeObj) == 0xD8, "BoneDynCubeObj");

typedef struct Camera {
    Matrix axes;
    Vec3d pos;
    float fov;
    float aspect;       /* vertical scale of the view around its centre */
    float priority;
    int frame;
    int count;
    void *owner;
    int unk58;
    int unk5C;
} Camera;

#endif
