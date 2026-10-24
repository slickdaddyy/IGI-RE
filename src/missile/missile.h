#ifndef MISSILE_MISSILE_H
#define MISSILE_MISSILE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "param/paramtypes.h"
#include "coll/colltypes.h"

typedef void (*TaskFn1)(Task *task);
typedef void (*TaskFn2)(Task *task, void *arg);

/* the physics object type a vehicle was made from (DefinePhysicsObjType) */
typedef struct PhysType {
    char unk0[0x2D4];
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
    } body;             /* 0xF0, see 0x4ECDB0 */
    char unk120[0x34];
    char name[0x14];    /* 0x154 */
    Vec3d pos2;         /* 0x168 */
    char unk180[0xC];
    Matrix matrix2;     /* 0x18C */
    PhysType *phys;     /* 0x1B4 */
    int unk1B8;
    char unk1BC[0x4D8];
    Matrix prev_matrix; /* 0x694 */
    char unk6BC[4];
    Vec3d prev_pos;     /* 0x6C0 */
    char unk6D8[0x48];
    float speed;        /* 0x720 */
    float turn[3];      /* 0x724 */
    float target_speed; /* 0x730 */
    float steer[3];     /* 0x734 */
    float unk740;
    char unk744;
    char unk745;
    char unk746[2];
} Vehicle;

/* per type data of a missile physics type, read by Missile_ParseArgs */
typedef struct MissileConfig {
    int unk0;
    struct {
        float rate;     /* degrees */
        float smooth;
    } axis[3];          /* 0x04 */
    float thrust;       /* 0x1C */
    float accel;        /* 0x20 */
    float accel_slow;   /* 0x24, below speed 0.7 */
    int part_count;     /* 0x28 */
    int parts[8][7];    /* 0x2C */
    char unk10C[0xC];
    int smoke;          /* 0x118 */
    int type;           /* 0x11C */
    float fuel;         /* 0x120 */
    float burn;         /* 0x124 */
    float unk128;
    float smoke_spacing; /* 0x12C */
    float unk130;
    float smoke_interval; /* 0x134 */
    float smoke_size;   /* 0x138 */
    char sprite[0x100]; /* 0x13C */
    char sound[0x10];   /* 0x23C */
    void *sprite_handle; /* 0x24C */
} MissileConfig;

enum {
    MISSILE_DIRECT,
    MISSILE_HIND_HELICOPTER,
    MISSILE_BALLISTIC,
    MISSILE_ANTITANK,
    MISSILE_KEYBOARD,
    MISSILE_BOMB,
};

typedef struct Missile {
    Vehicle v;
    int parts[8][7];    /* 0x748 */
    Vec3d target;       /* 0x828 */
    Vec3d dir;          /* 0x840 */
    Task **owner;       /* 0x858 */
    Task **target_task; /* 0x85C */
    Task **follow;      /* 0x860 */
    float fuel;         /* 0x864 */
    void *smoke;        /* 0x868 */
    float unk86C[3];
    int smoke_tick;     /* 0x878 */
    float unk87C;
    float smoke_interval; /* 0x880 */
    float smoke_spacing; /* 0x884 */
    int unk888;
    float smoke_size;   /* 0x88C */
    void *sound;        /* 0x890 */
    int unk894;
} Missile;

typedef struct Train {
    Vehicle v;
    float frac;         /* 0x748, model front as a fraction of its length */
    float length;       /* 0x74C */
    float accel;        /* 0x750 */
    float max_speed;    /* 0x754 */
    float start_speed;  /* 0x758 */
    int unk75C;
    int unk760;
    float velocity;     /* 0x764 */
    float dist;         /* 0x768, along the path */
    float start_dist;   /* 0x76C */
    float offset[2];    /* 0x770 */
    int wagons;         /* 0x778 */
    int path;           /* 0x77C */
    char model[0x100];  /* 0x780 */
    char reverse;       /* 0x880 */
    char unk881[3];
    float *sound_params; /* 0x884 */
    int last_tick;      /* 0x888 */
    Task *loop_sound;   /* 0x88C */
    VarString script;   /* 0x890 */
} Train;

_Static_assert(offsetof(Vehicle, coll) == 0xE8, "coll");
_Static_assert(offsetof(Vehicle, body.velocity) == 0x108, "velocity");
_Static_assert(offsetof(Vehicle, name) == 0x154, "name");
_Static_assert(offsetof(Vehicle, phys) == 0x1B4, "phys");
_Static_assert(offsetof(Vehicle, prev_matrix) == 0x694, "prev_matrix");
_Static_assert(offsetof(Vehicle, prev_pos) == 0x6C0, "prev_pos");
_Static_assert(offsetof(Vehicle, speed) == 0x720, "speed");
_Static_assert(sizeof(Vehicle) == 0x748, "Vehicle");
_Static_assert(offsetof(MissileConfig, smoke) == 0x118, "smoke");
_Static_assert(sizeof(MissileConfig) == 0x250, "MissileConfig");
_Static_assert(offsetof(Missile, target) == 0x828, "target");
_Static_assert(offsetof(Missile, fuel) == 0x864, "fuel");
_Static_assert(sizeof(Missile) == 0x898, "Missile");
_Static_assert(offsetof(Train, model) == 0x780, "model");
_Static_assert(sizeof(Train) == 0x8E0, "Train");

extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];

/* exe constants, read in place so the x87 code sees the same values */
extern float kZero;
extern float kOne;
extern float kFixed;      /* 4096 */
extern float kThirty;
extern float kTenth;
extern double kOneD;
extern double kMinusOneD;
extern float kHalf;
extern float kTwoPi;
extern float kKmh;
extern float kPerFrameHour;
extern float kDegToRad;
extern float kMilli;
extern float kSevenTenths;

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
static inline float max_scale(const Vehicle *v)
{
    float m = v->scale[2] < v->scale[1] ? v->scale[1] : v->scale[2];

    return v->scale[0] > m ? v->scale[0] : m;
}

/* m = m * r. Each element sums three products; rows set in b_rows add
   (p1 + p2) + p0, the others (p2 + p0) + p1, matching the inlined copies. */
static inline void mat_mul(Matrix *m, const Matrix *r, int b_rows)
{
    Matrix n;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            long double p0 = (long double)r->m[j] * m->m[i * 3];
            long double p1 = (long double)r->m[3 + j] * m->m[i * 3 + 1];
            long double p2 = (long double)r->m[6 + j] * m->m[i * 3 + 2];

            n.m[i * 3 + j] = b_rows & (1 << i) ? (p1 + p2) + p0 : (p2 + p0) + p1;
        }
    }
    n.count = r->count + m->count + 1;
    *m = n;
}

#endif
