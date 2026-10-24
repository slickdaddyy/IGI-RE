#ifndef LEVELFLOW_LEVELFLOWTYPES_H
#define LEVELFLOW_LEVELFLOWTYPES_H

#include "types.h"

/* what a detector sees from: a view cone and an optional second one */
typedef struct DetectView {
    Vec3d pos;
    Matrix matrix;
    double maxPitch;
    double maxYaw;
    double range;
    double maxPitch2;
    double maxYaw2;
    double range2;
    char hasSecond;
    char unk71[7];
} DetectView;
_Static_assert(sizeof(DetectView) == 0x78, "DetectView");

#endif
