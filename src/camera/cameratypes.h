#ifndef CAMERA_CAMERATYPES_H
#define CAMERA_CAMERATYPES_H

#include "task/tasktypes.h"

/* an entry of the list of humans the camera can follow */
typedef struct HumanCameraEntry {
    Task **ref;
    char unk4;
    char used;
    char pad6[2];
} HumanCameraEntry;

typedef struct HumanCameraInfo {
    Vec3d pos;
    Matrix matrix;
    float fov;
    Matrix body;
    float unk6C;
    int mode;
} HumanCameraInfo;

typedef struct HumanCamera {
    Task task;
    Vec3d pos;
    Matrix matrix;
    float fov;
    void *camera;
    Task **ref;
    int unk6C;
    HumanCameraInfo info;
    int prevMode;
    int unkEC;
    Vec3d look;
    float pitch;
    float yaw;
    float dist;
    float target;
    int index;
    int savedIndex;
    Task **savedRef;
    unsigned flags;
} HumanCamera;
_Static_assert(sizeof(HumanCamera) == 0x128, "HumanCamera");

#endif
