/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "missile/missile.h"
#include "levelflow/levelflowtypes.h"
#include "math/mathvars.h"
#include "levelflow/levelflowvars.h"
#include "win/winvars.h"

FN_EXTERN(Task_FalseHandler);

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4071D0, uint8_t, WeaponConfig_GetEvent, (void));
ORIG(0x413B00, uint8_t, WeaponType_GetSizeEvent, (void));
ORIG(0x413B10, uint8_t, WeaponType_GetInitEvent, (void));
ORIG(0x413B20, uint8_t, Weapon_GetFireEvent, (void));
ORIG(0x4148F0, long double, Vec3d_PitchAngle, (Vec3d v));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x414ED0, void, Model_GetLocalPoint, (Vec3f *out, void *obj, int i));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4169D0, void *, Game_AddSprite, (const char *name));
ORIG(0x480FE0, void, Explosion_Spawn, (Vec3d *pos, int a, int b, float c, float d, float e, float f, int g, int h,
                                  CollHit *hit, const char *sprite, const char *sound, int i, int j, float k,
                                  int weapon));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B30C0, void, Vec3d_SetLength, (Vec3d *v, double len));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, void, Script_GetArgString, (void *args, int i, char *out, int size));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CABF0, void, Coll_DropPoints, (void *world, CollHit *hit, Vec3d *points, int count, double *dz,
                                       int event, CollIgnore *ignore, int flags));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, Vec3d *pos));
ORIG(0x4EC2C0, void, Body_AddAeroForces, (void *parts, int count, float k, void *body, Matrix *m, Vec3f *v));
ORIG(0x4ECDB0, void, Body_Update, (void *body));
ORIG(0x4ECF50, void, Body_AddForce, (void *body, float *force));
ORIG(0x4EE0E0, uint8_t, PhysicsObjType_GetParseEvent, (void));
ORIG(0x4EE0F0, uint8_t, PhysicsObjType_GetSizeEvent, (void));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4F4640, void, Vehicle_Free, (void *obj));
ORIG(0x4F4660, void, Vehicle_RunParts, (void *obj));
ORIG(0x4F4680, void, Vehicle_Init, (void *obj));
ORIG(0x4F4800, void, Vehicle_NewParts, (void *obj));
ORIG(0x4F4820, void, Vehicle_DeleteParts, (void *obj));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *obj, PhysType *phys));
ORIG(0x4FC0E0, void, PhysicsObj_Collide, (void *obj, float a, float b, float c, void *world, int event, void *msg));
ORIG(0x4FC7B0, int, PhysicsObj_IsResting, (void *obj));
ORIG(0x4FC840, uint8_t, PhysicsObj_GetEvent0, (void));
ORIG(0x4FDF00, void *, Particle2D_New, (void *a, void *world, void *sprite, Vec3f *dir, Vec3f *scale, float *size,
                                    float b, int c, int d, float e, int f, int g, int h));
ORIG(0x4FE120, void, Particle2D_Add, (void *smoke, Vec3d *pos, Vec3f *color, float size, float a, float b, float c,
                                  int d, int e));
ORIG(0x4FE5D0, void, Particle2D_SetAutoDelete, (void *smoke, int on));
ORIG(0x4FE6B0, char *, TagItem_AddString16, (void *task, const char *name, const char *def));
ORIG(0x4FE730, char *, TagItem_Find, (void *task, const char *name));

extern uint16_t gMissileType;
extern void *gMissileParams;
extern char gMissileRandom[];

#define Missile_UnkHandlerC (FN(0x45F920, Task_FalseHandler))

extern float kSmokeSize;      /* 0.3 */
extern float kSmokeMin;       /* 0.15 */
extern float kSmokeLife;      /* 3 */
extern float kSmokeFade;      /* 0.6 */
extern float kSmokeSpin;
extern double kEighth;
extern double kAntitankNear; /* 122880 */
extern double kAntitankUp;   /* 102400 */
extern double kForward;      /* 4096000 */
extern float kSpeedScale;
extern float kBurnScale;     /* 1/30 */

/* collision filter: msg is {collide, missile, owner, target}; the missile hits
   anything but those three and other missiles */
void Missile_CollFilter(Task *t, char *msg)
{
    if (t != ((Task **)msg)[1] && t != ((Task **)msg)[2] && t != ((Task **)msg)[3] && t->type != gMissileType) {
        msg[0] = 1;
        return;
    }
    msg[0] = 0;
}
HOOK(0x427FC0, Missile_CollFilter);

void Missile_Init(Missile *m);
void Missile_Run(Missile *m);
void Missile_DeleteHandler(Missile *m);
void Missile_GetParams(Task *t, void **out);
void Missile_ParamsChanged(Missile *m);
void Missile_ParseArgs(Task *t, void **msg);
void Missile_GetSize(Task *t, int *out);
void Missile_Fire(Task *t, void **msg);
void Missile_ReadModelArg(Task *t, void **msg);
void Missile_GetModelSize(Task *t, int *out);
void Missile_ConfigWeapon(Task *t, void **msg);
uint16_t Missile_GetType(void);

#define FIELD(name) ((void *)offsetof(Missile, name))

void Missile_Open(void)
{
    gMissileType = TaskType_New("Missile", sizeof(Missile), Vehicle_GetType(), 0);
    TaskType_SetHandler(gMissileType, 0, Missile_Init);
    TaskType_SetHandler2(gMissileType, 1, Missile_Run);
    TaskType_SetHandler2(gMissileType, 2, Missile_DeleteHandler);
    TaskType_SetHandler(gMissileType, Param_GetTableEvent(), Missile_GetParams);
    TaskType_SetHandler(gMissileType, Param_GetChangedEvent(), Missile_ParamsChanged);
    TaskType_SetHandler(gMissileType, PhysicsObjType_GetParseEvent(), Missile_ParseArgs);
    TaskType_SetHandler(gMissileType, PhysicsObjType_GetSizeEvent(), Missile_GetSize);
    TaskType_SetHandler(gMissileType, Weapon_GetFireEvent(), Missile_Fire);
    TaskType_SetHandler(gMissileType, WeaponType_GetInitEvent(), Missile_ReadModelArg);
    TaskType_SetHandler(gMissileType, WeaponType_GetSizeEvent(), Missile_GetModelSize);
    TaskType_SetHandler(gMissileType, Game_GetEvent7(), Missile_CollFilter);
    TaskType_SetHandler(gMissileType, PhysicsObj_GetEvent0(), NULL);
    TaskType_SetHandler(gMissileType, 12, Missile_UnkHandlerC);
    TaskType_SetHandler(gMissileType, WeaponConfig_GetEvent(), Missile_ConfigWeapon);
    gMissileParams = ParamTable_New(3);
    /* the original passes a dummy Missile on its stack, only the difference
       of the two pointers is kept */
    ParamTable_Add(gMissileParams, NULL, FIELD(v.startPos), Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gMissileParams, NULL, FIELD(v.matrix), Param_Real32x9(), 7, NULL);
    ParamTable_Add(gMissileParams, NULL, FIELD(v.name), Param_String16(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_MISSILE", gMissileType);
    Script_RegisterConstant("MISSILE_SMOKE_NONE", 0);
    Script_RegisterConstant("MISSILE_SMOKE_NORMAL", 1);
    Script_RegisterConstant("MISSILE_TYPE_MISSILE_DIRECT", MISSILE_DIRECT);
    Script_RegisterConstant("MISSILE_TYPE_MISSILE_BALLISTIC", MISSILE_BALLISTIC);
    Script_RegisterConstant("MISSILE_TYPE_MISSILE_ANTITANK", MISSILE_ANTITANK);
    Script_RegisterConstant("MISSILE_TYPE_MISSILE_KEYBOARD", MISSILE_KEYBOARD);
    Script_RegisterConstant("MISSILE_TYPE_BOMB", MISSILE_BOMB);
    Script_RegisterConstant("MISSILE_TYPE_HIND_HELICOPTER", MISSILE_HIND_HELICOPTER);
    Random_Init(gMissileRandom);
}
HOOK(0x427FF0, Missile_Open);

void Missile_Reset(Missile *m);

void Missile_Init(Missile *m)
{
    Missile_Reset(m);
}
HOOK(0x428270, Missile_Init);

void Missile_Reset(Missile *m)
{
    Vec3d zero = {0, 0, 0};
    float scale, radius;

    RigidDynCubeObj_Init(Game_GetWorld(), m, NULL);
    scale = max_scale(&m->v);
    radius = VirModel_GetRadius(m->v.model) * scale;
    DynCube_Add(Game_GetWorld(), m, &zero, radius, 2);
    m->v.name[0] = 0;
    Vehicle_Init(m);
    m->v.start_speed = 0;
    m->follow = NULL;
    m->target_task = NULL;
    m->owner = NULL;
    m->sound = NULL;
    m->unk894 = 0;
}
HOOK(0x428280, Missile_Reset);

void Missile_SteerNone(Missile *m);
void Missile_SteerHind(Missile *m);
void Missile_SteerKeyboard(Missile *m);
void Missile_SteerBallistic(Missile *m, Vec3d *target);
void Missile_SteerAntitank(Missile *m, Vec3d *target);
void Missile_SteerTo(Missile *m, Vec3d *target);
void Missile_EmitSmoke(Missile *m);
void Missile_Move(Missile *m);
void Missile_UpdateSound(Missile *m);

void Missile_Run(Missile *m)
{
    MissileConfig *cfg = m->v.phys->data;
    CollIgnore ignore;
    CollHit hit;
    double dz;
    Vec3d pos;

    if (m->follow && *m->follow)
        m->target = ((Vehicle *)*m->follow)->pos;
    switch (cfg->type) {
    case MISSILE_DIRECT:
        Missile_SteerNone(m);
        break;
    case MISSILE_HIND_HELICOPTER:
        Missile_SteerHind(m);
        break;
    case MISSILE_BALLISTIC:
        Missile_SteerBallistic(m, &m->target);
        break;
    case MISSILE_ANTITANK:
        Missile_SteerAntitank(m, &m->target);
        break;
    case MISSILE_KEYBOARD:
        Missile_SteerKeyboard(m);
        break;
    }
    if (fne(m->fuel, kZero) && cfg->smoke)
        Missile_EmitSmoke(m);
    Missile_Move(m);
    Missile_UpdateSound(m);
    Vehicle_RunParts(m);
    BROADCAST(m->v.task.head, 1);
    if (m->v.touching) {
        pos = m->v.pos;
        ignore.task = &m->v.task;
        dz = -12288.0;
        Coll_DropPoints(Game_GetWorld(), &hit, &pos, 1, &dz, 0xC, &ignore, -1);
        Explosion_Spawn(&pos, 1, 1, 5.0f, 10240.0f, 7168.0f, 6144.0f, 10, 5, &hit, "1009_01_1", "explo_03_l", 0x15, 2,
                   102400.0f, m->unk894);
        Task_Delete(m);
        return;
    }
    if (!(gLevelFlow->nTick & 0x1F)) {
        pos = m->v.pos;
        ignore.task = &m->v.task;
        dz = -4096000.0;
        Coll_DropPoints(Game_GetWorld(), &hit, &pos, 1, &dz, 0xC, &ignore, -1);
        if (!hit.found)
            Task_Delete(m);
    }
}
HOOK(0x428390, Missile_Run);

void Missile_UpdateSound(Missile *m)
{
    if (m->sound)
        Sound_SetPos(m->sound, &m->v.pos);
}
HOOK(0x428610, Missile_UpdateSound);

void Missile_EmitSmoke(Missile *m)
{
    void **model = m->v.model;
    Vec3d pos, step;
    Vec3f color, d;
    long double inv;
    float size, spacing, fx, fy, a, b;
    int n, life, fade;

    n = round_int(m->smoke_interval);
    if (++m->smoke_tick < n)
        return;
    size = Random_Float(gMissileRandom) * kSmokeSize + kSmokeMin;
    spacing = m->v.body.speed;
    m->smoke_tick = 0;
    switch (*(short *)(*(char **)(*(char **)((char *)model[0] + 0xC) + 0xC) + 0x7C)) {
    case 0:
        pos = m->v.pos;
        break;
    case 1: {
        long double y, z;

        Model_GetLocalPoint(&d, m, 0);
        y = -d.y;
        pos = m->v.pos;
        fx = d.x * m->v.matrix.m[0] + d.z * m->v.matrix.m[2] + y * m->v.matrix.m[1];
        fy = d.x * m->v.matrix.m[3] + d.z * m->v.matrix.m[5] + y * m->v.matrix.m[4];
        z = d.x * m->v.matrix.m[6] + d.z * m->v.matrix.m[8] + y * m->v.matrix.m[7];
        pos.x = fx + pos.x;
        pos.y = fy + pos.y;
        pos.z = z + pos.z;
        break;
    }
    default:
        Model_GetPoint(&pos, m, 1);
        pos.x = pos.x + m->v.pos.x;
        pos.y = pos.y + m->v.pos.y;
        pos.z = pos.z + m->v.pos.z;
        break;
    }
    color = m->v.body.dir;
    n = round_int(spacing / m->smoke_spacing);
    if (n <= 1)
        n = 1;
    inv = kOne / (long double)n;
    step.x = inv * m->v.body.vel.x;
    step.y = (float)inv * (double)m->v.body.vel.y;
    step.z = (float)inv * (double)m->v.body.vel.z;
    do {
        Random_Float(gMissileRandom);
        Random_Float(gMissileRandom);
        Random_Float(gMissileRandom);
        life = round_int(m->smoke_size * kSmokeLife);
        fade = round_int((Random_Float(gMissileRandom) * kSmokeSize + kSmokeFade) * m->smoke_size * kThirty);
        a = Random_Float(gMissileRandom) * kTwoPi;
        b = (kHalf - Random_Float(gMissileRandom)) * kSmokeSpin;
        Particle2D_Add(m->smoke, &pos, &color, size, b, a, 1.0f, life, fade);
        pos.x = step.x + pos.x;
        pos.y = step.y + pos.y;
        pos.z = step.z + pos.z;
    } while (--n);
}
HOOK(0x428630, Missile_EmitSmoke);

void Missile_SteerNone(Missile *m)
{
    m->v.steer[0] = 0;
    m->v.steer[2] = 0;
}
HOOK(0x428910, Missile_SteerNone);

/* aims at the point 12288 units past the nearest point of the flight line */
void Missile_SteerHind(Missile *m)
{
    long double dx = (long double)m->v.pos.x - m->target.x;
    double dy = m->v.pos.y - m->target.y;
    long double dz = (long double)m->v.pos.z - m->target.z;
    long double t = dz * m->dir.z + dy * m->dir.y + dx * m->dir.x;
    double td = t;
    Vec3d p, v;

    p.x = t * dx;
    p.y = td * dy;
    p.z = td * (double)dz;
    v = p;
    Vec3d_SetLength(&v, 12288.0);
    p.x = v.x + p.x;
    p.y = v.y + p.y;
    p.z = v.z + p.z;
    Missile_SteerTo(m, &p);
}
HOOK(0x428930, Missile_SteerHind);

void Missile_SteerKeyboard(Missile *m)
{
    m->v.steer[2] = (int)((gKeys[0xCD] >> 7) - (gKeys[0xCB] >> 7));
    m->v.steer[0] = (int)((gKeys[0xD0] >> 7) - (gKeys[0xC8] >> 7));
}
HOOK(0x428A00, Missile_SteerKeyboard);

void Missile_SteerBallistic(Missile *m, Vec3d *target)
{
    Vec3d p;
    long double len;

    p.x = target->x - m->v.pos.x;
    p.y = target->y - m->v.pos.y;
    p.z = target->z - m->v.pos.z;
    len = Vec3d_Length(&p);
    p = *target;
    p.z = len * kEighth + p.z;
    Missile_SteerTo(m, &p);
}
HOOK(0x428A60, Missile_SteerBallistic);

void Missile_SteerAntitank(Missile *m, Vec3d *target)
{
    Vec3d p;
    long double len;

    p.x = target->x - m->v.pos.x;
    p.z = 0;
    p.y = target->y - m->v.pos.y;
    len = Vec3d_Length(&p);
    p = *target;
    if (len > kAntitankNear) {
        p.z = p.z + kAntitankUp;
        Missile_SteerTo(m, &p);
        return;
    }
    Missile_SteerBallistic(m, &p);
}
HOOK(0x428AD0, Missile_SteerAntitank);

/* steers towards a world point: pitch and yaw of it in missile space, clamped to -1..1 */
void Missile_SteerTo(Missile *m, Vec3d *target)
{
    const float *r = m->v.matrix.m;
    long double dx = (long double)target->x - m->v.pos.x;
    long double dy = (long double)target->y - m->v.pos.y;
    long double dz = (long double)target->z - m->v.pos.z;
    Vec3d l;
    double pitch;
    long double yaw;

    l.x = r[6] * dz + r[3] * dy + r[0] * dx;
    l.y = r[7] * dz + r[4] * dy + r[1] * dx;
    l.z = r[8] * dz + r[5] * dy + r[2] * dx;
    pitch = Vec3d_PitchAngle(l);
    yaw = Vec3d_YawAngle(l);
    if (pitch < kMinusOneD)
        pitch = -1.0;
    else if (!(pitch <= kOneD))
        pitch = 1.0;
    if (yaw < kMinusOneD)
        yaw = kMinusOneD;
    else if (!(yaw <= kOneD))
        yaw = kOneD;
    m->v.steer[0] = pitch;
    m->v.steer[2] = yaw;
}
HOOK(0x428B60, Missile_SteerTo);

void Missile_DeleteHandler(Missile *m)
{
    Vehicle_Free(m);
    if (m->owner)
        TaskRef_Remove(m->owner);
    if (m->target_task)
        TaskRef_Remove(m->target_task);
    if (m->follow)
        TaskRef_Remove(m->follow);
    DynCube_Remove(Game_GetWorld(), m);
}
HOOK(0x428CA0, Missile_DeleteHandler);

void Missile_Move(Missile *m)
{
    MissileConfig *cfg = m->v.phys->data;
    float rate = !(m->v.speed >= kSevenTenths) ? cfg->accel_slow : cfg->accel;
    float target = m->v.target_speed;
    long double s = m->v.speed, t0, t1;
    float t2;
    Matrix r;

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
    m->v.speed = s;
    t0 = (kOne - cfg->axis[0].smooth) * m->v.steer[0] + cfg->axis[0].smooth * m->v.turn[0];
    m->v.turn[0] = t0;
    t1 = (kOne - cfg->axis[1].smooth) * m->v.steer[1] + cfg->axis[1].smooth * m->v.turn[1];
    m->v.turn[1] = t1;
    t2 = (kOne - cfg->axis[2].smooth) * m->v.steer[2] + cfg->axis[2].smooth * m->v.turn[2];
    m->v.turn[2] = t2;
    if (fne(t0, kZero) || fne(t1, kZero) || fne(t2, kZero)) {
        float az = t2 * cfg->axis[2].rate * kDegToRad;
        float ax = t0 * cfg->axis[0].rate * kDegToRad;

        Matrix_SetRotX(&r, ax);
        mat_mul(&m->v.matrix, &r, 2);
        Matrix_SetRotZ(&r, az);
        mat_mul(&m->v.matrix, &r, 2);
    }
    if (!fne(m->fuel, kZero)) {
        float ax = (kHalf - Random_Float(gMissileRandom)) * kTenth;
        float ay = (kSevenTenths - Random_Float(gMissileRandom)) * kTenth;
        float az = (kSmokeSize - Random_Float(gMissileRandom)) * kTenth;

        Matrix_SetEuler(&r, ax, ay, az);
        mat_mul(&m->v.matrix, &r, 7);
    }
    if (fne(m->v.speed, kZero) && fne(m->fuel, kZero)) {
        float force[6];

        force[1] = cfg->thrust * m->v.speed * kThrustScale;
        force[0] = 0;
        force[2] = 0;
        force[3] = 0;
        force[4] = 0;
        force[5] = 0;
        Body_AddForce(&m->v.body, force);
    }
    {
        long double f = m->fuel - cfg->burn;

        m->fuel = f;
        m->fuel = 0 > f ? kZero : f;
    }
    if (!PhysicsObj_IsResting(m)) {
        void *world;
        int event;
        struct {
            int collide;
            Missile *m;
            Task *owner;
            Task *target;
            float unk10;
        } msg;

        msg.m = m;
        msg.owner = m->owner ? *m->owner : NULL;
        msg.target = m->target_task ? *m->target_task : NULL;
        msg.unk10 = 1.0f;
        if (cfg->part_count) {
            Vec3f v;

            v.x = -m->v.body.vel.x;
            v.y = -m->v.body.vel.y;
            v.z = -m->v.body.vel.z;
            Body_AddAeroForces(m->parts, cfg->part_count, 0x1.56e264p-12f, &m->v.body, &m->v.matrix, &v);
        }
        event = Game_GetEvent7();
        world = Game_GetWorld();
        PhysicsObj_Collide(m, 0x1.64ce9ep+5f, 0x1.e66666p-1f, 0x1.d70a3ep-1f, world, event, &msg);
    }
    if (m->v.attached) {
        struct {
            Vec3d *pos;
            Matrix *matrix;
            Missile *m;
        } msg = {&m->v.pos, &m->v.matrix, m};

        BROADCAST(m->v.attached->head, MagicObj_GetMoveEvent(), &msg);
    }
}
HOOK(0x428D00, Missile_Move);

void Missile_ParamsChanged(Missile *m)
{
    char buf[128], name[128];
    void **model;
    float scale, radius;

    m->v.startMatrix = m->v.matrix;
    model = VirModel_Find(m->v.name);
    Vehicle_DeleteParts(m);
    if (model) {
        RigidDynCubeObj_SetModel(m, model);
        PhysicsObj_SetPhysType(m, PhysicsObjType_Find(m->v.name));
    } else if (m->v.name[0] && m->v.name[1] != '*') {
        crt_sprintf(buf, "Model \"%s\" not available", m->v.name);
        strcpy(name, m->v.name);
        crt_sprintf(m->v.name, "** %s **", name);
    }
    m->v.target_speed = m->v.start_speed;
    m->v.speed = m->v.start_speed;
    scale = max_scale(&m->v);
    radius = VirModel_GetRadius(m->v.model) * scale;
    DynCube_Move(Game_GetWorld(), m, &m->v.startPos, radius);
    Vehicle_NewParts(m);
    BROADCAST(m->v.attached->head, Param_GetChangedEvent(), NULL);
}
HOOK(0x429540, Missile_ParamsChanged);

void Missile_GetSize(Task *t, int *out)
{
    *out = sizeof(MissileConfig);
}
HOOK(0x429720, Missile_GetSize);

/* reads the missile's own DefinePhysicsObjType arguments after the vehicle ones */
void Missile_ParseArgs(Task *t, void **msg)
{
    MissileConfig *cfg = msg[0];
    void *args = msg[1];
    int *next = (int *)&msg[2];
    uint8_t event = PhysicsObjType_GetParseEvent();

    ((TaskFn2)TASK_HANDLER(event, Vehicle_GetType()))(t, msg);
    cfg->smoke = Script_GetArgInt(args, (*next)++);
    cfg->type = Script_GetArgInt(args, (*next)++);
    cfg->fuel = Script_GetArgReal(args, (*next)++);
    cfg->burn = Script_GetArgReal(args, (*next)++) * kBurnScale;
    cfg->unk128 = Script_GetArgReal(args, (*next)++);
    cfg->smoke_spacing = Script_GetArgReal(args, (*next)++) * kFixed;
    cfg->unk130 = Script_GetArgReal(args, (*next)++) * kFixed;
    cfg->smoke_interval = Script_GetArgReal(args, (*next)++);
    cfg->smoke_size = Script_GetArgReal(args, (*next)++);
    Script_GetArgString(args, (*next)++, cfg->sprite, sizeof(cfg->sprite));
    Script_GetArgString(args, (*next)++, cfg->sound, sizeof(cfg->sound));
    cfg->sprite_handle = Game_AddSprite(cfg->sprite);
}
HOOK(0x429730, Missile_ParseArgs);

void Missile_GetParams(Task *t, void **out)
{
    *out = gMissileParams;
}
HOOK(0x429890, Missile_GetParams);

Missile *Missile_New(void *parent, Task *owner, Task *target, Vec3d *pos, float *matrix, float speed, Vec3d *aim,
                     Task *follow, const char *model, void *weapon);

/* weapon fire message: owner, target, position, ..., matrix, aim point, task to follow, weapon data */
void Missile_Fire(Task *t, void **msg)
{
    char *w = msg[8];

    Missile_New(Level_GetDynamic(), msg[0], msg[1], msg[2], msg[5], *(float *)(w + 0x660), msg[6], msg[7],
                *(char **)(w + 0x6E0), w);
}
HOOK(0x4298A0, Missile_Fire);

void Missile_ReadModelArg(Task *t, void **msg)
{
    Script_GetArgString(msg[1], (int)msg[2], msg[0], 16);
    msg[2] = (char *)msg[2] + 1;
}
HOOK(0x4298E0, Missile_ReadModelArg);

void Missile_GetModelSize(Task *t, int *out)
{
    *out = 16;
}
HOOK(0x429910, Missile_GetModelSize);

/* {config task, weapon type}: the weapon gets the "Missile - Model" setting as its missile model */
void Missile_ConfigWeapon(Task *t, void **msg)
{
    char **slot = (char **)((char *)msg[1] + 0x6E4);
    char buf[16];
    char *s;

    if (*slot) {
        Mem_Free(*slot);
        *(char **)((char *)msg[1] + 0x6E4) = NULL;
    }
    memcpy(buf, "110_02_1", 9);
    s = TagItem_Find(msg[0], "Missile - Model");
    if (!s)
        s = TagItem_AddString16(msg[0], "Missile - Model", buf);
    strcpy(buf, s + 0x124);
    *(char **)((char *)msg[1] + 0x6E4) = Mem_Alloc(16, 4);
    memcpy(*(char **)((char *)msg[1] + 0x6E4), buf, 16);
}
HOOK(0x429920, Missile_ConfigWeapon);

void Missile_Close(void)
{
    QTaskList_Free(gMissileParams);
    TaskType_Free(gMissileType);
}
HOOK(0x429A00, Missile_Close);

uint16_t Missile_GetType(void)
{
    return gMissileType;
}
HOOK(0x429A20, Missile_GetType);

void Missile_InitSmoke(Missile *m);
void Missile_StartSound(Missile *m);
void Missile_CopyParts(Missile *m);

Missile *Missile_New(void *parent, Task *owner, Task *target, Vec3d *pos, float *matrix, float speed, Vec3d *aim,
                     Task *follow, const char *model, void *weapon)
{
    Missile *m;
    MissileConfig *cfg;
    PhysType *phys;
    void **vm;
    float scale, radius;

    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        m = (Missile *)Task_New(parent, Missile_GetType(), NULL);
        Mem_ClearMode();
    } else {
        m = (Missile *)Task_New(parent, Missile_GetType(), NULL);
    }
    memcpy(&m->v.matrix, matrix, sizeof(Matrix));
    vm = VirModel_Find(model);
    if (!vm) {
        Error_Show("VirModel \"%s\" not available", model);
        for (;;)
            ;
    }
    RigidDynCubeObj_SetModel(m, vm);
    scale = max_scale(&m->v);
    radius = VirModel_GetRadius(m->v.model) * scale;
    DynCube_Move(Game_GetWorld(), m, pos, radius);
    phys = PhysicsObjType_Find(model);
    PhysicsObj_SetPhysType(m, phys);
    m->fuel = ((MissileConfig *)phys->data)->fuel;
    Missile_CopyParts(m);
    strcpy(m->v.name, model);
    m->v.target_speed = 1.0f;
    m->v.speed = 1.0f;
    Vehicle_DeleteParts(m);
    Vehicle_NewParts(m);
    Missile_InitSmoke(m);
    Missile_StartSound(m);
    cfg = m->v.phys->data;
    m->owner = NULL;
    if (owner)
        m->owner = TaskRef_Add(owner);
    m->target_task = NULL;
    if (target)
        m->target_task = TaskRef_Add(target);
    if (cfg->type == MISSILE_DIRECT) {
        long double x = matrix[1] * kForward;
        double y = matrix[4] * kForward;
        double z = matrix[7] * kForward;

        m->target.x = x + pos->x;
        m->target.y = y + pos->y;
        m->target.z = z + pos->z;
    } else if (cfg->type == MISSILE_HIND_HELICOPTER) {
        m->dir.x = aim->x - pos->x;
        m->dir.y = aim->y - pos->y;
        m->dir.z = aim->z - pos->z;
        m->target = *pos;
        m->target.x = m->dir.x + m->target.x;
        m->target.y = m->dir.y + m->target.y;
        m->target.z = m->dir.z + m->target.z;
    } else if (follow) {
        m->follow = TaskRef_Add(follow);
    } else if (aim) {
        m->target = *aim;
    }
    m->v.body.vel.x = matrix[1] * speed * kSpeedScale;
    m->v.body.vel.y = matrix[4] * speed * kSpeedScale;
    m->v.body.vel.z = matrix[7] * speed * kSpeedScale;
    Body_Update(&m->v.body);
    m->unk894 = (int)weapon;
    return m;
}
HOOK(0x429A30, Missile_New);

void Missile_StartSound(Missile *m)
{
    MissileConfig *cfg = m->v.phys->data;

    m->sound = Sound_Play(m, cfg->sound);
    if (m->sound)
        Sound_SetPos(m->sound, &m->v.pos);
}
HOOK(0x429D80, Missile_StartSound);

void Missile_InitSmoke(Missile *m)
{
    MissileConfig *cfg = m->v.phys->data;
    float size[2];
    Vec3f scale, dir;
    void *world;
    float k;

    m->unk86C[0] = 0x1.99999ap-4f;
    m->unk86C[1] = 0x1.99999ap-4f;
    m->unk86C[2] = 0x1.99999ap-4f;
    m->smoke_interval = cfg->smoke_interval;
    m->smoke_spacing = cfg->smoke_spacing;
    m->unk888 = 0;
    m->unk87C = 2048000.0f;
    m->smoke_size = cfg->smoke_size;
    dir.x = 0;
    dir.y = 0;
    dir.z = 0x1.64ce9ep-6f;
    scale.x = 0x1.966666p+3f;
    scale.y = 0x1.966666p+3f;
    scale.z = 0x1.966666p+3f;
    size[0] = cfg->smoke_spacing;
    size[1] = cfg->smoke_spacing;
    k = (cfg->unk130 / cfg->smoke_spacing - kOne) / (m->smoke_size * kThirty);
    world = Game_GetWorld();
    m->smoke = Particle2D_New(Level_GetDynamic(), world, (char *)cfg->sprite_handle + 0x18, &dir, &scale, size, 2048000.0f,
                          0x1080000, 0, k, 0, 2000, 0);
    Particle2D_SetAutoDelete(m->smoke, 1);
    m->smoke_tick = 0;
}
HOOK(0x429DC0, Missile_InitSmoke);

/* copies the type's parts, then mirrors parts 0 and 1 into 2 and 3 */
void Missile_CopyParts(Missile *m)
{
    MissileConfig *cfg = m->v.phys->data;
    int i;

    for (i = 0; i < cfg->part_count; i++)
        memcpy(m->parts[i], cfg->parts[i], sizeof(m->parts[i]));
    memcpy(m->parts[2], m->parts[0], sizeof(m->parts[0]));
    ((float *)m->parts[2])[5] = -((float *)m->parts[2])[5];
    memcpy(m->parts[3], m->parts[1], sizeof(m->parts[1]));
    ((float *)m->parts[3])[5] = -((float *)m->parts[3])[5];
}
HOOK(0x429EF0, Missile_CopyParts);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5333C8) const float kTenth INIT(0.1f);
ALIAS(kDepthFade2, kTenth, 0);
DATA(533540) double kAntitankUp INIT(102400.0);
DATA(533540) double kForward INIT(4096000.0);
DATA(533598) float kSmokeMin INIT(0.15f);
DATA(533680) float kSpeedScale INIT(136.53334f);
DATA(533698) float kSmokeSpin INIT(0.037699115f);
DATA(533698) float kSmokeFade INIT(0.6f);
DATA(533698) float kSmokeLife INIT(3.0f);
ALIAS(kGroundThrust, kSmokeFade, 0);
ALIAS(kPierceChance, kSmokeFade, 0);
ALIAS(kSixTenths, kSmokeFade, 0);
DATA(5336A8) double kEighth INIT(0.125);
DATA(5336A8) double kAntitankNear INIT(122880.0);
DATA(5336A8) double kMinusOneD INIT(-1.0);
DATA(5336C4) float kSevenTenths INIT(0.7f);
DATA(53BB8C) uint16_t gMissileType INIT(385);
PAD(igd, 53BB8C, 53BB8E, 0x2);
PAD(igd, 544344, 544344, 0x1) INIT({0x08});
DATA(544344) char str_544345[3] INIT("GS");
STORE(igb, 57BC88, 1) char store_57BC88[12];
ALIAS(gMissileRandom, store_57BC88, 0);
BSS(57BC88) void *gMissileParams;
