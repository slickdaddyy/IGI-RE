#ifndef MISSILE_MISSILE_H
#define MISSILE_MISSILE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "missile/missiletypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn1)(Task *task);
typedef void (*TaskFn2)(Task *task, void *arg);

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

_Static_assert(offsetof(MissileConfig, smoke) == 0x118, "smoke");
_Static_assert(sizeof(MissileConfig) == 0x250, "MissileConfig");
_Static_assert(offsetof(Missile, target) == 0x828, "target");
_Static_assert(offsetof(Missile, fuel) == 0x864, "fuel");
_Static_assert(sizeof(Missile) == 0x898, "Missile");

extern Task *gTaskStack[];
extern int gTaskStackTop;
#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];

/* exe constants, read in place so the x87 code sees the same values */
extern double kMinusOneD;

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
