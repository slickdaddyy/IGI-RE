#ifndef DOOR_DOORTYPES_H
#define DOOR_DOORTYPES_H

#include "magicobj/magicobjtypes.h"

typedef struct Cabinet {
    RigidDynCubeObj o;
    Vec3d place;        /* 0xF0 */
    float yaw;          /* 0x108 */
    char model[0x10];   /* 0x10C */
    char opening;       /* 0x11C */
    char closing;       /* 0x11D */
    char reset;         /* 0x11E */
    char inUse;         /* 0x11F */
    char lastInUse;     /* 0x120 */
    char unk121[3];
    int useTick;        /* 0x124 */
    char searched;      /* 0x128 */
    char registered;    /* 0x129 */
    char unk12A[2];
    float searchTime;   /* 0x12C */
    char moving;        /* 0x130 */
    char unk131[7];
} Cabinet;

#endif
