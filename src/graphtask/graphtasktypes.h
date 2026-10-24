#ifndef GRAPHTASK_GRAPHTASKTYPES_H
#define GRAPHTASK_GRAPHTASKTYPES_H

#include <stdint.h>

#include "types.h"
#include "task/tasktypes.h"
#include "coll/colltypes.h"
#include "res/restypes.h"

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

typedef struct Particle2D {
    Vec3d pos;
    Vec3f vel;          /* 0x18 */
    int16_t collideTimer; /* 0x24, frames to the next collision check */
    int16_t delay;      /* 0x26, frames before it starts to fade */
    int16_t frame;      /* 0x28 */
    int16_t frameTimer; /* 0x2A */
    float spin;         /* 0x2C */
    float level;        /* 0x30, fades to 0 */
    float fade;         /* 0x34 */
    float angle;        /* 0x38 */
    float scale;        /* 0x3C */
    Vec3d lastPos;      /* 0x40, at the last collision check */
} Particle2D;

struct Particle2DHit;

typedef struct Particle2DTask {
    Task task;
    char unk20[0x48];
    Particle2D *buf;    /* 0x68 */
    Particle2D **list;  /* 0x6C */
    CollHit *hits;      /* 0x70 */
    Vec3d *starts;      /* 0x74 */
    Vec3d *ends;        /* 0x78 */
    int *idx;           /* 0x7C */
    Sprite *sprite;     /* 0x80 */
    int unk84;
    Vec3d min;          /* 0x88 */
    Vec3d max;          /* 0xA0 */
    void *world;        /* 0xB8 */
    float size[2];      /* 0xBC */
    Vec3f gravity;      /* 0xC4 */
    Vec3f color;        /* 0xD0 */
    float light;        /* 0xDC */
    float maxDepth;     /* 0xE0 */
    float grow;         /* 0xE4 */
    uint32_t flags;     /* 0xE8 */
    void (*callback)(struct Particle2DHit *hit); /* 0xEC */
    int count;          /* 0xF0 */
    int frameDelay;     /* 0xF4 */
    int capacity;       /* 0xF8 */
    char animated;      /* 0xFC */
    char autoDelete;    /* 0xFD */
    char unkFE[2];
} Particle2DTask;
_Static_assert(sizeof(Particle2DTask) == 0x100, "Particle2DTask");

#endif
