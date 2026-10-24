#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "explosion/explosion.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416930, char *, Game_AddTexture, (const char *name));
ORIG(0x4169D0, char *, Game_AddSprite, (const char *name));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x450CE0, void, HumanAI_Broadcast, (FlashNoise *noise));
ORIG(0x460C60, QTaskList *, Human_GetList, (void));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4FDF00, void *, Particle2D_New, (void *root, void *world, void *sprite, Vec3f *dir, Vec3f *color, float *size,
                                    float life, int blend, int d, float e, int f, int count, void *h));
ORIG(0x4FE120, void, Particle2D_Add, (void *ps, Vec3d *pos, Vec3f *dir, float size, float a, float b, float c, int d,
                                  int e));

uint8_t BulletHole_GetEvent(void);

static const double kOne = 1.0;
static const double kOneHalf = 1.5;
static const double kQuarter = 0.25;
static const float kHalf = 0.5f;
static const float kRange = 40960.0f;
static const float kDecalSize = 2048.0f;
static const float kSmokeSize = 0x1.333334p+11f; /* 2457.6 */
static const float kLife = 819200.0f;
static const float kRate = 0x1.dddddep-7f;
extern const float kRise;
extern const float kRiseScale;
extern const float kSpread;
extern const float kSpreadBase;

void FlashBang_Create(FlashBang *t);
void FlashBang_Run(FlashBang *t);

void FlashBang_Init(void)
{
    gFlashEvent = TaskEvent_Alloc(0);
    gFlashType = TaskType_New("FlashBang", 0x48, Task_GetRootType(), 4);
    TaskType_SetHandler(gFlashType, 0, FlashBang_Create);
    TaskType_SetHandler2(gFlashType, 1, FlashBang_Run);
    TaskType_SetHandler2(gFlashType, 2, TASK_NOTHING);
    gFlashTexture = Game_AddTexture("burn1.tex");
    gFlashSprite = Game_AddSprite("smoke1.spr");
}
HOOK(0x47E700, FlashBang_Init);

void FlashBang_Create(FlashBang *t)
{
    Vec3f dir = {0.0f, 0.0f, 0.0f};
    Vec3f color = {127.0f, 127.0f, 127.0f};
    float size[2] = {kSmokeSize, kSmokeSize};
    void *world = Game_GetWorld();
    void *root = Level_GetDynamic();

    t->ps = Particle2D_New(root, world, gFlashSprite + 0x18, &dir, &color, size, kLife, 0x1000008, 0, kRate, 0, 20, NULL);
}
HOOK(0x47E790, FlashBang_Create);

/* three puffs of white smoke, a sound, the AI hears it, then it is gone */
void FlashBang_Run(FlashBang *t)
{
    FlashNoise noise;
    Task *sound;
    int n;

    for (n = 3; n; n--) {
        Vec3d p = t->pos;
        Vec3f v;
        long double r;
        float s, a;

        v.x = 0.0f;
        v.y = 0.0f;
        v.z = (float)((Random_Float(Game_GetRandom()) + kOne) * kOneHalf * kRise * kRiseScale);
        s = (float)((Random_Float(Game_GetRandom()) + kOne) * kQuarter);
        r = Random_Float(Game_GetRandom()) - kHalf;
        a = (float)((r + r) * kSpread + kSpreadBase);
        Particle2D_Add(t->ps, &p, &v, s, a, 0.0f, 1.0f, 90, 30);
    }
    sound = Sound_Play(Level_GetDynamic(), "explo_flash");
    if (sound)
        Sound_SetPos(sound, &t->pos);
    noise.type = 0x13;
    noise.unk4 = -1;
    noise.unk8 = 3;
    noise.range = kRange;
    noise.pos = &t->pos;
    noise.source = &t->task;
    noise.unk18 = 0;
    HumanAI_Broadcast(&noise);
    Task_Delete(t);
}
HOOK(0x47E820, FlashBang_Run);

void FlashBang_Close(void)
{
    TaskEvent_Free(gFlashEvent);
    TaskType_Free(gFlashType);
}
HOOK(0x47E970, FlashBang_Close);

uint8_t FlashBang_GetEvent(void)
{
    return gFlashEvent;
}
HOOK(0x47E990, FlashBang_GetEvent);

/* tells every human about the flash and burns a mark where it hit */
FlashBang *FlashBang_Explode(Vec3d *pos, HoleHit *hit, float radius, float falloff)
{
    FlashBang *t = (FlashBang *)Task_New(Level_GetDynamic(), gFlashType, NULL);
    struct {
        Vec3d *pos;
        float radius;
    } msg;
    int i;

    t->radius = radius;
    t->falloff = falloff;
    t->pos = *pos;
    msg.pos = pos;
    msg.radius = radius;
    for (i = 0; i < Human_GetList()->count; i++) {
        uint16_t type = Human_GetList()->items[i]->type;
        TaskFn fn = HANDLER(FlashBang_GetEvent(), type);

        fn(Human_GetList()->items[i], &msg);
    }
    if (hit && hit->task && HANDLER(BulletHole_GetEvent(), hit->task->type)) {
        HoleMsg m;
        Task *task = hit->task;

        m.hit = hit;
        m.texture = gFlashTexture + 0x18;
        m.unk4 = 0;
        m.size = kDecalSize;
        HANDLER(BulletHole_GetEvent(), task->type)(task, &m);
    }
    return t;
}
HOOK(0x47E9A0, FlashBang_Explode);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5334A4) const float kRise INIT(4.096e+06f);
ALIAS(kKmh, kRise, 0);
ALIAS(kKmhToUnits, kRise, 0);
DATA(533870) const float kSpread INIT(0.043633234f);
ALIAS(kPitchStep, kSpread, 0);
DATA(540914) uint8_t gFlashEvent INIT(255);
PAD(igd, 540914, 540915, 0x3);
BSS(5C1144) uint16_t gFlashType;
PAD(igb, 5C1144, 5C1146, 0x2);
BSS(5C1144) char *gFlashTexture;
BSS(5C1144) char *gFlashSprite;
