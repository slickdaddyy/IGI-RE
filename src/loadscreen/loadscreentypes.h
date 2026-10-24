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

typedef struct HumanDamage {
    float total;
    float max;
    HitAnims *zones;
    struct {
        float dmg;
        float mult;
    } part[17];
    int hitZone;
} HumanDamage;
_Static_assert(sizeof(HumanDamage) == 0x98, "HumanDamage");

#endif
