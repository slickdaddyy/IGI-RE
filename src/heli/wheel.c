#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "heli/heli.h"

FN_EXTERN(Rotor_FreeDef);
FN_EXTERN(Task_Nothing);
FN_EXTERN(VehiclePart_CollFilter);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4B33A0, void, Matrix_RotateEuler, (Matrix *out, const Matrix *m, float ax, float ay, float az));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (int unk, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (int unk, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (int unk, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4ECF80, void, Body_GetPointVelocity, (Vec3f *out, void *body, Vec3f *point));
ORIG(0x4EE0E0, uint8_t, PhysicsObjType_GetParseEvent, (void));
ORIG(0x4EE0F0, uint8_t, PhysicsObjType_GetSizeEvent, (void));
ORIG(0x4EE100, uint8_t, PhysicsObjType_GetFreeEvent, (void));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));

uint16_t Car_GetType(void);
long double Car_GetTrackFactor(Car *c, int joint);
void Car_LiftJoint(Car *c, int joint, float amount, float max);

extern uint16_t gWheelType;

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))
#define ROTOR_FREE_DEF (FN(0x42DA70, Rotor_FreeDef))
#define VEHICLEPART_COLL_FILTER (FN(0x426D30, VehiclePart_CollFilter))

static float radius(Wheel *w)
{
    float s = max_scale(w->scale);

    return VirModel_GetRadius(w->model) * s;
}

void Wheel_ParseDef(void *unused, ParseArg *a);
void Wheel_GetDefSize(void *unused, int *size);
void Wheel_Create(Wheel *w, PartCreateArg *a);
void Wheel_Delete(Wheel *w);
void Wheel_Run(Wheel *w);
void Wheel_Move(Wheel *w, PartMoveArg *a);
uint16_t Wheel_GetType(void);

void Wheel_RegisterDefHandlers(void)
{
    uint8_t slot;

    slot = PhysicsObjType_GetParseEvent();
    TaskType_SetHandler(Wheel_GetType(), slot, Wheel_ParseDef);
    slot = PhysicsObjType_GetSizeEvent();
    TaskType_SetHandler(Wheel_GetType(), slot, Wheel_GetDefSize);
}
HOOK(0x42FE40, Wheel_RegisterDefHandlers);

/* model name, radius, travel */
void Wheel_ParseDef(void *unused, ParseArg *a)
{
    WheelConfig *d = a->out;
    void *args = a->args;

    (void)unused;
    Script_GetArgString(args, a->index++, d->name, sizeof(d->name));
    d->radius = Script_GetArgReal(args, a->index++) * kFixed;
    d->unk2EC = Script_GetArgReal(args, a->index++) * kFixed;
}
HOOK(0x42FE80, Wheel_ParseDef);

void Wheel_GetDefSize(void *unused, int *size)
{
    (void)unused;
    *size = sizeof(WheelConfig);
}
HOOK(0x42FEF0, Wheel_GetDefSize);

void Wheel_Init(void)
{
    uint8_t slot;

    gWheelType = TaskType_New("Wheel", sizeof(Wheel), MagicObj_GetType(), 0);
    TaskType_SetHandler(gWheelType, 0, Wheel_Create);
    TaskType_SetHandler2(gWheelType, 1, Wheel_Run);
    TaskType_SetHandler2(gWheelType, 2, Wheel_Delete);
    slot = MagicObj_GetMoveEvent();
    TaskType_SetHandler(gWheelType, slot, Wheel_Move);
    slot = Game_GetEvent9();
    TaskType_SetHandler(gWheelType, slot, TASK_NOTHING);
    slot = Game_GetEvent11();
    TaskType_SetHandler(gWheelType, slot, TASK_NOTHING);
    slot = PhysicsObjType_GetFreeEvent();
    TaskType_SetHandler(gWheelType, slot, ROTOR_FREE_DEF);
    slot = AIDetection_GetEvent0();
    TaskType_SetHandler(gWheelType, slot, VEHICLEPART_COLL_FILTER);
    Script_RegisterConstant("TASKTYPE_WHEEL", gWheelType);
}
HOOK(0x42FF00, Wheel_Init);

void Wheel_Create(Wheel *w, PartCreateArg *a)
{
    void **model = VirModel_Find(a->model);
    Car *car;

    if (!model) {
        Error_Show("VirModel \"%s\" not available", a->model);
        for (;;)
            ;
    }
    w->offset = a->offset;
    w->local = a->local;
    RigidDynCubeObj_Init(a->unk4, w, model);
    DynCube_Add(a->unk4, w, &a->pos, radius(w), 2);
    w->matrix = a->matrix;
    w->phys = PhysicsObjType_Find(a->model);
    w->lift = 0;
    w->spin = 0;
    w->steer = 0;
    w->spin_speed = 0;
    car = (Car *)a->owner;
    w->owner = car;
    if (car->v.task.type == Car_GetType()) {
        w->index = car->wheel_slots;
        car->wheel_slots++;
        car->contacts[w->index].active = 1;
    }
    w->dead = 0;
}
HOOK(0x430000, Wheel_Create);

void Wheel_Delete(Wheel *w)
{
    Car *car = w->owner;

    if (car->v.task.type == Car_GetType()) {
        car->wheel_slots--;
        car->contacts[w->index].active = 0;
    }
    DynCube_Remove(w->unk68, w);
}
HOOK(0x430170, Wheel_Delete);

/* spins the wheel with the ground or the engine, steers it and pushes its joint up */
void Wheel_Run(Wheel *w)
{
    Car *owner = w->owner;
    uint16_t vehicle = Vehicle_GetType();
    Car *car;

    if (!TaskType_IsA(w->owner->v.task.type, vehicle))
        return;
    if (w->dead) {
        Task_Delete(w);
        return;
    }
    car = w->owner;
    if (car->v.task.type == Car_GetType()) {
        WheelConfig *wc = w->phys->data;
        CarConfig *cfg = car->v.phys->data;
        CarWheelConfig *c;
        int j = cfg->wheels[w->index].joint;
        float v = 0;

        if (j < 0)
            j = -10 - j;
        if (car->v.joints[j].touching) {
            Vec3f d;
            long double l;

            Body_GetPointVelocity(&d, &car->v.body, &car->v.joints[j].pos);
            l = x87_sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            v = l;
            if (l < kGripLimit)
                v = 0;
        } else if (cfg->wheels[w->index].driven) {
            CarConfig *oc = owner->v.phys->data;

            v = owner->v.speed * kThrustScale * oc->v.thrust / car->driven_count * kSixTenths;
        }
        if (cfg->tank)
            v = Car_GetTrackFactor(car, j) * v;
        c = &cfg->wheels[w->index];
        w->spin_speed = v / (wc->radius * kTwoPiD) * kTwoPiD * kThreeTenths + w->spin_speed * kSevenTenths;
        if (c->steered)
            w->steer = x87_neg(c->steer * owner->v.turn[2] * kDegToRad);
        if (fne(w->lift, kZero) && c->joint >= 0) {
            wc = w->phys->data;
            Car_LiftJoint(car, c->joint, w->lift * wc->unk2EC, wc->unk2EC);
        }
    } else {
        w->spin_speed = owner->v.speed;
    }
    if (owner->v.brake)
        w->spin_speed = 0;
    if (fne(w->lift, kZero)) {
        w->spin_speed = 0;
        w->spin = 0;
    }
    {
        /* not folded: after the zeroing above, 0 - 0 is -0 in round-down mode */
        long double spin = w->spin;

        __asm__("" : "+t"(spin));
        w->spin = spin - w->spin_speed;
    }
    BROADCAST(w->task.head, 1);
}
HOOK(0x4301C0, Wheel_Run);

/* follows the car: offset in the car's frame, the wheel resting on its joint */
void Wheel_Move(Wheel *w, PartMoveArg *a)
{
    Vec3d off = w->offset, p, np;
    Car *car = w->owner;
    const float *m;
    long double z;
    Matrix r, nm;
    float rad;
    int i;

    if (car->v.task.type == Car_GetType()) {
        CarConfig *cfg = car->v.phys->data;
        int j = cfg->wheels[w->index].joint;

        if (j < 0)
            j = -10 - j;
        z = ((Car *)a->vehicle)->v.joints[j].height + off.z - cfg->wheel_drop;
    } else {
        z = off.z;
    }
    m = a->matrix->m;
    p.x = m[1] * off.y + m[2] * z + m[0] * off.x;
    p.y = m[4] * off.y + m[3] * off.x + m[5] * z;
    p.z = m[7] * off.y + m[6] * off.x + m[8] * z;
    np.x = p.x + a->pos->x;
    np.y = p.y + a->pos->y;
    np.z = p.z + a->pos->z;
    rad = radius(w);
    DynCube_Move(Game_GetWorld(), w, &np, rad);
    Matrix_RotateEuler(&r, &w->local, w->spin, 0, w->steer);
    m = a->matrix->m;
    for (i = 0; i < 3; i++) {
        nm.m[i] = r.m[3 + i] * m[1] + r.m[6 + i] * m[2] + r.m[i] * m[0];
        nm.m[3 + i] = r.m[i] * m[3] + r.m[6 + i] * m[5] + r.m[3 + i] * m[4];
        nm.m[6 + i] = r.m[i] * m[6] + r.m[6 + i] * m[8] + r.m[3 + i] * m[7];
    }
    nm.count = a->matrix->count + r.count + 1;
    w->matrix = nm;
}
HOOK(0x430640, Wheel_Move);

void Wheel_Close(void)
{
    TaskType_Free(gWheelType);
}
HOOK(0x430940, Wheel_Close);

uint16_t Wheel_GetType(void)
{
    return gWheelType;
}
HOOK(0x430950, Wheel_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BD30) uint16_t gWheelType;
PAD(igb, 57BD30, 57BD32, 0x2);
