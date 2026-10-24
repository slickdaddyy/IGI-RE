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

#endif
