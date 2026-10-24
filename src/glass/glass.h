#ifndef GLASS_GLASS_H
#define GLASS_GLASS_H

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

typedef void (*TaskFn)();

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int unk24;
} Matrix;

typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

/* "RigidDynCubeObj" (0xF0 bytes), the fields used here */
typedef struct Obj {
    Task task;
    Vec3d pos;          /* 0x20 */
    char unk38[0x30];
    int world;          /* 0x68 */
    void **model;       /* 0x6C, VirModel */
    Matrix matrix;      /* 0x70 */
    float scale[3];     /* 0x98 */
    char unkA4[0x4C];
} Obj;

/* create argument of the RigidDynCubeObj kinds made by DefineMagicObj */
typedef struct MagicArg {
    const char *model;
    int world;
    Vec3d pos;          /* 0x08 */
    Vec3d offset;       /* 0x20, from the owner */
    Matrix matrix;      /* 0x38 */
    Matrix local;       /* 0x60 */
    void *owner;        /* 0x88 */
    void *data;         /* 0x8C, the DefineMagicObj data block */
} MagicArg;

/* what a magic object keeps from its create argument, at 0xF0 */
typedef struct MagicPart {
    char offset[0x18];  /* Vec3d, kept unaligned here */
    Matrix local;       /* 0x108 */
    void *owner;        /* 0x130 */
    char unk134[4];
    void *data;         /* 0x138 */
} MagicPart;

/* "ExplodeObject" (0x238 bytes), the fields used here */
typedef struct ExplodeObj {
    Obj o;
    char unkF0[0xA0];
    char exploded;      /* 0x190 */
    char unk191[0xA7];
} ExplodeObj;

/* {count, names} extra data of an enum parameter */
typedef struct ParamEnum {
    int count;
    char **names;
} ParamEnum;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(sizeof(Obj) == 0xF0, "Obj");
_Static_assert(offsetof(MagicArg, data) == 0x8C, "MagicArg");
_Static_assert(sizeof(MagicPart) == 0x4C, "MagicPart");
_Static_assert(sizeof(ExplodeObj) == 0x238, "ExplodeObj");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
extern Task *gTaskStack[];
extern int gTaskStackTop;
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* the frame counter lives at +0x68 */
extern char *gTickInfo;
#define gTick (*(int *)(gTickInfo + 0x68))

/* exe constants, read in place so the x87 code sees the same values */
extern float kZero;
extern float kOne;
extern float kFrame;     /* 1/30 */
extern float kDegToRad;

/* Calls the slot handler of each child of t (not deeper), keeping the next
   child on gTaskStack so handlers may delete tasks. slot is evaluated after
   the push, like the original. */
#define EACH_CHILD(t, slot, ...)                                        \
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
                fn_(c_, ##__VA_ARGS__);                                 \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

/* x87 "not equal" as tested with C3: false when unordered */
static inline int fne(long double a, long double b)
{
    return a < b || a > b;
}

/* the largest scale factor, compared like the original */
static inline float obj_scale(const Obj *o)
{
    float m = o->scale[2] < o->scale[1] ? o->scale[1] : o->scale[2];

    return o->scale[0] > m ? o->scale[0] : m;
}

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x4168D0, void *, Game_GetTaskList3, (void));
ORIG(0x416920, int, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x481340, uint16_t, ExplodeObject_GetType, (void));
ORIG(0x481350, void *, ExplodeObject_GetParams, (void));
ORIG(0x481360, void, ExplodeObject_Setup, (void *o, float a, float b, float c, float d, float delay, int f, int g,
                                          const char *sound));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *owner));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (int world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (int world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (int world, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4C7560, void, DynCube_Remove, (int world, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *table, int extra));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));

#endif
