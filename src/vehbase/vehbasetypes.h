#ifndef VEHBASE_VEHBASETYPES_H
#define VEHBASE_VEHBASETYPES_H

#include "types.h"
#include "task/tasktypes.h"
#include "graphtask/graphtasktypes.h"

typedef struct AnimKey {
    char channel;       /* ANIM_WAIT holds the value rounded to an int */
    char unk1[3];
    float value;
} AnimKey;

/* the "Vehicle" config block of a vehicle type */
typedef struct VehicleConfig {
    float unk0;         /* start speed scale */
    struct {
        float rate;     /* degrees */
        float smooth;
    } axis[3];
    float thrust;
    float accel;
    float accel_slow;   /* below speed 0.8 */
    int part_count;
    float parts[8][7];
    float unk10C[3];
} VehicleConfig;
_Static_assert(sizeof(VehicleConfig) == 0x118, "VehicleConfig");

/* "Vehicle" (0x748 bytes), a PhysicsObj with controls */
typedef struct Vehicle {
    Task task;
    Vec3d pos;
    char unk38[0x10];
    int unk48;
    char unk4C[0x14];
    int inSector;
    int unk64;
    void *world;
    void **model;       /* VirModel, one entry per LOD */
    Matrix matrix;
    float scale[3];
    int lod;
    Vec3f rgb[2];
    int unkC0[5];
    int unkD4;
    int unkD8;
    void *lightmapRefs;
    int unkE0;
    int unkE4;
    Task *attached;     /* its children get the move event */
    int hidden;
    Body body;
    char name[0x14];
    Vec3d startPos;
    Vec3f startVel;
    Matrix startMatrix;
    PhysType *phys;
    int touching;
    Contact contacts[22];
    int contactCount;
    char moved;
    char unk691[3];
    Matrix lastMatrix;
    int unk6BC;
    Vec3d lastPos;
    char collide;
    char airborne;
    char unk6DA[2];
    Matrix seat_matrix;
    int unk704;
    Vec3d seat_pos;
    float speed;
    float turn[3];
    float target_speed;
    float steer[3];
    float start_speed;
    char brake;
    char active;
    char unk746[2];
} Vehicle;
_Static_assert(offsetof(Vehicle, speed) == 0x720, "Vehicle");
_Static_assert(sizeof(Vehicle) == 0x748, "Vehicle");

#endif
