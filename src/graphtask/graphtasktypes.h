#ifndef GRAPHTASK_GRAPHTASKTYPES_H
#define GRAPHTASK_GRAPHTASKTYPES_H

#include <stdint.h>

/* a point of a physics object type */
typedef struct PhysJoint {
    int point;
    int unk4;
    int unk8;
    float bounce;
    float unk10;
    float friction;
} PhysJoint;

typedef struct PhysType {
    uint16_t type;
    int unk4;
    float mass;
    float size[3];
    char unk18[0x120];
    char model[0x10];
    int jointCount;
    PhysJoint joints[16];
    float restSpeed;
    float restSpin;     /* squared */
    void *data;
} PhysType;
_Static_assert(sizeof(PhysType) == 0x2D8, "PhysType");

#endif
