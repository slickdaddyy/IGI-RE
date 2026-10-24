#ifndef EDITCAM_EDITCAMTYPES_H
#define EDITCAM_EDITCAMTYPES_H

#include "task/tasktypes.h"

typedef struct EditCamera {
    Task task;
    Vec3d pos;
    float duration;
    float angles[3];
    Vec3d attachPos;    /* offset from the position target */
    Matrix attachRot;
    float fov;
    int posAttach;      /* task id followed by the position, -1 none */
    int rotAttach;      /* task id the camera looks at, -1 none */
    char smooth;
    char posFollow;
    char rotFollow;
    char unk97;
    float unk98;
    Task **refs[3];
    char unkA8;
    char filterName[0x23];
    int filter;
    float filterLevel;
    float color[3];
    float shake;
    int unkE4;
} EditCamera;

#endif
