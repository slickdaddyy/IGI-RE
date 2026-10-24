#ifndef PROPS_PROPS_H
#define PROPS_PROPS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "magicobj/magicobjtypes.h"

FN_EXTERN(Task_Nothing);

typedef void (*TaskFn)();

/* the physics object type a PhysicsObj was made from */
typedef struct PhysType {
    char unk0[8];
    float mass;         /* 0x08 */
} PhysType;

/* "PhysicsObj" (0x720 bytes), a RigidDynCubeObj moved by a rigid body */
typedef struct PhysObj {
    RigidDynCubeObj obj;
    struct {
        char unk0[0x18];
        Vec3f velocity; /* 0x108 */
        char unk24[0x30];
    } body;             /* 0xF0, see 0x4ECDB0 */
    char unk144[0x10];
    char name[0x14];    /* 0x154 */
    Vec3d start_pos;    /* 0x168 */
    Vec3f start_velocity; /* 0x180 */
    Matrix start_matrix; /* 0x18C */
    PhysType *phys;     /* 0x1B4 */
    char unk1B8[0x6D8 - 0x1B8];
    char unk6D8;        /* two Bool8 script parameters */
    char unk6D9;
    char unk6DA[0x720 - 0x6DA];
} PhysObj;

/* the search used to pick what a human can use (see 0x416AD0) */
typedef struct UseQuery {
    Task *human;
    int unk4;
    Vec3d pos;          /* 0x08 */
    Matrix matrix;      /* 0x20 */
    char unk48[0x28];
    float best_pitch;   /* 0x70 */
    float best_yaw;     /* 0x74 */
    double range;       /* 0x78 */
    void *best;         /* 0x80 */
} UseQuery;

/* sent with the RigidDynCubeObj move event */
typedef struct MoveMsg {
    Vec3d *pos;
    Matrix *matrix;
} MoveMsg;

/* the collision filter message of 0x4FC0E0 */
typedef struct CollMsg {
    int unk0;
    void *obj;
} CollMsg;

_Static_assert(offsetof(PhysObj, body.velocity) == 0x108, "velocity");
_Static_assert(offsetof(PhysObj, name) == 0x154, "name");
_Static_assert(offsetof(PhysObj, phys) == 0x1B4, "phys");
_Static_assert(sizeof(PhysObj) == 0x720, "PhysObj");
_Static_assert(offsetof(UseQuery, range) == 0x78, "UseQuery");

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

/* exe constants, read in place */
extern float kZero;
extern double kZeroD;

/* copies 4 bytes without going through the FPU, like the original */
#define MOV32(dst, src) memcpy(&(dst), &(src), 4)

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

/* fistp with the current rounding mode */
static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* the largest of the three scale factors, compared like the original */
static inline float max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

/* out = a * b. Each element sums three products p0..p2 (p_k = a[i][k] * b[k][j]);
   rows set in b_rows add (p1 + p2) + p0, the others (p0 + p2) + p1. */
static inline void mat_mul(Matrix *out, const Matrix *a, const Matrix *b, int b_rows)
{
    Matrix n;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            long double p0 = (long double)a->m[i * 3] * b->m[j];
            long double p1 = (long double)a->m[i * 3 + 1] * b->m[3 + j];
            long double p2 = (long double)a->m[i * 3 + 2] * b->m[6 + j];

            n.m[i * 3 + j] = b_rows & (1 << i) ? (p1 + p2) + p0 : (p0 + p2) + p1;
        }
    }
    n.count = a->count + b->count + 1;
    *out = n;
}

#endif
