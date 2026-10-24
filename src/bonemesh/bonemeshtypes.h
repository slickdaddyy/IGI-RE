#ifndef BONEMESH_BONEMESHTYPES_H
#define BONEMESH_BONEMESHTYPES_H

#include "types.h"

/* a view space vertex with its screen position and outcode */
typedef struct ClipVertex {
    float x, y, z;
    float sx, sy, rhw;
    int code;
} ClipVertex;

/* argument of the 0x4D14D0 event: the nearest view depth so far */
typedef struct NearZArg {
    int unk0;
    int unk4;
    float minZ;
} NearZArg;

typedef struct ExcludeArg {
    Vec3f *center;      /* view space */
    float radius;
    int *inside;
    int outside;
} ExcludeArg;

#endif
