#ifndef GRAPHTASK_H
#define GRAPHTASK_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

typedef struct Node {
    struct Node *next;
    struct Node *prev;
} Node;

typedef struct List {
    Node *head;
    Node *tail;
    Node *tailpred;
} List;

typedef struct Task {
    Node node;
    List children;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)(void *task, void *arg);

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define HANDLER(slot, type) gTaskHandlers[(uint8_t)(slot)][(uint16_t)(type)]
extern Task *gTaskStack[];
extern int gTaskStackTop;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* 3x3 rotation, row major, and a counter of the products it came from */
typedef struct Matrix {
    float m[9];
    int count;
} Matrix;

/* one result of Coll_Lines (0x70 bytes) */
typedef struct CollHit {
    Task *task;
    int unk4;
    Vec3d pos;
    char unk20[0x18];
    Vec3f normal;       /* 0x38 */
    int unk44;
    int unk48;
    void *material;     /* 0x4C */
    float *material2;   /* 0x50, its float at +0xC is the bounce */
    int unk54;
    int found;          /* 0x58 */
    int unk5C;
    double dist;        /* 0x60 */
    char unk68[8];
} CollHit;

/* the rigid body of a PhysicsObj (0x64 bytes, see the animtrig notes) */
typedef struct Body {
    float mass;
    float invMass;
    float speed;
    float invInertia[3];
    Vec3f vel;          /* 0x18 */
    Vec3f dir;          /* 0x24 */
    Vec3f angMom;       /* 0x30 */
    Vec3f force;        /* 0x3C */
    Vec3f angVel;       /* 0x48 */
    void *forces;       /* 0x54 */
    int maxForces;
    int forceCount;     /* 0x5C */
    int lastForceCount;
} Body;

/* a point of a physics object type */
typedef struct PhysJoint {
    int point;
    int unk4;
    int unk8;
    float bounce;
    float unk10;
    float friction;
} PhysJoint;

/* one DefinePhysicsObjType() entry */
typedef struct PhysType {
    uint16_t type;
    int unk4;
    float mass;         /* 0x08 */
    float size[3];
    char unk18[0x120];
    char model[0x10];   /* 0x138 */
    int jointCount;     /* 0x148 */
    PhysJoint joints[16];
    float restSpeed;    /* 0x2CC */
    float restSpin;     /* 0x2D0, squared */
    void *data;
} PhysType;

/* a model point the physics object stands on */
typedef struct Contact {
    int unk0;
    int unk4;
    float bounce;       /* 0x08 */
    Vec3f point;        /* 0x0C, object space */
    char active;        /* 0x18 */
    char enabled;       /* 0x19 */
    char touching;      /* 0x1A */
    char unk1B;
    float unk1C;
    float friction;     /* 0x20 */
    Vec3f normal;       /* 0x24 */
    float dist;         /* 0x30 */
    int unk34;
} Contact;

#define PHYS_CONTACTS 22

/* "PhysicsObj" (0x720 bytes): a RigidDynCubeObj moved by a rigid body */
typedef struct PhysObj {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x34];
    void **model;       /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x4C];
    Body body;          /* 0xF0 */
    char name[0x14];    /* 0x154 */
    Vec3d startPos;     /* 0x168 */
    Vec3f startVel;     /* 0x180 */
    Matrix startMatrix; /* 0x18C */
    PhysType *phys;     /* 0x1B4 */
    int touching;       /* 0x1B8 */
    Contact contacts[PHYS_CONTACTS]; /* 0x1BC */
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

/* sent to a task the physics object ran into, result 1 ignores the hit */
typedef struct PhysHitMsg {
    PhysObj *obj;
    CollHit *hit;
    int result;
} PhysHitMsg;

/* the motion of a point carried by a physics object */
typedef struct PhysPointMsg {
    Vec3d *point;
    Vec3d move;
    Matrix turn;
} PhysPointMsg;

_Static_assert(sizeof(CollHit) == 0x70, "CollHit");
_Static_assert(sizeof(Body) == 0x64, "Body");
_Static_assert(offsetof(PhysType, joints) == 0x14C, "PhysType.joints");
_Static_assert(offsetof(PhysType, data) == 0x2D4, "PhysType.data");
_Static_assert(sizeof(Contact) == 0x38, "Contact");
_Static_assert(offsetof(PhysObj, body) == 0xF0, "PhysObj.body");
_Static_assert(offsetof(PhysObj, contacts) == 0x1BC, "PhysObj.contacts");
_Static_assert(offsetof(PhysObj, contactCount) == 0x68C, "PhysObj.contactCount");
_Static_assert(offsetof(PhysObj, lastPos) == 0x6C0, "PhysObj.lastPos");
_Static_assert(offsetof(PhysObj, turn) == 0x6DC, "PhysObj.turn");
_Static_assert(sizeof(PhysObj) == 0x720, "PhysObj");
_Static_assert(offsetof(PhysPointMsg, turn) == 0x20, "PhysPointMsg");

/* x87 compare results: NaN counts as equal, as with fcomp + test ah,0x40 */
#define NONZERO(x) ((x) < 0 || (x) > 0)

#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }
#define F64(name, bits) static const union { uint64_t u; double d; } name = { bits }

#endif
