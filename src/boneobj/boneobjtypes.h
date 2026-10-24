#ifndef BONEOBJ_BONEOBJTYPES_H
#define BONEOBJ_BONEOBJTYPES_H

#include "types.h"

/* Loaded hierarchy: parent index and offset per bone */
typedef struct Skeleton {
    int count;
    int *parents;
    Vec3f *offsets;
} Skeleton;

typedef struct PosKey {
    float pos[3];
    float time;
    float out[3];
    float in[3];
} PosKey;

typedef struct PosTrack {
    int count;
    PosKey *keys;
} PosTrack;

typedef struct SlerpInfo {
    float angle;
    float invSin;
} SlerpInfo;

typedef struct RotKey {
    float q[4];
    float time;
    float a[4];
    float b[4];
} RotKey;

typedef struct RotTrack {
    int count;
    RotKey *keys;
    SlerpInfo *slerp;
    SlerpInfo *slerp2;
} RotTrack;

typedef struct AnimDesc {
    float length;       /* in ticks, 4800 per second */
    unsigned int flags;
    PosTrack *pos;
    RotTrack *rot;      /* one per bone */
    struct EventTrack *events;
} AnimDesc;

typedef struct AnimSet {
    AnimDesc *descs;
    AnimDesc **byId;
    Pool caches;
    int hasCaches;
    int count;
} AnimSet;

#endif
