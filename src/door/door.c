/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "door/door.h"
#include "coll/colltypes.h"

FN_EXTERN(Param_Three);
FN_EXTERN(Task_Nothing);
FN_EXTERN(Terminal_MapHandler);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CC0, void *, Task_FindAncestor, (void *task, uint16_t type));
ORIG(0x414ED0, void, Model_GetLocalPoint, (Vec3f *out, void *obj, int i));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x450CA0, uint8_t, HumanAI_GetDoorSlot, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x4871C0, void, StatusScreen_DrawActionIcon, (char *p, char *q));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B3790, void, Matrix_RotateZ, (Matrix *out, const Matrix *m, float a));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *results, Vec3d *starts, Vec3d *ends, int count, int unk,
                                  void *q, unsigned flags));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *obj));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
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
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x5096A0, uint8_t, AIGraph_GetEvent, (void));

/* asks for the door nearest to pos */
typedef struct NearQuery {
    Vec3d pos;
    double best;        /* 0x18, squared, below 0 for none yet */
    Door *found;        /* 0x20 */
} NearQuery;

/* sent to the children of unkE8 with the MagicObj move event */
typedef struct DoorMoveMsg {
    Vec3d *pos;
    Matrix *matrix;
    Door *door;
} DoorMoveMsg;

extern uint16_t gDoorType;
extern RangeF gDoorSlideRange;
extern RangeF gDoorTimeRange;
extern RangeF gDoorAngleRange;
extern RangeF gDoorUnitRange;
extern void *gDoorParams;

extern float kSlideScale;
extern float kDegToRad;
extern double kUseMin;
extern double kUseMaxX;
extern double kUseMaxY;

#define Terminal_MapHandler (FN(0x443DC0, Terminal_MapHandler))
#define sub_4EFB00 (FN(0x4EFB00, Param_Three))
#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

#define FIELD(name) ((void *)offsetof(Door, name))

void Door_CreateHandler(Door *d);
void Door_RunHandler(Door *d);
void Door_DeleteHandler(Door *d);
void Door_GetParams(void *task, void **out);
void Door_ParamsChanged(Door *d, char *arg);
void Door_CheckUse(Door *d, UseQuery *q);
void Door_DrawIcon(Door *d, char **arg);
void Door_AddSymbols(Door *d);
void Door_RemoveSymbols(Door *d);
void Door_UpdateLocal(Door *d);
char Door_IsLocked(Door *d);
char Door_IsPickable(Door *d);

/* 1 / length, the way the original adds up the squares */
static inline long double inv_len(long double sum)
{
    return kOneD / __builtin_sqrtl(sum);
}

/* puts the door at its place, slid along slide by slidePos */
void Door_Place(Door *d)
{
    const float *m = d->base.m;
    long double u = d->slide[0] * d->slidePos * kSlideScale;
    long double v = d->slide[1] * d->slidePos * kSlideScale;
    double x = m[1] * v + m[0] * u;
    double y = m[4] * v + m[3] * u;
    long double z = m[7] * v + m[6] * u;
    float scale, radius;
    Vec3d p;

    p.x = x + d->place.x;
    p.y = y + d->place.y;
    p.z = z + d->place.z;
    scale = max_scale(&d->o);
    radius = VirModel_GetRadius(d->o.model) * scale;
    DynCube_Move(Game_GetWorld(), d, &p, radius);
}
HOOK(0x446290, Door_Place);

void Door_GetMoveMsg(MoveArg *msg, RigidDynCubeObj *o)
{
    msg->pos = &o->pos;
    msg->matrix = &o->matrix;
}
HOOK(0x446450, Door_GetMoveMsg);

void Door_MoveChildren(Door *d)
{
    MoveArg msg;

    Door_GetMoveMsg(&msg, &d->o);
    BROADCAST(&d->o.task, RigidDynCubeObj_GetEvent4(), &msg);
}
HOOK(0x4463B0, Door_MoveChildren);

/* RigidDynCubeObj event 4: moves along with the object it is fixed to,
   then straightens the closed matrix again */
void Door_MoveHandler(Door *d, MoveArg *msg)
{
    const float *m = d->base.m;
    float x, y, z, a, b, c, e, f, g;
    long double k;

    place_from(&d->place, msg->matrix, &d->offset, msg->pos);
    mat_mul(&d->base, msg->matrix, &d->local, ORDER(SUM12_0, SUM12_0, SUM12_0));
    Door_Place(d);
    x = m[0];
    y = m[1];
    z = m[2];
    if (!(FEQ(x, kZero) && FEQ(y, kZero) && FEQ(z, kZero))) {
        k = inv_len(x * x + y * y + z * z);
        x = x * k;
        y = y * k;
        z = z * k;
    }
    a = m[2] * m[7] - m[1] * m[8];
    b = -(m[2] * m[6] - m[0] * m[8]);
    c = m[1] * m[6] - m[0] * m[7];
    e = c * y - b * z;
    f = -(c * x - a * z);
    g = b * x - a * y;
    if (!(FEQ(a, kZero) && FEQ(b, kZero) && FEQ(c, kZero))) {
        k = inv_len(a * a + c * c + b * b);
        a = a * k;
        b = b * k;
        c = c * k;
    }
    if (!(FEQ(e, kZero) && FEQ(f, kZero) && FEQ(g, kZero))) {
        k = inv_len(e * e + f * f + g * g);
        e = e * k;
        f = f * k;
        g = g * k;
    }
    d->base.m[0] = x;
    d->base.m[1] = y;
    d->base.m[2] = z;
    d->base.m[3] = a;
    d->base.m[4] = b;
    d->base.m[5] = c;
    d->base.m[6] = e;
    d->base.m[7] = f;
    d->base.m[8] = g;
    d->base.count = 0;
    d->o.matrix = d->base;
    Door_MoveChildren(d);
}
HOOK(0x445DE0, Door_MoveHandler);

void Door_FindNearest(Door *d, NearQuery *q)
{
    long double dist = Vec3d_DistanceSq(&q->pos, &d->o.pos);

    if (!(q->best >= kZeroD) || !(dist >= q->best)) {
        q->best = dist;
        q->found = d;
    }
}
HOOK(0x446470, Door_FindNearest);

void sub_4464B0(void *task, char *arg)
{
    arg[0] = arg[8] == 0;
}
HOOK(0x4464B0, sub_4464B0);

void Door_Open(void)
{
    int i;

    gDoorType = TaskType_New("Door", sizeof(Door), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gDoorType, 0, Door_CreateHandler);
    TaskType_SetHandler2(gDoorType, 1, Door_RunHandler);
    TaskType_SetHandler2(gDoorType, 2, Door_DeleteHandler);
    TaskType_SetHandler(gDoorType, Param_GetTableEvent(), Door_GetParams);
    TaskType_SetHandler(gDoorType, Param_GetChangedEvent(), Door_ParamsChanged);
    TaskType_SetHandler(gDoorType, Game_GetEvent2(), Door_CheckUse);
    TaskType_SetHandler(gDoorType, Game_GetEvent3(), sub_4EFB00);
    TaskType_SetHandler(gDoorType, Game_GetEvent4(), TASK_NOTHING);
    TaskType_SetHandler(gDoorType, Game_GetEvent6(), Door_DrawIcon);
    TaskType_SetHandler2(gDoorType, Level_GetEventDA8(), Door_AddSymbols);
    TaskType_SetHandler2(gDoorType, Level_GetEventDAC(), Door_RemoveSymbols);
    TaskType_SetHandler(gDoorType, RigidDynCubeObj_GetEvent4(), Door_MoveHandler);
    TaskType_SetHandler(gDoorType, HumanAI_GetDoorSlot(), Door_FindNearest);
    TaskType_SetHandler(gDoorType, AIGraph_GetEvent(), sub_4464B0);
    TaskType_SetHandler(gDoorType, ComputerObject_GetEvent0(), Terminal_MapHandler);
    gDoorParams = ParamTable_New(16);
    gDoorSlideRange.min = -10.0f;
    gDoorSlideRange.max = 10.0f;
    gDoorUnitRange.min = 0.0f;
    gDoorUnitRange.max = 1.0f;
    gDoorAngleRange.min = -360.0f;
    gDoorAngleRange.max = 360.0f;
    gDoorTimeRange.min = 0.05f;
    gDoorTimeRange.max = 100.0f;
    ParamTable_Add(gDoorParams, NULL, FIELD(place), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gDoorParams, NULL, FIELD(slide[0]), Param_Real32(), 7, &gDoorSlideRange);
    ParamTable_Add(gDoorParams, NULL, FIELD(slide[1]), Param_Real32(), 7, &gDoorSlideRange);
    ParamTable_Add(gDoorParams, NULL, FIELD(slidePos), Param_Real32(), 7, &gDoorUnitRange);
    ParamTable_Add(gDoorParams, NULL, FIELD(base), Param_Real32x9(), 7, NULL);
    ParamTable_Add(gDoorParams, NULL, FIELD(model), Param_String16(), 7, NULL);
    ParamTable_Add(gDoorParams, NULL, FIELD(openAngle), Param_Real32(), 7, &gDoorAngleRange);
    ParamTable_Add(gDoorParams, NULL, FIELD(openTime), Param_Real32(), 7, &gDoorTimeRange);
    ParamTable_Add(gDoorParams, NULL, FIELD(pickable), Param_Bool8(), 7, NULL);
    ParamTable_Add(gDoorParams, NULL, FIELD(pickTime), Param_Real32(), 7, NULL);
    ParamTable_Add(gDoorParams, NULL, FIELD(lockExpr), Param_VarString(), 7, NULL);
    ParamTable_Add(gDoorParams, NULL, FIELD(openExpr), Param_VarString(), 7, NULL);
    ParamTable_Add(gDoorParams, NULL, FIELD(closeExpr), Param_VarString(), 7, NULL);
    for (i = 0; i < 3; i++)
        ParamTable_Add(gDoorParams, NULL, FIELD(sounds[i]), Param_String16(), 7, NULL);
}
HOOK(0x4464C0, Door_Open);

void Door_CreateHandler(Door *d)
{
    static const Matrix id = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};
    float scale, radius;
    int i;

    QTaskList_Add(Game_GetTaskList0(), d);
    RigidDynCubeObj_Init(Game_GetWorld(), d, NULL);
    memset(&d->place, 0, sizeof(d->place));
    scale = max_scale(&d->o);
    radius = VirModel_GetRadius(d->o.model) * scale;
    DynCube_Add(Game_GetWorld(), d, &d->place, radius, 2);
    d->o.matrix = id;
    d->pickable = 0;
    d->openAngle = 90.0f;
    d->openTime = 0.5f;
    d->model[0] = 0;
    d->base = d->o.matrix;
    d->angleTarget = 0;
    d->angle = 0;
    d->slide[0] = 0;
    d->slide[1] = 0;
    d->isLocked = 0;
    d->isPicked = 0;
    d->registered = 0;
    VarString_Init(&d->openExpr);
    VarString_Init(&d->closeExpr);
    VarString_Init(&d->lockExpr);
    d->opening = 0;
    d->pickTime = 4.0f;
    d->openTicks = 0;
    d->slidePos = 0;
    d->slideLen = 0;
    for (i = 0; i < 3; i++)
        d->sounds[i][0] = 0;
    d->moveSound = NULL;
}
HOOK(0x4468F0, Door_CreateHandler);

void Door_DeleteHandler(Door *d)
{
    QTaskList_Remove(Game_GetTaskList0(), d);
    if (d->o.attached)
        Task_Delete(d->o.attached);
    if (Task_GetId(d) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), d->o.task.type);

        if (fn)
            fn(d);
    }
    VarString_Free(&d->openExpr);
    VarString_Free(&d->closeExpr);
    VarString_Free(&d->lockExpr);
    DynCube_Remove(d->o.world, d);
}
HOOK(0x446AC0, Door_DeleteHandler);

/* offers the door from both sides; locked doors only to be picked */
void Door_CheckUse(Door *d, UseQuery *q)
{
    const float *m = d->o.matrix.m;
    long double x, dx, dy, dz, x2;
    double a, b, y2, z2;
    Vec3d p;

    if (Door_IsLocked(d) && (!Door_IsLocked(d) || !Door_IsPickable(d)))
        return;
    Model_GetPoint(&p, d, 0);
    x = p.x + d->o.pos.x;
    p.x = x;
    p.y = p.y + d->o.pos.y;
    p.z = p.z + d->o.pos.z;
    dx = q->pos.x - x;
    dy = q->pos.y - p.y;
    dz = q->pos.z - p.z;
    a = m[3] * dy + m[6] * dz + m[0] * dx;
    b = m[5] * dy + m[8] * dz + m[2] * dx;
    if (!(a > kUseMin) || !(a < kUseMaxX) || !(b > kUseMin) || !(b < kUseMaxY))
        return;
    UseQuery_Consider(q, d, &p);
    x2 = m[2] * b + m[0] * a;
    y2 = m[5] * b + m[3] * a;
    z2 = m[8] * b + m[6] * a;
    p.x = x2 + p.x;
    p.y = y2 + p.y;
    p.z = z2 + p.z;
    UseQuery_Consider(q, d, &p);
}
HOOK(0x446B50, Door_CheckUse);

void Door_DrawIcon(Door *d, char **arg)
{
    char *h = *arg;

    StatusScreen_DrawActionIcon(h, h + (d->isLocked ? 0x41C : 0x464));
}
HOOK(0x446CE0, Door_DrawIcon);

/* a model point of the door where it was last frame */
static void last_point(Door *d, Vec3d *out, int i)
{
    const float *l = d->lastMatrix.m;
    Vec3f p;
    float x, y, z;

    Model_GetLocalPoint(&p, d, i);
    x = p.z * l[2] + p.x * l[0] + p.y * l[1];
    y = p.z * l[5] + p.x * l[3] + p.y * l[4];
    z = p.z * l[8] + p.x * l[6] + p.y * l[7];
    out->x = x + d->lastPos.x;
    out->y = y + d->lastPos.y;
    out->z = z + d->lastPos.z;
}

void Door_RunHandler(Door *d)
{
    int moved = 0;
    char closed;
    long double v;

    BROADCAST(&d->o.task, 1);
    closed = d->isClosed;
    d->isLastOpen = d->isOpen;
    d->isLastClosed = closed;
    d->lastPos = d->o.pos;
    d->lastMatrix = d->o.matrix;
    if (closed && VarString_HasExpr(&d->openExpr)) {
        v = VarString_EvalReal(&d->openExpr, d);
        if (!FEQ(v, kZeroD))
            d->opening = 1;
    }
    if (d->isOpen) {
        d->openTicks++;
        if (VarString_HasExpr(&d->closeExpr)) {
            v = VarString_EvalReal(&d->closeExpr, d);
            if (!FEQ(v, kZeroD))
                d->opening = 0;
        }
    } else {
        d->openTicks = 0;
    }
    if (VarString_HasExpr(&d->lockExpr))
        d->isLocked = (char)round_ll(VarString_EvalReal(&d->lockExpr, d));
    if (d->isPicked)
        d->isLocked = 0;
    if (d->opening) {
        d->slideTarget = 1.0f;
        MOV32(d->angleTarget, d->openAngle);
    } else {
        d->angleTarget = 0;
        d->slideTarget = 0;
    }

    if (!FEQ(d->angleTarget, d->angle)) {
        if (d->angle < d->angleTarget) {
            v = d->openAngle;
            if (!(d->openAngle >= kZero))
                v = -v;
            v = v / (d->openTime * kFps) + d->angle;
            d->angle = v;
            if (v > d->angleTarget)
                MOV32(d->angle, d->angleTarget);
        } else if (d->angle > d->angleTarget) {
            v = d->openAngle;
            if (!(d->openAngle >= kZero))
                v = -v;
            v = d->angle - v / (d->openTime * kFps);
            d->angle = v;
            if (!(v >= d->angleTarget))
                MOV32(d->angle, d->angleTarget);
        }
        Matrix_RotateZ(&d->o.matrix, &d->base, d->angle * kDegToRad);
        moved = 1;
    }

    if ((FEQ(d->slide[0], kZero) && FEQ(d->slide[1], kZero)) || FEQ(d->slidePos, d->slideTarget)) {
        MOV32(d->slidePos, d->slideTarget);
    } else {
        if (d->slidePos < d->slideTarget) {
            v = d->slideLen / (d->openTime * kFps) / d->slideLen + d->slidePos;
            d->slidePos = v;
            if (v > d->slideTarget)
                MOV32(d->slidePos, d->slideTarget);
        } else if (d->slidePos > d->slideTarget) {
            v = d->slidePos - d->slideLen / (d->openTime * kFps) / d->slideLen;
            d->slidePos = v;
            if (!(v >= d->slideTarget))
                MOV32(d->slidePos, d->slideTarget);
        }
        Door_Place(d);
        moved = 1;
    }

    if (FEQ(d->angleTarget, d->angle) && FEQ(d->slideTarget, d->slidePos)) {
        char c = FEQ(d->angleTarget, kZero) && FEQ(d->slideTarget, kZero);

        d->isClosed = c;
        d->isOpen = !c;
    } else {
        d->isClosed = 0;
        d->isOpen = 0;
    }

    if (moved) {
        char *mesh = *(char **)(*(char **)((char *)*d->o.model + 0xC) + 0xC);
        short solid = *(short *)(mesh + 0x7C);

        Door_UpdateLocal(d);
        if (solid) {
            /* a closing door that would hit something opens again */
            Vec3d from, to;
            CollIgnore ignore;
            CollHit hit;

            last_point(d, &from, 1);
            last_point(d, &to, 2);
            ignore.task = (Task *)d;
            Coll_Lines(Game_GetWorld(), &hit, &from, &to, 1, 0xFF, &ignore, 1);
            if (hit.found && hit.task) {
                d->slideTarget = 1.0f;
                MOV32(d->angleTarget, d->openAngle);
                d->isClosed = 0;
                d->isOpen = 0;
                d->opening = 1;
            }
        }
        Door_MoveChildren(d);
        if (d->o.attached) {
            DoorMoveMsg msg;

            msg.pos = &d->o.pos;
            msg.matrix = &d->o.matrix;
            msg.door = d;
            BROADCAST(d->o.attached, MagicObj_GetMoveEvent(), &msg);
        }
    }

    if (d->isClosed && !d->isLastClosed) {
        Task *snd = Sound_Play(d, d->sounds[1]);

        if (snd)
            Sound_SetPos(snd, &d->o.pos);
    }
    if (d->isOpen && !d->isLastOpen) {
        Task *snd = Sound_Play(d, d->sounds[0]);

        if (snd)
            Sound_SetPos(snd, &d->o.pos);
    }
    if (!d->isClosed && !d->isOpen) {
        if (!d->moveSound) {
            Task *snd = Sound_Play(d, d->sounds[2]);

            d->moveSound = snd;
            if (snd)
                Sound_SetPos(snd, &d->o.pos);
        }
    } else if (d->moveSound) {
        Sound_Stop(d->moveSound);
        d->moveSound = NULL;
    }
    if (d->o.attached)
        HANDLER(1, d->o.attached->type)(d->o.attached);
}
HOOK(0x446D10, Door_RunHandler);

/* remembers where the door sits on the object it is fixed to */
void Door_UpdateLocal(Door *d)
{
    RigidDynCubeObj *p = Task_FindAncestor(d, RigidDynCubeObj_GetType());

    if (p)
        to_local(&d->offset, &d->local, &d->place, &d->base, p);
    else
        clear_local(&d->offset, &d->local);
}
HOOK(0x4474E0, Door_UpdateLocal);

void Door_ParamsChanged(Door *d, char *arg)
{
    char buf[128], name[128];
    void **model = VirModel_Find(d->model);
    long double len;
    Task *t;

    if (!model && d->model[1] != '*') {
        crt_sprintf(buf, "Model \"%s\" not available", d->model);
        strcpy(name, d->model);
        crt_sprintf(d->model, "** %s **", name);
    }
    RigidDynCubeObj_SetModel(d, model);
    Door_Place(d);
    d->o.matrix = d->base;
    if (arg && *arg) {
        d->lastPos = d->o.pos;
        d->lastMatrix = d->base;
    }
    VarString_Bind(&d->openExpr, d);
    VarString_Bind(&d->closeExpr, d);
    VarString_Bind(&d->lockExpr, d);
    d->isClosed = 1;
    d->isLastClosed = 1;
    d->isOpen = 0;
    d->opening = 0;
    d->isLastOpen = 0;
    len = __builtin_sqrtl(d->slide[0] * d->slide[0] + d->slide[1] * d->slide[1]);
    if (!(len >= kZero))
        len = -len;
    d->slideLen = len;
    Door_UpdateLocal(d);
    if (d->moveSound) {
        Sound_Stop(d->moveSound);
        d->moveSound = NULL;
    }
    if (d->o.attached)
        Task_Delete(d->o.attached);
    t = MagicObjContainer_New(NULL, d);
    d->o.attached = t;
    RigidDynCubeObj_CreateMagicObjs(t, d, d);
    BROADCAST(&d->o.task, Param_GetChangedEvent(), arg);
    d->lastPos = d->o.pos;
    d->lastMatrix = d->o.matrix;
}
HOOK(0x447830, Door_ParamsChanged);

void Door_GetParams(void *task, void **out)
{
    *out = gDoorParams;
}
HOOK(0x447A80, Door_GetParams);

void Door_AddSymbols(Door *d)
{
    char name[64], buf[128];

    if (d->registered)
        return;
    d->registered = 1;
    Task_GetName(name, d);
    crt_sprintf(buf, "%s.isLocked", name);
    Script_RegisterBool8(buf, &d->isLocked);
    crt_sprintf(buf, "%s.isOpen", name);
    Script_RegisterBool8(buf, &d->isOpen);
    crt_sprintf(buf, "%s.isClosed", name);
    Script_RegisterBool8(buf, &d->isClosed);
    crt_sprintf(buf, "%s.isLastClosed", name);
    Script_RegisterBool8(buf, &d->isLastClosed);
    crt_sprintf(buf, "%s.isLastOpen", name);
    Script_RegisterBool8(buf, &d->isLastOpen);
    crt_sprintf(buf, "%s.isPicked", name);
    Script_RegisterBool8(buf, &d->isPicked);
    crt_sprintf(buf, "%s.nDoorOpenTicks", name);
    Script_RegisterInt2(buf, &d->openTicks);
}
HOOK(0x447A90, Door_AddSymbols);

void Door_RemoveSymbols(Door *d)
{
    char name[64], buf[128];

    if (!d->registered)
        return;
    Task_GetName(name, d);
    d->registered = 0;
    crt_sprintf(buf, "%s.isLocked", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isOpen", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isClosed", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastClosed", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastOpen", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isPicked", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nDoorOpenTicks", name);
    Script_RemoveSymbol(buf);
    d->registered = 0;
}
HOOK(0x447BE0, Door_RemoveSymbols);

void Door_Close(void)
{
    QTaskList_Free(gDoorParams);
    TaskType_Free(gDoorType);
}
HOOK(0x447D00, Door_Close);

uint16_t Door_GetType(void)
{
    return gDoorType;
}
HOOK(0x447D20, Door_GetType);

float Door_GetPickTime(Door *d)
{
    return d->pickTime;
}
HOOK(0x447D30, Door_GetPickTime);

char Door_IsLocked(Door *d)
{
    return d->isLocked;
}
HOOK(0x447D40, Door_IsLocked);

char Door_IsPickable(Door *d)
{
    return d->pickable;
}
HOOK(0x447D50, Door_IsPickable);

void Door_SetPicked(Door *d, char picked)
{
    d->isPicked = picked;
}
HOOK(0x447D60, Door_SetPicked);

/* 0 open, 1 closed, 2 opening, 3 closing */
int Door_GetState(Door *d)
{
    if (d->isOpen)
        return 0;
    if (d->isClosed)
        return 1;
    return 3 - (d->opening != 0);
}
HOOK(0x447D70, Door_GetState);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533498) double kUseMaxY INIT(4096.0);
ALIAS(kUnit, kUseMaxY, 0);
DATA(5338A8) double kUseMaxX INIT(1843.2);
DATA(5338A8) double kUseMin INIT(-4096.0);
BSS(57C1C0) uint16_t gDoorType;
PAD(igb, 57C1C0, 57C1C2, 0x6);
BSS(57C1C0) RangeF gDoorSlideRange;
BSS(57C1C0) RangeF gDoorTimeRange;
BSS(57C1C0) RangeF gDoorAngleRange;
BSS(57C1C0) RangeF gDoorUnitRange;
BSS(57C1C0) void *gDoorParams;
