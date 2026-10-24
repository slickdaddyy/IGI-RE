#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "fxobj/fxobj.h"

ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *o, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *o, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *o, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *o, Vec3d *pos, float radius, int unk));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *o));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6AB0, float *, SoundParams_New, (int n));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4F0F30, void *, Param_Degrees, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));

/* a model spinning around its axes ("RotatingObject", 0x200 bytes) */
typedef struct RotatingObject {
    RigidObj obj;
    char sound[16];
    char unk100[16];
    char model[16];
    Vec3f maxSpeed;
    Vec3f accel;
    char stoppable;
    Vec3f target;
    Vec3f speed;
    Vec3f angle;
    Matrix base;
    Vec3d start;
    Task **soundRef;
    float *soundParams;
    char stopped;
    VarString cond;
} RotatingObject;

/* what the sound of an object needs to follow it */
typedef struct SoundInfo {
    Vec3d *pos;
    Matrix *matrix;
    float *params;
} SoundInfo;

_Static_assert(offsetof(RotatingObject, maxSpeed) == 0x120, "maxSpeed");
_Static_assert(offsetof(RotatingObject, target) == 0x13C, "target");
_Static_assert(offsetof(RotatingObject, base) == 0x160, "base");
_Static_assert(offsetof(RotatingObject, start) == 0x188, "start");
_Static_assert(offsetof(RotatingObject, cond) == 0x1AC, "cond");
_Static_assert(sizeof(RotatingObject) == 0x200, "RotatingObject");

extern void *gRotatingParams;
extern uint16_t gRotatingType;

static const float kPerFrame = 0x1.111112p-5f;
static const float kFull = 360.0f;
static const float kDegToRad = 0x1.1df46ap-6f;
static const float kZeroF = 0.0f;
static const float kOneF = 1.0f;

void RotatingObject_Create(RotatingObject *t);
void RotatingObject_Run(RotatingObject *t);
void RotatingObject_Delete(RotatingObject *t);
void RotatingObject_Changed(RotatingObject *t);
void RotatingObject_Stop(RotatingObject *t);
void RotatingObject_GetSoundInfo(RotatingObject *t, SoundInfo *out);
void RotatingObject_GetParams(void *t, void **out);

void RotatingObject_Init(void)
{
    RotatingObject r;

    gRotatingType = TaskType_New("RotatingObject", sizeof(RotatingObject), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gRotatingType, 0, RotatingObject_Create);
    TaskType_SetHandler2(gRotatingType, 1, RotatingObject_Run);
    TaskType_SetHandler2(gRotatingType, 2, RotatingObject_Delete);
    TaskType_SetHandler(gRotatingType, Param_GetChangedEvent(), RotatingObject_Changed);
    TaskType_SetHandler(gRotatingType, Game_GetEvent9(), RotatingObject_Stop);
    TaskType_SetHandler(gRotatingType, Sound_GetInfoEvent(), RotatingObject_GetSoundInfo);
    TaskType_SetHandler(gRotatingType, Param_GetTableEvent(), RotatingObject_GetParams);
    gRotatingParams = ParamTable_New(12);
    ParamTable_Add(gRotatingParams, &r, &r.start, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.base, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, r.model, Param_String16(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, r.sound, Param_String16(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.maxSpeed.x, Param_Degrees(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.maxSpeed.y, Param_Degrees(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.maxSpeed.z, Param_Degrees(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.accel.x, Param_Degrees(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.accel.y, Param_Degrees(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.accel.z, Param_Degrees(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.stoppable, Param_Bool8(), 7, NULL);
    ParamTable_Add(gRotatingParams, &r, &r.cond, Param_VarString(), 7, NULL);
}
HOOK(0x438DF0, RotatingObject_Init);

void RotatingObject_Create(RotatingObject *t)
{
    static const Matrix id = {{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
    long double r;

    RigidDynCubeObj_Init(Game_GetWorld(), t, NULL);
    t->obj.pos.x = 0.0;
    t->obj.pos.y = 0.0;
    t->obj.pos.z = 0.0;
    {
        float s = max_scale(&t->obj);

        r = VirModel_GetRadius(t->obj.model) * s;
    }
    DynCube_Add(Game_GetWorld(), t, &t->obj.pos, r, 2);
    t->base = id;
    t->obj.matrix = t->base;
    t->soundRef = NULL;
    t->soundParams = SoundParams_New(1);
    t->maxSpeed.x = 10.0f;
    t->maxSpeed.y = 10.0f;
    t->maxSpeed.z = 10.0f;
    t->sound[0] = 0;
    t->accel.x = 1.0f;
    t->accel.y = 1.0f;
    t->accel.z = 1.0f;
    t->stoppable = 1;
    VarString_Init(&t->cond);
    t->stopped = 0;
}
HOOK(0x438C30, RotatingObject_Create);

void RotatingObject_Delete(RotatingObject *t)
{
    DynCube_Remove((void *)(intptr_t)t->obj.unk68, t);
    if (t->soundRef)
        TaskRef_Remove(t->soundRef);
    if (t->soundParams)
        QTaskList_Free(t->soundParams);
    VarString_Free(&t->cond);
}
HOOK(0x438DA0, RotatingObject_Delete);

/* moves speed towards target by accel / 30 */
static void approach(float *speed, float target, float accel)
{
    long double v;

    if (EQ_OR_NAN(*speed, target))
        return;
    if (*speed < target) {
        v = accel * kPerFrame + *speed;
        *speed = v;
        if (v > target)
            *speed = target;
    } else if (*speed > target) {
        v = *speed - accel * kPerFrame;
        *speed = v;
        if (!(v >= target))
            *speed = target;
    }
}

void RotatingObject_Run(RotatingObject *t)
{
    const float *b = t->base.m;
    float a0, a2;
    long double a1, v;
    Matrix e;

    if ((VarString_HasExpr(&t->cond) && !(char)round_int(VarString_EvalReal(&t->cond, t))) || t->stopped) {
        t->target.x = 0.0f;
        t->target.y = 0.0f;
        t->target.z = 0.0f;
    }
    approach(&t->speed.x, t->target.x, t->accel.x);
    approach(&t->speed.y, t->target.y, t->accel.y);
    approach(&t->speed.z, t->target.z, t->accel.z);
    a0 = t->speed.x * kPerFrame + t->angle.x;
    t->angle.x = a0;
    a1 = t->speed.y * kPerFrame + t->angle.y;
    t->angle.y = a1;
    a2 = t->speed.z * kPerFrame + t->angle.z;
    t->angle.z = a2;
    if (a0 >= kFull) {
        do {
            v = t->angle.x - kFull;
            t->angle.x = v;
        } while (v >= kFull);
    }
    if (a1 >= kFull) {
        do {
            v = t->angle.y - kFull;
            t->angle.y = v;
        } while (v >= kFull);
    }
    if (a2 >= kFull) {
        do {
            v = t->angle.z - kFull;
            t->angle.z = v;
        } while (v >= kFull);
    }
    Matrix_SetEuler(&e, t->angle.x * kDegToRad, t->angle.y * kDegToRad, t->angle.z * kDegToRad);
    {
        const float *m = e.m;
        float *o = t->obj.matrix.m;

        o[0] = m[3] * b[1] + m[6] * b[2] + m[0] * b[0];
        o[1] = m[4] * b[1] + m[7] * b[2] + m[1] * b[0];
        o[2] = m[5] * b[1] + m[8] * b[2] + m[2] * b[0];
        o[3] = m[6] * b[5] + m[0] * b[3] + m[3] * b[4];
        o[4] = m[7] * b[5] + m[1] * b[3] + m[4] * b[4];
        o[5] = m[8] * b[5] + m[2] * b[3] + m[5] * b[4];
        o[6] = m[3] * b[7] + m[6] * b[8] + m[0] * b[6];
        o[7] = m[4] * b[7] + m[7] * b[8] + m[1] * b[6];
        o[8] = m[5] * b[7] + m[8] * b[8] + m[2] * b[6];
        t->obj.matrix.stamp = t->base.stamp + e.stamp + 1;
    }
    if (t->soundParams) {
        float *p = t->soundParams;

        p[1] = t->speed.z / t->maxSpeed.z + t->speed.x / t->maxSpeed.x + t->speed.y / t->maxSpeed.y;
        p = t->soundParams;
        if (p[1] < kZeroF || p[1] != p[1])
            p[1] = 0.0f;
        else if (p[1] > kOneF)
            p[1] = kOneF;
    }
}
HOOK(0x439090, RotatingObject_Run);

void RotatingObject_Changed(RotatingObject *t)
{
    long double r;

    t->base = t->obj.matrix;
    RigidDynCubeObj_SetModel(t, VirModel_Find(t->model));
    {
        float s = max_scale(&t->obj);

        r = VirModel_GetRadius(t->obj.model) * s;
    }
    DynCube_Move(Game_GetWorld(), t, &t->start, r);
    t->target.x = t->maxSpeed.x;
    t->obj.matrix = t->base;
    t->speed.x = t->maxSpeed.x;
    t->target.y = t->maxSpeed.y;
    t->target.z = t->maxSpeed.z;
    t->speed.y = t->maxSpeed.y;
    t->speed.z = t->maxSpeed.z;
    t->angle.x = 0.0f;
    t->angle.y = 0.0f;
    t->angle.z = 0.0f;
    if (t->soundRef) {
        if (*t->soundRef)
            Task_Delete(*t->soundRef);
        TaskRef_Remove(t->soundRef);
        t->soundRef = NULL;
    }
    {
        Task *s = Sound_Play(t, t->sound);

        if (s)
            t->soundRef = TaskRef_Add(s);
    }
    VarString_Bind(&t->cond, t);
}
HOOK(0x4395D0, RotatingObject_Changed);

void RotatingObject_Stop(RotatingObject *t)
{
    if (t->stoppable)
        t->stopped = 1;
}
HOOK(0x439750, RotatingObject_Stop);

void RotatingObject_GetSoundInfo(RotatingObject *t, SoundInfo *out)
{
    out->pos = &t->obj.pos;
    out->matrix = &t->obj.matrix;
    out->params = t->soundParams;
}
HOOK(0x439770, RotatingObject_GetSoundInfo);

void RotatingObject_GetParams(void *t, void **out)
{
    (void)t;
    *out = gRotatingParams;
}
HOOK(0x439790, RotatingObject_GetParams);

void RotatingObject_Close(void)
{
    QTaskList_Free(gRotatingParams);
    TaskType_Free(gRotatingType);
}
HOOK(0x4397A0, RotatingObject_Close);
