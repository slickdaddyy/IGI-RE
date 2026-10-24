/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "plane/plane.h"

FN_EXTERN(Heli_GetStateSize);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401BE0, Task *, Task_FindChild, (Task *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (Task *task, uint16_t type));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *results, Vec3d *starts, Vec3d *ends, int count, int event,
                                  void *ignore, int flags));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6AB0, float *, SoundParams_New, (int count));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4EC2C0, void, Body_AddAeroForces, (void *parts, int count, float k, void *body, Matrix *m, float *v));
ORIG(0x4ECDB0, void, Body_Update, (void *body));
ORIG(0x4ECF50, void, Body_AddForce, (void *body, float *force));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F60, void *, Param_Real32x3, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
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
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4110, uint8_t, Vehicle_GetStateSizeEvent, (void));
ORIG(0x4F4120, uint8_t, Vehicle_GetSaveEvent, (void));
ORIG(0x4F4130, uint8_t, Vehicle_GetLoadEvent, (void));
ORIG(0x4F4140, uint8_t, Vehicle_GetKeysEvent, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4F4640, void, Vehicle_Free, (void *obj));
ORIG(0x4F4660, void, Vehicle_RunParts, (void *obj));
ORIG(0x4F4680, void, Vehicle_Init, (void *obj));
ORIG(0x4F46E0, void, Vehicle_SaveState, (void *obj, void **msg));
ORIG(0x4F4780, void, Vehicle_LoadState, (void *obj, void **msg));
ORIG(0x4F4800, void, Vehicle_NewParts, (void *obj));
ORIG(0x4F4820, void, Vehicle_DeleteParts, (void *obj));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *obj, PhysType *phys));
ORIG(0x4FC0E0, void, PhysicsObj_Collide, (void *obj, float a, float b, float c, void *world, int event, void *msg));
ORIG(0x4FC7B0, int, PhysicsObj_IsResting, (void *obj));
ORIG(0x435C40, uint16_t, Gear_GetType, (void));

#define HANDLER_432BD0 (FN(0x432BD0, Heli_GetStateSize))

extern uint16_t gPlaneType;
extern char gPlaneInputs[]; /* {name, 0, min, max} for each PlaneInput field */
extern void *gPlaneParams;
extern char gPlaneRandom[];
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];

extern float kKmhToFrame;
extern float kSlow;          /* 0.8 */
extern float kGroundThrust;  /* 0.6 */
extern float kMilli;
extern float kDamp;          /* 0.995 */
extern float kDampSpeed;
extern float kFlySpeed;      /* 10 */
extern float kHalfDegToRad;
extern float kBrakeTail;     /* 3 */
extern float kBrakeFlap;     /* 0.375 */
extern float kBrakeWing;     /* 0.75 */
extern float kCatapult;
extern float kThrust;
extern double kYaw;
extern float kPartsSpeed;
extern double kThrustStep;   /* 0.01 */
extern double kThrustMin;    /* 0.8 */
extern double kThrustMax;    /* 1.25 */
extern float kHalf;
extern double kWheelUp;      /* 8192 */
extern double kZeroD;
extern double kWheelNear;    /* 7577.6 */
extern float kWheelNearF;
extern float kWheelSpring;
extern double kWheelFar;
extern float kWheelForce;
extern float kWheelDrag;     /* 0.8 */

#define FIELD(name) ((void *)offsetof(Plane, name))

static float plane_radius(Plane *p)
{
    float s = max_scale(p->v.scale);

    return VirModel_GetRadius(p->v.model) * s;
}

void Plane_Create(Plane *p);
void Plane_Run(Plane *p);
void Plane_Delete(Plane *p);
void Plane_GetParams(Plane *p, void **out);
void Plane_ParamsChanged(Plane *p);
void Plane_GetSoundInfo(Plane *p, void **out);
void Plane_Control(Plane *p, PlaneControlArg *arg);
void Plane_GetInputInfo(Plane *p, void **msg);
void Plane_ReadKeys(Plane *p, PlaneInput **in);
void Plane_LoadState(Plane *p, void **msg);
void Plane_SaveState(Plane *p, void **msg);

void Plane_Init(void)
{
    gPlaneType = TaskType_New("Plane", sizeof(Plane), Vehicle_GetType(), 0);
    TaskType_SetHandler(gPlaneType, 0, Plane_Create);
    TaskType_SetHandler2(gPlaneType, 1, Plane_Run);
    TaskType_SetHandler2(gPlaneType, 2, Plane_Delete);
    TaskType_SetHandler(gPlaneType, Param_GetTableEvent(), Plane_GetParams);
    TaskType_SetHandler(gPlaneType, Param_GetChangedEvent(), Plane_ParamsChanged);
    TaskType_SetHandler(gPlaneType, Sound_GetInfoEvent(), Plane_GetSoundInfo);
    TaskType_SetHandler(gPlaneType, Vehicle_GetControlEvent(), Plane_Control);
    TaskType_SetHandler(gPlaneType, Vehicle_GetInputInfoEvent(), Plane_GetInputInfo);
    TaskType_SetHandler(gPlaneType, Vehicle_GetStateSizeEvent(), HANDLER_432BD0);
    TaskType_SetHandler(gPlaneType, Vehicle_GetKeysEvent(), Plane_ReadKeys);
    TaskType_SetHandler(gPlaneType, Vehicle_GetLoadEvent(), Plane_LoadState);
    TaskType_SetHandler(gPlaneType, Vehicle_GetSaveEvent(), Plane_SaveState);
    gPlaneParams = ParamTable_New(10);
    /* the original passes a dummy Plane on its stack, only the difference
       of the two pointers is kept */
    ParamTable_Add(gPlaneParams, NULL, FIELD(v.pos2), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(v.matrix2), Param_Real32x9(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(v.start_speed), Param_Real32(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(v.velocity2), Param_Real32x3(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(v.name), Param_String16(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(gear_init), Param_Bool8(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(cockpit_init), Param_Bool8(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(v.unk6D8), Param_Bool8(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(v.unk6D9), Param_Bool8(), 7, NULL);
    ParamTable_Add(gPlaneParams, NULL, FIELD(script), Param_VarString(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_PLANE", gPlaneType);
    Random_Init(gPlaneRandom);
}
HOOK(0x4330F0, Plane_Init);

void Plane_Reset(Plane *p);

void Plane_Create(Plane *p)
{
    Plane_Reset(p);
    VarString_Init(&p->script);
}
HOOK(0x4333F0, Plane_Create);

void Plane_ResetControls(Plane *p);

void Plane_Reset(Plane *p)
{
    Vec3d zero = {0, 0, 0};
    float radius;

    RigidDynCubeObj_Init(Game_GetWorld(), p, NULL);
    radius = plane_radius(p);
    DynCube_Add(Game_GetWorld(), p, &zero, radius, 2);
    p->sound_params = SoundParams_New(2);
    p->sound = NULL;
    p->v.name[0] = 0;
    Vehicle_Init(p);
    p->v.start_speed = 1.0f;
    p->cockpit_init = 0;
    p->gear_init = 1;
    Plane_ResetControls(p);
}
HOOK(0x433410, Plane_Reset);

void Plane_CopyParts(Plane *p);

void Plane_ResetControls(Plane *p)
{
    p->cockpit = p->cockpit_init;
    p->gear = p->gear_init;
    p->gear_state = p->gear_init;
    p->hatch_state = p->gear_init;
    p->v.brake = 0;
    p->catapult = 0;
    p->force_z = 0;
    if (p->v.phys)
        Plane_CopyParts(p);
}
HOOK(0x433530, Plane_ResetControls);

/* copies the type's parts, then makes parts 3 and 4 (the elevators) from part 2
   and writes them back to the type */
void Plane_CopyParts(Plane *p)
{
    VehicleConfig *cfg = p->v.phys->data;
    float *src = p->parts[2];
    float *a = p->parts[3], *b = p->parts[4];
    float half, neg;
    int i;

    for (i = 0; i < cfg->part_count; i++)
        memcpy(p->parts[i], cfg->parts[i], sizeof(p->parts[i]));
    half = src[0] * kHalf;
    a[4] = 16384.0f;
    a[1] = src[1];
    a[2] = src[2];
    a[3] = src[3];
    b[4] = -16384.0f;
    a[0] = half;
    a[6] = src[6];
    b[1] = src[1];
    b[2] = src[2];
    b[3] = src[3];
    neg = -src[5];
    a[5] = neg;
    b[0] = half;
    b[5] = neg;
    b[6] = src[6];
    memcpy(cfg->parts[3], a, sizeof(p->parts[3]));
    memcpy(cfg->parts[4], b, sizeof(p->parts[4]));
}
HOOK(0x433580, Plane_CopyParts);

void Plane_StopSound(Plane *p);

/* the engine sound plays while the script value is not zero */
void Plane_Run(Plane *p)
{
    if (VarString_HasExpr(&p->script) && !round_int(VarString_EvalReal(&p->script, p))) {
        Plane_StopSound(p);
    } else if (!p->sound) {
        Task *s = Sound_Play(p, "su27");

        if (s)
            p->sound = TaskRef_Add(s);
    }
    BROADCAST(p->v.task.head, 1);
}
HOOK(0x433690, Plane_Run);

/* releases the sound but keeps the stale reference, so it does not restart */
void Plane_StopSound(Plane *p)
{
    if (p->sound) {
        if (*p->sound)
            Sound_Stop(*p->sound);
        TaskRef_Remove(p->sound);
    }
}
HOOK(0x433760, Plane_StopSound);

void Plane_Delete(Plane *p)
{
    Vehicle_Free(p);
    DynCube_Remove(Game_GetWorld(), p);
    QTaskList_Free(p->sound_params);
    VarString_Free(&p->script);
    Plane_StopSound(p);
}
HOOK(0x433790, Plane_Delete);

void Plane_GetParams(Plane *p, void **out)
{
    (void)p;
    *out = gPlaneParams;
}
HOOK(0x4337D0, Plane_GetParams);

void Plane_Fly(Plane *p);

static int velocity2_zero(Plane *p)
{
    return !fne(p->v.velocity2[0], kZero) && !fne(p->v.velocity2[1], kZero) && !fne(p->v.velocity2[2], kZero);
}

/* the body velocity is velocity2, or along the nose at the start speed */
static void plane_start_velocity(Plane *p)
{
    if (velocity2_zero(p)) {
        long double k = (long double)p->v.speed * p->v.phys->data->unk0 * kStartSpeed;

        p->v.body.velocity[0] = k * p->v.matrix.m[1];
        p->v.body.velocity[1] = k * p->v.matrix.m[4];
        p->v.body.velocity[2] = k * p->v.matrix.m[7];
    } else {
        p->v.body.velocity[0] = p->v.velocity2[0];
        p->v.body.velocity[1] = p->v.velocity2[1];
        p->v.body.velocity[2] = p->v.velocity2[2];
    }
}

void Plane_Control(Plane *p, PlaneControlArg *arg)
{
    if (*(uint32_t *)&arg->input->thrust != 0xBF800000)
        p->v.target_speed = arg->input->thrust;
    p->v.steer[0] = arg->input->alpha;
    p->v.steer[1] = arg->input->beta;
    p->v.steer[2] = arg->input->gamma;
    if (*(uint32_t *)&arg->input->cockpit != 0xBF800000)
        p->cockpit ^= round_int(arg->input->cockpit);
    if (*(uint32_t *)&arg->input->gear != 0xBF800000)
        p->gear ^= round_int(arg->input->gear);
    p->v.brake = fne(arg->input->brake, -1.0f);
    p->catapult = fne(arg->input->catapult, -1.0f);
    if (*(uint32_t *)&arg->input->force_z != 0xBF800000)
        p->force_z ^= round_int(arg->input->force_z);
    if (!arg->mode) {
        float radius = plane_radius(p);
        PhysType *phys;

        DynCube_Move(Game_GetWorld(), p, &p->v.pos2, radius);
        phys = p->v.phys;
        p->v.matrix = p->v.matrix2;
        PhysicsObj_SetPhysType(p, phys);
        p->v.speed = p->v.start_speed;
        p->v.steer[0] = 0;
        p->v.steer[1] = 0;
        p->v.target_speed = p->v.start_speed;
        p->v.turn[0] = 0;
        p->v.turn[1] = 0;
        p->v.turn[2] = 0;
        p->v.steer[2] = 0;
        Vehicle_DeleteParts(p);
        Vehicle_NewParts(p);
        Plane_ResetControls(p);
        plane_start_velocity(p);
        Body_Update(&p->v.body);
        if (p->v.coll) {
            PartMoveArg m = {&p->v.pos, &p->v.matrix, p};

            BROADCAST(p->v.coll->head, MagicObj_GetMoveEvent(), &m);
        }
    }
    if (arg->mode > 0) {
        Plane_Fly(p);
        Vehicle_RunParts(p);
    }
}
HOOK(0x4337E0, Plane_Control);

void Plane_CheckWheels(Plane *p);

void Plane_Fly(Plane *p)
{
    PhysType *phys = p->v.phys;
    VehicleConfig *cfg = phys->data;
    long double ratio, s, f;
    float rate, target, t0, t1;
    long double t2;
    int i;

    ratio = p->v.body.unk8 / (cfg->thrust * kKmhToFrame);
    if (!(ratio >= kZero))
        ratio = kZero;
    else if (ratio > kOne)
        ratio = kOne;
    for (i = 0; i < phys->joint_count; i++)
        p->v.joints[i].active = !p->gear;

    /* speed towards the thrust setting, slower on the ground */
    rate = !(p->v.speed >= kSlow) ? cfg->accel_slow : cfg->accel;
    s = p->v.speed;
    if (p->v.unk1B8 && !(p->v.target_speed >= kOne))
        target = p->v.target_speed - kGroundThrust;
    else
        target = p->v.target_speed;
    if (!(s >= target)) {
        s = s + rate;
        if (s > target)
            s = target;
    } else if (s > target) {
        s = s - rate;
        if (!(s >= target))
            s = target;
    }
    if (!(s >= kMilli))
        s = kZero;
    p->v.speed = s;

    /* controls follow the input, damped more at speed */
    f = kDamp - ratio * kDampSpeed;
    t0 = ((kOne - cfg->axis[0].smooth) * p->v.steer[0] + cfg->axis[0].smooth * p->v.turn[0]) * f;
    p->v.turn[0] = t0;
    t1 = ((kOne - cfg->axis[1].smooth) * p->v.steer[1] + cfg->axis[1].smooth * p->v.turn[1]) * f;
    p->v.turn[1] = t1;
    t2 = ((kOne - cfg->axis[2].smooth) * p->v.steer[2] + cfg->axis[2].smooth * p->v.turn[2]) * f;
    p->v.turn[2] = t2;

    /* elevators: parts 3 and 4 turned by pitch minus and plus roll */
    if ((fne(t0, kZero) || fne(t1, kZero) || fne(t2, kZero)) && (!p->v.unk1B8 || p->v.body.unk8 >= kFlySpeed)) {
        float max = cfg->axis[0].rate;
        float pitch = t0 * cfg->axis[0].rate * kHalfDegToRad;
        long double roll = t1 * cfg->axis[1].rate * kHalfDegToRad;
        float rollf = roll;
        long double x = -(roll + pitch);
        float a;
        Matrix m1, m2;
        const float *q;

        a = x >= max ? max : (float)x;
        Matrix_SetRotX(&m1, a);
        x = rollf - pitch;
        a = x >= cfg->axis[0].rate ? cfg->axis[0].rate : (float)x;
        Matrix_SetRotX(&m2, a);
        q = cfg->parts[3];
        p->parts[3][1] = m1.m[0] * q[1] + m1.m[2] * q[3] + m1.m[1] * q[2];
        p->parts[3][2] = m1.m[5] * q[3] + m1.m[4] * q[2] + m1.m[3] * q[1];
        p->parts[3][3] = m1.m[8] * q[3] + m1.m[7] * q[2] + m1.m[6] * q[1];
        q = cfg->parts[4];
        p->parts[4][1] = m2.m[0] * q[1] + m2.m[2] * q[3] + m2.m[1] * q[2];
        p->parts[4][2] = m2.m[3] * q[1] + m2.m[5] * q[3] + m2.m[4] * q[2];
        p->parts[4][3] = m2.m[6] * q[1] + m2.m[8] * q[3] + m2.m[7] * q[2];
    }

    if (p->v.brake) {
        p->parts[1][0] = cfg->parts[1][0] * kBrakeTail;
        p->parts[3][0] = cfg->parts[2][0] * kBrakeFlap;
        p->parts[4][0] = cfg->parts[2][0] * kBrakeFlap;
        p->parts[2][0] = cfg->parts[2][0] * kBrakeWing;
    } else {
        p->parts[1][0] = cfg->parts[1][0];
        p->parts[3][0] = cfg->parts[3][0];
        p->parts[4][0] = cfg->parts[4][0];
        p->parts[2][0] = cfg->parts[2][0];
    }

    if (p->catapult) {
        float force[6] = {0, phys->power * kCatapult, 0, 0, 0, 0};

        Body_AddForce(&p->v.body, force);
    }
    if (fne(p->v.speed, kZero)) {
        float force[6];

        force[0] = 0;
        force[2] = 0;
        force[1] = phys->power * p->v.speed * kThrust;
        if (!p->v.unk1B8) {
            Matrix m;
            float g0, g1, g2;

            Matrix_SetRotZ(&m, cfg->axis[2].rate * p->v.turn[2] * kYaw);
            g0 = m.m[2] * force[2] + m.m[1] * force[1] + m.m[0] * force[0];
            g1 = m.m[5] * force[2] + m.m[4] * force[1] + m.m[3] * force[0];
            g2 = m.m[8] * force[2] + m.m[7] * force[1] + m.m[6] * force[0];
            force[0] = g0;
            force[1] = g1;
            force[2] = g2;
        }
        force[3] = 0;
        force[4] = 40960.0f;
        force[5] = 0;
        Body_AddForce(&p->v.body, force);
    }

    Plane_CheckWheels(p);
    if (!PhysicsObj_IsResting(p)) {
        struct {
            int collide;
            Plane *p;
        } msg;

        msg.p = p;
        if (cfg->part_count && (!p->v.unk1B8 || p->v.body.unk8 > kPartsSpeed)) {
            float v[3];

            v[0] = -p->v.body.velocity[0];
            v[1] = -p->v.body.velocity[1];
            v[2] = -p->v.body.velocity[2];
            Body_AddAeroForces(p->parts, cfg->part_count, 0x1.56e264p-12f, &p->v.body, &p->v.matrix, v);
        }
        PhysicsObj_Collide(p, 0x1.64ce9ep+5f, 1.0f, 0x1.d70a3ep-1f, Game_GetWorld(), 12, &msg);
    }
    if (p->v.coll) {
        PartMoveArg m = {&p->v.pos, &p->v.matrix, p};

        BROADCAST(p->v.coll->head, MagicObj_GetMoveEvent(), &m);
    }
    if (p->sound_params) {
        p->sound_params[1] = p->v.speed;
        p->sound_params[2] = p->v.speed > kHundredth ? 1 : 0;
    }
    {
        PartMoveArg m = {&p->v.pos, &p->v.matrix, NULL};

        BROADCAST(p->v.task.head, RigidDynCubeObj_GetEvent4(), &m);
    }
}
HOOK(0x433B50, Plane_Fly);

/* one collision result of Coll_Lines */
typedef struct WheelHit {
    char unk0[0x58];
    int found;
    char unk5C[4];
    double dist;
    char unk68[8];
} WheelHit;

_Static_assert(sizeof(WheelHit) == 0x70, "WheelHit");

/* casts a ray down through the point of each Gear part (at most four) and
   pushes the plane up where one is closer than 8192 to the ground */
void Plane_CheckWheels(Plane *p)
{
    Vec3d starts[4], ends[4];
    double down[4];
    float pts[4][3];
    WheelHit hits[4];
    struct {
        int unk0;
        Plane *p;
    } ignore;
    double lift = -0x1.fffffffffffffp+1023;
    int found = 0, n = 0, i;
    Task *g;

    g = Task_FindChild(p->v.coll, Gear_GetType());
    if (!g)
        return;
    while (n < 4) {
        Part *gear = (Part *)g;
        Vec3d *s = &starts[n];
        float *pt = pts[n];
        double z;
        float w0, w1, w2;

        Model_GetPoint(s, g, 0);
        pt[0] = s->x;
        pt[1] = s->y;
        z = s->z;
        pt[2] = z;
        w0 = z * gear->matrix.m[6] + gear->matrix.m[3] * pt[1] + gear->matrix.m[0] * pt[0];
        w1 = z * gear->matrix.m[7] + gear->matrix.m[4] * pt[1] + gear->matrix.m[1] * pt[0];
        w2 = z * gear->matrix.m[8] + gear->matrix.m[5] * pt[1] + gear->matrix.m[2] * pt[0];
        pt[0] = gear->offset.x + w0;
        pt[1] = gear->offset.y + w1;
        pt[2] = gear->offset.z + w2;
        down[n] = -8192.0;
        s->x = gear->pos.x + s->x;
        s->y = gear->pos.y + s->y;
        s->z = gear->pos.z + s->z + kWheelUp;
        ends[n] = *s;
        ends[n].z = ends[n].z - kWheelUp;
        n++;
        g = Task_FindNext(g, Gear_GetType());
        if (!g)
            break;
    }
    ignore.p = p;
    Coll_Lines(Game_GetWorld(), hits, starts, ends, n, 12, &ignore, -1);
    for (i = 0; i < n; i++) {
        long double a, t;
        float force[6];

        if (!hits[i].found)
            continue;
        a = down[i];
        if (a < kZeroD)
            a = -a;
        if (!(a >= hits[i].dist))
            continue;
        t = kWheelUp - hits[i].dist;
        force[0] = 0;
        force[1] = 0;
        if (!(hits[i].dist >= kWheelNear)) {
            long double u;

            force[2] = ((kWheelNearF - hits[i].dist) * kWheelSpring + kOne) * p->v.phys->power * kWheelForce;
            u = kWheelNear - hits[i].dist;
            if (lift < u)
                lift = u;
            found = 1;
        } else {
            force[2] = t * kWheelFar * p->v.phys->power * kWheelForce;
        }
        force[3] = pts[i][0];
        force[4] = pts[i][1];
        force[5] = pts[i][2];
        Body_AddForce(&p->v.body, force);
        p->v.body.velocity[0] = p->v.body.velocity[0] * p->v.speed * kWheelDrag;
        p->v.body.velocity[1] = p->v.body.velocity[1] * p->v.speed * kWheelDrag;
        p->v.body.spin[0] = p->v.body.spin[0] * kKeep;
        p->v.body.spin[1] = p->v.body.spin[1] * kKeep;
        p->v.body.spin[2] = p->v.body.spin[2] * kKeep;
    }
    if (found) {
        Vec3d pos = p->v.pos;
        float radius;

        pos.z = pos.z + lift;
        radius = plane_radius(p);
        DynCube_Move(Game_GetWorld(), p, &pos, radius);
    }
}
HOOK(0x434310, Plane_CheckWheels);

void Plane_ParamsChanged(Plane *p)
{
    char buf[128], name[128];
    void **model = VirModel_Find(p->v.name);
    float radius;

    Vehicle_DeleteParts(p);
    if (model) {
        RigidDynCubeObj_SetModel(p, model);
        PhysicsObj_SetPhysType(p, PhysicsObjType_Find(p->v.name));
        Plane_ResetControls(p);
    } else if (p->v.name[0] && p->v.name[1] != '*') {
        crt_sprintf(buf, "Model \"%s\" not available", p->v.name);
        strcpy(name, p->v.name);
        crt_sprintf(p->v.name, "** %s **", name);
    }
    p->v.target_speed = p->v.start_speed;
    p->v.speed = p->v.start_speed;
    if (!velocity2_zero(p) || p->v.phys) {
        plane_start_velocity(p);
        Body_Update(&p->v.body);
    }
    radius = plane_radius(p);
    DynCube_Move(Game_GetWorld(), p, &p->v.pos2, radius);
    p->v.matrix = p->v.matrix2;
    Vehicle_NewParts(p);
    BROADCAST(p->v.coll->head, Param_GetChangedEvent(), NULL);
    VarString_Bind(&p->script, p);
}
HOOK(0x434770, Plane_ParamsChanged);

/* {position, unused, sound parameters} for the engine sound */
void Plane_GetSoundInfo(Plane *p, void **out)
{
    out[1] = NULL;
    out[0] = &p->v.pos;
    out[2] = p->sound_params;
}
HOOK(0x434A30, Plane_GetSoundInfo);

/* {index, out}: the name and range of one PlaneInput field */
void Plane_GetInputInfo(Plane *p, void **msg)
{
    int i = (int)msg[0];

    (void)p;
    if (i >= 0 && i < 10) {
        msg[1] = gPlaneInputs + i * 16;
        return;
    }
    msg[1] = NULL;
}
HOOK(0x434A50, Plane_GetInputInfo);

static int pressed(int key)
{
    return (gKeys[key] & 0x80) && !(gKeysPrev[key] & 0x80);
}

static const uint32_t kThrottleKeys[10] = {
    0x3F4CCCCD, 0x3F59999A, 0x3F666666, 0x3F733333, 0x3F800000,
    0x3F866666, 0x3F8CCCCD, 0x3F933333, 0x3F99999A, 0x3FA00000,
};

void Plane_ReadKeys(Plane *p, PlaneInput **in)
{
    int i;

    if (!(gKeys[0x9D] & 0x80) && !(gKeys[0x1D] & 0x80)) {
        (*in)->beta = (int)((gKeys[0xCD] >> 7) - (gKeys[0xCB] >> 7));
        (*in)->alpha = (int)((gKeys[0xD0] >> 7) - (gKeys[0xC8] >> 7));
    }
    (*in)->gamma = (int)((gKeys[0x2D] >> 7) - (gKeys[0x2C] >> 7));
    *(uint32_t *)&(*in)->thrust = 0xBF800000;
    if (gKeys[0x29] & 0x80)
        (*in)->thrust = 0;
    for (i = 0; i < 10; i++)
        if (gKeys[2 + i] & 0x80)
            *(uint32_t *)&(*in)->thrust = kThrottleKeys[i];
    if (pressed(0x31)) {
        long double t = p->v.target_speed - kThrustStep;

        if (kThrustMin > t)
            t = kThrustMin;
        (*in)->thrust = t;
    }
    if (pressed(0x32)) {
        long double t = p->v.target_speed + kThrustStep;

        if (kThrustMax < t)
            t = kThrustMax;
        (*in)->thrust = t;
    }
    (*in)->cockpit = -1.0f;
    if (pressed(0x2E))
        (*in)->cockpit = 1.0f;
    (*in)->gear = -1.0f;
    if (pressed(0x22))
        (*in)->gear = 1.0f;
    (*in)->brake = -1.0f;
    if (gKeys[0x30] & 0x80)
        (*in)->brake = 1.0f;
    (*in)->catapult = -1.0f;
    if (pressed(0x25))
        (*in)->catapult = 1.0f;
    (*in)->force_z = -1.0f;
    if (pressed(0x10))
        (*in)->force_z = 1.0f;
}
HOOK(0x434A80, Plane_ReadKeys);

void Plane_LoadState(Plane *p, void **msg)
{
    char *state = msg[0];
    float radius;

    Vehicle_LoadState(p, msg);
    p->cockpit = state[0x70];
    p->gear = state[0x71];
    p->v.brake = state[0x72];
    p->catapult = state[0x73];
    p->force_z = state[0x74];
    radius = plane_radius(p);
    DynCube_Move(Game_GetWorld(), p, (Vec3d *)(state + 0x10), radius);
}
HOOK(0x434CD0, Plane_LoadState);

void Plane_SaveState(Plane *p, void **msg)
{
    char *state = msg[0];

    Vehicle_SaveState(p, msg);
    state[0x70] = p->cockpit;
    state[0x71] = p->gear;
    state[0x72] = p->v.brake;
    state[0x73] = p->catapult;
    state[0x74] = p->force_z;
}
HOOK(0x434DA0, Plane_SaveState);

void Plane_Close(void)
{
    QTaskList_Free(gPlaneParams);
    TaskType_Free(gPlaneType);
}
HOOK(0x434DF0, Plane_Close);
