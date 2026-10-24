#ifndef HELI_HELI_H
#define HELI_HELI_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

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

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24; /* sum of the inputs' values plus one on each multiply */
} Matrix;

/* script string that may hold an expression */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

/* a physics object type (DefinePhysicsObjType); data is its parsed arguments */
typedef struct PhysType {
    char unk0[8];
    float power;
    char unkC[0x14C - 0xC];
    struct {
        int point;
        char unk4[0x14];
    } joints[16];       /* 0x14C */
    char unk2CC[8];
    void *data;
} PhysType;

/* "Vehicle" (0x748 bytes), a subtype of RigidDynCubeObj */
typedef struct Vehicle {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x30];
    int unk68;
    void **model;       /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x44];
    Task *coll;         /* 0xE8, its children get the move event */
    char unkEC[4];
    struct {
        char unk0[8];
        float unk8;
        char unkC[0xC];
        Vec3f velocity; /* 0x108 */
        Vec3f unk24;    /* 0x114 */
        Vec3f spin;     /* 0x120 */
    } body;             /* 0xF0, see 0x4ECDB0 */
    char unk12C[0x28];
    char name[0x14];    /* 0x154 */
    Vec3d pos2;         /* 0x168 */
    Vec3f unk180;
    Matrix matrix2;     /* 0x18C */
    PhysType *phys;     /* 0x1B4 */
    int contacts;       /* 0x1B8 */
    struct {
        int side;
        char unk4[8];
        Vec3f pos;      /* 0x0C */
        char unk18[2];
        char touching;  /* 0x1A */
        char unk1B;
        float unk1C;
        float unk20;
        char unk24[0x10];
        float height;   /* 0x34 */
    } joints[22];       /* 0x1BC */
    char unk68C[4];
    char moved;         /* 0x690 */
    char unk691[3];
    Matrix prev_matrix; /* 0x694 */
    char unk6BC[4];
    Vec3d prev_pos;     /* 0x6C0 */
    char unk6D8;
    char airborne;      /* 0x6D9 */
    char unk6DA[2];
    Matrix seat_matrix; /* 0x6DC */
    char unk704[4];
    Vec3d seat_pos;     /* 0x708 */
    float speed;        /* 0x720 */
    float turn[3];      /* 0x724 */
    float target_speed; /* 0x730 */
    float steer[3];     /* 0x734 */
    float start_speed;  /* 0x740 */
    char brake;         /* 0x744 */
    char unk745;
    char unk746[2];
} Vehicle;

/* parsed DefinePhysicsObjType arguments shared by the vehicles */
typedef struct VehicleConfig {
    int unk0;
    struct {
        float rate;     /* degrees */
        float smooth;
    } axis[3];          /* 0x04 */
    float thrust;       /* 0x1C */
    float accel;        /* 0x20 */
    float accel_slow;   /* 0x24, below speed 0.8 */
    int part_count;     /* 0x28 */
    int parts[8][7];    /* 0x2C */
} VehicleConfig;

typedef struct CarWheelConfig {
    char steered;
    char driven;
    char pad[2];
    float steer;        /* degrees at full lock */
    int joint;          /* -1 none, below -1 the joint of -10 - joint */
    float radius;
    float unk10;
} CarWheelConfig;

typedef struct CarConfig {
    VehicleConfig v;
    char unk10C[0x118 - 0x10C];
    int wheel_count;    /* 0x118 */
    CarWheelConfig wheels[16];
    float wheel_drop;   /* 0x25C */
    char wheel_objects; /* 0x260, wheels are separate Wheel tasks */
    char tank;          /* 0x261, tracks: wheels turn against each other */
} CarConfig;

typedef struct HeliConfig {
    VehicleConfig v;
    char unk10C[0x128 - 0x10C];
    int explode_points[10]; /* 0x128 */
    int explode_count;  /* 0x150 */
} HeliConfig;

typedef struct CarContact {
    char active;
    char hit;
    char pad[2];
    Vec3f normal;       /* 0x04 */
    Vec3f pos;          /* 0x10 */
} CarContact;

/* "Car" (0x2ED8 bytes) */
typedef struct Car {
    Vehicle v;
    Task *engine_sound; /* 0x748 */
    float *sound_params; /* 0x74C */
    int wheel_slots;    /* 0x750 */
    int steered_count;  /* 0x754 */
    int driven_count;   /* 0x758 */
    CarContact contacts[16]; /* 0x75C */
    char exploding;     /* 0x91C */
    char unk91D;
    char pad91E[2];
    int explode_timer;
    int last_tick;
    VarString vars[3];  /* 0x928 */
    char unkA18[0xA58 - 0xA18];
    double light_cfg[3]; /* 0xA58 */
    char unkA70[0x20];
    char light[0x2EC0 - 0xA90]; /* 0xA90 */
    float light_params[3]; /* 0x2EC0: two angles in degrees, a distance */
    char unk2ECC[4];
    Task *attacker;     /* 0x2ED0 */
    char registered;    /* 0x2ED4 */
    char exploded;      /* 0x2ED5 */
    char pad2ED6[2];
} Car;

/* "Wheel" (0x158 bytes), a part that follows a car */
typedef struct Wheel {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x30];
    int unk68;
    void **model;       /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x4C];
    Vec3d offset;       /* 0xF0 */
    Matrix local;       /* 0x108 */
    struct Car *owner;  /* 0x130 */
    int unk134;
    float spin_speed;   /* 0x138 */
    float lift;         /* 0x13C */
    float spin;         /* 0x140 */
    float steer;        /* 0x144 */
    int index;          /* 0x148 */
    PhysType *phys;     /* 0x14C */
    char dead;          /* 0x150 */
    char pad151[7];
} Wheel;

/* parsed "Wheel" arguments, after the vehicle ones (0x2F0 bytes) */
typedef struct WheelConfig {
    char unk0[0x2D8];
    char name[0x10];    /* 0x2D8 */
    float radius;       /* 0x2E8 */
    float unk2EC;
} WheelConfig;

/* "Heli" (0x2D00 bytes) */
typedef struct Heli {
    Vehicle v;
    float *sound_params; /* 0x748 */
    Task **rotor_sound; /* 0x74C */
    char exploding;     /* 0x750 */
    char lights;        /* 0x751 */
    char pad752[2];
    int explode_timer;  /* 0x754 */
    VarString rotor_expr; /* 0x758 */
    VarString light_expr; /* 0x7A8 */
    VarString engine_expr; /* 0x7F8 */
    Vec3d light_pos;    /* 0x848 */
    Matrix light_matrix; /* 0x860 */
    double light_cfg[3]; /* 0x888 */
    char unk8A0[0x18];
    char unk8B8;
    char pad8B9[7];
    char light[0x2CF0 - 0x8C0]; /* 0x8C0 */
    float light_params[3]; /* 0x2CF0 */
    char unk2CFC[4];
} Heli;

typedef struct CollHit {
    char unk0[0x58];
    int found;
    int unk5C;
    double dist;        /* 0x60 */
    char unk68[8];
} CollHit;

/* the argument of the create handler of the vehicle parts */
typedef struct PartCreateArg {
    const char *model;
    int unk4;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    Task *owner;
    void *params;
} PartCreateArg;

/* sent by a vehicle each frame to move its parts along */
typedef struct PartMoveArg {
    Vec3d *pos;
    Matrix *matrix;
    void *vehicle;
} PartMoveArg;

/* the argument of the physics type argument parsers */
typedef struct ParseArg {
    void *out;
    void *args;
    int index;
} ParseArg;

/* sent to a vehicle each tick by its driver; tick 0 resets it */
typedef struct ControlArg {
    int tick;
    float *axes;
} ControlArg;

/* name and range of a vehicle control */
typedef struct ControlInfo {
    const char *name;
    int unk4;
    float min;
    float max;
} ControlInfo;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(offsetof(PhysType, data) == 0x2D4, "PhysType");
_Static_assert(offsetof(Vehicle, contacts) == 0x1B8, "Vehicle");
_Static_assert(offsetof(Vehicle, moved) == 0x690, "Vehicle");
_Static_assert(offsetof(Vehicle, airborne) == 0x6D9, "Vehicle");
_Static_assert(offsetof(Vehicle, seat_pos) == 0x708, "Vehicle");
_Static_assert(sizeof(Vehicle) == 0x748, "Vehicle");
_Static_assert(sizeof(VehicleConfig) == 0x10C, "VehicleConfig");
_Static_assert(offsetof(CarConfig, wheels) == 0x11C, "CarConfig");
_Static_assert(offsetof(CarConfig, tank) == 0x261, "CarConfig");
_Static_assert(offsetof(HeliConfig, explode_count) == 0x150, "HeliConfig");
_Static_assert(sizeof(CarContact) == 0x1C, "CarContact");
_Static_assert(offsetof(Car, exploding) == 0x91C, "Car");
_Static_assert(offsetof(Car, light) == 0xA90, "Car");
_Static_assert(offsetof(Car, attacker) == 0x2ED0, "Car");
_Static_assert(sizeof(Car) == 0x2ED8, "Car");
_Static_assert(offsetof(Wheel, owner) == 0x130, "Wheel");
_Static_assert(sizeof(Wheel) == 0x158, "Wheel");
_Static_assert(sizeof(WheelConfig) == 0x2F0, "WheelConfig");
_Static_assert(offsetof(Heli, light_matrix) == 0x860, "Heli");
_Static_assert(offsetof(Heli, light) == 0x8C0, "Heli");
_Static_assert(sizeof(Heli) == 0x2D00, "Heli");
_Static_assert(offsetof(PartCreateArg, owner) == 0x88, "PartCreateArg");
_Static_assert(sizeof(CollHit) == 0x70, "CollHit");

extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];

/* exe constants, read in place so the x87 code sees the same values */
extern float kFixed;      /* 4096 */
extern float kTenth;
extern float kOne;
extern float kZero;
extern double kZeroD;
extern double kOneD;
extern float kThrustScale; /* 44.6 */
extern float kHalf;
extern float kQuarter;
extern float kHeliDamp;       /* 0.95 */
extern float kTwo;
extern float kTwentieth;
extern float kFour;
extern float kDegToRad;
extern float kSixTenths;
extern float kThreeTenths;
extern float kMilli;
extern float kSevenTenths;
extern float kDamp98;
extern float kTwoHundredth;
extern float kHundredth;
extern float kMinusHundredth;
extern float kGripLimit;  /* 40.96 */
extern float kGrip;       /* 0.24 */
extern double kSpeedStep; /* 0.01 */
extern double kTwoPiD;

static inline Task *next_sibling(Task *t)
{
    return t->next && t->next->next ? t->next : NULL;
}

/* calls the slot handler of each task in a child list, keeping the next one
   on gTaskStack so handlers may delete tasks; the slot is read after the push */
#define BROADCAST(first, SLOT, ...)                                     \
    do {                                                                \
        Task *c_ = (first);                                             \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            void *fn_;                                                  \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = TASK_HANDLER(SLOT, c_->type);                         \
            if (fn_)                                                    \
                ((void (*)())fn_)(c_, ##__VA_ARGS__);                   \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* x87 "not equal" as tested with C3: false when unordered */
static inline int fne(long double a, long double b)
{
    return a < b || a > b;
}

/* fistp with the current rounding mode */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

/* fchs on the x87 value, so gcc cannot move the negation after a rounding */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

/* the largest of the three scale factors */
static inline float max_scale(const float *s)
{
    float m = s[2] < s[1] ? s[1] : s[2];

    return s[0] > m ? s[0] : m;
}

#endif
