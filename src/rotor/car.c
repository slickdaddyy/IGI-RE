#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "rotor/rotor.h"

FN_EXTERN(Car_Blast);
FN_EXTERN(Car_CollFilter);
FN_EXTERN(Car_Explode);
FN_EXTERN(Car_GetControlInfo);
FN_EXTERN(Car_GetFrame);
FN_EXTERN(Car_GetLight);
FN_EXTERN(Car_GetSoundParams);
FN_EXTERN(Car_LoadState);
FN_EXTERN(Car_OnWorldEvent1);
FN_EXTERN(Car_ParamsChanged);
FN_EXTERN(Car_ReadKeys);
FN_EXTERN(Car_RegisterSymbols);
FN_EXTERN(Car_RemoveSymbols);
FN_EXTERN(Car_SaveState);
FN_EXTERN(Heli_GetStateSize);
FN_EXTERN(Task_YesHandler);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401780, void, Task_DeleteChildren, (void *task));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401BE0, Task *, Task_FindChild, (Task *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (Task *task, uint16_t type));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int point));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416890, uint8_t, Game_GetEvent14, (void));
ORIG(0x4168D0, void *, Game_GetTaskList3, (void));
ORIG(0x4168E0, void *, Game_GetTaskList4, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x426D70, uint16_t, CarDoor_GetType, (void));
ORIG(0x426D80, void, CarDoor_SetOpen, (Task *door, int open));
ORIG(0x430D10, void, Car_UpdateWheel, (Car *o, int wheel, int joint));
ORIG(0x42FC80, void, Car_OnWorldEvent1, (Car *o, void **world));
ORIG(0x42FDC0, uint16_t, Car_GetType, (void));
ORIG(0x464250, int, Human_GetMaskType, (void));
ORIG(0x46AB20, uint8_t, ComputerMap_GetSelectEvent, (void));
ORIG(0x470400, uint16_t, AIStationaryGun_GetType, (void));
ORIG(0x480FE0, void, Explosion_Spawn, (Vec3d *pos, int a, int b, float c, float d, float e, float f, int g, int h,
                                       int i, const char *sprite, const char *sound, int j, int k, float l, int m));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x489630, uint8_t, AIDetection_GetEvent2, (void));
ORIG(0x4896C0, void, AIDetector_Init, (void *p, int a, int b, int c, int d, int e));
ORIG(0x489750, void, AIDetector_Update, (void *p, Vec3d *pos, Car *o, int a));
ORIG(0x489B90, void, AIDetection_RemoveRefs, (void *p));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *results, Vec3d *starts, Vec3d *ends, int count, char unk,
                                  void *query, unsigned int flags));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *o, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *o, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *o, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *o));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4ECF50, void, Body_AddForce, (float *body, float *force));
ORIG(0x4E6AB0, float *, SoundParams_New, (int n));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4EC2C0, void, Body_AddAeroForces, (void *a, int n, float k, float *body, Matrix *m, float *v));
ORIG(0x4EE0E0, uint8_t, PhysicsObjType_GetParseEvent, (void));
ORIG(0x4EE0F0, uint8_t, PhysicsObjType_GetSizeEvent, (void));
ORIG(0x4F0EE0, int, Level_GetDynamic, (void));
ORIG(0x4F0F30, void *, Param_Degrees, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F60, void *, Param_Real32x3, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F3390, void, EditRigidObj_New, (int a, const char *name, Vec3d *pos, Matrix *m, float *scale, int *b));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4110, uint8_t, Vehicle_GetStateSizeEvent, (void));
ORIG(0x4F4120, uint8_t, Vehicle_GetSaveEvent, (void));
ORIG(0x4F4130, uint8_t, Vehicle_GetLoadEvent, (void));
ORIG(0x4F4140, uint8_t, Vehicle_GetKeysEvent, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4F4600, void, Vehicle_ResetControls, (Car *o));
ORIG(0x4F4640, void, Vehicle_Free, (Car *o));
ORIG(0x4F4660, void, Vehicle_RunParts, (Car *o));
ORIG(0x4F4680, void, Vehicle_Init, (Car *o));
ORIG(0x4F4820, void, Vehicle_DeleteParts, (Car *o));
ORIG(0x4F4850, uint8_t, Vehicle_GetExplodeEvent, (void));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (Car *o, MagicDef *def));
ORIG(0x4FC0E0, void, PhysicsObj_Collide, (Car *o, float a, float b, float c, void *world, int n, void *arg));
ORIG(0x4FC7B0, int, PhysicsObj_IsResting, (Car *o));

#define CAR_PARAMS_CHANGED (FN(0x42F530, Car_ParamsChanged))
#define CAR_HANDLER_42F850 (FN(0x42F850, Car_GetSoundParams))
#define CAR_HANDLER_42F870 (FN(0x42F870, Car_GetControlInfo))
#define CAR_HANDLER_42F8A0 (FN(0x42F8A0, Car_ReadKeys))
#define CAR_HANDLER_42FA60 (FN(0x42FA60, Car_LoadState))
#define CAR_HANDLER_42FB10 (FN(0x42FB10, Car_SaveState))
#define CAR_HANDLER_42FB40 (FN(0x42FB40, Car_Blast))
#define CAR_HANDLER_42FC00 (FN(0x42FC00, Car_GetFrame))
#define CAR_HANDLER_42FC40 (FN(0x42FC40, Car_Explode))
#define CAR_HANDLER_42FC50 (FN(0x42FC50, Car_CollFilter))
#define CAR_HANDLER_42FC70 (FN(0x42FC70, Car_GetLight))
#define CAR_HANDLER_42FC80 (FN(0x42FC80, Car_OnWorldEvent1))
#define CAR_HANDLER_42FCE0 (FN(0x42FCE0, Car_RegisterSymbols))
#define CAR_HANDLER_42FD40 (FN(0x42FD40, Car_RemoveSymbols))
#define CAR_HANDLER_432BD0 (FN(0x432BD0, Heli_GetStateSize))
#define TASK_HANDLER_4E4280 (FN(0x4E4280, Task_YesHandler))

extern uint16_t gCarType;
extern float gCarRange[];
extern void *gCarParams;

/* one result of Coll_Lines (0x70 bytes) */
typedef struct CollHit {
    char unk0[8];
    Vec3d pos;
    char unk20[0x18];
    float normal[3];
    char unk44[0x14];
    int found;
    char unk5C[4];
    double dist;
    char unk68[8];
} CollHit;

_Static_assert(sizeof(CollHit) == 0x70, "CollHit");

typedef struct CarControlArg {
    int unk0;
    float *input;
} CarControlArg;

F32(kMeter, 0x45800000);        /* 4096 */
F32(kBlastBig, 0x3E800000);     /* 0.25 */
F32(kBlastMid, 0x3D4CCCCD);     /* 0.05 */
F32(kBlastSmall, 0x3BA3D70A);   /* 0.005 */
F32(kOne, 0x3F800000);
F32(kZero, 0x00000000);
F32(kMinusOne, 0xBF800000);
F32(kStop, 0x3C23D70A);         /* 0.01 */
F32(kPitch, 0x3A83126F);        /* 0.001 */
F32(kDamp, 0x39AB7132);
F32(kLift, 0x4232674F);
F32(kGripB, 0x3F7851EC);
F32(kGripC, 0x3F666666);
F32(kWheelUp, 0x44CCCCCD);      /* 1638.4 */
F32(kSnap, 0x3A83126F);         /* 0.001 */
F32(kSmoothOld, 0x3D4CCCCD);    /* 0.05 */
F32(kSmoothNew, 0x3F733333);    /* 0.95 */
F32(kTwo, 0x40000000);
static const double kEffectUp = 8192.0;
static const double kHalfMeter = 2048.0;
static const double kMeterD = 4096.0;
static const double kInvMeter = 1.0 / 4096.0;
static const double kOneD = 1.0;

static float car_radius(Car *o)
{
    float s = max_scale(o->scale);

    return (float)(VirModel_GetRadius(o->model) * s);
}

static CarData *car_data(Car *o)
{
    return o->def->data;
}

void Car_ParseDef(void *task, DefParseArg *a);
void Car_GetDefSize(void *unused, int *size);

void Car_RegisterDefHandlers(void)
{
    uint8_t ev = PhysicsObjType_GetParseEvent();

    TaskType_SetHandler(Car_GetType(), ev, Car_ParseDef);
    ev = PhysicsObjType_GetSizeEvent();
    TaskType_SetHandler(Car_GetType(), ev, Car_GetDefSize);
}
HOOK(0x42DAC0, Car_RegisterDefHandlers);

void Car_ParseDef(void *task, DefParseArg *a)
{
    CarData *d = a->out;
    void *args = a->args;
    uint8_t ev = PhysicsObjType_GetParseEvent();
    int i;

    gTaskHandlers[ev][Vehicle_GetType()](task, a);
    d->wheelCount = Script_GetArgInt(args, a->index++);
    for (i = 0; i < d->wheelCount; i++) {
        CarWheel *w = &d->wheels[i];

        w->unk0 = Script_GetArgInt(args, a->index++);
        w->unk1 = Script_GetArgInt(args, a->index++);
        w->unk4 = (float)Script_GetArgReal(args, a->index++);
        w->joint = Script_GetArgInt(args, a->index++);
        w->radius = (float)(Script_GetArgReal(args, a->index++) * kMeter.f);
        w->unk10 = (float)Script_GetArgReal(args, a->index++);
    }
    d->wheelDrop = (float)(Script_GetArgReal(args, a->index++) * kMeter.f);
    d->unk260 = Script_GetArgInt(args, a->index++);
    d->unk261 = Script_GetArgInt(args, a->index++);
    Script_GetArgString(args, a->index++, d->engineSound, 0x20);
    d->explodeCount = Script_GetArgInt(args, a->index++);
    for (i = 0; i < d->explodeCount; i++)
        d->explodePoints[i] = Script_GetArgInt(args, a->index++);
    Script_GetArgString(args, a->index++, d->explodeObj, 0x10);
}
HOOK(0x42DB00, Car_ParseDef);

void Car_GetDefSize(void *unused, int *size)
{
    (void)unused;
    *size = sizeof(CarData);
}
HOOK(0x42DCE0, Car_GetDefSize);

void Car_Create(Car *o);
void Car_Run(Car *o);
void Car_Delete(Car *o);
void Car_GetParamTable(Car *o, void **out);
void Car_Control(Car *o, CarControlArg *arg);

void Car_Init(void)
{
    Car c;

    gCarType = TaskType_New("Car", sizeof(Car), Vehicle_GetType(), 0);
    TaskType_SetHandler(gCarType, 0, Car_Create);
    TaskType_SetHandler2(gCarType, 1, Car_Run);
    TaskType_SetHandler2(gCarType, 2, Car_Delete);
    TaskType_SetHandler(gCarType, Param_GetTableEvent(), Car_GetParamTable);
    TaskType_SetHandler(gCarType, Param_GetChangedEvent(), CAR_PARAMS_CHANGED);
    TaskType_SetHandler(gCarType, Sound_GetInfoEvent(), CAR_HANDLER_42F850);
    TaskType_SetHandler(gCarType, Game_GetEvent11(), CAR_HANDLER_42FB40);
    TaskType_SetHandler(gCarType, Game_GetEvent14(), CAR_HANDLER_42FC00);
    TaskType_SetHandler(gCarType, Vehicle_GetControlEvent(), Car_Control);
    TaskType_SetHandler(gCarType, Vehicle_GetInputInfoEvent(), CAR_HANDLER_42F870);
    TaskType_SetHandler(gCarType, Vehicle_GetStateSizeEvent(), CAR_HANDLER_432BD0);
    TaskType_SetHandler(gCarType, Vehicle_GetKeysEvent(), CAR_HANDLER_42F8A0);
    TaskType_SetHandler(gCarType, Vehicle_GetLoadEvent(), CAR_HANDLER_42FA60);
    TaskType_SetHandler(gCarType, Vehicle_GetSaveEvent(), CAR_HANDLER_42FB10);
    TaskType_SetHandler(gCarType, AIDetection_GetEvent0(), CAR_HANDLER_42FC50);
    TaskType_SetHandler(gCarType, Vehicle_GetExplodeEvent(), CAR_HANDLER_42FC40);
    TaskType_SetHandler(gCarType, ComputerMap_GetSelectEvent(), TASK_HANDLER_4E4280);
    TaskType_SetHandler(gCarType, AIDetection_GetEvent2(), CAR_HANDLER_42FC70);
    TaskType_SetHandler(gCarType, World_GetEvent1(), CAR_HANDLER_42FC80);
    TaskType_SetHandler2(gCarType, Level_GetEventDA8(), CAR_HANDLER_42FCE0);
    TaskType_SetHandler2(gCarType, Level_GetEventDAC(), CAR_HANDLER_42FD40);
    gCarParams = ParamTable_New(13);
    gCarRange[0] = 0;
    gCarRange[1] = 1000;
    ParamTable_Add(gCarParams, &c, &c.spawnPos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.spawnMatrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.maxThrottle, Param_Real32(), 7, NULL);
    ParamTable_Add(gCarParams, &c, c.unk180, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gCarParams, &c, c.name, Param_String16(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.unk6D8, Param_Bool8(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.unk6D9, Param_Bool8(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.doorExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.unk978, Param_VarString(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.unk9C8, Param_VarString(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.unk2EC0, Param_Degrees(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.unk2EC4, Param_Degrees(), 7, NULL);
    ParamTable_Add(gCarParams, &c, &c.unk2EC8, Param_Real32(), 7, gCarRange);
    Script_RegisterConstant("TASKTYPE_CAR", gCarType);
}
HOOK(0x42DCF0, Car_Init);

void Car_Reset(Car *o);

void Car_Create(Car *o)
{
    Car_Reset(o);
    QTaskList_Add(Game_GetTaskList3(), o);
}
HOOK(0x42E160, Car_Create);

void Car_Reset(Car *o)
{
    Vec3d zero = {0, 0, 0};
    float radius;

    RigidDynCubeObj_Init(Game_GetWorld(), o, NULL);
    radius = car_radius(o);
    DynCube_Add(Game_GetWorld(), o, &zero, radius, 2);
    o->engineParams = SoundParams_New(3);
    o->engineSound = NULL;
    o->name[0] = 0;
    o->unk754 = 0;
    o->unk750 = 0;
    o->unk758 = 0;
    Vehicle_Init(o);
    o->maxThrottle = 0;
    o->unk91D = 0;
    o->lastTick = -1;
    QTaskList_Add(Game_GetTaskList4(), o);
    o->exploding = 0;
    o->wrecked = 0;
    o->explodeTimer = -1;
    VarString_Init(&o->doorExpr);
    VarString_Init(&o->unk978);
    VarString_Init(&o->unk9C8);
    o->unk2EC0 = 180;
    o->unk2EC4 = 360;
    o->unk2EC8 = 30;
    o->unk2ECC = 1;
    o->unk2ED0 = 0;
    o->unk2ED4 = 0;
}
HOOK(0x42E180, Car_Reset);

void Car_UpdateDoors(Car *o);
void Car_UpdateDetector(Car *o);
void Car_UpdateExplosion(Car *o);

void Car_Run(Car *o)
{
    if (o->lastTick == Flow_GetFrames())
        return;
    o->lastTick = Flow_GetFrames();
    if (o->wrecked)
        return;
    if (VarString_HasExpr(&o->unk9C8) && (int)x87_round(VarString_EvalReal(&o->unk9C8, o)) == 0) {
        if (o->engineSound) {
            Task_Delete(o->engineSound);
            o->engineSound = NULL;
        }
    } else if (!o->engineSound && o->def) {
        Task *s = Sound_Play(o, car_data(o)->engineSound);

        if (s)
            o->engineSound = s;
    }
    Car_UpdateDoors(o);
    Car_UpdateDetector(o);
    RUN_CHILDREN(&o->task);
    if (o->parts)
        RUN_CHILDREN(o->parts);
    if (!o->unk2ECC && o->parts) {
        CarMoveArg arg;
        Task *c;

        arg.pos = &o->pos;
        arg.matrix = &o->matrix;
        arg.car = o;
        EACH_CHILD(o->parts, MagicObj_GetMoveEvent(), &arg);
        for (c = Task_FindChild(o->parts, AIStationaryGun_GetType()); c;
             c = Task_FindNext(c, AIStationaryGun_GetType()))
            ((void (*)(void *))HANDLER(1, c->type))(c);
    }
    o->unk2ECC = 0;
    o->unk2ED0 = 0;
    Car_UpdateExplosion(o);
}
HOOK(0x42E320, Car_Run);

void Car_UpdateExplosion(Car *o)
{
    float radius;
    CarData *d;
    int n;

    if (!o->exploding)
        return;
    radius = car_radius(o);
    d = car_data(o);
    n = d->explodeCount;
    if (o->explodeTimer == -1) {
        if (n) {
            if (n > 0) {
                int *pt = d->explodePoints;
                int left;

                for (left = n; left; left--, pt++) {
                    int r = Random_Int(Game_GetRandom(), 30);
                    Vec3d v;

                    Model_GetPoint(&v, o, *pt);
                    v.x = v.x + o->pos.x;
                    v.y = v.y + o->pos.y;
                    v.z = v.z + o->pos.z;
                    Explosion_Spawn(&v, 1, r, 1.0f, 12288.0f, 3072.0f, 3072.0f, 3, 2, 0, "1009_01_1", "explo_03_l",
                                    0x15, 2, 102400.0f, 0);
                }
            }
            o->explodeTimer = 30;
            return;
        }
        Explosion_Spawn(&o->pos, 1, 0, 1.0f, (float)(radius * kBlastBig.f), (float)(radius * kBlastMid.f),
                        (float)(radius * kBlastSmall.f), 3, 1, 0, "1009_01_1", "explo_03_l", 0x15, 2, 102400.0f, 0);
        o->explodeTimer = 10;
        return;
    }
    if (--o->explodeTimer == 0) {
        float scale[3];
        int zero[3];
        void *world;

        zero[0] = zero[1] = zero[2] = 0;
        scale[0] = scale[1] = scale[2] = 1.0f;
        EditRigidObj_New(Level_GetDynamic(), d->explodeObj, &o->pos, &o->matrix, scale, zero);
        world = Game_GetWorld();
        Task_DeleteChildren(o);
        o->engineSound = NULL;
        Vehicle_DeleteParts(o);
        Car_OnWorldEvent1(o, &world);
        o->wrecked = 1;
    }
}
HOOK(0x42E5A0, Car_UpdateExplosion);

void Car_UpdateDoors(Car *o)
{
    if (VarString_HasExpr(&o->doorExpr)) {
        int open = (int)x87_round(VarString_EvalReal(&o->doorExpr, o));
        Task *door = Task_FindChild(o->parts, CarDoor_GetType());

        if (door)
            CarDoor_SetOpen(door, open);
    }
}
HOOK(0x42E810, Car_UpdateDoors);

void Car_UpdateDetector(Car *o)
{
    if (VarString_HasExpr(&o->unk978)) {
        char was = o->unk91D;

        o->unk91D = (char)x87_round(VarString_EvalReal(&o->unk978, o));
        if (o->unk91D) {
            o->unkA18 = o->pos;
            o->unkA18.z = o->unkA18.z + kEffectUp;
            o->unkA30 = o->matrix;
            o->unkA88 = 0;
            AIDetector_Update(o->unkA90, &o->unkA18, o, 1);
        }
        if (!o->unk91D && was) {
            AIDetection_RemoveRefs(o->unkA90);
            AIDetector_Init(o->unkA90, 1, 4, Human_GetMaskType(), 4, 8);
        }
    }
}
HOOK(0x42E870, Car_UpdateDetector);

void Car_Delete(Car *o)
{
    Vehicle_Free(o);
    if (!o->wrecked)
        DynCube_Remove(Game_GetWorld(), o);
    QTaskList_Remove(Game_GetTaskList3(), o);
    if (o->engineParams)
        QTaskList_Free(o->engineParams);
    if (Task_GetId(o) >= 0) {
        TaskFn fn = ((TaskFn *)gTaskHandlers)[Level_GetEventDAC() * 0x180 + o->task.type];

        if (fn)
            ((void (*)(void *))fn)(o);
    }
    VarString_Free(&o->doorExpr);
    VarString_Free(&o->unk978);
    VarString_Free(&o->unk9C8);
    QTaskList_Remove(Game_GetTaskList4(), o);
    AIDetection_RemoveRefs(o->unkA90);
}
HOOK(0x42E940, Car_Delete);

void Car_GetParamTable(Car *o, void **out)
{
    (void)o;
    *out = gCarParams;
}
HOOK(0x42EA00, Car_GetParamTable);

void Car_ApplyWheelDrop(Car *o);
void Car_Drive(Car *o);
void Car_UpdateWheels(Car *o);

void Car_Control(Car *o, CarControlArg *arg)
{
    float *in = arg->input;
    float radius;

    if (*(uint32_t *)&in[1] != 0xBF800000)
        o->targetThrottle = in[1];
    o->targetSteer = arg->input[0];
    o->unk744 = arg->input[3] < kMinusOne.f || arg->input[3] > kMinusOne.f;
    if (arg->unk0) {
        Vehicle_RunParts(o);
        o->unk2ECC = 1;
        Car_Drive(o);
        return;
    }
    radius = car_radius(o);
    DynCube_Move(Game_GetWorld(), o, &o->spawnPos, radius);
    o->matrix = o->spawnMatrix;
    *(uint32_t *)&arg->input[1] = 0xBF800000;
    *(uint32_t *)&arg->input[2] = 0xC0000000;
    PhysicsObj_SetPhysType(o, o->def);
    Vehicle_ResetControls(o);
    Car_ApplyWheelDrop(o);
    o->throttle = o->maxThrottle;
    o->targetThrottle = o->maxThrottle;
    o->exploding = 0;
    o->wrecked = 0;
    o->steer = 0;
    o->targetSteer = 0;
    if (o->parts) {
        CarMoveArg m;

        m.pos = &o->pos;
        m.matrix = &o->matrix;
        m.car = o;
        EACH_CHILD(o->parts, MagicObj_GetMoveEvent(), &m);
    }
}
HOOK(0x42EA10, Car_Control);

void Car_ApplyWheelDrop(Car *o)
{
    CarData *d;
    int i;

    if (!o->def)
        return;
    d = car_data(o);
    for (i = 0; i < d->wheelCount; i++) {
        int j = d->wheels[i].joint;

        if (j >= 0)
            o->joints[j].pos[2] = o->joints[j].pos[2] - d->wheelDrop;
    }
}
HOOK(0x42EC30, Car_ApplyWheelDrop);

void Car_Drive(Car *o)
{
    CarData *d = car_data(o);
    struct {
        int unk0;
        Car *car;
    } ctx;

    o->throttle = (float)((kOne.f - d->throttleLag) * o->targetThrottle + (long double)d->throttleLag * o->throttle);
    d = car_data(o);
    o->steer = (float)((kOne.f - d->steerLag) * o->targetSteer + (long double)o->steer * d->steerLag);
    if (o->targetThrottle == kZero.f) {
        float t = o->throttle;

        if (t < kZero.f)
            t = -t;
        if (t < kStop.f)
            o->throttle = 0;
    }
    Car_UpdateWheels(o);
    if (!PhysicsObj_IsResting(o)) {
        ctx.car = o;
        if (car_data(o)->unk28) {
            float v[3];

            v[0] = -o->spin[0];
            v[1] = -o->spin[1];
            v[2] = -o->spin[2];
            d = car_data(o);
            Body_AddAeroForces(d->unk2C, d->unk28, kDamp.f, o->body, &o->matrix, v);
        }
        PhysicsObj_Collide(o, kLift.f, kGripB.f, kGripC.f, Game_GetWorld(), 0xC, &ctx);
    }
    if (o->engineParams) {
        long double p;

        o->engineParams[1] = o->throttle;
        o->engineParams[2] = o->throttle > kStop.f ? 1 : 0;
        p = (long double)o->body[2] * kPitch.f;
        o->engineParams[3] = p < kOne.f ? (float)p : kOne.f;
    }
    if (o->parts) {
        CarMoveArg m;

        m.pos = &o->pos;
        m.matrix = &o->matrix;
        m.car = o;
        EACH_CHILD(o->parts, MagicObj_GetMoveEvent(), &m);
    }
}
HOOK(0x42ECA0, Car_Drive);

/* casts a ray down from each wheel, pushes the body up where a wheel
   touches the ground and lifts the car out when one is sunk in */
void Car_UpdateWheels(Car *o)
{
    struct {
        int unk0;
        Car *self;
    } q;
    MagicDef *def = o->def;
    CarData *d = def->data;
    Vec3d starts[16], ends[16];
    int wheelOf[16];
    CollHit hits[16];
    CarJoint *joints = o->joints;
    CarContact *ct = o->contacts;
    double maxDepth = -0x1.fffffffffffffp+1023;
    int n = 0, any = 0, i;

    for (i = 0; i < d->wheelCount; i++, ct++) {
        CarWheel *w = &d->wheels[i];
        int j = w->joint;

        if (j >= 0 && ct->active) {
            CarJoint *jp = &joints[j];
            long double reach = (long double)w->radius + kHalfMeter;
            const float *m = o->matrix.m;
            Vec3d *s = &starts[n], *e = &ends[n], v;
            long double z;

            s->x = jp->pos[0];
            s->y = jp->pos[1];
            s->z = jp->pos[2];
            v.x = (long double)m[0] * s->x + (long double)m[2] * jp->pos[2] + (long double)m[1] * s->y;
            v.y = (long double)m[3] * s->x + (long double)m[5] * jp->pos[2] + (long double)m[4] * s->y;
            v.z = (long double)m[6] * s->x + (long double)m[8] * jp->pos[2] + (long double)m[7] * s->y;
            *s = v;
            s->x = o->pos.x + s->x;
            s->y = o->pos.y + s->y;
            z = (long double)o->pos.z + s->z;
            s->z = (long double)w->radius + z + kMeterD;
            *e = *s;
            e->z = (long double)e->z - reach - kMeterD;
            jp->unk18 = 0;
            wheelOf[n++] = i;
        }
        if (!ct->active)
            joints[j].unk18 = 1;
    }
    q.self = o;
    Coll_Lines(Game_GetWorld(), hits, starts, ends, n, 0xC, &q, 0xFFFFFFFF);
    if (n <= 0)
        return;
    for (i = 0; i < n; i++) {
        int w = wheelOf[i];
        CollHit *h = &hits[i];
        float radius = d->wheels[w].radius;
        int j = d->wheels[w].joint;
        CarJoint *jp;
        float push = 0;
        int sink = 0, hit = 0;
        long double diff;

        if (h->found) {
            if (!(h->dist >= kMeterD)) {
                long double depth = kMeterD - h->dist;

                push = radius;
                sink = any = hit = 1;
                if (maxDepth < depth)
                    maxDepth = depth;
            } else if (h->dist > kMeterD) {
                long double top = (long double)radius + kMeter.f;

                if (top > h->dist) {
                    hit = 1;
                    push = (float)(top - h->dist);
                }
            }
            if (hit) {
                float force[6];

                force[0] = 0;
                force[1] = 0;
                force[2] = (float)((long double)push / radius * d->wheels[w].unk10 * def->power * kLift.f);
                if (sink)
                    force[2] = (float)((kTwo.f - h->dist * kInvMeter) * force[2]);
                force[3] = joints[j].pos[0];
                force[4] = joints[j].pos[1];
                force[5] = joints[j].pos[2];
                Body_AddForce(o->body, force);
            }
        }
        jp = &joints[j];
        diff = (long double)jp->unk34 - push;
        if (diff < 0)
            diff = -diff;
        if (diff > kSnap.f) {
            if (push > jp->unk34)
                jp->unk34 = push;
            else
                jp->unk34 = (float)((long double)jp->unk34 * kSmoothOld.f + (long double)push * kSmoothNew.f);
        }
        ct = &o->contacts[w];
        ct->hit = hit;
        jp->unk1A = hit;
        if (hit) {
            ct->normal[0] = h->normal[0];
            ct->normal[1] = h->normal[1];
            ct->normal[2] = h->normal[2];
            if (ct->normal[0] != 0 || ct->normal[1] != 0 || ct->normal[2] != 0) {
                long double x = ct->normal[0], y = ct->normal[1], z = ct->normal[2];
                long double inv = kOneD / x87_sqrt(x * x + y * y + z * z);

                ct->normal[0] = (float)(ct->normal[0] * inv);
                ct->normal[1] = (float)(ct->normal[1] * inv);
                ct->normal[2] = (float)(ct->normal[2] * inv);
            }
            ct->pos[0] = jp->pos[0];
            ct->pos[1] = jp->pos[1];
            ct->pos[2] = jp->pos[2];
            ct->pos[2] = (float)(ct->pos[2] + (long double)kWheelUp.f);
            o->unk1B8 = 1;
            Car_UpdateWheel(o, w, j);
        }
    }
    if (any) {
        Vec3d p = o->pos;
        float radius;

        p.z = p.z + maxDepth;
        radius = car_radius(o);
        DynCube_Move(Game_GetWorld(), o, &p, radius);
    }
}
HOOK(0x42EF00, Car_UpdateWheels);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53BDA8) uint16_t gCarType INIT(385);
PAD(igd, 53BDA8, 53BDAA, 0x2);
STORE(igb, 57BD20, 4) float store_57BD20[2];
ALIAS(gCarRange, store_57BD20, 0);
BSS(57BD20) void *gCarParams;
