#ifndef TRANSCTX_TRANSCTXTYPES_H
#define TRANSCTX_TRANSCTXTYPES_H

#include "types.h"

typedef struct SoundParams {
    float volume;
    float pitch;
    float pan;
    int unkC;
    double minDist;
    double maxDist;
    Vec3d pos;
    Vec3d vel;
    float start;
    int unk54;
} SoundParams;
_Static_assert(sizeof(SoundParams) == 0x58, "SoundParams");

typedef struct SoundNode {
    struct SoundNode *next;
    struct SoundNode *prev;
} SoundNode;

typedef struct SoundList {
    SoundNode *head;
    SoundNode *tail;
    SoundNode *tailpred;
} SoundList;

#endif
