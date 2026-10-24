#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "heli/heli.h"

FN_EXTERN(Heli_GetConfigSize);
FN_EXTERN(Heli_GetSoundInfo);
FN_EXTERN(Heli_LoadState);
FN_EXTERN(Heli_ParseArgs);
FN_EXTERN(Heli_RemoveFromWorld);
FN_EXTERN(Heli_SaveState);
FN_EXTERN(Task_Nothing);
FN_EXTERN(Task_YesHandler);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A60, int, Task_GetCount, (void));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401BE0, Task *, Task_FindChild, (Task *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (Task *task, uint16_t type));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int point));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x4168D0, void *, Game_GetTaskList3, (void));
ORIG(0x4168E0, void *, Game_GetTaskList4, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x426D80, void, CarDoor_SetOpen, (Task *door, int open));
ORIG(0x427290, uint16_t, HeliWheel_GetType, (void));
ORIG(0x427820, uint16_t, HeliDoor_GetType, (void));
ORIG(0x464250, int, Human_GetMaskType, (int a, int b));
ORIG(0x46AB20, uint8_t, ComputerMap_GetSelectEvent, (void));
ORIG(0x480FE0, void, Explosion_Spawn, (Vec3d *pos, int a, int b, float c, float d, float e, float f, int g, int h,
                                       int i, const char *sprite, const char *sound, int j, int k, float l, int m));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x489630, uint8_t, AIDetection_GetEvent2, (void));
ORIG(0x4896C0, void, AIDetector_Init, (void *light, int a, int b, int c, int d, int e));
ORIG(0x489750, void, AIDetector_Update, (void *light, Vec3d *pos, void *owner, int a));
ORIG(0x489B90, void, AIDetection_RemoveRefs, (void *light));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B34F0, void, Matrix_RotateX, (Matrix *out, const Matrix *m, float a));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *results, Vec3d *starts, Vec3d *ends, int count, char unk,
                                  void *query, unsigned int flags));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6AB0, float *, SoundParams_New, (int count));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4EC2C0, void, Body_AddAeroForces, (void *parts, int count, float k, void *body, Matrix *m, Vec3f *v));
ORIG(0x4ECDB0, void, Body_Update, (void *body));
ORIG(0x4ECF50, void, Body_AddForce, (void *body, float *force));
ORIG(0x4EE0E0, uint8_t, PhysicsObjType_GetParseEvent, (void));
ORIG(0x4EE0F0, uint8_t, PhysicsObjType_GetSizeEvent, (void));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x4F0F30, void *, Param_Degrees, (void));
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
ORIG(0x4F1C90, void *, Param_Range02, (void));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4110, uint8_t, Vehicle_GetStateSizeEvent, (void));
ORIG(0x4F4120, uint8_t, Vehicle_GetSaveEvent, (void));
ORIG(0x4F4130, uint8_t, Vehicle_GetLoadEvent, (void));
ORIG(0x4F4140, uint8_t, Vehicle_GetKeysEvent, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4F4600, void, Vehicle_ResetControls, (void *v));
ORIG(0x4F4640, void, Vehicle_Free, (void *v));
ORIG(0x4F4660, void, Vehicle_RunParts, (void *v));
ORIG(0x4F4680, void, Vehicle_Init, (void *v));
ORIG(0x4F4800, void, Vehicle_NewParts, (void *v));
ORIG(0x4F4820, void, Vehicle_DeleteParts, (void *v));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *v, PhysType *phys));
ORIG(0x4FC0E0, void, PhysicsObj_Collide, (void *obj, float a, float b, float c, void *world, int event, void *msg));
ORIG(0x4FC7B0, int, PhysicsObj_IsResting, (void *obj));

void Car_CollFilter(Task *t, char *msg);

#define gHeliType VAR(uint16_t, 0x53BE58)
#define gHeliParams VAR(void *, 0x57BD40)
#define gHeliRange ((float *)0x57BD38)
#define gHeliControls ((ControlInfo *)0x53BE60)

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))
#define HELI_LOAD_STATE (FN(0x432F20, Heli_LoadState))
#define HELI_SAVE_STATE (FN(0x432F40, Heli_SaveState))
#define HELI_PARSE_DEF (FN(0x432F60, Heli_ParseArgs))
#define HELI_GET_DEF_SIZE (FN(0x432F80, Heli_GetConfigSize))
#define HELI_ON_WORLD_EVENT1 (FN(0x432F90, Heli_RemoveFromWorld))
#define HELI_HANDLER_433000 (FN(0x433000, Heli_GetSoundInfo))
#define TASK_HANDLER_4E4280 (FN(0x4E4280, Task_YesHandler))

#define kAccelSpeed VAR(double, 0x533758)  /* 0.8 */
#define kMaxThrottle VAR(double, 0x533770) /* 1.25 */
#define kTwoMetersD VAR(double, 0x533708)  /* 8192 */
#define kTwoMeters VAR(float, 0x533768)
#define kPitchScale VAR(double, 0x533740)
#define kPitchDown VAR(float, 0x533748)    /* -0.7 */
#define kYawForce VAR(float, 0x533750)     /* -0.8 */
#define kRollForce VAR(float, 0x53374C)    /* 0.08 */
#define kLift VAR(float, 0x533764)         /* 11.15 */
#define kDampZ VAR(float, 0x533760)        /* 0.97 */

static float radius(Heli *h)
{
    float s = max_scale(h->v.scale);

    return VirModel_GetRadius(h->v.model) * s;
}

void Heli_Create(Heli *h);
void Heli_Run(Heli *h);
void Heli_Delete(Heli *h);
void Heli_GetParamTable(Heli *h, void **out);
void Heli_ParamsChanged(Heli *h);
void Heli_GetSoundParams(Heli *h, void **out);
void Heli_Control(Heli *h, ControlArg *arg);
void Heli_GetControlInfo(Heli *h, int *arg);
void Heli_GetStateSize(Heli *h, int *out);
void Heli_ReadKeys(Heli *h, float **arg);

void Heli_Init(void)
{
    Heli h;
    void *type, *range;

    gHeliType = TaskType_New("Heli", sizeof(Heli), Vehicle_GetType(), 0);
    TaskType_SetHandler(gHeliType, 0, Heli_Create);
    TaskType_SetHandler2(gHeliType, 1, Heli_Run);
    TaskType_SetHandler2(gHeliType, 2, Heli_Delete);
    TaskType_SetHandler(gHeliType, Param_GetTableEvent(), Heli_GetParamTable);
    TaskType_SetHandler(gHeliType, Param_GetChangedEvent(), Heli_ParamsChanged);
    TaskType_SetHandler(gHeliType, Sound_GetInfoEvent(), Heli_GetSoundParams);
    TaskType_SetHandler(gHeliType, Game_GetEvent9(), TASK_NOTHING);
    TaskType_SetHandler(gHeliType, Game_GetEvent11(), TASK_NOTHING);
    TaskType_SetHandler(gHeliType, Vehicle_GetControlEvent(), Heli_Control);
    TaskType_SetHandler(gHeliType, Vehicle_GetInputInfoEvent(), Heli_GetControlInfo);
    TaskType_SetHandler(gHeliType, Vehicle_GetStateSizeEvent(), Heli_GetStateSize);
    TaskType_SetHandler(gHeliType, Vehicle_GetKeysEvent(), Heli_ReadKeys);
    TaskType_SetHandler(gHeliType, Vehicle_GetLoadEvent(), HELI_LOAD_STATE);
    TaskType_SetHandler(gHeliType, Vehicle_GetSaveEvent(), HELI_SAVE_STATE);
    TaskType_SetHandler(gHeliType, PhysicsObjType_GetParseEvent(), HELI_PARSE_DEF);
    TaskType_SetHandler(gHeliType, PhysicsObjType_GetSizeEvent(), HELI_GET_DEF_SIZE);
    TaskType_SetHandler(gHeliType, World_GetEvent1(), HELI_ON_WORLD_EVENT1);
    TaskType_SetHandler(gHeliType, AIDetection_GetEvent0(), Car_CollFilter);
    TaskType_SetHandler(gHeliType, ComputerMap_GetSelectEvent(), TASK_HANDLER_4E4280);
    TaskType_SetHandler(gHeliType, AIDetection_GetEvent2(), HELI_HANDLER_433000);
    gHeliParams = ParamTable_New(13);
    gHeliRange[0] = 0;
    gHeliRange[1] = 1000;
    ParamTable_Add(gHeliParams, &h, &h.v.pos2, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.v.matrix2, Param_Real32x9(), 7, NULL);
    range = Param_Range02();
    type = Param_Real32();
    ParamTable_Add(gHeliParams, &h, &h.v.start_speed, type, 7, range);
    ParamTable_Add(gHeliParams, &h, &h.v.unk180, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, h.v.name, Param_String16(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.v.unk6D8, Param_Bool8(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.v.airborne, Param_Bool8(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.rotor_expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.light_expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.engine_expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.light_params[0], Param_Degrees(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.light_params[1], Param_Degrees(), 7, NULL);
    ParamTable_Add(gHeliParams, &h, &h.light_params[2], Param_Real32(), 7, gHeliRange);
    Script_RegisterConstant("TASKTYPE_HELI", gHeliType);
}
HOOK(0x4310F0, Heli_Init);

void Heli_Reset(Heli *h);

void Heli_Create(Heli *h)
{
    Heli_Reset(h);
    QTaskList_Add(Game_GetTaskList3(), h);
}
HOOK(0x431540, Heli_Create);

void Heli_Reset(Heli *h)
{
    Vec3d zero = {0, 0, 0};
    float r;

    RigidDynCubeObj_Init(Game_GetWorld(), h, NULL);
    r = radius(h);
    DynCube_Add(Game_GetWorld(), h, &zero, r, 2);
    h->sound_params = SoundParams_New(2);
    h->rotor_sound = NULL;
    h->v.name[0] = 0;
    Vehicle_Init(h);
    h->v.start_speed = 1.0f;
    h->exploding = 0;
    h->explode_timer = -1;
    VarString_Init(&h->rotor_expr);
    VarString_Init(&h->light_expr);
    VarString_Init(&h->engine_expr);
    h->lights = 0;
    QTaskList_Add(Game_GetTaskList4(), h);
    h->light_params[0] = 180.0f;
    h->light_params[1] = 360.0f;
    h->light_params[2] = 60.0f;
}
HOOK(0x431560, Heli_Reset);

void Heli_StopSound(Heli *h);
Task *Heli_FindDoor(Heli *h);

void Heli_Run(Heli *h)
{
    if (!VarString_HasExpr(&h->engine_expr) || round_int(VarString_EvalReal(&h->engine_expr, h)) != 0) {
        if (!h->rotor_sound) {
            Task *s = Sound_Play(h, "helicopter_1");

            if (s)
                h->rotor_sound = TaskRef_Add(s);
        }
    } else {
        Heli_StopSound(h);
    }
    if (h->exploding) {
        float r = radius(h);
        HeliConfig *cfg = h->v.phys->data;
        int n = cfg->explode_count;

        if (h->explode_timer == -1) {
            if (n == 0) {
                Explosion_Spawn(&h->v.pos, 1, 0, 1.0f, r * kQuarter, r * kTwentieth, r * kTwoHundredth, 3, 1, 0,
                                "1009_01_1", "explo_03_l", -1, 0, 102400.0f, 0);
            } else {
                int *point = cfg->explode_points;

                Task_GetCount();
                for (; n > 0; n--, point++) {
                    int delay = Random_Int(Game_GetRandom(), 30);
                    Vec3d p;

                    Model_GetPoint(&p, h, *point);
                    p.x = p.x + h->v.pos.x;
                    p.y = p.y + h->v.pos.y;
                    p.z = p.z + h->v.pos.z;
                    Explosion_Spawn(&p, 1, delay, 1.0f, 12288.0f, 3072.0f, 3072.0f, 3, 2, 0, "1009_01_1",
                                    "explo_03_l", -1, 0, 102400.0f, 0);
                }
            }
            h->explode_timer = 30;
        } else if (--h->explode_timer == 0) {
            Task_Delete(h);
            return;
        }
    }
    if (VarString_HasExpr(&h->rotor_expr)) {
        int open = round_int(VarString_EvalReal(&h->rotor_expr, h));
        Task *door = Heli_FindDoor(h);

        if (door)
            CarDoor_SetOpen(door, open);
    }
    if (VarString_HasExpr(&h->light_expr)) {
        int was = h->lights;

        h->lights = round_int(VarString_EvalReal(&h->light_expr, h));
        if (h->lights) {
            Matrix m = h->v.matrix;

            Matrix_RotateX(&m, &m, -0x1.921fb6p+0f);
            h->light_pos = h->v.pos;
            h->light_matrix = m;
            h->unk8B8 = 0;
            AIDetector_Update(h->light, &h->light_pos, h, 1);
        }
        if (!h->lights && was) {
            AIDetection_RemoveRefs(h->light);
            AIDetector_Init(h->light, 1, 4, Human_GetMaskType(4, 8), 4, 8);
        }
    }
    BROADCAST(h->v.task.head, 1);
}
HOOK(0x4316D0, Heli_Run);

void Heli_StopSound(Heli *h)
{
    if (!h->rotor_sound)
        return;
    if (*h->rotor_sound)
        Sound_Stop(*h->rotor_sound);
    TaskRef_Remove(h->rotor_sound);
    h->rotor_sound = NULL;
}
HOOK(0x431A80, Heli_StopSound);

Task *Heli_FindDoor(Heli *h)
{
    uint16_t type = HeliDoor_GetType();

    return Task_FindChild(h->v.coll, type);
}
HOOK(0x431AC0, Heli_FindDoor);

void Heli_Delete(Heli *h)
{
    Vehicle_Free(h);
    DynCube_Remove(Game_GetWorld(), h);
    QTaskList_Remove(Game_GetTaskList3(), h);
    QTaskList_Free(h->sound_params);
    Heli_StopSound(h);
    VarString_Free(&h->rotor_expr);
    VarString_Free(&h->light_expr);
    VarString_Free(&h->engine_expr);
    QTaskList_Remove(Game_GetTaskList4(), h);
    AIDetection_RemoveRefs(h->light);
}
HOOK(0x431AE0, Heli_Delete);

void Heli_GetParamTable(Heli *h, void **out)
{
    *out = gHeliParams;
}
HOOK(0x431B60, Heli_GetParamTable);

void Heli_Move(Heli *h);

/* {tick, controls}: tick 0 puts the helicopter back where it started */
void Heli_Control(Heli *h, ControlArg *arg)
{
    if (*(uint32_t *)&arg->axes[3] != 0xBF800000)
        h->v.target_speed = arg->axes[3];
    h->v.steer[0] = arg->axes[0];
    h->v.steer[1] = arg->axes[1];
    h->v.steer[2] = arg->axes[2];
    if (arg->tick == 0) {
        float r = radius(h);
        const float *m;
        Vec3f v;

        DynCube_Move(Game_GetWorld(), h, &h->v.pos2, r);
        arg->axes[3] = -1.0f;
        arg->axes[4] = -2.0f;
        arg->axes[0] = 0;
        arg->axes[1] = 0;
        arg->axes[2] = 0;
        h->v.matrix = h->v.matrix2;
        PhysicsObj_SetPhysType(h, h->v.phys);
        Vehicle_ResetControls(h);
        h->v.speed = h->v.start_speed;
        h->v.target_speed = h->v.start_speed;
        h->v.turn[0] = 0;
        h->v.turn[1] = 0;
        h->v.turn[2] = 0;
        h->v.steer[0] = 0;
        h->v.steer[1] = 0;
        h->v.steer[2] = 0;
        Vehicle_DeleteParts(h);
        Vehicle_NewParts(h);
        m = h->v.matrix.m;
        v.x = m[2] * h->v.unk180.z + m[1] * h->v.unk180.y + h->v.unk180.x * m[0];
        v.y = m[5] * h->v.unk180.z + m[4] * h->v.unk180.y + h->v.unk180.x * m[3];
        v.z = m[8] * h->v.unk180.z + m[7] * h->v.unk180.y + h->v.unk180.x * m[6];
        h->v.body.velocity = v;
        Body_Update(&h->v.body);
        if (h->v.coll) {
            PartMoveArg msg = {&h->v.pos, &h->v.matrix, h};

            BROADCAST(h->v.coll->head, MagicObj_GetMoveEvent(), &msg);
        }
    }
    if (arg->tick > 0) {
        Vehicle_RunParts(h);
        Heli_Move(h);
    }
}
HOOK(0x431B70, Heli_Control);

void Heli_UpdateWheels(Heli *h);

/* speed towards the throttle, smoothed turning forces, nose pitch from the
   heading when airborne, then collisions and the parts */
void Heli_Move(Heli *h)
{
    PhysType *phys = h->v.phys;
    VehicleConfig *cfg = phys->data;
    float rate = !(h->v.speed >= kAccelSpeed) ? cfg->accel_slow : cfg->accel;
    float target = h->v.target_speed;
    long double s = h->v.speed;
    float t0, t1, t2, stale = 0;
    void *msg[3] = {NULL, NULL, NULL};

    if (!(s >= target)) {
        s = s + rate;
        if (s > target)
            s = target;
    } else if (s > target) {
        s = s - rate;
        if (!(s >= target))
            s = target;
    }
    if (!(s >= kMilli) && !fne(h->v.target_speed, kZero))
        s = kZero;
    h->v.speed = s;
    t0 = (kOne - cfg->axis[0].smooth) * h->v.steer[0] + cfg->axis[0].smooth * h->v.turn[0];
    h->v.turn[0] = t0;
    t1 = (kOne - cfg->axis[1].smooth) * h->v.steer[1] + cfg->axis[1].smooth * h->v.turn[1];
    h->v.turn[1] = t1;
    t2 = (kOne - cfg->axis[2].smooth) * h->v.steer[2] + cfg->axis[2].smooth * h->v.turn[2];
    h->v.turn[2] = t2;
    if ((fne(t0, kZero) || fne(t1, kZero) || fne(t2, kZero)) &&
        (!h->v.contacts || h->v.body.unk8 > kTwo)) {
        VehicleConfig *c = phys->data;
        float a0 = t0 * c->axis[0].rate;
        float a1 = t1 * c->axis[1].rate;
        float f1[6] = {0, 0, 0, 0, 0, 819.2f};
        float f2[6] = {0, 0, 0, 0, 0, 409.6f};
        float f3[6] = {0, 0, 0, 0, 12288.0f, 0};

        f1[1] = a0 * phys->power * kYawForce;
        f2[0] = a1 * phys->power;
        f3[0] = s * (t2 * c->axis[2].rate) * phys->power * kRollForce;
        stale = f2[0];
        Body_AddForce(&h->v.body, f1);
        Body_AddForce(&h->v.body, f2);
        Body_AddForce(&h->v.body, f3);
    }
    if (!h->v.contacts && h->v.body.unk8 > kTwo) {
        float *m = h->v.matrix.m;
        long double pitch;
        Matrix r, nm;
        int i;

        if (fne(m[0], kZero) || fne(m[3], kZero)) {
            long double inv = kOneD / x87_sqrt(m[0] * m[0] + m[3] * m[3]);
            float cx = m[0] * inv;
            long double cy = m[3] * inv;

            pitch = kZero * m[8] + cy * m[5] + cx * m[2];
            if (m[8] > kZero)
                pitch = pitch * kPitchDown;
            else
                pitch = pitch * kSevenTenths;
        } else {
            /* the original reuses a stack slot here */
            pitch = stale;
        }
        Matrix_SetRotX(&r, pitch * ((VehicleConfig *)h->v.phys->data)->axis[2].rate * kPitchScale);
        for (i = 0; i < 3; i++) {
            nm.m[i] = r.m[i] * m[0] + r.m[6 + i] * m[2] + r.m[3 + i] * m[1];
            nm.m[3 + i] = r.m[i] * m[3] + r.m[6 + i] * m[5] + r.m[3 + i] * m[4];
            nm.m[6 + i] = r.m[6 + i] * m[8] + r.m[i] * m[6] + r.m[3 + i] * m[7];
        }
        nm.unk24 = h->v.matrix.unk24 + r.unk24 + 1;
        h->v.matrix = nm;
    }
    if (!PhysicsObj_IsResting(h)) {
        VehicleConfig *c;

        msg[1] = h;
        Heli_UpdateWheels(h);
        c = h->v.phys->data;
        if (c->part_count) {
            Vec3f v;

            v.x = -h->v.body.velocity.x;
            v.y = -h->v.body.velocity.y;
            v.z = -h->v.body.velocity.z;
            Body_AddAeroForces(c->parts, c->part_count, 0x1.56e264p-12f, &h->v.body, &h->v.matrix, &v);
        }
        PhysicsObj_Collide(h, 0x1.64ce9ep+5f, 1.0f, 0x1.d70a3ep-1f, Game_GetWorld(), 12, msg);
    }
    if (h->v.coll) {
        msg[0] = &h->v.pos;
        msg[1] = &h->v.matrix;
        msg[2] = h;
        BROADCAST(h->v.coll->head, MagicObj_GetMoveEvent(), msg);
    }
    if (h->sound_params) {
        int fast = 1;

        h->sound_params[1] = h->v.speed;
        if (!(h->v.speed > kHundredth))
            fast = 0;
        h->sound_params[2] = fast;
    }
    msg[0] = &h->v.pos;
    msg[1] = &h->v.matrix;
    BROADCAST(h->v.task.head, RigidDynCubeObj_GetEvent4(), msg);
}
HOOK(0x431E30, Heli_Move);

/* pushes the helicopter up on its landing wheels (at most 4) */
void Heli_UpdateWheels(Heli *h)
{
    Vec3d starts[4], ends[4];
    double depth[4], len = 0, top = 0;
    double max_lift = -0x1.fffffffffffffp+1023;
    Vec3f offs[4];
    CollHit hits[4];
    struct {
        int unk0;
        Heli *self;
    } q;
    int count = 0, landed = 0, i;
    uint16_t type = HeliWheel_GetType();
    Task *w = Task_FindChild(h->v.coll, type);

    h->v.contacts = 0;
    if (!w)
        return;
    do {
        float *params = *(float **)((char *)w + 0x138);
        Vec3d *off = (Vec3d *)((char *)w + 0xF0);
        long double d;

        if (count >= 4)
            break;
        starts[count] = *(Vec3d *)((char *)w + 0x20);
        starts[count].z = starts[count].z + kTwoMetersD;
        len = params[0];
        top = params[1];
        offs[count].x = off->x;
        offs[count].y = off->y;
        offs[count].z = off->z;
        d = -(len + kTwoMetersD);
        depth[count] = d;
        ends[count] = starts[count];
        ends[count].z = d + ends[count].z;
        count++;
        type = HeliWheel_GetType();
        w = Task_FindNext(w, type);
    } while (w);
    if (!count)
        return;
    q.self = h;
    Coll_Lines(Game_GetWorld(), hits, starts, ends, count, 12, &q, 0xFFFFFFFF);
    for (i = 0; i < count; i++) {
        CollHit *hit = &hits[i];
        long double a, dist, t;
        double pen;
        float force[6];

        if (!hit->found)
            continue;
        a = depth[i];
        if (!(a >= kZeroD))
            a = -a;
        if (!(a >= hit->dist))
            continue;
        dist = hit->dist;
        if (!(dist >= kZeroD))
            dist = -dist;
        pen = len - (dist - kTwoMetersD);
        force[0] = 0;
        force[1] = 0;
        t = top + kTwoMetersD;
        if (t > hit->dist) {
            float u = top + kTwoMeters;

            landed = 1;
            force[2] = ((u - dist) / u + kOne) * h->v.phys->power * kLift;
            if (t - hit->dist > max_lift)
                max_lift = t - hit->dist;
        } else {
            landed = 0;
            force[2] = pen / len * h->v.phys->power * kLift;
        }
        force[3] = offs[i].x;
        force[4] = offs[i].y;
        force[5] = offs[i].z;
        Body_AddForce(&h->v.body, force);
        h->v.contacts++;
        h->v.body.velocity.x = h->v.body.velocity.x * kHeliDamp;
        h->v.body.velocity.y = h->v.body.velocity.y * kHeliDamp;
        h->v.body.velocity.z = h->v.body.velocity.z * kDampZ;
        h->v.body.spin.x = h->v.body.spin.x * kDamp98;
        h->v.body.spin.y = h->v.body.spin.y * kDamp98;
        h->v.body.spin.z = h->v.body.spin.z * kDamp98;
    }
    if (landed) {
        Vec3d p = h->v.pos;
        float r;

        p.z = p.z + max_lift;
        r = radius(h);
        DynCube_Move(Game_GetWorld(), h, &p, r);
    }
}
HOOK(0x432550, Heli_UpdateWheels);

void Heli_ParamsChanged(Heli *h)
{
    void **model = VirModel_Find(h->v.name);
    char name[128], buf[128];
    float r;

    if (model) {
        RigidDynCubeObj_SetModel(h, model);
        PhysicsObj_SetPhysType(h, PhysicsObjType_Find(h->v.name));
        Vehicle_ResetControls(h);
        h->light_cfg[0] = h->light_params[0] * kDegToRad;
        h->light_cfg[1] = h->light_params[1] * kDegToRad;
        h->light_cfg[2] = h->light_params[2] * kFixed;
        AIDetector_Init(h->light, 1, 4, Human_GetMaskType(4, 8), 4, 8);
    } else if (h->v.name[0] && h->v.name[1] != '*') {
        crt_sprintf(buf, "Model \"%s\" not available", h->v.name);
        strcpy(name, h->v.name);
        crt_sprintf(h->v.name, "** %s **", name);
    }
    h->v.target_speed = h->v.start_speed;
    h->v.speed = h->v.start_speed;
    r = radius(h);
    DynCube_Move(Game_GetWorld(), h, &h->v.pos2, r);
    h->v.matrix = h->v.matrix2;
    Vehicle_DeleteParts(h);
    Vehicle_NewParts(h);
    BROADCAST(h->v.coll->head, Param_GetChangedEvent(), NULL);
    VarString_Bind(&h->rotor_expr, h);
    VarString_Bind(&h->light_expr, h);
    VarString_Bind(&h->engine_expr, h);
    h->lights = 0;
}
HOOK(0x432950, Heli_ParamsChanged);

void Heli_GetSoundParams(Heli *h, void **out)
{
    out[1] = NULL;
    out[0] = &h->v.pos;
    out[2] = h->sound_params;
}
HOOK(0x432BB0, Heli_GetSoundParams);

/* the vehicles' saved state is 0x78 bytes; the car uses this one too */
void Heli_GetStateSize(Heli *h, int *out)
{
    (void)h;
    *out = 0x78;
}
HOOK(0x432BD0, Heli_GetStateSize);

void Heli_GetControlInfo(Heli *h, int *arg)
{
    int i = arg[0];

    if (i >= 0 && i < 5) {
        ((ControlInfo **)arg)[1] = &gHeliControls[i];
        return;
    }
    arg[1] = 0;
}
HOOK(0x432BE0, Heli_GetControlInfo);

#define DOWN(k) (gKeys[k] & 0x80)
#define PRESSED(k) (DOWN(k) && !(gKeysPrev[k] & 0x80))

/* controls: pitch, roll, yaw, throttle, pilot animation */
void Heli_ReadKeys(Heli *h, float **arg)
{
    static const float throttle[10] = {0.84f, 0.88f, 0.92f, 0.96f, 1.0f, 1.04f, 1.08f, 1.12f, 1.16f, 1.2f};
    float *out = *arg;
    int i;

    out[1] = 0;
    out[0] = 0;
    if (!DOWN(0x9D) && !DOWN(0x1D)) {
        out[1] = (gKeys[0xCD] >> 7) - (gKeys[0xCB] >> 7);
        out[0] = (gKeys[0xD0] >> 7) - (gKeys[0xC8] >> 7);
    }
    out[2] = (gKeys[0x2D] >> 7) - (gKeys[0x2C] >> 7);
    out[3] = -1.0f;
    if (!DOWN(0x1D) && !DOWN(0x9D)) {
        if (DOWN(0x29))
            out[3] = 0;
        for (i = 2; i <= 0xB; i++)
            if (DOWN(i))
                out[3] = throttle[i - 2];
        if (PRESSED(0x31)) {
            long double v = h->v.target_speed - kSpeedStep;

            if (kAccelSpeed > v)
                v = kAccelSpeed;
            out[3] = v;
        }
        if (PRESSED(0x32)) {
            long double v = h->v.target_speed + kSpeedStep;

            if (kMaxThrottle < v)
                v = kMaxThrottle;
            out[3] = v;
        }
    }
    out[4] = -2.0f;
    /* A with ` or 1 to 0 picks a pilot animation */
    if (DOWN(0x1E)) {
        if (PRESSED(0x29))
            out[4] = -1.0f;
        for (i = 2; i <= 0xB; i++)
            if (PRESSED(i))
                out[4] = i < 0xB ? i - 1 : 0;
    }
}
HOOK(0x432C10, Heli_ReadKeys);
