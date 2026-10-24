#ifndef LEVELFLOW_LEVELFLOWTYPES_H
#define LEVELFLOW_LEVELFLOWTYPES_H

#include <stddef.h>

#include "types.h"
#include "param/paramtypes.h"
#include "task/tasktypes.h"

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

/* the level script's root: completion and failure conditions, tick
   counter and the script view of the number keys */
typedef struct LevelFlow {
    Task task;
    Vec3d pos;
    Matrix matrix;
    char isComplete;
    char isFailed;
    char unk62[2];
    int eDifficulty;
    int nTick;
    float startHour;
    char unk70;
    char unk71[3];
    float timeLimit;
    char registered;
    char unk79[3];
    VarString complete;
    VarString failed;
    char wasFailed;
    char unk11D[3];
    void *menu;
    int unk124;
} LevelFlow;

_Static_assert(offsetof(LevelFlow, complete) == 0x7C, "LevelFlow");
_Static_assert(sizeof(LevelFlow) == 0x128, "LevelFlow");

#endif
