#ifndef VEHBASE_VEHBASE_H
#define VEHBASE_VEHBASE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"

FN_EXTERN(Task_Nothing);

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)();

typedef struct Node {
    struct Node *next;
    struct Node *prev;
} Node;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24;
} Matrix;

/* a script string parameter that may hold an expression */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

/* "RigidDynCubeObj" (0xF0 bytes), an object with a model */
typedef struct RigidObj {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x30];
    void *room;         /* 0x68 */
    void **model;       /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    int lod;            /* 0xA4 */
    Vec3f rgb[2];       /* 0xA8 */
    char unkC0[0x28];
    Task *parts;        /* 0xE8, a MagicObjContainer */
    int unkEC;
} RigidObj;

/* "EditRigidObj" (0x1A0 bytes) */
typedef struct EditRigidObj {
    RigidObj obj;
    Vec3d pos;          /* 0xF0 */
    Matrix matrix;      /* 0x108 */
    char model[0x10];   /* 0x130 */
    Vec3f rgb[2];       /* 0x140 */
    Vec3d offset;       /* 0x158, from the RigidDynCubeObj it sits on */
    Matrix local;       /* 0x170 */
    int active;         /* 0x198 */
    int unk19C;
} EditRigidObj;

/* sent with the RigidDynCubeObj move event */
typedef struct MoveMsg {
    Vec3d *pos;
    Matrix *matrix;
} MoveMsg;

/* World events 0 and 1 */
typedef struct WorldMsg {
    void *unk0;
    void *world;
} WorldMsg;

typedef struct AnimKey {
    char channel;       /* ANIM_WAIT holds the value rounded to an int */
    char unk1[3];
    float value;
} AnimKey;

enum {
    ANIM_CHANNELS = 0x22,
    ANIM_WAIT = 0x20,   /* frames left before the next key */
    ANIM_POS = 0x21,    /* index of the next key */
    ANIM_KEYS = 0x4000,
};

/* "AnimTask" (0x20160 bytes): plays a list of keys into 34 channels and sends
   them to a task (usually a vehicle) each tick */
typedef struct AnimTask {
    Task task;
    Vec3d pos;          /* 0x20 */
    Matrix matrix;      /* 0x38 */
    Node node;          /* 0x60, on gAnimTasks */
    struct AnimTask *self; /* 0x68 */
    float channels[ANIM_CHANNELS]; /* 0x6C */
    AnimKey keys[ANIM_KEYS]; /* 0xF4 */
    char isRun;         /* 0x200F4 */
    char registered;    /* 0x200F5 */
    char unk200F6[2];
    VarString cond;     /* 0x200F8 */
    Task **ref;         /* 0x20148 */
    int target;         /* 0x2014C, task id */
    int keyCount;       /* 0x20150 */
    int tick;           /* 0x20154 */
    char button[2];     /* 0x20158 */
    char unk2015A[6];
} AnimTask;

/* the AnimData parameter as copied by its copy / paste ops */
typedef struct AnimData {
    int keyCount;
    float channels[ANIM_CHANNELS];
    AnimKey keys[ANIM_KEYS];
} AnimData;

/* "Vehicle" (0x748 bytes), a subtype of PhysicsObj */
typedef struct Vehicle {
    RigidObj obj;
    char unkF0[0x18];
    Vec3f velocity;     /* 0x108, of the rigid body */
    char unk114[0x720 - 0x114];
    float speed;        /* 0x720 */
    float turn[3];      /* 0x724 */
    float target_speed; /* 0x730 */
    float steer[3];     /* 0x734 */
    float start_speed;  /* 0x740 */
    char brake;         /* 0x744 */
    char active;        /* 0x745 */
    char unk746[2];
} Vehicle;

/* saved by Vehicle_SaveState */
typedef struct VehicleState {
    Vec3f velocity;
    int unkC;
    Vec3d pos;          /* 0x10 */
    Matrix matrix;      /* 0x28 */
    float speed;        /* 0x50 */
    float turn[3];
    float target_speed;
    float steer[3];
} VehicleState;

/* RigidDynCubeObj slot 7 */
typedef struct ObjInfo {
    Vec3d pos;
    Vec3f velocity;     /* 0x18 */
    Matrix matrix;      /* 0x24 */
    float radius;       /* 0x4C */
    int unk50;
} ObjInfo;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(sizeof(RigidObj) == 0xF0, "RigidObj");
_Static_assert(offsetof(EditRigidObj, offset) == 0x158, "offset");
_Static_assert(sizeof(EditRigidObj) == 0x1A0, "EditRigidObj");
_Static_assert(offsetof(AnimTask, channels) == 0x6C, "channels");
_Static_assert(offsetof(AnimTask, isRun) == 0x200F4, "isRun");
_Static_assert(offsetof(AnimTask, ref) == 0x20148, "ref");
_Static_assert(offsetof(AnimTask, button) == 0x20158, "button");
_Static_assert(sizeof(AnimTask) == 0x20160, "AnimTask");
_Static_assert(sizeof(AnimData) == 0x2008C, "AnimData");
_Static_assert(offsetof(Vehicle, speed) == 0x720, "speed");
_Static_assert(sizeof(Vehicle) == 0x748, "Vehicle");
_Static_assert(sizeof(VehicleState) == 0x70, "VehicleState");
_Static_assert(offsetof(ObjInfo, unk50) == 0x50, "ObjInfo");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

/* calls the slot handler of each child of t, keeping the next child on
   gTaskStack so handlers may delete tasks; the slot is read after the push */
#define BROADCAST(t, SLOT, ...)                                         \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = HANDLER(SLOT, c_->type);                              \
            if (fn_)                                                    \
                fn_(c_, ##__VA_ARGS__);                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* the largest of the three scale factors, compared like the original */
static inline float max_scale(const RigidObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
