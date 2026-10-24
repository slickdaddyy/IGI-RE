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

typedef struct SplineObj {
    Task task;
    int unk20;
    int maxSegments;
    float length;
    Vec3f pos;          /* moves all waypoints when changed */
    Vec3f oldPos;
    float angle;        /* turns all waypoints around the first one */
    float oldAngle;
    char snap;
    char collide;
    char rigid;
    char autoOrient;
    Vec3f color1;
    Vec3f color2;
} SplineObj;
_Static_assert(sizeof(SplineObj) == 0x68, "SplineObj");

#endif
