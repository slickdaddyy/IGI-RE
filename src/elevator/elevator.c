#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "elevator/elevator.h"

#define Elevator_DeleteHandler ((void *)0x43DCF0)
#define Elevator_GetParams ((void *)0x43DDA0)
#define Elevator_ParamsChanged ((void *)0x43DDB0)
#define Elevator_Activate ((void *)0x43DF20)
#define Elevator_Deactivate ((void *)0x43E0A0)
#define Elevator_MoveHandler ((void *)0x43E1F0)
#define Elevator_GetMotion ((void *)0x43E290)

ORIG(0x43D960, void, Elevator_UpdatePlace, (void *elevator));
ORIG(0x5020C0, long double, SplineObj_GetLength, (int id));
ORIG(0x502130, int, SplineObj_GetWaypointCount, (int id));
ORIG(0x502180, long double, SplineObj_GetLengthTo, (int id, int floor));

/* a platform moving between the floors of a graph task */
typedef struct Elevator {
    Obj obj;
    Vec3d place;
    Vec3d lastPos;
    Matrix placeMatrix;
    Matrix lastMatrix;
    char model[16];
    int graph;
    float height;
    float unk188;
    char unk18C;
    char unk18D;
    char pad18E[2];
    float speed;
    float maxSpeed;
    float accel;
    int wanted;
    double floorPos;
    double lastFloorPos;
    char request[10];
    char pad1BA[2];
    int restTicks;
    char unk1C0;
    char pad1C1[3];
    VarString wantedFloor[10];
    VarString up;
    VarString down;
    VarString move;
    char lastUp;
    char lastDown;
    char isMoving;
    char isStarting;
    char isStopping;
    char startSound[32];
    char stopSound[32];
    char moveSound[32];
    char pad639[3];
    Task *moving;
    char lastMove;
    char pad641[3];
    int lastTick;
} Elevator;

_Static_assert(offsetof(Elevator, graph) == 0x180, "Elevator");
_Static_assert(offsetof(Elevator, floorPos) == 0x1A0, "Elevator");
_Static_assert(offsetof(Elevator, wantedFloor) == 0x1C4, "Elevator");
_Static_assert(offsetof(Elevator, lastUp) == 0x5D4, "Elevator");
_Static_assert(offsetof(Elevator, moving) == 0x63C, "Elevator");
_Static_assert(sizeof(Elevator) == 0x648, "Elevator");

#define gElevatorType VAR(uint16_t, 0x57C108)
#define gElevatorParams VAR(void *, 0x57C0E0)
#define gElevatorFloorNames ((char (*)[32])0x57BF90)
#define gElevatorMaxSpeedRange VAR(RangeF, 0x57C0D0)
#define gElevatorAccelRange VAR(RangeF, 0x57C0D8)
#define gElevator188Range VAR(RangeF, 0x57C0E8)

static const float kZero = 0.0f;
static const float kNear = 16384.0f;
static const float kSlowdown = 0x1p-14f;
static const float kMinSpeed = 0x1.99999ap-4f;
static const float kStepUnits = 0x1.111112p+7f;
static const float kEpsilon = 0x1.0624dep-10f;

void Elevator_CreateHandler(Elevator *e);
void Elevator_RunHandler(Elevator *e);

void Elevator_Init(void)
{
    Elevator e;
    int i;

    gElevatorType = TaskType_New("Elevator", sizeof(Elevator), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gElevatorType, 0, Elevator_CreateHandler);
    TaskType_SetHandler2(gElevatorType, 1, Elevator_RunHandler);
    TaskType_SetHandler2(gElevatorType, 2, Elevator_DeleteHandler);
    TaskType_SetHandler(gElevatorType, Param_GetTableEvent(), Elevator_GetParams);
    TaskType_SetHandler(gElevatorType, Param_GetChangedEvent(), Elevator_ParamsChanged);
    TaskType_SetHandler2(gElevatorType, Level_GetEventDA8(), Elevator_Activate);
    TaskType_SetHandler2(gElevatorType, Level_GetEventDAC(), Elevator_Deactivate);
    TaskType_SetHandler(gElevatorType, RigidDynCubeObj_GetEvent4(), Elevator_MoveHandler);
    TaskType_SetHandler(gElevatorType, Game_GetEvent14(), Elevator_GetMotion);
    gElevator188Range = (RangeF){0.0f, 10000000.0f};
    gElevatorMaxSpeedRange = (RangeF){0.0f, 10000.0f};
    gElevatorAccelRange = (RangeF){0.0f, 1.0f};
    gElevatorParams = ParamTable_New(25);
    ParamTable_Add(gElevatorParams, &e, &e.place, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, &e.placeMatrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, e.model, Param_String16(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, &e.graph, Param_Int32(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, &e.unk18C, Param_Bool8(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, &e.unk18D, Param_Bool8(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, &e.unk188, Param_Real32(), 7, &gElevator188Range);
    ParamTable_Add(gElevatorParams, &e, &e.maxSpeed, Param_Real32(), 7, &gElevatorMaxSpeedRange);
    ParamTable_Add(gElevatorParams, &e, &e.accel, Param_Real32(), 7, &gElevatorAccelRange);
    ParamTable_Add(gElevatorParams, &e, &e.down, Param_VarString(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, &e.up, Param_VarString(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, &e.move, Param_VarString(), 7, NULL);
    for (i = 0; i < 10; i++) {
        crt_sprintf(gElevatorFloorNames[i], "Wanted floor %d", i);
        ParamTable_Add(gElevatorParams, &e, &e.wantedFloor[i], Param_VarString(), 7, NULL);
    }
    ParamTable_Add(gElevatorParams, &e, e.startSound, Param_String32(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, e.stopSound, Param_String32(), 7, NULL);
    ParamTable_Add(gElevatorParams, &e, e.moveSound, Param_String32(), 7, NULL);
}
HOOK(0x43CC70, Elevator_Init);

void Elevator_CreateHandler(Elevator *e)
{
    Vec3d zero = {0, 0, 0};
    int i;

    RigidDynCubeObj_Init(gCurrentWorld, e, NULL);
    DynCube_Add(gCurrentWorld, e, &zero, obj_radius(&e->obj), 2);
    e->place = zero;
    for (i = 0; i < 9; i++)
        e->placeMatrix.m[i] = i % 4 == 0 ? 1.0f : 0.0f;
    e->placeMatrix.unk24 = 0;
    e->graph = -1;
    e->model[0] = 0;
    e->height = 0.0f;
    e->speed = 0.0f;
    e->maxSpeed = 5.0f;
    e->accel = 0x1.fae148p-1f;
    e->floorPos = 0;
    e->lastFloorPos = 0;
    e->restTicks = 0;
    e->wanted = 0;
    for (i = 0; i < 10; i++) {
        e->request[i] = 0;
        VarString_Init(&e->wantedFloor[i]);
    }
    VarString_Init(&e->up);
    VarString_Init(&e->down);
    VarString_Init(&e->move);
    e->lastPos = e->obj.pos;
    e->unk1C0 = 0;
    e->lastMatrix = e->obj.matrix;
    e->lastUp = 0;
    e->lastDown = 0;
    e->isMoving = 0;
    e->isStarting = 0;
    e->isStopping = 0;
    e->lastMove = 1;
    e->startSound[0] = 0;
    e->stopSound[0] = 0;
    e->moveSound[0] = 0;
    e->moving = NULL;
    e->lastTick = -1;
    e->unk18C = 0;
    e->unk18D = 0;
}
HOOK(0x43D010, Elevator_CreateHandler);

void Elevator_Move(Elevator *e);

static int eval_int(VarString *v, Elevator *e, int def)
{
    if (VarString_HasExpr(v))
        return (int)round_ll(VarString_EvalReal(v, e));
    return def;
}

void Elevator_RunHandler(Elevator *e)
{
    int count = SplineObj_GetWaypointCount(e->graph);
    float prev = e->height;
    float start, target, below = 0.0f;
    int up = 0, down = 0, move = 1;
    long double h;
    Task *s;
    int i;

    if (e->lastTick == Timer_GetTicks())
        return;
    e->lastTick = Timer_GetTicks();
    CALL_CHILDREN(&e->obj.task, 1);
    start = SplineObj_GetLengthTo(e->graph, e->wanted);
    for (i = 0; i < 10; i++) {
        if (VarString_HasExpr(&e->wantedFloor[i]))
            e->request[i] = (char)round_ll(VarString_EvalReal(&e->wantedFloor[i], e));
    }
    up = eval_int(&e->up, e, up);
    down = eval_int(&e->down, e, down);
    move = eval_int(&e->move, e, move);
    if (up && !e->lastUp) {
        if (e->wanted < count - 1)
            e->wanted++;
    } else if (down && !e->lastDown) {
        if (e->wanted > 0)
            e->wanted--;
    } else {
        for (i = 0; i < 10; i++) {
            if (e->request[i])
                e->wanted = i;
        }
    }
    e->lastUp = (char)up;
    e->lastDown = (char)down;
    e->lastPos = e->obj.pos;
    e->lastMatrix = e->obj.matrix;
    if (move)
        Elevator_Move(e);

    e->lastFloorPos = e->floorPos;
    e->floorPos = 0;
    i = 1;
    if (count > 1) {
        for (; i < count; i++) {
            h = SplineObj_GetLengthTo(e->graph, i);
            if (h > e->height) {
                e->floorPos = i - (h - e->height) / (h - below);
                break;
            }
            below = h;
        }
    }
    if (i == count)
        e->floorPos = count - 1;

    target = SplineObj_GetLengthTo(e->graph, e->wanted);
    e->isMoving = e->height < target || e->height > target;
    if (e->isMoving) {
        if (!e->moving) {
            e->moving = Sound_Play(Sound_GetOwner(), e->moveSound);
            if (e->moving)
                Sound_SetPos(e->moving, &e->obj.pos);
        }
    } else if (e->moving) {
        Sound_Stop(e->moving);
        e->moving = NULL;
    }
    e->isStarting = 0;
    if (!(prev < start || prev > start) && (prev < e->height || prev > e->height)) {
        s = Sound_Play(Sound_GetOwner(), e->startSound);
        e->isStarting = 1;
        if (s)
            Sound_SetPos(s, &e->obj.pos);
    }
    e->isStopping = 0;
    if (!(e->height < target || e->height > target) && (prev < e->height || prev > e->height)) {
        s = Sound_Play(Sound_GetOwner(), e->stopSound);
        e->isStopping = 1;
        if (s)
            Sound_SetPos(s, &e->obj.pos);
    }
    if (!(e->height < target || e->height > target) && !e->isMoving && move == e->lastMove) {
        e->lastMove = (char)move;
        e->restTicks++;
        return;
    }
    e->restTicks = 0;
    e->lastMove = (char)move;
}
HOOK(0x43D280, Elevator_RunHandler);

void Elevator_MoveChildren(Elevator *e);

void Elevator_Move(Elevator *e)
{
    long double h, d, a, v;
    float step;

    if (e->graph == -1)
        return;
    SplineObj_GetLength(e->graph);
    if (!SplineObj_GetWaypointCount(e->graph))
        return;
    h = SplineObj_GetLengthTo(e->graph, e->wanted);
    d = h - e->height;
    a = d >= kZero ? d : -d;
    if (!(a >= kNear)) {
        v = a * e->maxSpeed * kSlowdown;
        e->speed = v;
        e->speed = v > kMinSpeed ? v : (long double)kMinSpeed;
    } else if (!(e->speed >= e->maxSpeed)) {
        v = e->accel + e->speed;
        e->speed = v;
        if (v > e->maxSpeed)
            e->speed = e->maxSpeed;
    } else if (e->speed > e->maxSpeed) {
        v = e->speed - e->accel;
        e->speed = v;
        if (!(v >= e->maxSpeed))
            e->speed = e->maxSpeed;
    }
    /* read back, not folded at compile time: the product depends on the rounding mode */
    step = *(volatile float *)&e->speed * kStepUnits;
    if (d > kEpsilon) {
        v = step + e->height;
        step = v;
        if (!(v > h)) {
            e->height = step;
            Elevator_UpdatePlace(e);
            Elevator_MoveChildren(e);
            return;
        }
    } else if (!(d >= -kEpsilon)) {
        v = e->height - step;
        step = v;
        if (v >= h) {
            e->height = step;
            Elevator_UpdatePlace(e);
            Elevator_MoveChildren(e);
            return;
        }
    }
    e->height = h;
    Elevator_UpdatePlace(e);
    Elevator_MoveChildren(e);
}
HOOK(0x43D6E0, Elevator_Move);

void Elevator_MoveChildren(Elevator *e)
{
    MoveArg arg;

    arg.pos = &e->place;
    arg.matrix = &e->obj.matrix;
    CALL_CHILDREN(&e->obj.task, RigidDynCubeObj_GetEvent4(), &arg);
}
HOOK(0x43D8C0, Elevator_MoveChildren);
