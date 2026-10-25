/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "camera/camera.h"
#include "humanai/humanaitypes.h"
#include "explosion/explosiontypes.h"
#include "task/taskvars.h"
#include "explosion/explosionvars.h"

/* sent to the object an explosion burns a mark on */
typedef struct BurnMsg {
    Task **target;
    int unk4;
    void *texture;
    float unkC;
} BurnMsg;

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x450CE0, void, HumanAI_Broadcast, (AIMessage *noise));
ORIG(0x47E6F0, uint8_t, BulletHole_GetEvent, (void));
ORIG(0x47F200, void, Explosion_Burst, (Explosion *e));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4B47C0, int, Random_Int, (void *rng, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4D0D30, void, ModelObj_FreeLodData, (void *p));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x503C00, void *, SimpointObjContainer_New, (int frames, const char *a, const char *b, float c, float d, float e,
                                              float f, float *dir, int h));

static const double kOne = 1.0;
static const float kHalf = 0.5f;
static const float kRise = 0x1.333334p-2f;     /* 0.3 */
static const float kSpin = 0x1.41b2f8p-1f;     /* 0.628 */
static const float kSmokeScale = 0x1.ccccccp-13f;

void FireBall_DeleteHandler(char *f)
{
    ModelObj_FreeLodData(f + 0xC0);
    DynCube_Remove(Game_GetWorld(), f);
}
HOOK(0x480F00, FireBall_DeleteHandler);

void Explosion_CreateHandler(Explosion *e)
{
    e->timer = 0;
    e->unk84 = 0;
}
HOOK(0x480F30, Explosion_CreateHandler);

void Explosion_RunHandler(Explosion *e)
{
    if (e->timer)
        e->timer--;
    if (e->timer || !e->bursts)
        return;
    Explosion_Burst(e);
    if (--e->bursts == 0) {
        Task_Delete(e);
        return;
    }
    e->timer = Random_Int(Game_GetRandom(), e->interval);
}
HOOK(0x480F40, Explosion_RunHandler);

void Explosion_Close(void)
{
    TaskType_Free(gExplosionSmokeType);
    TaskType_Free(gFragmentType);
    TaskType_Free(gFireBallType);
    TaskType_Free(gExplosionType);
}
HOOK(0x480FA0, Explosion_Close);

Explosion *Explosion_New(Vec3d *pos, int count, int delay, float unk50, float radius, float unk58, float burnSize,
                         int unk68, int unk6C, Task **burn, const char *sprite, const char *sound, int noiseType,
                         int noiseUnk8, float noiseRadius, float *dir, float angle, float speedMin, float speedMax,
                         int unk84);

/* an explosion thrown in a random direction */
Explosion *Explosion_Spawn(Vec3d *pos, int count, int delay, float unk50, float radius, float unk58,
                           float burnSize, int unk68, int unk6C, Task **burn, const char *sprite, const char *sound,
                           int noiseType, int noiseUnk8, float noiseRadius, int unk84)
{
    float dir[3];

    dir[0] = Random_Float(Game_GetRandom()) - kHalf;
    dir[1] = Random_Float(Game_GetRandom()) - kHalf;
    dir[2] = Random_Float(Game_GetRandom()) - kRise;
    return Explosion_New(pos, count, delay, unk50, radius, unk58, burnSize, unk68, unk6C, burn, sprite, sound,
                         noiseType, noiseUnk8, noiseRadius, dir, 0x1.921fb6p+0f, 272.0f, 1360.0f, unk84);
}
HOOK(0x480FE0, Explosion_Spawn);

Explosion *Explosion_New(Vec3d *pos, int count, int delay, float unk50, float radius, float unk58, float burnSize,
                         int unk68, int unk6C, Task **burn, const char *sprite, const char *sound, int noiseType,
                         int noiseUnk8, float noiseRadius, float *dir, float angle, float speedMin, float speedMax,
                         int unk84)
{
    Explosion *e = (Explosion *)Task_New(Level_GetDynamic(), gExplosionType, NULL);
    float v[3];

    e->pos = *pos;
    e->interval = delay;
    e->bursts = count;
    e->timer = delay;
    e->blast = unk50;
    e->unk58 = unk58;
    e->size = radius;
    e->dir.x = dir[0];
    e->dir.y = dir[1];
    e->dir.z = dir[2];
    if (!x87_eq(e->dir.x, 0) || !x87_eq(e->dir.y, 0) || !x87_eq(e->dir.z, 0)) {
        long double inv = kOne / x87_sqrt(e->dir.x * e->dir.x + e->dir.y * e->dir.y + e->dir.z * e->dir.z);

        e->dir.x = e->dir.x * inv;
        e->dir.y = e->dir.y * inv;
        e->dir.z = e->dir.z * inv;
    }
    e->spread = angle;
    e->minSpeed = speedMin;
    e->maxSpeed = speedMax;
    e->fragments = unk68;
    e->fireballs = unk6C;
    if (burn && *burn && gTaskHandlers[BulletHole_GetEvent()][(*burn)->type]) {
        BurnMsg msg;
        Task *t = *burn;

        msg.target = burn;
        msg.texture = gBurnTexture + 0x18;
        msg.unk4 = 0;
        msg.unkC = burnSize;
        gTaskHandlers[BulletHole_GetEvent()][t->type](t, &msg);
    }
    strcpy(e->sound, sound);
    v[0] = Random_Float(Game_GetRandom()) * kSpin;
    v[1] = Random_Float(Game_GetRandom()) * kSpin;
    v[2] = Random_Float(Game_GetRandom()) * kSpin;
    e->group = SimpointObjContainer_New(0x2D, sprite, NULL, 0x1.f5c29p-1f, 0x1.333334p-1f, 0x1.64ce9ep+5f,
                                    e->size * kSmokeScale, v, 0);
    if (noiseType != -1) {
        AIMessage n;

        n.type = noiseType;
        n.team = 1;
        n.flags = noiseUnk8;
        n.radius = noiseRadius;
        n.pos = pos;
        n.from = (Task *)e;
        n.about = NULL;
        HumanAI_Broadcast(&n);
    }
    e->unk84 = unk84;
    return e;
}
HOOK(0x4810A0, Explosion_New);
