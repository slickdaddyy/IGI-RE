#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "fxobj/fxobj.h"
#include "task/taskvars.h"

FN_EXTERN(AIStationaryGun_GetDefSize);

ORIG(0x408320, uint8_t, GameMaterial_GetSizeEvent, (void));
ORIG(0x408330, uint8_t, GameMaterial_GetParseEvent, (void));
ORIG(0x408340, uint8_t, GameMaterial_GetImpactEvent, (void));
ORIG(0x4148F0, long double, Vec3d_PitchAngle, (Vec3d v));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x448550, uint16_t, GenericTBA_GetType, (void));
ORIG(0x448560, void *, GenericTBA_GetParams, (void));
ORIG(0x47E6F0, uint8_t, BulletHole_GetEvent, (void));
ORIG(0x4810A0, void, Explosion_New, (Vec3d *pos, int a, int b, int c, float d, int e, int f, int count, int g, int h,
                                  const char *name, const char *name2, int i, int j, float k, void *dir, float l,
                                  float m, float n, int o));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B4740, unsigned int, Random_Next, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *base, int count));
ORIG(0x4FDF00, void *, Particle2D_New, (void *root, void *world, void *sprite, Vec3f *dir, Vec3f *scale, float *size,
                                    float b, int blend, int d, float e, int f, int count, int h));
ORIG(0x4FE120, void, Particle2D_Add, (void *ps, Vec3d *pos, Vec3f *dir, float size, float a, float b, float c, int d,
                                  int e));

/* what a material shows when a bullet hits it (0x60 bytes) */
typedef struct GunShotDef {
    char dust;
    char debris;
    char decal[24];
    char decal2[24];
    char sound[32];
    float decalSize;
    char *decalTex;
    char *decal2Tex;
} GunShotDef;

/* a bullet hit, as sent to the material handlers */
typedef struct ShotHit {
    Task *task;
    int unk4;
    Vec3d pos;
    char unk20[0x18];
    Vec3f normal;
} ShotHit;

typedef struct ImpactArg {
    ShotHit *hit;
    GunShotDef *def;
    int unk8[3];
    int exit;
} ImpactArg;

/* sent to the task that was hit to leave a decal */
typedef struct DecalArg {
    ShotHit *hit;
    int unk4;
    char *texture;
    float size;
} DecalArg;

/* "PlaceExplosiveTBA" (0x2B0 bytes), a spot where the player can plant a charge */
typedef struct PlaceExplosive {
    Task task;
    Vec3d pos;
    char unk38[0x240 - 0x38];
    char active;
    char unk241[0x2A8 - 0x241];
    float range;
    int targetId;
} PlaceExplosive;

/* the player's search for the charge spot he is looking at */
typedef struct AimArg {
    int unk0[2];
    Vec3d eye;
    Matrix view;
    char unk48[0x70 - 0x48];
    float pitch;
    float yaw;
    int unk78[2];
    PlaceExplosive *found;
} AimArg;

_Static_assert(sizeof(GunShotDef) == 0x60, "GunShotDef");
_Static_assert(offsetof(ShotHit, normal) == 0x38, "ShotHit");
_Static_assert(offsetof(PlaceExplosive, active) == 0x240, "PlaceExplosive");
_Static_assert(sizeof(PlaceExplosive) == 0x2B0, "PlaceExplosive");
_Static_assert(offsetof(AimArg, found) == 0x80, "AimArg");

extern uint16_t gGunShotEffectType;
extern char *gGunShotDust;
extern char *gGunShotDust2;
extern uint16_t gPlaceExplosiveType;
extern void *gPlaceExplosiveParams;

extern const char gEmptyString[];
#define GameMaterial_GetSize (FN(0x46FB50, AIStationaryGun_GetDefSize))

static const double kDecalUnit = 4096.0;
static const double kThree = 3.0;
static const double kQuarter = 0.25;
static const double kThreeQuarters = 0.75;
static const double kTenth15 = 0x1.3333333333333p-3;
static const double kZero = 0.0;
static const float kZeroF = 0.0f;
static const float kHalfF = 0.5f;
static const float kUnit = 4096.0f;
static const float kDustSpeed = 4096000.0f;
static const float kDustScale = 0x1.36b06ep-17f;
static const float kSpreadScale = 0x1.aceeap-5f;
static const float kSpreadBase = 0x1.a3667ap-7f;
static const float kDust2Scale = 1.25f;

void GunShotEffect_Impact(void *t, ImpactArg *a);
void GunShotEffect_ParseArgs(void *t, ParseArg *a);

void GunShotEffect_Init(void)
{
    gGunShotEffectType = TaskType_New("GunShotEffect", 0x20, Task_GetRootType(), 0);
    TaskType_SetHandler(gGunShotEffectType, GameMaterial_GetImpactEvent(), GunShotEffect_Impact);
    TaskType_SetHandler(gGunShotEffectType, GameMaterial_GetParseEvent(), GunShotEffect_ParseArgs);
    TaskType_SetHandler(gGunShotEffectType, GameMaterial_GetSizeEvent(), GameMaterial_GetSize);
    Script_RegisterConstant("TASKTYPE_GUNSHOTEFFECT", gGunShotEffectType);
    gGunShotDust = Game_AddSprite("concrete1.spr");
    gGunShotDust2 = Game_AddSprite("concrete2.spr");
}
HOOK(0x438390, GunShotEffect_Init);

/* debris, a decal on the task that was hit and a puff of dust */
void GunShotEffect_Impact(void *t, ImpactArg *a)
{
    ShotHit *hit = a->hit;
    GunShotDef *def = a->def;
    int isExcluded;
    char *tex;

    (void)t;
    if (hit->task) {
        uint16_t base = BoneDynCubeObj_GetType();

        isExcluded = TaskType_IsA(hit->task->type, base);
    } else {
        isExcluded = 0;
    }
    if (def->debris && !isExcluded) {
        int n = Random_Int(Game_GetRandom(), 3);

        Explosion_New(&a->hit->pos, 1, 0, 0, 2048.0f, 0, 0, n + 2, 0, 0, def->sound, gEmptyString, -1, 2,
                   20480.0f, a->unk8, 0x1.0c1524p-1f, 340.0f, 680.0f, 0);
    }
    if (hit->task && ((a->exit && (tex = def->decalTex)) || (tex = def->decal2Tex))) {
        if (HANDLER(BulletHole_GetEvent(), hit->task->type)) {
            DecalArg d;
            Task *task;

            d.texture = tex + 0x18;
            task = hit->task;
            d.hit = hit;
            d.unk4 = 0;
            d.size = def->decalSize;
            HANDLER(BulletHole_GetEvent(), task->type)(task, &d);
        }
    }
    if (def->dust && !isExcluded && (Random_Next(Game_GetRandom()) & 1)) {
        Vec3f zero, color, dir;
        float size[2];
        void *ps, *world, *root;
        Vec3d p, p2;
        long double s, z, f;
        float sz, spread;

        zero.x = 0.0f;
        zero.y = 0.0f;
        zero.z = 0.0f;
        color.x = 127.0f;
        color.y = 127.0f;
        color.z = 127.0f;
        size[0] = 163.84f;
        size[1] = 163.84f;
        world = Game_GetWorld();
        root = Level_GetDynamic();
        ps = Particle2D_New(root, world, gGunShotDust + 0x18, &zero, &color, size, 409600.0f, 0x1080000, 0,
                        0x1.6c16c2p-4f, 0, 2, 0);
        p.x = hit->normal.x;
        p.y = hit->normal.y;
        p.z = hit->normal.z;
        Vec3d_Normalize(&p);
        s = size[0] * kThree;
        p.x = s * p.x;
        p.y = s * p.y;
        p.z = s * p.z;
        p.x = p.x + hit->pos.x;
        p.y = p.y + hit->pos.y;
        z = p.z + hit->pos.z;
        dir.x = 0.0f;
        dir.y = 0.0f;
        dir.z = 0.0f;
        p.z = z;
        p2 = p;
        p2.z = size[0] * kQuarter + z;
        dir.z = (Random_Float(Game_GetRandom()) * kThreeQuarters + kTenth15) * kDustSpeed * kDustScale;
        sz = Random_Float(Game_GetRandom()) * kQuarter + kThreeQuarters;
        f = Random_Float(Game_GetRandom()) - kHalfF;
        spread = (f + f) * kSpreadScale + kSpreadBase;
        Particle2D_Add(ps, &p, &dir, sz, spread, 0, 1.0f, 15, 30);
        dir.z = dir.z * kDust2Scale;
        f = Random_Float(Game_GetRandom()) - kHalfF;
        spread = (f + f) * kSpreadScale + kSpreadBase;
        Particle2D_Add(ps, &p2, &dir, sz, spread, 0, 1.0f, 15, 30);
    }
}
HOOK(0x438440, GunShotEffect_Impact);

void GunShotEffect_ParseArgs(void *t, ParseArg *a)
{
    GunShotDef *d = a->out;
    void *args = a->args;
    int i = a->index;

    (void)t;
    d->dust = Script_GetArgInt(args, i);
    d->debris = Script_GetArgInt(args, i + 1);
    Script_GetArgString(args, i + 2, d->decal, 24);
    Script_GetArgString(args, i + 3, d->decal2, 24);
    d->decalSize = Script_GetArgReal(args, i + 4) * kDecalUnit;
    Script_GetArgString(args, i + 5, d->sound, 32);
    d->decalTex = strlen(d->decal) ? Game_AddTexture(d->decal) : NULL;
    d->decal2Tex = strlen(d->decal2) ? Game_AddTexture(d->decal2) : NULL;
    a->used = i + 6 - a->index;
}
HOOK(0x4387B0, GunShotEffect_ParseArgs);

void GunShotEffect_Close(void)
{
    TaskType_Free(gGunShotEffectType);
}
HOOK(0x438890, GunShotEffect_Close);

int PlaceExplosiveTBA_GetTargetId(PlaceExplosive *t)
{
    return t->targetId;
}
HOOK(0x4388A0, PlaceExplosiveTBA_GetTargetId);

void PlaceExplosiveTBA_Create(PlaceExplosive *t, void *arg);
void PlaceExplosiveTBA_GetParams(void *t, void **out);
void PlaceExplosiveTBA_CheckAim(PlaceExplosive *t, AimArg *a);
void PlaceExplosiveTBA_GetKind(void *t, int *out);

void PlaceExplosiveTBA_Init(void)
{
    PlaceExplosive p;

    gPlaceExplosiveType = TaskType_New("PlaceExplosiveTBA", sizeof(PlaceExplosive), GenericTBA_GetType(), 0);
    TaskType_SetHandler(gPlaceExplosiveType, 0, PlaceExplosiveTBA_Create);
    TaskType_SetHandler(gPlaceExplosiveType, Param_GetTableEvent(), PlaceExplosiveTBA_GetParams);
    TaskType_SetHandler(gPlaceExplosiveType, Game_GetEvent2(), PlaceExplosiveTBA_CheckAim);
    TaskType_SetHandler(gPlaceExplosiveType, Game_GetEvent3(), PlaceExplosiveTBA_GetKind);
    gPlaceExplosiveParams = ParamTable_Extend(GenericTBA_GetParams(), 2);
    ParamTable_Add(gPlaceExplosiveParams, &p, &p.range, Param_Real32(), 7, NULL);
    ParamTable_Add(gPlaceExplosiveParams, &p, &p.targetId, Param_Int32(), 7, NULL);
}
HOOK(0x4388B0, PlaceExplosiveTBA_Init);

void PlaceExplosiveTBA_Create(PlaceExplosive *t, void *arg)
{
    gTaskHandlers[0][GenericTBA_GetType()](t, arg);
    t->range = 0.75f;
    t->targetId = -1;
}
HOOK(0x4389A0, PlaceExplosiveTBA_Create);

/* keeps the spot nearest to the middle of the view, if in range and ahead */
void PlaceExplosiveTBA_CheckAim(PlaceExplosive *t, AimArg *a)
{
    const float *m = a->view.m;
    Vec3d p, d, l;
    long double ly, yaw, r;
    float pitch;

    if (!t->active)
        return;
    Model_GetPoint(&p, t, 1);
    p.x = p.x + t->pos.x;
    p.y = p.y + t->pos.y;
    p.z = p.z + t->pos.z;
    d.x = p.x - a->eye.x;
    d.y = p.y - a->eye.y;
    d.z = p.z - a->eye.z;
    if (Vec3d_Length(&d) >= t->range * kUnit || a->found)
        return;
    l.x = m[6] * d.z + m[3] * d.y + m[0] * d.x;
    ly = m[7] * d.z + m[4] * d.y + m[1] * d.x;
    l.y = ly;
    l.z = m[8] * d.z + m[5] * d.y + m[2] * d.x;
    d = l;
    if (!(ly > kZero))
        return;
    r = Vec3d_PitchAngle(d);
    pitch = r < kZeroF || r != r ? -Vec3d_PitchAngle(d) : Vec3d_PitchAngle(d);
    r = Vec3d_YawAngle(d);
    yaw = r < kZeroF || r != r ? -Vec3d_YawAngle(d) : Vec3d_YawAngle(d);
    if (!(pitch >= a->pitch) && !(yaw >= a->yaw)) {
        a->found = t;
        a->yaw = yaw;
        a->pitch = pitch;
    }
}
HOOK(0x4389E0, PlaceExplosiveTBA_CheckAim);

void PlaceExplosiveTBA_GetKind(void *t, int *out)
{
    (void)t;
    *out = 13;
}
HOOK(0x438BF0, PlaceExplosiveTBA_GetKind);

void PlaceExplosiveTBA_GetParams(void *t, void **out)
{
    (void)t;
    *out = gPlaceExplosiveParams;
}
HOOK(0x438C00, PlaceExplosiveTBA_GetParams);

void PlaceExplosiveTBA_Close(void)
{
    QTaskList_Free(gPlaceExplosiveParams);
    TaskType_Free(gPlaceExplosiveType);
}
HOOK(0x438C10, PlaceExplosiveTBA_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53C1DC) uint16_t gPlaceExplosiveType INIT(385);
PAD(igd, 53C1DC, 53C1DE, 0x3A) INIT({0x00, 0x00, 0x50, 0x6C, 0x61, 0x63, 0x65, 0x45, 0x78, 0x70, 0x6C, 0x6F, 0x73, 0x69, 0x76, 0x65, 0x54, 0x42, 0x41, 0x00, 0x00, 0x00, 0x52, 0x6F, 0x74, 0x61, 0x74, 0x69, 0x6E, 0x67, 0x4F, 0x62, 0x6A, 0x65, 0x63, 0x74, 0x00, 0x00, 0x50, 0xC2, 0x53, 0x00, 0x44, 0xC2, 0x53, 0x00, 0x34, 0xC2, 0x53, 0x00, 0x28, 0xC2, 0x53, 0x00, 0x18, 0xC2, 0x53});
DATA(53C1DC) char str_53C218[14] INIT("Explode sound");
PAD(igd, 53C1DC, 53C226, 0x2);
DATA(53C1DC) char str_53C228[9] INIT("On sound");
PAD(igd, 53C1DC, 53C231, 0x3);
DATA(53C1DC) char str_53C234[16] INIT("Destroyde model");
DATA(53C1DC) char str_53C244[10] INIT("Off model");
PAD(igd, 53C1DC, 53C24E, 0x2);
DATA(53C1DC) char str_53C250[9] INIT("On model");
PAD(igd, 53C1DC, 53C259, 0x3);
PAD(igd, 544648, 544648, 0x1) INIT({0xB6});
DATA(544648) uint8_t dat_544649[510] INIT({3, 0, 0, 207, 162, 228, 162, 26, 0, 229, 162, 232, 162, 91, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 129, 254, 0, 0, 0, 0, 0, 0, 64, 126, 161, 254, 0, 0, 0, 0, 81, 5, 0, 0, 81, 218, 94, 218, 32, 0, 95, 218, 106, 218, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 129, 211, 216, 222, 224, 249, 0, 0, 49, 126, 129, 254, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 176, 70, 84, 0, 176, 70, 84, 0, 200, 70, 84, 0, 200, 70, 84, 0, 255, 255, 255, 255, 255, 255, 255, 255, 240, 0, 0, 0, 241});
DATA(544648) char str_544847[1] INIT("");
BSS(57BE78) uint16_t gGunShotEffectType;
PAD(igb, 57BE78, 57BE7A, 0x2);
BSS(57BE78) char *gGunShotDust;
BSS(57BE78) char *gGunShotDust2;
PAD(igb, 57BE78, 57BE84, 0x8);
BSS(57BE78) void *gPlaceExplosiveParams;
