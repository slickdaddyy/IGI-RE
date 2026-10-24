#ifndef VEHICLE_VEHICLE_H
#define VEHICLE_VEHICLE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

FN_EXTERN(Task_Nothing);

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

typedef struct Display {
    int unk0;
    int width;
    int height;
} Display;

/* "RigidDynCubeObj" (0xF0 bytes), a moving object with a model */
typedef struct RigidObj {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    int unk68;
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0xF0 - 0xA4];
} RigidObj;

/* the argument of the create handler of the vehicle parts */
typedef struct PartCreateArg {
    const char *model;
    int unk4;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    Task *owner;
    void *params;
} PartCreateArg;

/* sent by the vehicle each frame to move its parts along */
typedef struct PartMoveArg {
    Vec3d *pos;
    Matrix *matrix;
} PartMoveArg;

/* the argument of the script argument handlers */
typedef struct PartScriptArg {
    void *out;
    void *args;
    int index;
} PartScriptArg;

/* node of a list of task references (vehicle seats, targets) */
typedef struct RefNode {
    struct RefNode *next;
    struct RefNode *prev;
    Task **ref;
    char unkC[0x1D];
    char active;
} RefNode;

_Static_assert(sizeof(VarString) == 0x50, "VarString");
_Static_assert(sizeof(RigidObj) == 0xF0, "RigidObj");
_Static_assert(offsetof(PartCreateArg, owner) == 0x88, "PartCreateArg");
_Static_assert(offsetof(RefNode, active) == 0x29, "RefNode");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
extern Task *gTaskStack[];
extern int gTaskStackTop;

/* the handler a task type has for a slot */
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (int unk68, void *o, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *o, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (int unk, void *o, Vec3d *pos, float radius, int unk2));
ORIG(0x4C7560, void, DynCube_Remove, (int unk68, void *o));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));

/* calls the slot handler of each child of t (not deeper), keeping the next
   child on gTaskStack so handlers may delete tasks */
#define CALL_CHILDREN(t, slot)                                          \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] =                               \
                c_->next && c_->next->next ? c_->next : NULL;           \
            fn_ = gTaskHandlers[slot][c_->type];                        \
            if (fn_)                                                    \
                ((void (*)(void *))fn_)(c_);                            \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

/* the largest of the three scale factors */
static inline float max_scale(const RigidObj *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

#endif
