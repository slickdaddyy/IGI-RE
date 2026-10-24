#ifndef PROPS_PROPSTYPES_H
#define PROPS_PROPSTYPES_H

#include "types.h"
#include "task/tasktypes.h"

typedef struct UseQuery {
    Task *human;
    int unk4;
    Vec3d pos;          /* 0x08 */
    Matrix matrix;      /* 0x20 */
    char unk48[0x28];
    float best_pitch;   /* 0x70 */
    float best_yaw;     /* 0x74 */
    double range;       /* 0x78 */
    void *best;         /* 0x80 */
} UseQuery;

#endif
