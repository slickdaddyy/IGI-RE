/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "props/props.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x414ED0, void, sub_414ED0, (Vec3f *out, void *obj, int i));
ORIG(0x4167D0, uint8_t, Game_GetEvent0, (void));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x416870, uint8_t, Game_GetEvent12, (void));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x4168B0, void *, Game_GetTaskList1, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x44C830, Task *, sub_44C830, (void *parent));
ORIG(0x44C860, void, sub_44C860, (Task *t, const Vec3d *pos, const Matrix *m, const Vec3d *size, const char *sound));
ORIG(0x4871C0, void, sub_4871C0, (Task *human, void *sprite));
ORIG(0x4B2B30, long double, Vec3_Angle, (const Vec3f *a, const Vec3f *b));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *room, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *room, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4F0F20, void *, sub_4F0F20, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

/* "Fence" (0x178 bytes): a fence that shocks humans while a script
   expression is true, with a buzzing spark task on top */
typedef struct Fence {
    RigidObj obj;
    Vec3d pos;          /* 0xF0 */
    float angle;        /* 0x108, about z */
    char model[0x10];   /* 0x10C */
    char isOn;          /* 0x11C */
    char unk11D[3];
    VarString cond;     /* 0x120 */
    Task *spark;        /* 0x170 */
    char unk174[4];
} Fence;

_Static_assert(offsetof(Fence, cond) == 0x120, "cond");
_Static_assert(sizeof(Fence) == 0x178, "Fence");

/* Game event 0, sent when a human touches something */
typedef struct TouchMsg {
    Task *human;
    int unk4;
    Vec3d pos;
} TouchMsg;

#define gFenceType VAR(uint16_t, 0x57C154)
#define gFenceParams VAR(void *, 0x57C150)

#define sub_4EF8B0 ((void *)0x4EF8B0)

#define kHalf VAR(float, 0x533504)
#define kSparkUp VAR(double, 0x533708)    /* 8192 */
#define kSparkScale VAR(float, 0x533840)  /* 1/8192 */
#define kSparkBase VAR(double, 0x533838)  /* 1.5 */
#define kShockSide VAR(double, 0x533850)  /* 1228.8 */
#define kShockHeight VAR(double, 0x533848) /* 12288 */
#define kUseAngle VAR(float, 0x533858)    /* pi / 3 */

void Fence_Create(Fence *f);
void Fence_Run(Fence *f);
void Fence_Delete(Fence *f);
void Fence_GetParams(void *task, void **out);
void Fence_ParamsChanged(Fence *f);
void Fence_Touch(Fence *f, TouchMsg *msg);
void Fence_Use(Fence *f, UseQuery *q);
void Fence_DrawIcon(Fence *f, Task **human);

void Fence_Open(void)
{
    Fence f;

    gFenceType = TaskType_New("Fence", sizeof(Fence), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gFenceType, 0, Fence_Create);
    TaskType_SetHandler2(gFenceType, 1, Fence_Run);
    TaskType_SetHandler2(gFenceType, 2, Fence_Delete);
    TaskType_SetHandler(gFenceType, Param_GetTableEvent(), Fence_GetParams);
    TaskType_SetHandler(gFenceType, Param_GetChangedEvent(), Fence_ParamsChanged);
    TaskType_SetHandler(gFenceType, Game_GetEvent0(), Fence_Touch);
    TaskType_SetHandler(gFenceType, Game_GetEvent2(), Fence_Use);
    TaskType_SetHandler(gFenceType, Game_GetEvent3(), sub_4EF8B0);
    TaskType_SetHandler(gFenceType, Game_GetEvent4(), TASK_NOTHING);
    TaskType_SetHandler(gFenceType, Game_GetEvent6(), Fence_DrawIcon);
    gFenceParams = ParamTable_New(4);
    ParamTable_Add(gFenceParams, &f, &f.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gFenceParams, &f, &f.angle, sub_4F0F20(), 7, NULL);
    ParamTable_Add(gFenceParams, &f, f.model, Param_String16(), 7, NULL);
    ParamTable_Add(gFenceParams, &f, &f.cond, Param_VarString(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_FENCE", gFenceType);
}
HOOK(0x43F900, Fence_Open);

void Fence_Create(Fence *f)
{
    Vec3d zero = {0, 0, 0};
    float scale, radius;

    QTaskList_Add(Game_GetTaskList0(), f);
    QTaskList_Add(Game_GetTaskList1(), f);
    RigidDynCubeObj_Init(Game_GetWorld(), f, NULL);
    scale = max_scale(&f->obj);
    radius = VirModel_GetRadius(f->obj.model) * scale;
    DynCube_Add(Game_GetWorld(), f, &zero, radius, 1);
    memset(&f->pos, 0, sizeof(f->pos));
    memset(&f->angle, 0, sizeof(f->angle));
    f->model[0] = 0;
    f->isOn = 0;
    VarString_Init(&f->cond);
    f->spark = NULL;
}
HOOK(0x43FAE0, Fence_Create);

void Fence_Run(Fence *f)
{
    if (VarString_HasExpr(&f->cond))
        f->isOn = (char)round_ll(VarString_EvalReal(&f->cond, f));
    else
        f->isOn = 0;
    if (!f->spark) {
        if (f->isOn) {
            const float *m = f->obj.matrix.m;
            Vec3f a, b;
            Vec3d p, size;
            long double c;
            TaskFn fn;

            f->spark = sub_44C830(f);
            sub_414ED0(&a, f, 0);
            sub_414ED0(&b, f, 1);
            c = (b.x + a.x) * kHalf;
            p.x = m[0] * c + m[2] * kSparkUp;
            p.y = m[3] * c + m[5] * kSparkUp;
            p.z = m[6] * c + m[8] * kSparkUp;
            p.x = p.x + f->obj.pos.x;
            p.y = p.y + f->obj.pos.y;
            p.z = p.z + f->obj.pos.z;
            size.x = (b.x - a.x) * kSparkScale + kSparkBase;
            size.y = 1.75;
            size.z = 2.0;
            sub_44C860(f->spark, &p, &f->obj.matrix, &size, "elfence_loop");
            fn = HANDLER(Param_GetChangedEvent(), f->spark->type);
            if (fn)
                fn(f->spark, NULL);
        }
    } else if (!f->isOn) {
        Task_Delete(f->spark);
        f->spark = NULL;
    }
    BROADCAST(&f->obj.task, 1);
}
HOOK(0x43FC30, Fence_Run);

void Fence_Delete(Fence *f)
{
    QTaskList_Remove(Game_GetTaskList0(), f);
    QTaskList_Remove(Game_GetTaskList1(), f);
    DynCube_Remove(Game_GetWorld(), f);
    VarString_Free(&f->cond);
}
HOOK(0x43FE50, Fence_Delete);

void Fence_GetParams(void *task, void **out)
{
    *out = gFenceParams;
}
HOOK(0x43FE90, Fence_GetParams);

void Fence_ParamsChanged(Fence *f)
{
    void **model = VirModel_Find(f->model);
    float scale, radius;

    if (model)
        RigidDynCubeObj_SetModel(f, model);
    scale = max_scale(&f->obj);
    radius = VirModel_GetRadius(f->obj.model) * scale;
    DynCube_Move(Game_GetWorld(), f, &f->pos, radius);
    Matrix_SetRotZ(&f->obj.matrix, f->angle);
    VarString_Bind(&f->cond, f);
}
HOOK(0x43FEA0, Fence_ParamsChanged);

/* Game event 0: a human touching a live fence gets event 12 */
void Fence_Touch(Fence *f, TouchMsg *msg)
{
    const float *m = f->obj.matrix.m;
    Task *human = msg->human;
    long double dx, dy, dz, y;
    double lx, ly, lz;
    Vec3f a, b;
    TaskFn fn;

    if (!f->isOn)
        return;
    dx = msg->pos.x - f->obj.pos.x;
    dy = msg->pos.y - f->obj.pos.y;
    dz = msg->pos.z - f->obj.pos.z;
    lx = (m[6] * dz + m[3] * dy) + m[0] * dx;
    ly = (m[7] * dz + m[4] * dy) + m[1] * dx;
    lz = (m[8] * dz + m[5] * dy) + m[2] * dx;
    sub_414ED0(&a, f, 0);
    sub_414ED0(&b, f, 1);
    if (a.x - kShockSide >= lx)
        return;
    if (!(b.x + kShockSide > lx))
        return;
    y = ly;
    if (!(ly >= kZeroD))
        y = -y;
    if (y >= kShockSide || lz >= kShockHeight || !(lz > kZeroD))
        return;
    fn = HANDLER(Game_GetEvent12(), human->type);
    if (fn)
        fn(human, NULL);
}
HOOK(0x43FF70, Fence_Touch);

/* Game event 2: a dead fence can be climbed from either side */
void Fence_Use(Fence *f, UseQuery *q)
{
    const float *m = f->obj.matrix.m;
    long double dx, dy, dz, y;
    double lx, ly;
    Vec3f a, b, u, n;

    if (f->isOn)
        return;
    dx = q->pos.x - f->obj.pos.x;
    dy = q->pos.y - f->obj.pos.y;
    dz = q->pos.z - f->obj.pos.z;
    lx = (m[6] * dz + m[3] * dy) + m[0] * dx;
    ly = (m[4] * dy + m[7] * dz) + m[1] * dx;
    sub_414ED0(&a, f, 0);
    sub_414ED0(&b, f, 1);
    y = ly;
    if (!(y >= kZeroD))
        y = -y;
    if (y >= q->range || a.x >= lx || !(b.x > lx))
        return;
    u.x = q->matrix.m[1];
    u.y = q->matrix.m[4];
    u.z = q->matrix.m[7];
    n.x = m[1];
    n.y = m[4];
    n.z = m[7];
    if (ly > kZeroD) {
        n.x = -n.x;
        n.y = -n.y;
        n.z = -n.z;
    }
    if (!(Vec3_Angle(&u, &n) >= kUseAngle)) {
        q->best_pitch = 0;
        q->best_yaw = 0;
        q->best = f;
    }
}
HOOK(0x4400C0, Fence_Use);

void Fence_DrawIcon(Fence *f, Task **human)
{
    sub_4871C0(*human, (char *)*human + 0x47C);
}
HOOK(0x440230, Fence_DrawIcon);

void Fence_Close(void)
{
    TaskType_Free(gFenceType);
    QTaskList_Free(gFenceParams);
}
HOOK(0x440250, Fence_Close);
