#ifndef DEFINE_DEFINETYPES_H
#define DEFINE_DEFINETYPES_H

#include "types.h"
#include "task/tasktypes.h"
#include "viewport/viewporttypes.h"

typedef struct SoundDefGraph {
    Task task;
    SoundHandle h;
    Vec3d pos;
    Vec3d offset;
    float volume;
    float volume2;
    float pitch;
    float unkC4;
    float minDist;
    float maxDist;
    float range;
    char isDef;
    char stop;
    char padD6[2];
    int volumeIndex;
    int pitchIndex;
    int pitchAddIndex;
    int sampleIndex;
    float volumeGraph[11];
    float pitchGraph[11];
    char name[0x20];
    char sample[0x20];
    int bucket;
    int hash;
} SoundDefGraph;
_Static_assert(sizeof(SoundDefGraph) == 0x188, "SoundDefGraph");

typedef struct SoundWriteArg {
    char *buf;
    int indent;
} SoundWriteArg;

typedef struct SoundPlayArg {
    struct SoundSysMicrophone *listener;
    char defs;          /* 1: definitions do not play */
} SoundPlayArg;

typedef struct SoundInfo {
    Vec3d *pos;
    float *matrix;
    int *values; /* count, then one value per index */
} SoundInfo;

typedef struct SoundCreateArg {
    void *def;
    char isDef;
} SoundCreateArg;

#endif
