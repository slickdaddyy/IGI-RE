#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "vehicle/vehicle.h"

ORIG(0x4B3160, long double, Vec3d_LengthSq, (const Vec3d *v));
ORIG(0x4B31F0, long double, Vec3d_Angle, (const Vec3d *a, const Vec3d *b));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F9720, int *, Graph_GetNode, (void *graph, int id));
ORIG(0x4F9770, void, Graph_GetNodePos, (Vec3d *out, int *node));
ORIG(0x5096C0, int *, Graph_NextStep, (void *graph, int from, int to));
ORIG(0x5096F0, void, Graph_FindNearest, (int **out, void *graph, const Vec3d *pos));
ORIG(0x5098A0, uint16_t, Graph_GetType, (void));
ORIG(0x453540, int, AICmd_GetType, (void *cmd));
ORIG(0x453720, void *, AICmdList_Current, (void *list));
ORIG(0x453730, void *, AICmdList_Next, (void *list));
ORIG(0x453760, void, AICmdList_Advance, (void *list));
ORIG(0x4537E0, void, AICmdList_Init, (void *list, int id, int unk));
ORIG(0x485320, int, AICmd_GetArg, (void *cmd));
ORIG(0x463170, char, sub_463170, (Task *t));
ORIG(0x42FDC0, uint16_t, Car_GetType, (void));
ORIG(0x4F40D0, uint8_t, Car_GetControlEvent, (void));

/* the parts of the "Car" task used here */
typedef struct Car {
    Task task;
    Vec3d pos;
    char unk38[0x38];
    Matrix matrix;
    char unk98[0x60];
    float speed;
    char unkFC[0x720 - 0xFC];
    float unk720;
    char unk724[0x91D - 0x724];
    char hunting;
    char unk91E[0xAA8 - 0x91E];
    RefNode *passengers;
    char unkAAC[0x2ED0 - 0xAAC];
    Task *attacker;
} Car;

/* drives a car along a graph, following an AI command list */
typedef struct CarAI {
    Task task;
    int graphId;
    int pathId;
    int carId;
    Task *graph;
    int cmds[7];
    Car *car;
    int tick;
    char started;
    char pad55[3];
    float throttle;
    float steer;
    float brake;
    float speed;
    int *target;
    float maxSpeed;
    float stop;
    Task *enemy;
    int chaseTicks;
    int waitTicks;
    float cmdSpeed;
} CarAI;

/* what the car gets from its driver each frame */
typedef struct CarControl {
    int tick;
    float *axes;
    float steer;
    float throttle;
    float unk10;
    float brake;
} CarControl;

_Static_assert(offsetof(Car, attacker) == 0x2ED0, "Car");
_Static_assert(sizeof(CarAI) == 0x84, "CarAI");

#define gCarAIType VAR(uint16_t, 0x53BA50)
#define gCarAIParams VAR(void *, 0x57BC74)

static const double kFar = 1677721600.0;
static const double kFarChase = 3288334336.0;
static const double kNear = 150994944.0;
static const double kZero = 0.0;
static const double kPi = 0x1.921fb54442d1bp+1;
static const double kInvPi = 0x1.45f306dc9c880p-2;
static const double kSpeedMin = 5.0;
static const double kSlowDown = 0x1.47ae147ae147bp-31;
static const double kTurnMax = 0x1.6571855555555p-3;
static const double kTurnSpan = 0x1.7bc89ceeed7c6p+1;
static const double kTurnScale = 0x1.591f70b6d4d3fp-2;
static const double kBrakeDiff = -5.0;
static const float kSpeedMinF = 5.0f;
static const float kZeroF = 0.0f;
static const float kOneF = 1.0f;
static const float kSteerMax = 0x1.657186p-3f;
static const float kSteerScale = -0x1.e8ec8ap+0f;
static const float kSteerDamp = 0x1.e66666p-1f;
static const float kSteerMin = 0x1.99999ap-5f;
static const float kCarSpeed = 0x1.bp-6f;
static const float kThrottleScale = 0x1.47ae14p-7f;
static const float kTurnSpeed = 0x1.ca5dcp+4f;
static const float kNoBrake = -1.0f;

/* how the original's fcom / test ah,40h treats a float */
#define ZERO_OR_NAN(x) (!((x) < kZeroF || (x) > kZeroF))

/* x, y, z in the frame of rotation m */
static Vec3d to_local(const float *m, long double x, long double y, long double z)
{
    Vec3d l;

    l.x = m[6] * z + m[3] * y + m[0] * x;
    l.y = m[7] * z + m[4] * y + m[1] * x;
    l.z = m[8] * z + m[5] * y + m[2] * x;
    return l;
}

/* 5 at an angle of pi, maxSpeed straight ahead */
static long double speed_for_angle(float maxSpeed, long double a)
{
    return (maxSpeed - kSpeedMinF) * (kPi - a) * kInvPi + kSpeedMin;
}

Vec3d *CarAI_GetCarPos(CarAI *ai);
int *CarAI_NextNode(Vec3d *pos, void *graph, int target, CarAI *ai);
void CarAI_GetPathPos(Vec3d *out, void *graph, Vec3d *pos, int target, CarAI *ai);
int CarAI_IsMoveCmd(int type);
int CarAI_IsStopCmd(int type);

/* the speed to drive at towards the target node */
long double CarAI_GetSpeed(CarAI *ai)
{
    Vec3d *c = CarAI_GetCarPos(ai);
    void *cmd = AICmdList_Current(ai->cmds);
    Vec3d t, d, e, p, q;
    long double lsq, a;
    double dist;
    int *node;

    AICmd_GetType(cmd);
    Graph_GetNodePos(&t, ai->target);
    d.x = c->x - t.x;
    d.y = c->y - t.y;
    d.z = c->z - t.z;
    lsq = Vec3d_LengthSq(&d);
    dist = lsq;
    if (lsq > kFar) {
        a = Vec3d_YawAngle(to_local(ai->car->matrix.m, -d.x, -d.y, -d.z));
        if (!(a >= kZero))
            a = -a;
        return speed_for_angle(ai->maxSpeed, a);
    }
    node = Graph_GetNode(ai->graph, AICmd_GetArg(cmd));
    Graph_GetNodePos(&p, node);
    if (node != ai->target) {
        int *near, *next;

        Graph_FindNearest(&near, ai->graph, &p);
        next = CarAI_NextNode(CarAI_GetCarPos(ai), ai->graph, *near, ai);
        Graph_GetNodePos(&q, next);
        CarAI_GetPathPos(&q, ai->graph, &q, *next, ai);
    } else {
        void *after = AICmdList_Next(ai->cmds);
        int type;

        if (!after)
            return ai->maxSpeed;
        type = AICmd_GetType(after);
        if (!CarAI_IsMoveCmd(type)) {
            if (CarAI_IsStopCmd(type))
                return ai->maxSpeed * dist * kSlowDown;
            return ai->maxSpeed;
        }
        node = Graph_GetNode(ai->graph, AICmd_GetArg(after));
        Graph_GetNodePos(&q, node);
        CarAI_GetPathPos(&q, ai->graph, &q, *node, ai);
    }
    e.x = q.x - t.x;
    e.y = q.y - t.y;
    e.z = q.z - t.z;
    a = Vec3d_Angle(&d, &e);
    if (!(ai->maxSpeed >= kSpeedMinF))
        return ai->maxSpeed;
    return speed_for_angle(ai->maxSpeed, a);
}
HOOK(0x4255A0, CarAI_GetSpeed);

int CarAI_IsMoveCmd(int type)
{
    return type == 2 || type == 3;
}
HOOK(0x4258C0, CarAI_IsMoveCmd);

int CarAI_IsStopCmd(int type)
{
    return type == 7 || type == 6;
}
HOOK(0x4258E0, CarAI_IsStopCmd);

/* the speed to drive at while chasing the enemy */
long double CarAI_GetChaseSpeed(CarAI *ai)
{
    Vec3d *c = CarAI_GetCarPos(ai);
    Vec3d t, d, e, p, q;
    long double lsq, a;
    double dist;
    int *near, *next;

    Graph_GetNodePos(&t, ai->target);
    d.x = c->x - t.x;
    d.y = c->y - t.y;
    d.z = c->z - t.z;
    lsq = Vec3d_LengthSq(&d);
    dist = lsq;
    if (lsq > kFarChase) {
        a = Vec3d_YawAngle(to_local(ai->car->matrix.m, -d.x, -d.y, -d.z));
        if (!(a >= kZero))
            a = -a;
        return speed_for_angle(ai->maxSpeed, a);
    }
    Graph_FindNearest(&near, ai->graph, DynCubeObj_GetPosition(ai->enemy));
    Graph_GetNodePos(&p, near);
    if (near == ai->target)
        return ai->maxSpeed * dist * kSlowDown;
    next = Graph_NextStep(ai->graph, *ai->target, *near);
    if (next)
        Graph_GetNodePos(&q, Graph_GetNode(ai->graph, *next));
    else
        q = p;
    e.x = q.x - t.x;
    e.y = q.y - t.y;
    e.z = q.z - t.z;
    a = Vec3d_Angle(&d, &e);
    if (!(ai->maxSpeed >= kSpeedMinF))
        return ai->maxSpeed;
    if (a >= kTurnMax)
        return kSpeedMinF;
    return (ai->maxSpeed - kSpeedMinF) * (kTurnSpan - a) * kTurnScale + kSpeedMin;
}
HOOK(0x425900, CarAI_GetChaseSpeed);

Vec3d *CarAI_GetCarPos(CarAI *ai)
{
    return &ai->car->pos;
}
HOOK(0x425B70, CarAI_GetCarPos);

/* where to go from pos to reach node target */
void CarAI_GetPathPos(Vec3d *out, void *graph, Vec3d *pos, int target, CarAI *ai)
{
    int *node, *next;

    Graph_FindNearest(&node, graph, pos);
    if (!node)
        return;
    if (*node == target) {
        Graph_GetNodePos(out, node);
        return;
    }
    next = Graph_NextStep(graph, *node, target);
    if (next) {
        Graph_GetNodePos(out, Graph_GetNode(graph, *next));
        return;
    }
    Warning_Show("CarAI: Error in graph %d. No link between nodes %d and %d.", ai->graphId, *node, target);
}
HOOK(0x425B80, CarAI_GetPathPos);

/* the next node on the way from pos to node target */
int *CarAI_NextNode(Vec3d *pos, void *graph, int target, CarAI *ai)
{
    int *node, *next;

    Graph_FindNearest(&node, graph, pos);
    if (node && *node != target) {
        next = Graph_NextStep(graph, *node, target);
        if (next)
            return Graph_GetNode(graph, *next);
        Warning_Show("CarAI: Error in graph %d. No link between nodes %d and %d.", ai->graphId, *node, target);
    }
    return node;
}
HOOK(0x425C10, CarAI_NextNode);

void CarAI_Create(CarAI *ai);
void CarAI_Run(CarAI *ai);
void CarAI_GetParams(void *task, void **out);
void CarAI_Changed(CarAI *ai);

void CarAI_Init(void)
{
    CarAI ai;

    gCarAIType = TaskType_New("CarAI", sizeof(CarAI), Task_GetRootType(), 0);
    TaskType_SetHandler(gCarAIType, 0, CarAI_Create);
    TaskType_SetHandler2(gCarAIType, 1, CarAI_Run);
    TaskType_SetHandler2(gCarAIType, 2, TASK_NOTHING);
    TaskType_SetHandler(gCarAIType, Param_GetTableEvent(), CarAI_GetParams);
    TaskType_SetHandler(gCarAIType, Param_GetChangedEvent(), CarAI_Changed);
    gCarAIParams = ParamTable_New(3);
    ParamTable_Add(gCarAIParams, &ai, &ai.carId, Param_Int32(), 7, NULL);
    ParamTable_Add(gCarAIParams, &ai, &ai.graphId, Param_Int32(), 7, NULL);
    ParamTable_Add(gCarAIParams, &ai, &ai.pathId, Param_Int32(), 7, NULL);
}
HOOK(0x425C80, CarAI_Init);

void CarAI_Create(CarAI *ai)
{
    ai->carId = -1;
    ai->graphId = -1;
    ai->pathId = -1;
    ai->graph = NULL;
    ai->car = NULL;
    ai->tick = 0;
    ai->cmds[0] = 0;
    ai->started = 0;
    ai->throttle = 0;
    ai->steer = 0;
    ai->brake = 0;
    ai->speed = 0;
    ai->target = NULL;
    ai->stop = 0;
    ai->chaseTicks = 0;
    ai->enemy = NULL;
    ai->waitTicks = 0;
    ai->cmdSpeed = 0;
}
HOOK(0x425D90, CarAI_Create);

int CarAI_UpdateChase(CarAI *ai);
int CarAI_IsNear(CarAI *ai, int id);
void CarAI_Drive(CarAI *ai);
void CarAI_Chase(CarAI *ai);
void CarAI_SendControl(CarAI *ai, int tick, float throttle, float steer, float brake);

void CarAI_Run(CarAI *ai)
{
    if (!ai->started && ai->pathId != -1) {
        AICmdList_Init(ai->cmds, ai->pathId, 0);
        ai->started = 1;
    }
    if (!ai->car || !ai->graph || !ai->cmds[0])
        return;
    if (CarAI_UpdateChase(ai)) {
        int *near;

        Graph_FindNearest(&near, ai->graph, DynCubeObj_GetPosition(ai->enemy));
        if (!CarAI_IsNear(ai, *near)) {
            ai->target = CarAI_NextNode(CarAI_GetCarPos(ai), ai->graph, *near, ai);
            ai->maxSpeed = 20.0f;
            ai->stop = 0;
        } else {
            ai->maxSpeed = 0;
            ai->stop = 1.0f;
        }
        CarAI_Chase(ai);
    } else {
        void *cmd = AICmdList_Current(ai->cmds);
        int type = AICmd_GetType(cmd);
        int n;

        if (type == 2 || type == 3) {
            if (CarAI_IsNear(ai, AICmd_GetArg(cmd))) {
                AICmdList_Advance(ai->cmds);
            } else {
                int id = AICmd_GetArg(cmd);

                ai->target = CarAI_NextNode(CarAI_GetCarPos(ai), ai->graph, id, ai);
                ai->maxSpeed = ai->cmdSpeed;
                ai->stop = 0;
                CarAI_Drive(ai);
            }
        } else if (type == 1) {
            ai->maxSpeed = 0;
            n = ai->waitTicks;
            ai->stop = 1.0f;
            ai->waitTicks = n + 1;
            if (n > AICmd_GetArg(cmd)) {
                AICmdList_Advance(ai->cmds);
                ai->waitTicks = 0;
            }
        } else if (type == 6 || type == 7) {
            ai->maxSpeed = 0;
            ai->stop = 1.0f;
            ai->brake = 1.0f;
            ai->speed = 0;
            ai->steer = 0;
        } else if (type == 8) {
            n = AICmd_GetArg(cmd);
            ai->stop = 0;
            ai->cmdSpeed = n;
            AICmdList_Advance(ai->cmds);
        } else {
            Warning_Show("Command not supported by CARAI: %d.", type);
            AICmdList_Advance(ai->cmds);
        }
    }
    CarAI_SendControl(ai, ai->tick, ai->throttle, ai->steer, ai->brake);
    ai->tick++;
}
HOOK(0x425DE0, CarAI_Run);

/* picks the enemy to chase: whoever attacked the car or is in it, for 600
   frames after the last sighting */
int CarAI_UpdateChase(CarAI *ai)
{
    Car *car = ai->car;
    RefNode *n;

    if (!car->hunting) {
        ai->enemy = NULL;
        ai->chaseTicks = 0;
        return 0;
    }
    if (ai->enemy && sub_463170(ai->enemy)) {
        ai->enemy = NULL;
        ai->chaseTicks = 0;
        return 0;
    }
    if (ai->car->attacker) {
        ai->chaseTicks = 600;
        ai->enemy = ai->car->attacker;
        return 1;
    }
    for (n = car->passengers; n->next; n = n->next) {
        if (n->active && *n->ref) {
            ai->chaseTicks = 600;
            ai->enemy = *n->ref;
            return 1;
        }
    }
    if (!ai->chaseTicks) {
        ai->enemy = NULL;
        return 0;
    }
    ai->chaseTicks--;
    return 1;
}
HOOK(0x426000, CarAI_UpdateChase);

int CarAI_IsNear(CarAI *ai, int id)
{
    Vec3d p, *c;
    long double dx, dy, dz;

    Graph_GetNodePos(&p, Graph_GetNode(ai->graph, id));
    c = CarAI_GetCarPos(ai);
    dx = p.x - c->x;
    dy = p.y - c->y;
    dz = p.z - c->z;
    return !(dz * dz + dy * dy + dx * dx >= kNear);
}
HOOK(0x4260C0, CarAI_IsNear);

void CarAI_GetPedals(float *throttle, float *brake, CarAI *ai);

/* steers towards the target node */
void CarAI_Drive(CarAI *ai)
{
    Vec3d *c;
    Car *car;
    Vec3d t;
    long double a;
    float af;

    ai->speed = CarAI_GetSpeed(ai);
    CarAI_GetPedals(&ai->throttle, &ai->brake, ai);
    c = CarAI_GetCarPos(ai);
    car = ai->car;
    Graph_GetNodePos(&t, ai->target);
    a = Vec3d_YawAngle(to_local(car->matrix.m, t.x - c->x, t.y - c->y, t.z - c->z));
    af = a;
    if (a > kSteerMax)
        ai->steer = -1.0f;
    else if (!(af >= -kSteerMax))
        ai->steer = 1.0f;
    else
        ai->steer = af * kSteerScale;
    ai->steer = ((kOneF - ai->throttle) * kSteerDamp + kSteerMin) * ai->steer;
}
HOOK(0x426140, CarAI_Drive);

void CarAI_GetPedals(float *throttle, float *brake, CarAI *ai)
{
    Car *car = ai->car;
    long double d = ai->speed - car->speed * kCarSpeed;
    long double r = car->unk720 + d * kThrottleScale;

    *throttle = r;
    if (!(r >= kZeroF))
        *throttle = 0;
    else if (r > kOneF)
        *throttle = 1.0f;
    if (ZERO_OR_NAN(ai->stop) && d >= kBrakeDiff)
        *brake = 0;
    else
        *brake = 1.0f;
}
HOOK(0x426260, CarAI_GetPedals);

/* steers towards the target node, or the enemy once close to it */
void CarAI_Chase(CarAI *ai)
{
    int close = ZERO_OR_NAN(ai->maxSpeed);
    Vec3d *c = CarAI_GetCarPos(ai);
    float *m = ai->car->matrix.m;
    Vec3d t, d;
    long double a;
    float af, abs;

    Graph_GetNodePos(&t, ai->target);
    d.x = t.x - c->x;
    d.y = t.y - c->y;
    d.z = t.z - c->z;
    Vec3d_LengthSq(&d);
    if (close) {
        d.x = DynCubeObj_GetPosition(ai->enemy)->x - c->x;
        d.y = DynCubeObj_GetPosition(ai->enemy)->y - c->y;
        d.z = DynCubeObj_GetPosition(ai->enemy)->z - c->z;
    }
    a = Vec3d_YawAngle(to_local(m, d.x, d.y, d.z));
    af = a;
    if (a > kSteerMax)
        ai->steer = -1.0f;
    else if (!(af >= -kSteerMax))
        ai->steer = 1.0f;
    else
        ai->steer = af * kSteerScale;
    abs = !(af >= kZeroF) ? -af : af;
    if (!close)
        ai->speed = CarAI_GetChaseSpeed(ai);
    else if (abs > kSteerMax)
        ai->speed = 5.0f;
    else
        ai->speed = abs * kTurnSpeed;
    CarAI_GetPedals(&ai->throttle, &ai->brake, ai);
    if (close && abs > kSteerMax)
        ai->brake = 0;
    else
        ai->brake = ai->stop;
}
HOOK(0x4262F0, CarAI_Chase);

void CarAI_SendControl(CarAI *ai, int tick, float throttle, float steer, float brake)
{
    CarControl ctl;
    Car *car;

    ctl.axes = &ctl.steer;
    ctl.tick = tick;
    ctl.steer = steer;
    ctl.throttle = throttle;
    ctl.brake = ZERO_OR_NAN(brake) ? kNoBrake : brake;
    car = ai->car;
    ((void (*)(void *, void *))HANDLER(Car_GetControlEvent(), car->task.type))(car, &ctl);
}
HOOK(0x426510, CarAI_SendControl);

void CarAI_GetParams(void *task, void **out)
{
    *out = gCarAIParams;
}
HOOK(0x426590, CarAI_GetParams);

void CarAI_Changed(CarAI *ai)
{
    ai->car = (Car *)Task_FindById(ai->carId);
    if (ai->car && ai->car->task.type != Car_GetType())
        ai->car = NULL;
    ai->graph = Task_FindById(ai->graphId);
    if (ai->graph && ai->graph->type != Graph_GetType())
        ai->graph = NULL;
    if (ai->car)
        CarAI_SendControl(ai, 0, 0, 0, 0);
}
HOOK(0x4265A0, CarAI_Changed);

void CarAI_Close(void)
{
    QTaskList_Free(gCarAIParams);
    TaskType_Free(gCarAIType);
}
HOOK(0x426610, CarAI_Close);
