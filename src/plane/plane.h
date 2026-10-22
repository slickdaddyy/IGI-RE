#ifndef PLANE_PLANE_H
#define PLANE_PLANE_H

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

/* per type data of a vehicle physics type, parsed by 0x5031F0 */
typedef struct VehicleConfig {
    float unk0;         /* start speed scale */
    struct {
        float rate;
        float smooth;
    } axis[3];          /* 0x04 */
    float thrust;       /* 0x1C */
    float accel;        /* 0x20 */
    float accel_slow;   /* 0x24 */
    int part_count;     /* 0x28 */
    float parts[8][7];  /* 0x2C */
    float unk10C[3];
} VehicleConfig;

/* the extra config of "Heli" */
typedef struct HeliConfig {
    VehicleConfig v;
    int count1;         /* 0x118 */
    int list[13];       /* 0x11C, the second list starts at list[3] */
    int count2;         /* 0x150 */
} HeliConfig;

/* the physics object type a vehicle was made from (DefinePhysicsObjType) */
typedef struct PhysType {
    char unk0[8];
    float power;
    char unkC[0x148 - 0xC];
    int joint_count;    /* 0x148 */
    char unk14C[0x2D4 - 0x14C];
    VehicleConfig *data;
} PhysType;

typedef struct Joint {
    char unk0[0x18];
    char active;
    char unk19[0x38 - 0x19];
} Joint;

/* "Vehicle" (0x748 bytes), a subtype of RigidDynCubeObj */
typedef struct Vehicle {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x34];
    void **model;       /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x44];
    Task *coll;         /* 0xE8, its children get the move event */
    char unkEC[4];
    struct {
        char unk0[8];
        float unk8;     /* 0xF8 */
        char unkC[0xC];
        float velocity[3]; /* 0x108 */
        float unk24[3]; /* 0x114 */
        float spin[3];  /* 0x120 */
    } body;             /* 0xF0, see 0x4ECDB0 */
    char unk12C[0x28];
    char name[0x14];    /* 0x154 */
    Vec3d pos2;         /* 0x168 */
    float velocity2[3]; /* 0x180 */
    Matrix matrix2;     /* 0x18C */
    PhysType *phys;     /* 0x1B4 */
    int unk1B8;
    Joint joints[23];   /* 0x1BC */
    char unk6C4[0x14];
    char unk6D8;
    char unk6D9;
    char unk6DA[0x46];
    float speed;        /* 0x720 */
    float turn[3];      /* 0x724 */
    float target_speed; /* 0x730 */
    float steer[3];     /* 0x734 */
    float start_speed;  /* 0x740 */
    char brake;         /* 0x744 */
    char unk745[3];
} Vehicle;

/* "Plane" (0x888 bytes) */
typedef struct Plane {
    Vehicle v;
    float parts[8][7];  /* 0x748 */
    float *sound_params; /* 0x828 */
    char cockpit;       /* 0x82C, canopy open */
    char gear;          /* 0x82D, wheels touch the ground while 0 */
    char catapult;      /* 0x82E */
    char force_z;       /* 0x82F */
    char cockpit_init;  /* 0x830 */
    char gear_init;     /* 0x831 */
    signed char gear_state;  /* 0x832, set by the Gear part */
    signed char hatch_state; /* 0x833, set by the Hatch part */
    Task **sound;       /* 0x834 */
    VarString script;   /* 0x838 */
} Plane;

/* the "Heli" fields used here */
typedef struct Heli {
    Vehicle v;
    char unk748[4];
    Task **sound;       /* 0x74C */
    char unk750[0x8C0 - 0x750];
    char unk8C0[1];
} Heli;

/* the plane's input, see Plane_GetControl */
typedef struct PlaneInput {
    float alpha, beta, gamma;
    float thrust;       /* -1: unchanged */
    float fire;
    float cockpit;      /* the toggles below are -1 or 1 */
    float gear;
    float brake;
    float catapult;
    float force_z;
} PlaneInput;

typedef struct PlaneControlArg {
    int mode;           /* 0: reset to the start position, > 0: fly */
    PlaneInput *input;
} PlaneControlArg;

/* create argument of the vehicle parts */
typedef struct PartCreateArg {
    const char *model;
    void *world;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    Plane *owner;
    void *def;
} PartCreateArg;

/* sent by a vehicle to move its parts along */
typedef struct PartMoveArg {
    Vec3d *pos;
    Matrix *matrix;
    Plane *owner;
} PartMoveArg;

/* argument of the MagicObj definition parsers */
typedef struct DefParseArg {
    void *out;
    void *args;
    int index;
} DefParseArg;

typedef struct GearDef {
    float angle;
    char roll;
    float unk8, unkC, unk10;
} GearDef;

typedef struct RudderDef {
    int kind;
    float angle;
} RudderDef;

/* CockpitShield, Gear, Hatch and Rudder (0x140 bytes) */
typedef struct Part {
    Task task;
    Vec3d pos;
    char unk38[0x34];
    void **model;       /* 0x6C */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x4C];
    Vec3d offset;       /* 0xF0 */
    Matrix local;       /* 0x108 */
    Plane *owner;       /* 0x130 */
    int unk134;
    void *def;          /* 0x138 */
    float angle;        /* 0x13C */
} Part;

_Static_assert(offsetof(Vehicle, coll) == 0xE8, "coll");
_Static_assert(offsetof(Vehicle, body.spin) == 0x120, "spin");
_Static_assert(offsetof(Vehicle, name) == 0x154, "name");
_Static_assert(offsetof(Vehicle, phys) == 0x1B4, "phys");
_Static_assert(offsetof(Vehicle, joints[0].active) == 0x1D4, "joints");
_Static_assert(offsetof(Vehicle, speed) == 0x720, "speed");
_Static_assert(sizeof(Vehicle) == 0x748, "Vehicle");
_Static_assert(sizeof(VehicleConfig) == 0x118, "VehicleConfig");
_Static_assert(offsetof(HeliConfig, count2) == 0x150, "HeliConfig");
_Static_assert(offsetof(PhysType, data) == 0x2D4, "PhysType");
_Static_assert(offsetof(Plane, sound_params) == 0x828, "sound_params");
_Static_assert(sizeof(Plane) == 0x888, "Plane");
_Static_assert(offsetof(Heli, unk8C0) == 0x8C0, "Heli");
_Static_assert(offsetof(PartCreateArg, owner) == 0x88, "PartCreateArg");
_Static_assert(sizeof(GearDef) == 0x14, "GearDef");
_Static_assert(offsetof(Part, offset) == 0xF0, "offset");
_Static_assert(sizeof(Part) == 0x140, "Part");

typedef void (*TaskFn1)(void *task);
typedef void (*TaskFn2)(void *task, void *arg);

#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define TASK_HANDLER(slot, type) (((void **)0xA96AE0)[(slot) * 0x180 + (type)])

/* exe constants, read in place so the x87 code sees the same values */
#define kZero VAR(float, 0x5333EC)
#define kOne VAR(float, 0x5333E0)
#define kTenth VAR(float, 0x5333C8)
#define kHundredth VAR(float, 0x533714)
#define kTenthD VAR(double, 0x5337C8)
#define kDegToRad VAR(float, 0x53367C)
#define kKeep VAR(float, 0x5336DC)      /* 0.98 */
#define kBlend VAR(float, 0x5336D8)     /* 0.02 */
#define kStartSpeed VAR(float, 0x533778)

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

/* the largest scale factor, compared like the original */
static inline float max_scale(const float *s)
{
    float m = s[2] < s[1] ? s[1] : s[2];

    return s[0] > m ? s[0] : m;
}

/* 1 when the part is near its rest angle, 0 near the full angle, else -1 */
static inline signed char part_state(long double full, float angle)
{
    long double a = angle, d;

    if (a < kZero)
        a = -a;
    if (a < kTenth)
        return 1;
    d = full - a;
    if (d < kZero)
        d = -d;
    return d < kTenthD ? 0 : -1;
}

/* the common part of the part create handlers, in part.c */
void PlanePart_Init(Part *o, PartCreateArg *arg);
float PlanePart_Radius(Part *o);
/* the position of a part: offset turned by the owner matrix plus the owner position;
   the x sum is kept unrounded when keep_x */
void PlanePart_Place(Part *o, PartMoveArg *a, int keep_x);
void CockpitShield_SetMatrix(Part *o, Matrix *parent);

#endif
