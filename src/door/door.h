#ifndef DOOR_DOOR_H
#define DOOR_DOOR_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "door/doortypes.h"
#include "props/propstypes.h"
#include "levelflow/levelflowtypes.h"
#include "task/taskvars.h"
#include "math/mathvars.h"

typedef void (*TaskFn)();

/* "Switch" (0x228) */
typedef struct Switch {
    RigidDynCubeObj o;
    Vec3d place;        /* 0xF0 */
    Matrix matrix;      /* 0x108 */
    Vec3d offset;       /* 0x130, place relative to the parent object */
    Matrix local;       /* 0x148, matrix relative to the parent object */
    char models[5][0x10]; /* 0x170: on, on pressed, off, off pressed, destroyed */
    char isOn;          /* 0x1C0 */
    char isLastOn;
    char isDestroyed;
    char isLastDestroyed;
    char isPressed;     /* 0x1C4 */
    char isLastPressed;
    char unk1C6[2];
    int pressTick;      /* 0x1C8 */
    char registered;    /* 0x1CC */
    char unk1CD[3];
    VarString on;       /* 0x1D0 */
    char destroyable;   /* 0x220 */
    char unk221[7];
} Switch;

/* "Door" (0x328): turns around z and/or slides in x/y */
typedef struct Door {
    RigidDynCubeObj o;
    Vec3d place;        /* 0xF0 */
    float slide[2];     /* 0x108 */
    Matrix base;        /* 0x110, the closed matrix */
    float slidePos;     /* 0x138, 0 closed .. 1 open */
    char pickable;      /* 0x13C */
    char unk13D[3];
    float openAngle;    /* 0x140, degrees */
    float openTime;     /* 0x144, seconds */
    char model[0x10];   /* 0x148 */
    char sounds[3][0x10]; /* 0x158: opened, closed, moving */
    float angleTarget;  /* 0x188 */
    float angle;        /* 0x18C */
    float slideTarget;  /* 0x190 */
    float slideLen;     /* 0x194 */
    char isLocked;      /* 0x198 */
    char isOpen;
    char isClosed;
    char isLastOpen;
    char isLastClosed;
    char isPicked;
    char registered;    /* 0x19E */
    char unk19F;
    VarString openExpr; /* 0x1A0 */
    VarString closeExpr; /* 0x1F0 */
    VarString lockExpr; /* 0x240 */
    char opening;       /* 0x290 */
    char unk291[3];
    float pickTime;     /* 0x294 */
    int openTicks;      /* 0x298 */
    Matrix lastMatrix;  /* 0x29C */
    Vec3d lastPos;      /* 0x2C8 */
    Task *moveSound;    /* 0x2E0 */
    int unk2E4;
    Vec3d offset;       /* 0x2E8, place relative to the parent object */
    Matrix local;       /* 0x300, base relative to the parent object */
} Door;

_Static_assert(offsetof(Switch, models) == 0x170, "Switch");
_Static_assert(offsetof(Switch, on) == 0x1D0, "Switch");
_Static_assert(sizeof(Switch) == 0x228, "Switch");
_Static_assert(offsetof(Door, model) == 0x148, "Door");
_Static_assert(offsetof(Door, openExpr) == 0x1A0, "Door");
_Static_assert(offsetof(Door, lastPos) == 0x2C8, "Door");
_Static_assert(sizeof(Door) == 0x328, "Door");

extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* copies 4 bytes without going through the FPU, like the original */
#define MOV32(dst, src) memcpy(&(dst), &(src), 4)

/* x87 equality: unordered counts as equal */
#define FEQ(a, b) (!((a) < (b) || (a) > (b)))

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

ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x416AD0, char, UseQuery_Consider, (UseQuery *q, void *obj, const Vec3d *pos));

extern double kUseWidth;

/* offers o to the human when it stands in front of model point 0 */
static inline void check_use(RigidDynCubeObj *o, UseQuery *q)
{
    const float *m = o->matrix.m;
    long double x, a, b;
    double dx, dy, dz;
    Vec3d p;

    Model_GetPoint(&p, o, 0);
    x = p.x + o->pos.x;
    p.x = x;
    p.y = p.y + o->pos.y;
    p.z = p.z + o->pos.z;
    dx = q->pos.x - x;
    dy = q->pos.y - p.y;
    dz = q->pos.z - p.z;
    a = m[6] * dz + m[3] * dy + m[0] * dx;
    b = m[7] * dz + m[4] * dy + m[1] * dx;
    if (!(b > kZeroD))
        return;
    if (!(a >= kZeroD))
        a = -a;
    if (!(a >= kUseWidth))
        UseQuery_Consider(q, o, &p);
}

/* the largest of the three scale factors, compared like the original */
static inline float max_scale(const RigidDynCubeObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

/* the order the three products of a row are added in */
enum { SUM01_2, SUM02_1, SUM12_0 };
#define ORDER(r0, r1, r2) ((r0) | (r1) << 2 | (r2) << 4)

/* out = a * b, row i summed in the order (ord >> 2i) & 3 */
static inline void mat_mul(Matrix *out, const Matrix *a, const Matrix *b, int ord)
{
    Matrix n;
    int i, j;

    for (i = 0; i < 3; i++) {
        int o = ord >> (2 * i) & 3;

        for (j = 0; j < 3; j++) {
            long double p0 = (long double)a->m[i * 3] * b->m[j];
            long double p1 = (long double)a->m[i * 3 + 1] * b->m[3 + j];
            long double p2 = (long double)a->m[i * 3 + 2] * b->m[6 + j];

            n.m[i * 3 + j] = o == SUM01_2 ? (p0 + p1) + p2 : o == SUM02_1 ? (p0 + p2) + p1 : (p1 + p2) + p0;
        }
    }
    n.count = a->count + b->count + 1;
    *out = n;
}

/* place = m * offset + pos, like the move handlers */
static inline void place_from(Vec3d *place, const Matrix *m, const Vec3d *offset, const Vec3d *pos)
{
    Vec3d p;

    p.x = m->m[2] * offset->z + m->m[1] * offset->y + m->m[0] * offset->x;
    p.y = m->m[5] * offset->z + m->m[4] * offset->y + m->m[3] * offset->x;
    p.z = m->m[8] * offset->z + m->m[7] * offset->y + m->m[6] * offset->x;
    *place = p;
    place->x = pos->x + place->x;
    place->y = pos->y + place->y;
    place->z = pos->z + place->z;
}

/* offset and local are place and m seen from the parent object */
static inline void to_local(Vec3d *offset, Matrix *local, const Vec3d *place, const Matrix *m, const RigidDynCubeObj *parent)
{
    const float *t = parent->matrix.m;
    long double z;
    Vec3d r;

    offset->x = place->x - parent->pos.x;
    offset->y = place->y - parent->pos.y;
    z = (long double)place->z - parent->pos.z;
    offset->z = z;
    r.x = t[0] * offset->x + t[3] * offset->y + t[6] * z;
    r.y = t[1] * offset->x + t[7] * z + t[4] * offset->y;
    r.z = t[2] * offset->x + t[8] * z + t[5] * offset->y;
    *offset = r;
    local->m[0] = t[0];
    local->m[1] = t[3];
    local->m[2] = t[6];
    local->m[3] = t[1];
    local->m[4] = t[4];
    local->m[5] = t[7];
    local->m[6] = t[2];
    local->m[7] = t[5];
    local->m[8] = t[8];
    local->count = parent->matrix.count;
    mat_mul(local, m, local, ORDER(SUM01_2, SUM02_1, SUM12_0));
}

/* offset 0 and local identity, for objects without a parent */
static inline void clear_local(Vec3d *offset, Matrix *local)
{
    static const Matrix id = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};

    memset(offset, 0, sizeof(*offset));
    *local = id;
}

#endif
