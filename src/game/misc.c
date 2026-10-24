/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "game/game.h"

ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x406220, char *, Profile_GetActive, (void));
ORIG(0x4B28A0, long double, Math_ACos, (double x));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3320, char, Vec3d_IsZero, (const Vec3d *v));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4CABF0, void, Coll_DropPoints, (void *world, void *results, Vec3d *points, int count, double *dz,
                                       uint8_t event, void *ignore, int flags));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0930, void, Mesh3D_GetBoneWorldVertex, (void *out, Task *task, int i, ...));
ORIG(0x4D0970, void, Mesh3D_GetVertex, (void *out, Task *task, int i));
ORIG(0x4D0A50, void, Mesh3D_GetWorldVertex, (void *out, Task *task, int i));
ORIG(0x4D0D20, int, ModelLod_GetVertexCount, (void *model));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));

extern int gCheatsEnabled;

#define KEY_LCONTROL 0x1D
#define KEY_LSHIFT 0x2A

/* only the fields read here */
typedef struct CollHit {
    char unk0[0x18];
    double z;
    char unk20[0x38];
    int found;
    char unk5C[0x14];
} CollHit;

/* a task with model instances */
typedef struct ModelTask {
    Task task;
    char unk20[0x4C];
    void **model;
    int unk70;
    void **model2;
} ModelTask;

/* angle of v turned into the y/z plane, from +y, negative below */
long double Vec3d_PitchAngle(Vec3d v)
{
    long double a;

    v.x = 0.0;
    if (Vec3d_IsZero(&v))
        return 0.0f;
    Vec3d_Normalize(&v);
    a = Math_ACos(v.y);
    if (!(v.z >= 0.0))
        a = -a;
    return a;
}
HOOK(0x4148F0, Vec3d_PitchAngle);

/* angle of v turned into the x/y plane, from +y, negative towards +x */
long double Vec3d_YawAngle(Vec3d v)
{
    long double a;

    v.z = 0.0;
    if (Vec3d_IsZero(&v))
        return 0.0f;
    Vec3d_Normalize(&v);
    a = Math_ACos(v.y);
    if (v.x > 0.0)
        a = -a;
    return a;
}
HOOK(0x414950, Vec3d_YawAngle);

/* moves pos to the ground below it, or above it if there is none below */
void Game_DropToGround(void *task, Vec3d *pos)
{
    struct {
        int unk0;
        void *task;
    } ignore;
    CollHit hit;
    double dz = -1638400.0;
    uint8_t event;

    ignore.task = task;
    event = Game_GetEvent13();
    Coll_DropPoints(Game_GetWorld(), &hit, pos, 1, &dz, event, &ignore, -1);
    if (!hit.found) {
        dz = -dz;
        event = Game_GetEvent13();
        Coll_DropPoints(Game_GetWorld(), &hit, pos, 1, &dz, event, &ignore, -1);
        if (!hit.found)
            return;
    }
    memcpy(&pos->z, &hit.z, sizeof(double));
}
HOOK(0x4149B0, Game_DropToGround);

static const float kNearlyUp = 0.95f;

/* fchs on the x87 value, so gcc cannot turn -(a - b) into b - a (they differ in round-down) */
static inline long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

/* v - 0.0 done by the x87: in round-down mode +0 - 0 gives -0 */
static inline long double x87_sub_zero(float v)
{
    long double zero = 0.0L;

    __asm__("" : "+t"(zero));
    return v - zero;
}

/* zero or NaN, like the original's fcomp test */
static int IsZero(float f)
{
    return !(f < 0.0f || f > 0.0f);
}

static int Vec3f_IsZero(const Vec3f *v)
{
    return IsZero(v->x) && IsZero(v->y) && IsZero(v->z);
}

/* rotation with dir as its y axis (second column) */
void Matrix_SetYAxis(float *m, const Vec3f *dir)
{
    float x = dir->x, y = dir->y, z = dir->z;
    float az = dir->z < 0.0f ? -dir->z : dir->z;
    Vec3f u, v, w;
    long double inv, wz;

    if (az > kNearlyUp) {
        u.x = 0.0f;
        u.y = (float)x87_neg(x87_sub_zero(z));
        u.z = (float)x87_sub_zero(y);
        if (!Vec3f_IsZero(&u)) {
            inv = 1.0 / __builtin_sqrtl((long double)u.y * u.y + (long double)u.z * u.z + (long double)u.x * u.x);
            u.x = u.x * inv;
            u.y = u.y * inv;
            u.z = u.z * inv;
        }
        v.x = (long double)y * u.z - (long double)z * u.y;
        v.y = x87_neg((long double)x * u.z - (long double)z * u.x);
        v.z = (long double)x * u.y - (long double)y * u.x;
        w = v;
        wz = w.z;
        if (!Vec3f_IsZero(&v)) {
            inv = 1.0 / __builtin_sqrtl((long double)v.x * v.x + (long double)v.y * v.y + (long double)v.z * v.z);
            w.x = v.x * inv;
            w.y = w.y * inv;
            wz = inv * w.z;
        }
    } else {
        v.x = (float)x87_sub_zero(y);
        v.y = (float)x87_neg(x87_sub_zero(x));
        v.z = 0.0f;
        w = v;
        wz = w.z;
        if (!Vec3f_IsZero(&v)) {
            inv = 1.0 / __builtin_sqrtl((long double)v.x * v.x + (long double)v.y * v.y + (long double)v.z * v.z);
            w.x = v.x * inv;
            w.y = w.y * inv;
            wz = inv * w.z;
        }
        v.x = (long double)z * w.y - (long double)y * wz;
        v.y = x87_neg((long double)z * w.x - (long double)x * wz);
        v.z = (long double)y * w.x - (long double)x * w.y;
        u = v;
        if (!Vec3f_IsZero(&v)) {
            inv = 1.0 / __builtin_sqrtl((long double)v.x * v.x + (long double)v.y * v.y + (long double)v.z * v.z);
            u.x = v.x * inv;
            u.y = u.y * inv;
            u.z = u.z * inv;
        }
    }
    m[0] = w.x;
    m[1] = x;
    m[2] = u.x;
    m[3] = w.y;
    m[4] = y;
    m[5] = u.y;
    m[6] = wz;
    m[7] = z;
    m[8] = u.z;
}
HOOK(0x414A60, Matrix_SetYAxis);

void Model_GetPoint(int *out, ModelTask *t, int i)
{
    if (TaskType_IsA(t->task.type, RigidDynCubeObj_GetType())) {
        if (t->model && i < ModelLod_GetVertexCount(*t->model)) {
            Mesh3D_GetWorldVertex(out, &t->task, i);
            return;
        }
    } else if (TaskType_IsA(t->task.type, BoneDynCubeObj_GetType())) {
        if (t->model2 && i < ModelLod_GetVertexCount(*t->model2)) {
            Mesh3D_GetBoneWorldVertex(out, &t->task, i);
            return;
        }
    }
    memset(out, 0, 6 * sizeof(int));
}
HOOK(0x414E20, Model_GetPoint);

void Model_GetLocalPoint(int *out, ModelTask *t, int i)
{
    if (TaskType_IsA(t->task.type, RigidDynCubeObj_GetType()) && t->model && i < ModelLod_GetVertexCount(*t->model)) {
        Mesh3D_GetVertex(out, &t->task, i);
        return;
    }
    memset(out, 0, 3 * sizeof(int));
}
HOOK(0x414ED0, Model_GetLocalPoint);

void Model_GetBonePoint(int *out, ModelTask *t, int i, const char *name)
{
    void **model;

    if (!TaskType_IsA(t->task.type, RigidDynCubeObj_GetType()) && TaskType_IsA(t->task.type, BoneDynCubeObj_GetType())) {
        model = VirModel_Find(name);
        if (model && i < ModelLod_GetVertexCount(*model)) {
            Mesh3D_GetBoneWorldVertex(out, &t->task, i, name);
            return;
        }
    }
    memset(out, 0, 6 * sizeof(int));
}
HOOK(0x414F40, Model_GetBonePoint);

/* key held with left ctrl and shift, once the player has unlocked cheats */
int Game_CheatKeyDown(int key)
{
    if ((gKeys[key] & 0x80) && (gKeys[KEY_LCONTROL] & 0x80) && (gKeys[KEY_LSHIFT] & 0x80) &&
        Profile_GetActive() && *(int *)(Profile_GetActive() + 0x560) >= 14 && gCheatsEnabled)
        return 1;
    return 0;
}
HOOK(0x414FD0, Game_CheatKeyDown);

/* same, but only on the frame the key goes down */
int Game_CheatKeyPressed(int key)
{
    if ((gKeys[key] & 0x80) && !(gKeysPrev[key] & 0x80) && (gKeys[KEY_LCONTROL] & 0x80) &&
        (gKeys[KEY_LSHIFT] & 0x80) && Profile_GetActive() && *(int *)(Profile_GetActive() + 0x560) >= 14 &&
        gCheatsEnabled)
        return 1;
    return 0;
}
HOOK(0x415020, Game_CheatKeyPressed);
