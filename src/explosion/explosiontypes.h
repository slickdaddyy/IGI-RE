#ifndef EXPLOSION_EXPLOSIONTYPES_H
#define EXPLOSION_EXPLOSIONTYPES_H

#include "types.h"
#include "task/tasktypes.h"

typedef struct Explosion {
    Task task;
    Vec3d pos;
    int bursts;   /* 0x38 */
    int interval; /* 0x3C */
    int timer;    /* 0x40 */
    Vec3f dir;    /* 0x44 */
    float blast;  /* 0x50 */
    float size;   /* 0x54 */
    float unk58;
    float spread;   /* 0x5C */
    float minSpeed; /* 0x60 */
    float maxSpeed; /* 0x64 */
    int fragments;  /* 0x68 */
    int fireballs;  /* 0x6C */
    char sound[16]; /* 0x70 */
    void *group;    /* 0x80, the fragments */
    int unk84;
} Explosion;

#endif
