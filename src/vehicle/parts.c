#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "vehicle/vehicle.h"

ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B3BA0, void, Matrix_SetRotY, (Matrix *m, float a));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4CE410, char, Coll_LineOfSight, (void *world, const Vec3d *from, const Vec3d *to, int event, void *filter,
                                         int flags, int trees, float *visibility));
ORIG(0x4F0EE0, void *, sub_4F0EE0, (void));
ORIG(0x429A30, void, sub_429A30, (void *parent, void *owner, void *shooter, Vec3d *pos, Matrix *dir, float speed,
                                  Vec3d *target, int unk, const char *name, int unk2));
ORIG(0x4F4860, uint8_t, sub_4F4860, (void));
ORIG(0x4D14D0, uint8_t, sub_4D14D0, (void));

/* a moving part of a vehicle: door, wheel, rocket pod */
typedef struct VehiclePart {
    RigidObj obj;
    Vec3d offset;
    Matrix local;
    Task *owner;
    int unk134;
    void *params;
    union {
        float angle;
        int cooldown;
    };
    char open;
} VehiclePart;

typedef struct DoorParams {
    int axis;
    float speed;
    float open;
} DoorParams;

/* the vehicle a rocket pod is on */
typedef struct Vehicle {
    char unk0[0xF8];
    float speed;
    char unkFC[0x8D8 - 0xFC];
    RefNode *targets;
} Vehicle;

/* passed to the line of sight filter */
typedef struct SightFilter {
    char visible;
    Task *unk4;
    Task *self;
    Task *target;
} SightFilter;

_Static_assert(offsetof(VehiclePart, owner) == 0x130, "VehiclePart");
_Static_assert(offsetof(VehiclePart, open) == 0x140, "VehiclePart");
_Static_assert(offsetof(Vehicle, targets) == 0x8D8, "Vehicle");

#define gCarDoorType VAR(uint16_t, 0x53BABC)
#define gHeliWheelType VAR(uint16_t, 0x57BC7C)
#define gHeliDoorType VAR(uint16_t, 0x53BB18)
#define gRocketPodType VAR(uint16_t, 0x53BB3C)
#define gHitZoneType VAR(uint16_t, 0x53BB6C)

#define HANDLER_42D9B0 ((void *)0x42D9B0)
#define HANDLER_45F920 ((void *)0x45F920)
#define HANDLER_51BAA0 ((void *)0x51BAA0)

static const float kZeroF = 0.0f;
static const float kDegToRad = 0x1.1df46ap-6f;
static const float kPerFrame = 0x1.111112p-5f;
static const float kAngleUnits = 0x1.0p+12f;
static const float kHeliDoorSpeed = 0x1.111112p+7f;
static const float kRocketReach = 0x1.0p+11f;
static const float kRocketSpeed = 0x1.ep-8f;
static const float kRocketSpeedMin = 5.0f;
static const double kZero = 0.0;
static const double kRocketCone = 0x1.4cccccccccccdp+0;

static float part_radius(VehiclePart *p)
{
    float s = max_scale(&p->obj);

    return VirModel_GetRadius(p->obj.model) * s;
}

/* pos and matrix of the vehicle, the offset turned with it */
static void part_place(Vec3d *l, const PartMoveArg *arg)
{
    const float *m = arg->matrix->m;
    Vec3d r;

    r.x = m[2] * l->z + m[1] * l->y + m[0] * l->x;
    r.y = m[5] * l->z + m[4] * l->y + m[3] * l->x;
    r.z = m[8] * l->z + m[7] * l->y + m[6] * l->x;
    l->x = r.x + arg->pos->x;
    l->y = r.y + arg->pos->y;
    l->z = r.z + arg->pos->z;
}

/* the vehicle matrix times the local one */
static void part_turn(VehiclePart *p, const Matrix *m)
{
    const float *b = p->local.m;
    Matrix c;
    int i, j;

    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            c.m[i * 3 + j] = m->m[i * 3 + 1] * b[3 + j] + m->m[i * 3 + 2] * b[6 + j] + m->m[i * 3] * b[j];
    c.unk24 = m->unk24 + p->local.unk24 + 1;
    p->obj.matrix = c;
}

void CarDoor_Create(VehiclePart *p, PartCreateArg *arg);
void HeliDoor_Run(VehiclePart *p);
void VehiclePart_Delete(VehiclePart *p);
void CarDoor_Move(VehiclePart *p, PartMoveArg *arg);
void CarDoor_ParseArgs(void *task, PartScriptArg *a);
void VehiclePart_CollFilter(Task *t, SightFilter *f);

void CarDoor_Init(void)
{
    gCarDoorType = TaskType_New("CarDoor", 0x148, RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gCarDoorType, 0, CarDoor_Create);
    TaskType_SetHandler2(gCarDoorType, 1, HeliDoor_Run);
    TaskType_SetHandler2(gCarDoorType, 2, VehiclePart_Delete);
    TaskType_SetHandler(gCarDoorType, RigidDynCubeObj_GetMoveEvent(), CarDoor_Move);
    TaskType_SetHandler(gCarDoorType, Game_GetEvent9(), HANDLER_42D9B0);
    TaskType_SetHandler(gCarDoorType, Game_GetEvent11(), NULL);
    TaskType_SetHandler(gCarDoorType, RigidDynCubeObj_GetEvent0(), CarDoor_ParseArgs);
    TaskType_SetHandler(gCarDoorType, RigidDynCubeObj_GetEvent1(), HANDLER_51BAA0);
    TaskType_SetHandler(gCarDoorType, sub_489610(), VehiclePart_CollFilter);
    Script_RegisterConstant("TASKTYPE_CARDOOR", gCarDoorType);
}
HOOK(0x426630, CarDoor_Init);

void CarDoor_Create(VehiclePart *p, PartCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    p->offset = arg->offset;
    p->local = arg->local;
    RigidDynCubeObj_Init(arg->unk4, p, model);
    sub_4C7140(arg->unk4, p, &arg->pos, part_radius(p), 2);
    p->obj.matrix = arg->matrix;
    p->params = arg->params;
    p->owner = arg->owner;
    p->open = 0;
    p->angle = 0;
}
HOOK(0x426740, CarDoor_Create);

void VehiclePart_Delete(VehiclePart *p)
{
    sub_4C7560(p->obj.unk68, p);
}
HOOK(0x426860, VehiclePart_Delete);

/* the door turns about one of its local axes */
void CarDoor_Move(VehiclePart *p, PartMoveArg *arg)
{
    Vec3d l = p->offset;
    const float *m;
    float radius;
    void *world;
    Matrix r, a, c;
    int axis, i, j;

    part_place(&l, arg);
    radius = part_radius(p);
    world = Game_GetWorld();
    sub_4C6940(world, p, &l, radius);
    axis = *(int *)p->params;
    if (axis == 0)
        Matrix_SetRotX(&r, p->angle);
    else if (axis == 1)
        Matrix_SetRotY(&r, p->angle);
    else if (axis == 2)
        Matrix_SetRotZ(&r, p->angle);
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            a.m[i * 3 + j] = r.m[6 + j] * p->local.m[i * 3 + 2] + r.m[j] * p->local.m[i * 3] +
                             r.m[3 + j] * p->local.m[i * 3 + 1];
    a.unk24 = p->local.unk24 + r.unk24 + 1;
    m = arg->matrix->m;
    for (j = 0; j < 3; j++) {
        c.m[j] = a.m[j] * m[0] + a.m[3 + j] * m[1] + a.m[6 + j] * m[2];
        c.m[3 + j] = a.m[6 + j] * m[5] + a.m[j] * m[3] + a.m[3 + j] * m[4];
        c.m[6 + j] = a.m[6 + j] * m[8] + a.m[j] * m[6] + a.m[3 + j] * m[7];
    }
    c.unk24 = arg->matrix->unk24 + a.unk24 + 1;
    p->obj.matrix = c;
}
HOOK(0x426880, CarDoor_Move);

/* axis, speed (degrees per second) and open angle (degrees) */
void CarDoor_ParseArgs(void *task, PartScriptArg *a)
{
    DoorParams *p = a->out;
    int i = a->index;

    p->axis = Script_GetArgInt(a->args, i);
    p->speed = Script_GetArgReal(a->args, i + 1) * kDegToRad * kPerFrame;
    p->open = Script_GetArgReal(a->args, i + 2) * kDegToRad;
}
HOOK(0x426CD0, CarDoor_ParseArgs);

/* line of sight filter: skips the shooter, the target and the shooter's parts */
void VehiclePart_CollFilter(Task *t, SightFilter *f)
{
    if (f->self != t && f->target != t && ((VehiclePart *)t)->owner != f->self)
        f->visible = 1;
    else
        f->visible = 0;
}
HOOK(0x426D30, VehiclePart_CollFilter);

void CarDoor_Close(void)
{
    TaskType_Free(gCarDoorType);
}
HOOK(0x426D60, CarDoor_Close);

uint16_t CarDoor_GetType(void)
{
    return gCarDoorType;
}
HOOK(0x426D70, CarDoor_GetType);

void CarDoor_SetOpen(VehiclePart *p, char open)
{
    p->open = open;
}
HOOK(0x426D80, CarDoor_SetOpen);

void HeliWheel_Create(VehiclePart *p, PartCreateArg *arg);
void HeliWheel_Move(VehiclePart *p, PartMoveArg *arg);
void HeliWheel_ParseArgs(void *task, PartScriptArg *a);
void HitZone_GetArgSize(void *task, int *out);

void HeliWheel_Init(void)
{
    gHeliWheelType = TaskType_New("HeliWheel", 0x140, RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gHeliWheelType, 0, HeliWheel_Create);
    TaskType_SetHandler2(gHeliWheelType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gHeliWheelType, 2, VehiclePart_Delete);
    TaskType_SetHandler(gHeliWheelType, RigidDynCubeObj_GetMoveEvent(), HeliWheel_Move);
    TaskType_SetHandler(gHeliWheelType, Game_GetEvent9(), TASK_NOTHING);
    TaskType_SetHandler(gHeliWheelType, Game_GetEvent11(), TASK_NOTHING);
    TaskType_SetHandler(gHeliWheelType, RigidDynCubeObj_GetEvent0(), HeliWheel_ParseArgs);
    TaskType_SetHandler(gHeliWheelType, RigidDynCubeObj_GetEvent1(), HitZone_GetArgSize);
    Script_RegisterConstant("TASKTYPE_HELIWHEEL", gHeliWheelType);
}
HOOK(0x426D90, HeliWheel_Init);

void HeliWheel_Create(VehiclePart *p, PartCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    p->offset = arg->offset;
    p->local = arg->local;
    RigidDynCubeObj_Init(arg->unk4, p, model);
    sub_4C7140(arg->unk4, p, &arg->pos, part_radius(p), 2);
    p->obj.matrix = arg->matrix;
    p->owner = arg->owner;
    p->params = arg->params;
}
HOOK(0x426E90, HeliWheel_Create);

void HeliWheel_Move(VehiclePart *p, PartMoveArg *arg)
{
    Vec3d l = p->offset;
    float radius;
    void *world;

    part_place(&l, arg);
    radius = part_radius(p);
    world = Game_GetWorld();
    sub_4C6940(world, p, &l, radius);
    part_turn(p, arg->matrix);
}
HOOK(0x426FA0, HeliWheel_Move);

void HeliWheel_ParseArgs(void *task, PartScriptArg *a)
{
    float *p = a->out;
    int i = a->index;

    p[1] = Script_GetArgReal(a->args, i) * kAngleUnits;
    p[0] = Script_GetArgReal(a->args, i + 1) * kAngleUnits;
}
HOOK(0x427240, HeliWheel_ParseArgs);

void HeliWheel_Close(void)
{
    TaskType_Free(gHeliWheelType);
}
HOOK(0x427280, HeliWheel_Close);

uint16_t HeliWheel_GetType(void)
{
    return gHeliWheelType;
}
HOOK(0x427290, HeliWheel_GetType);

void HeliDoor_Move(VehiclePart *p, PartMoveArg *arg);
void HeliDoor_ParseArgs(void *task, PartScriptArg *a);

void HeliDoor_Init(void)
{
    gHeliDoorType = TaskType_New("HeliDoor", 0x148, RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gHeliDoorType, 0, CarDoor_Create);
    TaskType_SetHandler2(gHeliDoorType, 1, HeliDoor_Run);
    TaskType_SetHandler2(gHeliDoorType, 2, VehiclePart_Delete);
    TaskType_SetHandler(gHeliDoorType, RigidDynCubeObj_GetMoveEvent(), HeliDoor_Move);
    TaskType_SetHandler(gHeliDoorType, Game_GetEvent9(), HANDLER_42D9B0);
    TaskType_SetHandler(gHeliDoorType, Game_GetEvent11(), NULL);
    TaskType_SetHandler(gHeliDoorType, RigidDynCubeObj_GetEvent0(), HeliDoor_ParseArgs);
    TaskType_SetHandler(gHeliDoorType, RigidDynCubeObj_GetEvent1(), HANDLER_51BAA0);
    TaskType_SetHandler(gHeliDoorType, sub_489610(), VehiclePart_CollFilter);
    Script_RegisterConstant("TASKTYPE_HELIDOOR", gHeliDoorType);
}
HOOK(0x4272A0, HeliDoor_Init);

/* moves angle towards the open position, or back to 0 */
void HeliDoor_Run(VehiclePart *p)
{
    long double v;

    if (p->open) {
        DoorParams *dp = p->params;

        if (!(p->angle < dp->open || p->angle > dp->open))
            return;
        if (!(p->angle >= dp->open)) {
            v = dp->speed + p->angle;
            p->angle = v;
            if (v > dp->open)
                p->angle = dp->open;
        } else if (p->angle > dp->open) {
            v = p->angle - dp->speed;
            p->angle = v;
            if (!(v >= dp->open))
                p->angle = dp->open;
        }
    } else {
        if (!(p->angle < kZeroF || p->angle > kZeroF))
            return;
        if (!(p->angle >= kZeroF)) {
            v = ((DoorParams *)p->params)->speed + p->angle;
            p->angle = v;
            if (v > kZeroF)
                p->angle = 0;
        } else if (p->angle > kZeroF) {
            v = p->angle - ((DoorParams *)p->params)->speed;
            p->angle = v;
            if (!(v >= kZeroF))
                p->angle = 0;
        }
    }
}
HOOK(0x4273B0, HeliDoor_Run);

/* the door slides along one of its local axes */
void HeliDoor_Move(VehiclePart *p, PartMoveArg *arg)
{
    int axis = *(int *)p->params;
    Vec3d l = p->offset;
    float radius;
    void *world;

    if (axis == 0)
        l.x = p->angle + l.x;
    else if (axis == 1)
        l.y = p->angle + l.y;
    else if (axis == 2)
        l.z = p->angle + l.z;
    part_place(&l, arg);
    radius = part_radius(p);
    world = Game_GetWorld();
    sub_4C6940(world, p, &l, radius);
    part_turn(p, arg->matrix);
}
HOOK(0x4274E0, HeliDoor_Move);

void HeliDoor_ParseArgs(void *task, PartScriptArg *a)
{
    DoorParams *p = a->out;
    int i = a->index;

    p->axis = Script_GetArgInt(a->args, i);
    p->speed = Script_GetArgReal(a->args, i + 1) * kHeliDoorSpeed;
    p->open = Script_GetArgReal(a->args, i + 2) * kAngleUnits;
}
HOOK(0x4277C0, HeliDoor_ParseArgs);

void HeliDoor_Close(void)
{
    TaskType_Free(gHeliDoorType);
}
HOOK(0x427810, HeliDoor_Close);

uint16_t HeliDoor_GetType(void)
{
    return gHeliDoorType;
}
HOOK(0x427820, HeliDoor_GetType);

void RocketPod_Create(VehiclePart *p, PartCreateArg *arg);
void RocketPod_Run(VehiclePart *p);
void RocketPod_ParseArgs(void *task, PartScriptArg *a);
void RocketPod_GetArgSize(void *task, int *out);

void RocketPod_Init(void)
{
    gRocketPodType = TaskType_New("RocketPod", 0x140, RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gRocketPodType, 0, RocketPod_Create);
    TaskType_SetHandler2(gRocketPodType, 1, RocketPod_Run);
    TaskType_SetHandler2(gRocketPodType, 2, VehiclePart_Delete);
    TaskType_SetHandler(gRocketPodType, RigidDynCubeObj_GetMoveEvent(), HeliWheel_Move);
    TaskType_SetHandler(gRocketPodType, Game_GetEvent9(), HANDLER_42D9B0);
    TaskType_SetHandler(gRocketPodType, Game_GetEvent11(), NULL);
    TaskType_SetHandler(gRocketPodType, RigidDynCubeObj_GetEvent0(), RocketPod_ParseArgs);
    TaskType_SetHandler(gRocketPodType, RigidDynCubeObj_GetEvent1(), RocketPod_GetArgSize);
    TaskType_SetHandler(gRocketPodType, sub_489610(), VehiclePart_CollFilter);
    Script_RegisterConstant("TASKTYPE_ROCKETPOD", gRocketPodType);
}
HOOK(0x427830, RocketPod_Init);

void RocketPod_Create(VehiclePart *p, PartCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    p->offset = arg->offset;
    p->local = arg->local;
    RigidDynCubeObj_Init(arg->unk4, p, model);
    sub_4C7140(arg->unk4, p, &arg->pos, part_radius(p), 2);
    p->obj.matrix = arg->matrix;
    p->params = arg->params;
    p->owner = arg->owner;
    p->cooldown = Random_Int(Game_GetRandom(), *(int *)p->params);
}
HOOK(0x427940, RocketPod_Create);

/* fires at the first target in front of the pod and in sight, then waits
   between delay and twice delay frames */
void RocketPod_Run(VehiclePart *p)
{
    Vehicle *owner = (Vehicle *)p->owner;
    RefNode *n, *next;

    if (p->cooldown < 0) {
        for (n = owner->targets; (next = n->next) != NULL; n = next) {
            const float *m = p->obj.matrix.m;
            Vec3d l, pos, target;
            SightFilter filter;
            long double dz, a;
            double dx, dy;
            float visibility, speed;
            uint8_t event;
            Vec3d *to;
            void *world;
            int delay;

            if (!n->active || !*n->ref)
                continue;
            dx = DynCubeObj_GetPosition(*n->ref)->x - p->obj.pos.x;
            dy = DynCubeObj_GetPosition(*n->ref)->y - p->obj.pos.y;
            dz = DynCubeObj_GetPosition(*n->ref)->z - p->obj.pos.z;
            l.x = m[3] * dy + m[6] * dz + m[0] * dx;
            l.y = m[4] * dy + m[7] * dz + m[1] * dx;
            l.z = m[5] * dy + m[8] * dz + m[2] * dx;
            if (!(l.y >= kZero))
                continue;
            if (!(Vec3d_YawAngle(l) >= kZeroF))
                a = -Vec3d_YawAngle(l);
            else
                a = Vec3d_YawAngle(l);
            if (a >= kRocketCone)
                continue;
            filter.unk4 = &p->obj.task;
            filter.self = &p->obj.task;
            filter.target = *n->ref;
            event = sub_489610();
            to = DynCubeObj_GetPosition(*n->ref);
            world = Game_GetWorld();
            if (!Coll_LineOfSight(world, &p->obj.pos, to, event, &filter, -1, -1, &visibility))
                continue;
            pos = p->obj.pos;
            pos.x = m[1] * kRocketReach + pos.x;
            pos.y = (float)(m[4] * kRocketReach) + pos.y;
            pos.z = (float)(m[7] * kRocketReach) + pos.z;
            target = ((RigidObj *)*n->ref)->pos;
            speed = owner->speed * kRocketSpeed + kRocketSpeedMin;
            sub_429A30(sub_4F0EE0(), p, p, &pos, &p->obj.matrix, speed, &target, 0, "700_10_1", 0);
            delay = *(int *)p->params;
            p->cooldown = Random_Int(Game_GetRandom(), delay) + *(int *)p->params;
        }
    }
    p->cooldown--;
}
HOOK(0x427A70, RocketPod_Run);

/* the delay between shots, in seconds */
void RocketPod_ParseArgs(void *task, PartScriptArg *a)
{
    *(int *)a->out = Script_GetArgInt(a->args, a->index) * 30;
}
HOOK(0x427D40, RocketPod_ParseArgs);

void RocketPod_GetArgSize(void *task, int *out)
{
    *out = 4;
}
HOOK(0x427D70, RocketPod_GetArgSize);

void RocketPod_Close(void)
{
    TaskType_Free(gRocketPodType);
}
HOOK(0x427D80, RocketPod_Close);

void HitZone_Create(VehiclePart *p);
void HitZone_ParseArgs(void *task, PartScriptArg *a);
void HitZone_ClearHit(void *task, void **arg);

void HitZone_Init(void)
{
    gHitZoneType = TaskType_New("HitZone", 0x148, RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gHitZoneType, 0, HitZone_Create);
    TaskType_SetHandler2(gHitZoneType, 1, TASK_NOTHING);
    TaskType_SetHandler2(gHitZoneType, 2, TASK_NOTHING);
    TaskType_SetHandler(gHitZoneType, RigidDynCubeObj_GetMoveEvent(), TASK_NOTHING);
    TaskType_SetHandler(gHitZoneType, Game_GetEvent9(), NULL);
    TaskType_SetHandler(gHitZoneType, Game_GetEvent11(), NULL);
    TaskType_SetHandler(gHitZoneType, RigidDynCubeObj_GetEvent0(), HitZone_ParseArgs);
    TaskType_SetHandler(gHitZoneType, RigidDynCubeObj_GetEvent1(), HitZone_GetArgSize);
    TaskType_SetHandler(gHitZoneType, sub_4F4860(), HitZone_ClearHit);
    TaskType_SetHandler(gHitZoneType, sub_4D14D0(), NULL);
    TaskType_SetHandler(gHitZoneType, 9, NULL);
    TaskType_SetHandler(gHitZoneType, 12, HANDLER_45F920);
    TaskType_SetHandler(gHitZoneType, World_GetEvent1(), NULL);
    TaskType_SetHandler(gHitZoneType, World_GetEvent0(), NULL);
    TaskType_SetHandler(gHitZoneType, RigidDynCubeObj_GetEvent2(), NULL);
    TaskType_SetHandler(gHitZoneType, 4, NULL);
    Script_RegisterConstant("TASKTYPE_HITZONE", gHitZoneType);
}
HOOK(0x427D90, HitZone_Init);

void HitZone_Create(VehiclePart *p)
{
    *(int *)((char *)p + 0xDC) = 0;
    *(int *)((char *)p + 0xE8) = 0;
    p->obj.model = NULL;
}
HOOK(0x427F30, HitZone_Create);

void HitZone_ParseArgs(void *task, PartScriptArg *a)
{
    float *p = a->out;
    int i = a->index;

    p[0] = Script_GetArgReal(a->args, i);
    p[1] = Script_GetArgReal(a->args, i + 1);
}
HOOK(0x427F50, HitZone_ParseArgs);

void HitZone_GetArgSize(void *task, int *out)
{
    *out = 8;
}
HOOK(0x427F80, HitZone_GetArgSize);

void HitZone_ClearHit(void *task, void **arg)
{
    char *hit = *(char **)((char *)*arg + 0x20);

    *(int *)(hit + 0x58) = 0;
    hit = *(char **)((char *)*arg + 0x20);
    *(int *)(hit + 0x50) = 0;
    hit = *(char **)((char *)*arg + 0x20);
    *(int *)(hit + 0x4C) = 0;
}
HOOK(0x427F90, HitZone_ClearHit);
