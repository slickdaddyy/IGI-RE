#ifndef EDITOBJ_EDITOBJTYPES_H
#define EDITOBJ_EDITOBJTYPES_H

#include "types.h"
#include "magicobj/magicobjtypes.h"

typedef struct SplineObjWaypoint {
    RigidDynCubeObj obj;
    struct SplineObjSegment *seg;
    int unkF4;
    Vec3d point;
    Matrix rot;
    Vec3d attachPos;    /* point in the frame of the attached object */
    Matrix attachRot;
    char modelName[16];
    char segModel[16];
    int segments;
    void *tree;
    char inTree;
    char straight;
    char rigid;
    char unk1A3;
    char reversed;
    char autoOrient;
    char unk1A6[2];
} SplineObjWaypoint;

#endif
