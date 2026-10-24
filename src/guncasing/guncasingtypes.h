#ifndef GUNCASING_GUNCASINGTYPES_H
#define GUNCASING_GUNCASINGTYPES_H

#include "types.h"
#include "magicobj/magicobjtypes.h"

typedef struct Mine {
    RigidDynCubeObj o;
    Vec3d placePos;     /* 0xF0 */
    Matrix placeMatrix; /* 0x108 */
    char unk130[0x40];
    char model[0x20];   /* 0x170 */
    char exploded;      /* 0x190 */
    char unk191[0x53];
    char explode;       /* 0x1E4 */
    char unk1E5[0x53];
    int armDelay;       /* 0x238, frames */
    int countdown;      /* 0x23C */
    float delay;        /* 0x240, seconds */
    float radius;       /* 0x244, meters */
    char triggered;     /* 0x248 */
    char remote;        /* 0x249 */
    char unk24A[6];
} Mine;
_Static_assert(sizeof(Mine) == 0x250, "Mine");

#endif
