#ifndef DIRLIGHT_DIRLIGHTTYPES_H
#define DIRLIGHT_DIRLIGHTTYPES_H

#include "types.h"
#include "task/tasktypes.h"
#include "sector/sectortypes.h"

typedef struct DirlightKeyframe {
    Task task;
    float angleY;
    float angleZ;
    Vec3f dir;
    Vec3f color;
    Vec3f ambient;
    float editTime;
    float time;
} DirlightKeyframe;

typedef struct Dirlight {
    Task task;
    void *world;
    Vec3f dir;
    Vec3f color;
    Vec3f ambient;
    Light *light;
    char unk4C;
    char on;
    char unk4E[2];
    float unk50;
    float minTime;
    float maxTime;
    float time;
    void **slot;
} Dirlight;

#endif
