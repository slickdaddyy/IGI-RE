#ifndef ROTOR_ROTOR_H
#define ROTOR_ROTOR_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "vehicle/vehicletypes.h"
#include "plane/planetypes.h"
#include "heli/helitypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn)(void *task, void *arg);

/* a MagicObj definition; data is the block parsed from its script arguments */
/* the same with the vehicle itself */
typedef struct CarMoveArg {
    Vec3d *pos;
    Matrix *matrix;
    struct Car *car;
} CarMoveArg;

/* Afterburner and Rotor (0x140 bytes) */
typedef struct Part {
    RigidDynCubeObj obj;
    Vec3d offset;
    Matrix local;
    struct Car *owner;
    int unk134;
    float value;
    PhysType *def;
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
_Static_assert(sizeof(Part) == 0x140, "Part");
_Static_assert(offsetof(Prim, color) == 0x24, "Prim");
_Static_assert(sizeof(CarData) == 0x2C0, "CarData");
_Static_assert(offsetof(CarData, explodePoints) == 0x284, "CarData");

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

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
