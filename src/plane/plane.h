#ifndef PLANE_PLANE_H
#define PLANE_PLANE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "magicobj/magicobjtypes.h"
#include "vehicle/vehicletypes.h"
#include "graphtask/graphtasktypes.h"
#include "plane/planetypes.h"
#include "vehbase/vehbasetypes.h"
#include "heli/helitypes.h"

typedef struct Joint {
    char unk0[0x18];
    char active;
    char unk19[0x38 - 0x19];
} Joint;

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

typedef struct GearDef {
    float angle;
    char roll;
    float unk8, unkC, unk10;
} GearDef;

/* CockpitShield, Gear, Hatch and Rudder (0x140 bytes) */
typedef struct PlanePart {
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
} PlanePart;

_Static_assert(offsetof(Plane, sound_params) == 0x828, "sound_params");
_Static_assert(sizeof(Plane) == 0x888, "Plane");
_Static_assert(sizeof(GearDef) == 0x14, "GearDef");
_Static_assert(offsetof(PlanePart, offset) == 0xF0, "offset");
_Static_assert(sizeof(PlanePart) == 0x140, "PlanePart");

typedef void (*TaskFn1)(void *task);
typedef void (*TaskFn2)(void *task, void *arg);

extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])

/* exe constants, read in place so the x87 code sees the same values */
extern float kZero;
extern float kOne;
extern float kTenth;
extern float kHundredth;
extern double kTenthD;
extern float kDegToRad;
extern float kKeep;      /* 0.98 */
extern float kBlend;     /* 0.02 */
extern float kStartSpeed;

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
void PlanePart_Init(PlanePart *o, MagicCreateArg *arg);
float PlanePart_Radius(PlanePart *o);
/* the position of a part: offset turned by the owner matrix plus the owner position;
   the x sum is kept unrounded when keep_x */
void PlanePart_Place(PlanePart *o, PartMoveArg *a, int keep_x);
void CockpitShield_SetMatrix(PlanePart *o, Matrix *parent);

#endif
