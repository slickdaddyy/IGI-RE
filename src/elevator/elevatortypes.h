#ifndef ELEVATOR_ELEVATORTYPES_H
#define ELEVATOR_ELEVATORTYPES_H

#include "types.h"
#include "magicobj/magicobjtypes.h"
#include "param/paramtypes.h"

typedef struct AlarmLight {
    RigidDynCubeObj obj;
    Vec3d place;
    Matrix placeMatrix;
    char offModel[16];
    char onModel[16];
    char deadModel[16];
    char onSound[32];
    char deadSound[32];
    char isOn;
    char isDestroyed;
    char registered;
    char pad1A3;
    VarString cond;
    Task *sound;
} AlarmLight;

typedef struct Elevator {
    RigidDynCubeObj obj;
    Vec3d place;            /* 0xF0, point on the graph */
    Vec3d lastPos;          /* 0x108 */
    Matrix placeMatrix;     /* 0x120 */
    Matrix lastMatrix;      /* 0x148 */
    char model[0x10];       /* 0x170 */
    int graph;              /* 0x180 */
    float height;           /* 0x184, distance along the graph */
    float unk188;           /* start distance */
    char unk18C;            /* straight line from the first to the last floor */
    char unk18D;            /* keep the matrix */
    char pad18E[2];
    float speed;            /* 0x190 */
    float maxSpeed;
    float accel;
    int wanted;             /* 0x19C */
    double floorPos;        /* 0x1A0 */
    double lastFloorPos;    /* 0x1A8 */
    char request[10];       /* 0x1B0 */
    char pad1BA[2];
    int restTicks;          /* 0x1BC */
    char registered;        /* 0x1C0 */
    char pad1C1[3];
    VarString wantedFloor[10]; /* 0x1C4 */
    VarString up;           /* 0x4E4 */
    VarString down;         /* 0x534 */
    VarString move;         /* 0x584 */
    char lastUp;            /* 0x5D4 */
    char lastDown;
    char isMoving;          /* 0x5D6 */
    char isStarting;
    char isStopping;
    char startSound[32];
    char stopSound[32];
    char moveSound[32];
    char pad639[3];
    Task *moving;           /* 0x63C, sound */
    char lastMove;          /* 0x640 */
    char pad641[3];
    int lastTick;
} Elevator;

#endif
