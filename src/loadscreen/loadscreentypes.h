#ifndef LOADSCREEN_LOADSCREENTYPES_H
#define LOADSCREEN_LOADSCREENTYPES_H

#include "task/tasktypes.h"

typedef struct HitAnims {
    float mult;
    int hit;
    int death[2];
} HitAnims;

/* what hit a human */
typedef struct HumanHit {
    struct Shot *shot;
    float damage;
    float dir[3];
    void *attacker;
} HumanHit;

typedef struct SightFilter {
    char visible;
    Task *self;
    Task *self2;
    Task *target;
} SightFilter;

#endif
