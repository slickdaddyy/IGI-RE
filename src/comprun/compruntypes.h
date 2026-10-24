#ifndef COMPRUN_COMPRUNTYPES_H
#define COMPRUN_COMPRUNTYPES_H

#include "magicobj/magicobjtypes.h"

typedef struct Computer {
    RigidDynCubeObj obj;
    Vec3d eye;    /* 0xF0 */
    Matrix look;  /* 0x108 */
    Task *view;   /* 0x130 */
    int unk134;
    char camera[0x60];
} Computer;

#endif
