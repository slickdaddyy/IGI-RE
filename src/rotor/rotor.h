#ifndef ROTOR_ROTOR_H
#define ROTOR_ROTOR_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"

typedef void (*TaskFn)(void *task, void *arg);

typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

/* a MagicObj definition; data is the block parsed from its script arguments */
typedef struct MagicDef {
    char unk0[8];
    float power;
    char unkC[0x2D4 - 0xC];
    void *data;
} MagicDef;

/* "RigidDynCubeObj" (0xF0 bytes) */
typedef struct RigidObj {
    Task task;
    Vec3d pos;
    char unk38[0x10];
    int unk48;
    char unk4C[0x20];
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0xF0 - 0xA4];
} RigidObj;

/* create argument of the vehicle parts */
typedef struct PartCreateArg {
    const char *model;
    int unk4;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    struct Car *owner;
    void *params;
} PartCreateArg;

/* sent by a vehicle each frame to move its parts along */
typedef struct PartMoveArg {
    Vec3d *pos;
    Matrix *matrix;
} PartMoveArg;

/* the same with the vehicle itself */
typedef struct CarMoveArg {
    Vec3d *pos;
    Matrix *matrix;
    struct Car *car;
} CarMoveArg;

/* argument of the MagicObj definition parsers */
typedef struct DefParseArg {
    void *out;
    void *args;
    int index;
} DefParseArg;

/* Afterburner and Rotor (0x140 bytes) */
typedef struct Part {
    RigidObj obj;
    Vec3d offset;
    Matrix local;
    struct Car *owner;
    int unk134;
    float value;
    MagicDef *def;
} Part;

/* a draw call queued for the frame (0x24 bytes, 0x30 for Afterburner) */
typedef struct Prim {
    int unk0;
    int type;
    int unk8;
    void *obj;
    int unk10;
    int unk14;
    int unk18;
    int unk1C;
    int unk20;
    float color[3];
} Prim;

/* position and orientation as sent to the children by value */
typedef struct Frame {
    Vec3d pos;
    Matrix matrix;
} Frame;

typedef struct CarWheel {
    char unk0;
    char unk1;
    char pad[2];
    float unk4;
    int joint;
    float radius;
    float unk10;
} CarWheel;

/* parsed "Car" MagicObj arguments (0x2C0 bytes) */
typedef struct CarData {
    float unk0;
    char unk4[0x14];
    float steerLag;
    char unk1C[4];
    float throttleLag;
    char unk24[4];
    int unk28;
    char unk2C[0x118 - 0x2C];
    int wheelCount;
    CarWheel wheels[16];
    float wheelDrop;
    char unk260;
    char unk261;
    char engineSound[0x20];
    char pad282[2];
    int explodePoints[10];
    int explodeCount;
    char explodeObj[0x10];
} CarData;

/* a model joint of the vehicle */
typedef struct CarJoint {
    char unk0[0xC];
    float pos[3];
    char unk18;
    char unk19;
    char unk1A;
    char unk1B[0x34 - 0x1B];
    float unk34;
} CarJoint;

typedef struct CarContact {
    char active;
    char hit;
    char pad[2];
    float normal[3];
    float pos[3];
} CarContact;

typedef struct Car {
    Task task;
    Vec3d pos;
    char unk38[0x10];
    int unk48;
    char unk4C[0x20];
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0xE8 - 0xA4];
    Task *parts;
    char unkEC[4];
    float body[6];
    float spin[3];
    char unk114[0x154 - 0x114];
    char name[0x10];
    char unk164[4];
    Vec3d spawnPos;
    float unk180[3];
    Matrix spawnMatrix;
    MagicDef *def;
    int unk1B8;
    CarJoint joints[23];
    char unk6C4[0x6D8 - 0x6C4];
    char unk6D8;
    char unk6D9;
    char unk6DA[0x720 - 0x6DA];
    float throttle;
    char unk724[8];
    float steer;
    float targetThrottle;
    char unk734[8];
    float targetSteer;
    float maxThrottle;
    char unk744;
    char pad745[3];
    Task *engineSound;
    float *engineParams;
    int unk750;
    int unk754;
    int unk758;
    CarContact contacts[16];
    char exploding;
    char unk91D;
    char pad91E[2];
    int explodeTimer;
    int lastTick;
    VarString doorExpr;
    VarString unk978;
    VarString unk9C8;
    Vec3d unkA18;
    Matrix unkA30;
    char unkA58[0xA88 - 0xA58];
    char unkA88;
    char unkA89[7];
    char unkA90[0x2EC0 - 0xA90];
    float unk2EC0;
    float unk2EC4;
    float unk2EC8;
    char unk2ECC;
    char pad2ECD[3];
    int unk2ED0;
    char unk2ED4;
    char wrecked;
    char pad2ED6[2];
} Car;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(sizeof(RigidObj) == 0xF0, "RigidObj");
_Static_assert(offsetof(PartCreateArg, owner) == 0x88, "PartCreateArg");
_Static_assert(sizeof(Part) == 0x140, "Part");
_Static_assert(offsetof(Prim, color) == 0x24, "Prim");
_Static_assert(sizeof(CarData) == 0x2C0, "CarData");
_Static_assert(offsetof(CarData, explodePoints) == 0x284, "CarData");
_Static_assert(sizeof(CarJoint) == 0x38, "CarJoint");
_Static_assert(sizeof(CarContact) == 0x1C, "CarContact");
_Static_assert(offsetof(Car, def) == 0x1B4, "Car");
_Static_assert(offsetof(Car, throttle) == 0x720, "Car");
_Static_assert(offsetof(Car, contacts) == 0x75C, "Car");
_Static_assert(offsetof(Car, doorExpr) == 0x928, "Car");
_Static_assert(offsetof(Car, unkA88) == 0xA88, "Car");
_Static_assert(sizeof(Car) == 0x2ED8, "Car");

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;

#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* Calls fn(child, arg) for each child of t (not deeper), keeping the next
   child on gTaskStack so handlers may delete tasks. slot is evaluated
   once per child, after the push, like the original. */
#define EACH_CHILD(t, slot, arg)                                        \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next)                                                  \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = HANDLER(slot, c_->type);                              \
            if (fn_)                                                    \
                fn_(c_, arg);                                           \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* the same for the run slot, whose handlers take no argument */
#define RUN_CHILDREN(t)                                                 \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next)                                                  \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = HANDLER(1, c_->type);                                 \
            if (fn_)                                                    \
                ((void (*)(void *))fn_)(c_);                            \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode */
static inline long long x87_round(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

static inline long double x87_cos(long double x)
{
    __asm__("fcos" : "+t"(x));
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

/* out = a * b. The original inlines this everywhere and sums the three
   products in an order that depends on the row: lastN is the k added last
   in row N. */
static inline void mat_mul(Matrix *out, const Matrix *a, const Matrix *b, int last0, int last1, int last2)
{
    int i, j;

    for (i = 0; i < 3; i++) {
        int last = i == 0 ? last0 : i == 1 ? last1 : last2;
        int k0 = last == 0 ? 1 : 0;
        int k1 = last == 2 ? 1 : 2;

        for (j = 0; j < 3; j++)
            out->m[i * 3 + j] = (float)((long double)a->m[i * 3 + k0] * b->m[k0 * 3 + j]
                                        + (long double)a->m[i * 3 + k1] * b->m[k1 * 3 + j]
                                        + (long double)a->m[i * 3 + last] * b->m[last * 3 + j]);
    }
    out->count = a->count + b->count + 1;
}

#endif
