#ifndef HELI_HELI_H
#define HELI_HELI_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "coll/colltypes.h"
#include "magicobj/magicobjtypes.h"
#include "vehicle/vehicletypes.h"
#include "graphtask/graphtasktypes.h"
#include "vehbase/vehbasetypes.h"
#include "heli/helitypes.h"

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
    int wheel_count;    /* 0x118 */
    CarWheelConfig wheels[16];
    float wheel_drop;   /* 0x25C */
    char wheel_objects; /* 0x260, wheels are separate Wheel tasks */
    char tank;          /* 0x261, tracks: wheels turn against each other */
} CarConfig;

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

_Static_assert(offsetof(CarConfig, wheels) == 0x11C, "CarConfig");
_Static_assert(offsetof(CarConfig, tank) == 0x261, "CarConfig");
_Static_assert(offsetof(Wheel, owner) == 0x130, "Wheel");
_Static_assert(sizeof(Wheel) == 0x158, "Wheel");
_Static_assert(sizeof(WheelConfig) == 0x2F0, "WheelConfig");

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
