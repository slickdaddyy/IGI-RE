#ifndef BINOCULAR_BINOCULAR_H
#define BINOCULAR_BINOCULAR_H

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

typedef void (*TaskFn)(void *task, void *arg);

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24;
} Matrix;

/* a script string parameter that may hold an expression */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

/* "RigidDynCubeObj" (0xF0 bytes), a moving object with a model */
typedef struct RigidObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    void *world;
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0xF0 - 0xA4];
} RigidObj;

/* argument of the create handler of vehicle parts */
typedef struct PartCreateArg {
    const char *model;
    void *world;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    RigidObj *owner;
    void *def;
} PartCreateArg;

/* sent by a vehicle each frame to move its parts along */
typedef struct PartMoveArg {
    Vec3d *pos;
    Matrix *matrix;
} PartMoveArg;

/* argument of the script argument parsers */
typedef struct ScriptArg {
    void *out;
    void *args;
    int index;
} ScriptArg;

/* argument of the weapon config handlers */
typedef struct ConfigArg {
    void *config;
    char *weaponType;
} ConfigArg;

/* node of a list of task references */
typedef struct RefNode {
    struct RefNode *next;
    struct RefNode *prev;
    Task **ref;
    char unkC[0x1D];
    char active;
} RefNode;

/* what a weapon gets through the Human event 0 slot */
typedef struct WeaponUseArg {
    char fire;
    char unk1;
    char pad2[2];
    Vec3d *pos;
    Matrix *matrix;
    int unkC;
    int unk10;
    int trigger;
    int unk18;
    int unk1C;
    int unk20;
    int unk24;
    float unk28;
    char *human;
} WeaponUseArg;

/* the view the frame is drawn with (0xBCAAE0) */
typedef struct View {
    float m[9];
    int unk24;
    Vec3d pos;
    float unk40;
    float unk44;
    float scaleX;
    float scaleY;
    float centerX;
    float centerY;
} View;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(sizeof(RigidObj) == 0xF0, "RigidObj");
_Static_assert(offsetof(PartCreateArg, owner) == 0x88, "PartCreateArg");
_Static_assert(offsetof(RefNode, active) == 0x29, "RefNode");
_Static_assert(sizeof(WeaponUseArg) == 0x30, "WeaponUseArg");
_Static_assert(offsetof(View, centerY) == 0x54, "View");

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define gView VAR(View, 0xBCAAE0)

#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

#define TASK_NOTHING ((void *)0x4D3710)

/* Calls fn(child, arg) for each child of t (not deeper), keeping the next
   child on gTaskStack so handlers may delete tasks. slot is evaluated once
   per child, after the push, like the original. */
#define EACH_CHILD(t, slot, arg)                                        \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
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

/* the same without the check for a missing handler */
#define EACH_CHILD_ANY(t, slot, arg)                                    \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            HANDLER(slot, c_->type)(c_, arg);                           \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* the run slot, whose handlers take no argument */
#define RUN_CHILDREN(t)                                                 \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
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

static inline long double x87_tan(long double x)
{
    __asm__("fptan\n\tfstp %%st(0)" : "+t"(x));
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
    out->unk24 = a->unk24 + b->unk24 + 1;
}

/* the same where column 0 of b is still unrounded in col0 */
static inline void mat_mul_col0(Matrix *out, const Matrix *a, const Matrix *b, const long double *col0, int last0,
                                int last1, int last2)
{
    int i, j;

    for (i = 0; i < 3; i++) {
        int last = i == 0 ? last0 : i == 1 ? last1 : last2;
        int k0 = last == 0 ? 1 : 0;
        int k1 = last == 2 ? 1 : 2;

        for (j = 0; j < 3; j++) {
            long double b0 = j ? b->m[k0 * 3 + j] : col0[k0];
            long double b1 = j ? b->m[k1 * 3 + j] : col0[k1];
            long double b2 = j ? b->m[last * 3 + j] : col0[last];

            out->m[i * 3 + j] = (float)(a->m[i * 3 + k0] * b0 + a->m[i * 3 + k1] * b1 + a->m[i * 3 + last] * b2);
        }
    }
    out->unk24 = a->unk24 + b->unk24 + 1;
}

/* column 0 of a * b without rounding, summed like mat_mul */
static inline void mat_mul_col0_ext(long double *col0, const Matrix *a, const Matrix *b, int last0, int last1,
                                    int last2)
{
    int i;

    for (i = 0; i < 3; i++) {
        int last = i == 0 ? last0 : i == 1 ? last1 : last2;
        int k0 = last == 0 ? 1 : 0;
        int k1 = last == 2 ? 1 : 2;

        col0[i] = (long double)a->m[i * 3 + k0] * b->m[k0 * 3] + (long double)a->m[i * 3 + k1] * b->m[k1 * 3]
                  + (long double)a->m[i * 3 + last] * b->m[last * 3];
    }
}

#endif
