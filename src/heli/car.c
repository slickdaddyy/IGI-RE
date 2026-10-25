#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "heli/heli.h"
#include "math/mathvars.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401BE0, Task *, Task_FindChild, (Task *task, uint16_t type));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int point));
ORIG(0x414ED0, void, Model_GetLocalPoint, (Vec3f *out, void *obj, int point));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (void *blast, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x42EC30, void, Car_ApplyWheelDrop, (Car *c));
ORIG(0x464250, int, Human_GetMaskType, (int a, int b));
ORIG(0x475300, uint16_t, StationaryGun_GetType, (void));
ORIG(0x4896C0, void, AIDetector_Init, (void *light, int a, int b, int c, int d, int e));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4ECF50, void, Body_AddForce, (void *body, float *force));
ORIG(0x4ECF80, void, Body_GetPointVelocity, (Vec3f *out, void *body, Vec3f *point));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4F4600, void, Vehicle_ResetControls, (void *v));
ORIG(0x4F46E0, void, Vehicle_SaveState, (void *v, void **arg));
ORIG(0x4F4780, void, Vehicle_LoadState, (void *v, void **arg));
ORIG(0x4F4800, void, Vehicle_NewParts, (void *v));
ORIG(0x4F4820, void, Vehicle_DeleteParts, (void *v));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *v, PhysType *phys));

extern uint16_t gCarType;
extern void *gCarParams;
extern ControlInfo gCarControls[];

static float radius(void *obj)
{
    Vehicle *v = obj;
    float s = max_scale(v->scale);

    return VirModel_GetRadius(v->model) * s;
}

void Car_InitWheels(Car *c);

void Car_ParamsChanged(Car *c)
{
    void **model = VirModel_Find(c->v.name);
    char name[128], buf[128];
    float r;

    c->exploding = 0;
    c->exploded = 0;
    if (model) {
        PhysType *phys;

        RigidDynCubeObj_SetModel(c, model);
        phys = PhysicsObjType_Find(c->v.name);
        if (phys) {
            PhysicsObj_SetPhysType(c, phys);
            Vehicle_ResetControls(c);
            Car_ApplyWheelDrop(c);
            Car_InitWheels(c);
            c->light_cfg[0] = c->light_params[0] * kDegToRad;
            c->light_cfg[1] = c->light_params[1] * kDegToRad;
            c->light_cfg[2] = c->light_params[2] * kFixed;
            AIDetector_Init(&c->light, 1, 4, Human_GetMaskType(4, 8), 4, 8);
        }
    } else if (c->v.name[0] && c->v.name[1] != '*') {
        crt_sprintf(buf, "Model \"%s\" not available", c->v.name);
        strcpy(name, c->v.name);
        crt_sprintf(c->v.name, "** %s **", name);
    }
    c->v.target_speed = c->v.start_speed;
    c->v.speed = c->v.start_speed;
    c->unk91D = 0;
    r = radius(c);
    DynCube_Move(Game_GetWorld(), c, &c->v.startPos, r);
    c->v.matrix = c->v.startMatrix;
    Vehicle_DeleteParts(c);
    Vehicle_NewParts(c);
    BROADCAST(c->v.attached->head, Param_GetChangedEvent(), NULL);
    VarString_Bind(&c->vars[0], c);
    VarString_Bind(&c->vars[1], c);
    VarString_Bind(&c->vars[2], c);
}
HOOK(0x42F530, Car_ParamsChanged);

/* counts the steered and driven wheels; without wheel objects all contacts are used */
void Car_InitWheels(Car *c)
{
    CarConfig *cfg = c->v.phys->data;
    int i;

    c->driven_count = 0;
    c->steered_count = 0;
    if (!cfg->wheel_objects)
        c->wheel_slots = cfg->wheel_count;
    for (i = 0; i < cfg->wheel_count; i++) {
        if (cfg->wheels[i].driven)
            c->driven_count++;
        if (cfg->wheels[i].steered)
            c->steered_count++;
        c->contacts[i].active = !cfg->wheel_objects;
    }
}
HOOK(0x42F7C0, Car_InitWheels);

void Car_GetSoundParams(Car *c, void **out)
{
    out[1] = NULL;
    out[0] = &c->v.pos;
    out[2] = c->sound_params;
}
HOOK(0x42F850, Car_GetSoundParams);

/* {control index, info out} */
void Car_GetControlInfo(Car *c, int *arg)
{
    int i = arg[0];

    if (i >= 0 && i < 5) {
        ((ControlInfo **)arg)[1] = &gCarControls[i];
        return;
    }
    arg[1] = 0;
}
HOOK(0x42F870, Car_GetControlInfo);

#define DOWN(k) (gKeys[k] & 0x80)
#define PRESSED(k) (DOWN(k) && !(gKeysPrev[k] & 0x80))

/* controls: steer, throttle, pilot animation, brake, lights */
void Car_ReadKeys(Car *c, float **arg)
{
    static const float gears[11] = {
        0.0f, -0.25f, -0.125f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f,
    };
    float *out = *arg;
    int i;

    out[0] = 0;
    if (!DOWN(0x1D) && !DOWN(0x9D))
        out[0] = (gKeys[0xCD] >> 7) - (gKeys[0xCB] >> 7);
    out[1] = -1.0f;
    /* ` and 1 to 0 pick a fixed throttle */
    if (DOWN(0x29))
        out[1] = gears[0];
    for (i = 2; i <= 0xB; i++)
        if (DOWN(i))
            out[1] = gears[i - 1];
    if (PRESSED(0x31)) {
        long double v = c->v.target_speed - kSpeedStep;

        if (kZeroD > v)
            v = kZeroD;
        out[1] = v;
    }
    if (PRESSED(0x32)) {
        long double v = c->v.target_speed + kSpeedStep;

        if (kOneD < v)
            v = kOneD;
        out[1] = v;
    }
    out[3] = -1.0f;
    if (DOWN(0x30))
        out[3] = 1.0f;
    out[4] = -1.0f;
    if (PRESSED(0x26))
        out[4] = 1.0f;
}
HOOK(0x42F8A0, Car_ReadKeys);

void Car_LoadState(Car *c, void **arg)
{
    char *s = arg[0];
    float r;

    Vehicle_LoadState(c, arg);
    c->v.brake = s[0x74];
    r = radius(c);
    DynCube_Move(Game_GetWorld(), c, (Vec3d *)(s + 0x10), r);
}
HOOK(0x42FA60, Car_LoadState);

void Car_SaveState(Car *c, void **arg)
{
    char *s = arg[0];

    Vehicle_SaveState(c, arg);
    *(int *)(s + 0x70) = 0;
    s[0x74] = c->v.brake;
    s[0x75] = 0;
}
HOOK(0x42FB10, Car_SaveState);

/* an explosion of kind 12 that reaches the car blows it up */
void Car_Blast(Car *c, char *blast)
{
    int *kind = *(int **)(blast + 0x18);

    if (kind && *kind == 12) {
        if (fne(Game_BlastFalloff(blast, c, &c->v.pos, radius(c), 1), kZero))
            c->exploding = 1;
    }
}
HOOK(0x42FB40, Car_Blast);

/* runs the car, then hands out where a passenger sits */
void Car_GetFrame(Car *c, void **out)
{
    ((void (*)(void *))TASK_HANDLER(1, c->v.task.type))(c);
    out[0] = &c->v.pos;
    out[1] = &c->v.seat_matrix;
    out[2] = &c->v.seat_pos;
}
HOOK(0x42FC00, Car_GetFrame);

void Car_Explode(Car *c)
{
    c->exploding = 1;
}
HOOK(0x42FC40, Car_Explode);

/* collision filter: skips the two tasks in the query */
void Car_CollFilter(Task *t, char *msg)
{
    if (((Task **)msg)[2] != t && ((Task **)msg)[3] != t) {
        msg[0] = 1;
        return;
    }
    msg[0] = 0;
}
HOOK(0x42FC50, Car_CollFilter);

void Car_GetLight(Car *c, void **out)
{
    out[0] = &c->light;
}
HOOK(0x42FC70, Car_GetLight);

void Car_OnWorldEvent1(Car *c, void *arg)
{
    uint8_t slot;

    if (c->exploded)
        return;
    slot = World_GetEvent1();
    ((void (*)(void *, void *))TASK_HANDLER(slot, Vehicle_GetType()))(c, arg);
    if (c->engine_sound) {
        Task_Delete(c->engine_sound);
        c->engine_sound = NULL;
    }
}
HOOK(0x42FC80, Car_OnWorldEvent1);

void Car_RegisterSymbols(Car *c)
{
    char name[64], buf[128];

    if (c->registered)
        return;
    c->registered = 1;
    Task_GetName(name, c);
    crt_sprintf(buf, "%s.isExploded", name);
    Script_RegisterBool8(buf, &c->exploded);
}
HOOK(0x42FCE0, Car_RegisterSymbols);

void Car_RemoveSymbols(Car *c)
{
    char name[64], buf[128];

    if (!c->registered)
        return;
    Task_GetName(name, c);
    c->registered = 0;
    crt_sprintf(buf, "%s.isExploded", name);
    Script_RemoveSymbol(buf);
    c->registered = 0;
}
HOOK(0x42FD40, Car_RemoveSymbols);

void Car_Close(void)
{
    QTaskList_Free(gCarParams);
    TaskType_Free(gCarType);
}
HOOK(0x42FDA0, Car_Close);

uint16_t Car_GetType(void)
{
    return gCarType;
}
HOOK(0x42FDC0, Car_GetType);

Task *Car_GetMountedGun(Car *c)
{
    Task *coll = c->v.attached;

    return Task_FindChild(coll, StationaryGun_GetType());
}
HOOK(0x42FDD0, Car_GetMountedGun);

/* position of model point 8 in the world, and the car's matrix */
void Car_GetGunnerPos(Car *c, Vec3d *pos, Matrix *m)
{
    *m = c->v.matrix;
    Model_GetPoint(pos, c, 8);
    pos->x = c->v.pos.x + pos->x;
    pos->y = c->v.pos.y + pos->y;
    pos->z = c->v.pos.z + pos->z;
}
HOOK(0x42FDF0, Car_GetGunnerPos);

/* tracks: when steering the inner side slows down */
long double Car_GetTrackFactor(Car *c, int joint)
{
    float s = c->v.turn[2];

    if (s > kHundredth) {
        if (c->v.contacts[joint].side == 1) {
            long double d = kHalf - s;

            return d + d;
        }
    } else if (!(s >= kMinusHundredth)) {
        if (c->v.contacts[joint].side == 0) {
            long double d = s + kHalf;

            return d + d;
        }
    }
    return kOne;
}
HOOK(0x4304B0, Car_GetTrackFactor);

/* moves a joint up or down by amount, within max of its model point */
void Car_LiftJoint(Car *c, int joint, float amount, float max)
{
    float *z = &c->v.contacts[joint].point.z;
    CarConfig *cfg = c->v.phys->data;
    long double d, top, n;
    float pm, lim;
    Vec3f p;

    Model_GetLocalPoint(&p, c, c->v.phys->joints[joint].point);
    d = p.z - *z;
    if (!(d >= kZero))
        d = -d;
    if (!(max + cfg->wheel_drop > d + kMilli))
        return;
    top = p.z + max;
    pm = top;
    top = top - cfg->wheel_drop;
    if (top > *z) {
        n = amount + *z;
        *z = n;
        lim = pm - cfg->wheel_drop;
        if (n > lim)
            *z = lim;
    } else if (!(top >= *z)) {
        n = *z - amount;
        *z = n;
        lim = pm - cfg->wheel_drop;
        if (!(n >= lim))
            *z = lim;
    }
    c->v.moved = 1;
}
HOOK(0x430530, Car_LiftJoint);

/* grip of a wheel on the ground: the sliding velocity at the contact point,
   turned into the wheel's frame and scaled per axis, pushes back on the body */
void Car_WheelForce(float *body, const Matrix *mat, const Vec3f *p, const Vec3f *n, float angle, float grip_x,
                    float grip_y, float unused)
{
    const float *m = mat->m;
    float load = body[0] * kThrustScale * n->z;
    long double dot, a, b, c, s0, s1;
    Vec3f lp, v, t, g, h, k, f;
    float len, force[6];
    Matrix r;

    (void)unused;
    lp.x = m[0] * p->x + p->y * m[1] + p->z * m[2];
    lp.y = m[5] * p->z + m[3] * p->x + m[4] * p->y;
    lp.z = p->z * m[8] + p->y * m[7] + p->x * m[6];
    Body_GetPointVelocity(&v, body, &lp);
    dot = v.z * n->z + v.y * n->y + v.x * n->x;
    t.x = n->x * dot;
    t.y = n->y * dot;
    t.z = n->z * dot;
    a = t.x - v.x;
    b = t.y - v.y;
    c = t.z - v.z;
    len = x87_sqrt(a * a + b * b + c * c);
    if (fne(a, kZero) || fne(b, kZero) || fne(c, kZero)) {
        long double inv = kOneD / len;

        a = a * inv;
        b = b * inv;
        c = c * inv;
    }
    g.x = b * m[3] + a * m[0] + c * m[6];
    g.y = b * m[4] + c * m[7] + a * m[1];
    g.z = b * m[5] + a * m[2] + c * m[8];
    Matrix_SetRotZ(&r, angle);
    h.x = r.m[2] * g.z + r.m[1] * g.y + r.m[0] * g.x;
    h.y = r.m[5] * g.z + r.m[4] * g.y + r.m[3] * g.x;
    h.z = r.m[8] * g.z + r.m[7] * g.y + r.m[6] * g.x;
    s0 = h.x * grip_x;
    s1 = h.y * grip_y;
    k.x = r.m[6] * h.z + r.m[3] * s1 + r.m[0] * s0;
    k.y = r.m[7] * h.z + r.m[4] * s1 + r.m[1] * s0;
    k.z = r.m[8] * h.z + r.m[5] * s1 + r.m[2] * s0;
    if (!(load * kFour / body[0] >= len)) {
        f.x = k.x * load * kGrip;
        f.y = k.y * load * kGrip;
        f.z = k.z * load * kGrip;
    } else {
        f.x = k.x * len * body[0] * kTenth;
        f.y = len * k.y * body[0] * kTenth;
        f.z = len * k.z * body[0] * kTenth;
    }
    force[0] = f.x;
    force[1] = f.y;
    force[2] = f.z;
    force[3] = p->x;
    force[4] = p->y;
    force[5] = p->z;
    Body_AddForce(body, force);
}
HOOK(0x430960, Car_WheelForce);

/* engine drive of a driven wheel, then its grip */
void Car_UpdateWheel(Car *c, int i, int joint)
{
    float *body = (float *)&c->v.body;
    CarConfig *cfg = c->v.phys->data;
    CarWheelConfig *wc = &cfg->wheels[i];
    CarContact *ct = &c->contacts[i];
    float angle = wc->steer * kDegToRad * c->v.turn[2];

    if (wc->driven && ((ct->hit && fne(c->v.speed, kZero)) || c->v.airborne)) {
        CarConfig *pc = c->v.phys->data;
        const float *m = c->v.matrix.m;
        float drag = body[2];
        Vec3f pos = ct->pos;
        Vec3f u = {m[1], m[4], m[7]};
        /* the original leaves this unset when the normal is u */
        Vec3f w = {0, 0, 0}, l, f;
        long double power = pc->v.thrust * c->v.speed * kThrustScale / c->driven_count, wz;
        float pf = power, force[6];
        Matrix r;

        if (fne(ct->normal.x, u.x) || fne(ct->normal.y, u.y) || fne(ct->normal.z, u.z)) {
            const Vec3f *n = &ct->normal;
            long double a = u.z * n->y - u.y * n->z;
            long double b = x87_neg(u.z * n->x - u.x * n->z);
            long double cc = u.y * n->x - u.x * n->y;

            w.x = b * n->z - cc * n->y;
            w.y = x87_neg(a * n->z - cc * n->x);
            w.z = a * n->y - b * n->x;
        }
        if (cfg->tank)
            power = Car_GetTrackFactor(c, joint) * pf;
        power = power - drag;
        w.x = power * w.x;
        w.y = power * w.y;
        wz = power * w.z;
        l.x = w.x * m[0] + wz * m[6] + w.y * m[3];
        l.y = w.y * m[4] + w.x * m[1] + wz * m[7];
        l.z = w.x * m[2] + wz * m[8] + w.y * m[5];
        Matrix_SetRotZ(&r, -angle);
        f.x = r.m[0] * l.x + r.m[2] * l.z + r.m[1] * l.y;
        f.y = r.m[3] * l.x + r.m[5] * l.z + r.m[4] * l.y;
        f.z = r.m[6] * l.x + r.m[8] * l.z + r.m[7] * l.y;
        force[0] = f.x;
        force[1] = f.y;
        force[2] = f.z;
        force[3] = pos.x;
        force[4] = pos.y;
        force[5] = pos.z;
        Body_AddForce(body, force);
    }
    if (ct->hit && joint >= 0) {
        float steer = wc->steered ? angle : 0;
        float gx = c->v.contacts[joint].unk1C;
        float gy = c->v.contacts[joint].friction;

        if (c->v.brake) {
            c->v.speed = c->v.speed * kHeliDamp;
            gy = gx;
        }
        Car_WheelForce(body, &c->v.matrix, &ct->pos, &ct->normal, steer, gx, gy, 0x1.47ae14p+6f);
    }
}
HOOK(0x430D10, Car_UpdateWheel);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(str_53BD08);
REF_EXTERN(str_53BD14);
REF_EXTERN(str_53BE00);
REF_EXTERN(str_53BE08);
REF_EXTERN(str_53BE10);
DATA(53346C) float kThrustScale INIT(44.600887f);
ALIAS(kCasingGravity, kThrustScale, 0);
ALIAS(kThrust, kThrustScale, 0);
DATA(533734) float kMinusHundredth INIT(-0.01f);
DATA(533734) float kGrip INIT(0.24f);
STORE(igd, 53BDB0, 4) __typeof__(gCarControls[0]) store_53BDB0[5] INIT({{.name = (__typeof__(store_53BDB0[0].name))ref_str_53BE10, .min = -360.0f, .max = 360.0f}, {.name = (__typeof__(store_53BDB0[1].name))ref_str_53BD14, .min = 0.0f, .max = 1.0f}, {.name = (__typeof__(store_53BDB0[2].name))ref_str_53BD08, .min = 0.0f, .max = 1.0f}, {.name = (__typeof__(store_53BDB0[3].name))ref_str_53BE08, .min = 0.0f, .max = 1.0f}, {.name = (__typeof__(store_53BDB0[4].name))ref_str_53BE00, .min = 0.0f, .max = 1.0f}});
ALIAS(gCarControls, store_53BDB0, 0);
DATA(53BDB0) char str_53BE00[7] INIT("Lights");
PAD(igd, 53BDB0, 53BE07, 0x1);
DATA(53BDB0) char str_53BE08[6] INIT("Break");
PAD(igd, 53BDB0, 53BE0E, 0x2);
DATA(53BDB0) char str_53BE10[7] INIT("dGamma");
PAD(igd, 53BDB0, 53BE17, 0x1);
