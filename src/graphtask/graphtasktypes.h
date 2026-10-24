#ifndef GRAPHTASK_GRAPHTASKTYPES_H
#define GRAPHTASK_GRAPHTASKTYPES_H

#include <stdint.h>

#include "types.h"
#include "task/tasktypes.h"

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

typedef struct BodyForce {
    float force[3];
    float point[3];
} BodyForce;

/* Rigid body of vehicles and physics objects. Forces are collected with
   Body_AddForce and applied by Body_Integrate. */
typedef struct Body {
    float mass;
    float invMass;
    float speed;
    float invInertia[3];
    Vec3f vel;
    Vec3f dir;
    Vec3f angMom;
    Vec3f force;
    Vec3f angVel;
    BodyForce *forces;
    int maxForces;
    int forceCount;
    int lastForceCount;
} Body;
_Static_assert(sizeof(Body) == 0x64, "Body");

/* a model point the physics object stands on */
typedef struct Contact {
    int side;
    int unk4;
    float bounce;
    Vec3f point;        /* object space */
    char active;
    char enabled;
    char touching;
    char unk1B;
    float unk1C;
    float friction;
    Vec3f normal;
    float dist;
    float height;
} Contact;
_Static_assert(sizeof(Contact) == 0x38, "Contact");

/* "PhysicsObj" (0x720 bytes), a RigidDynCubeObj with a rigid body */
typedef struct PhysObj {
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
    Task *attached;     /* a MagicObjContainer for the objects on it */
    int hidden;
    Body body;          /* 0xF0 */
    char name[0x14];    /* 0x154 */
    Vec3d startPos;     /* 0x168 */
    Vec3f startVel;     /* 0x180 */
    Matrix startMatrix; /* 0x18C */
    PhysType *phys;     /* 0x1B4 */
    int touching;       /* 0x1B8 */
    Contact contacts[22]; /* 0x1BC */
    int contactCount;   /* 0x68C */
    char unk690;
    char unk691[3];
    Matrix lastMatrix;  /* 0x694 */
    int unk6BC;
    Vec3d lastPos;      /* 0x6C0 */
    char collide;       /* 0x6D8 */
    char upright;       /* 0x6D9 */
    char unk6DA[2];
    Matrix turn;        /* 0x6DC */
    int unk704;
    Vec3d move;         /* 0x708 */
} PhysObj;
_Static_assert(sizeof(PhysObj) == 0x720, "PhysObj");

#endif
