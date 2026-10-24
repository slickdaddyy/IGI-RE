#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "fxobj/fxobj.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B34F0, void, Matrix_RotateX, (Matrix *out, const Matrix *m, float a));
ORIG(0x4B3790, void, Matrix_RotateZ, (Matrix *out, const Matrix *m, float a));
ORIG(0x4B4720, void, Random_Init, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B7FC0, void, Script_RegisterReal, (const char *name, double *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4CEBA0, void *, VirModel_Add, (const char *name));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F80, void *, Param_Real64, (void));
ORIG(0x4FDF00, void *, Particle2D_New, (void *root, void *world, void *sprite, Vec3f *dir, Vec3f *scale, float *size,
                                    float b, int blend, int d, float e, int f, int count, int h));
ORIG(0x4FE210, void, Particle2D_Emit, (void *ps, Vec3d *pos, Vec3f *dir, float spread, float a, float b, float c, float d,
                                  int e, float f, int g, int h));
ORIG(0x4FE5D0, void, Particle2D_SetAutoDelete, (void *ps, int on));
ORIG(0x4FE610, void, Particle2D_Translate, (void *ps, Vec3d *delta));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

/* a particle emitter ("Smoke", 0x178 bytes) */
typedef struct Smoke {
    Task task;
    char registered;
    char pad21[7];
    Vec3d origin;
    Vec3d pos;
    Vec3d lastPos;
    Vec3f dir;
    float pitch;
    float yaw;
    float unk84;
    float unk88;
    float unk8C;
    float spread;
    float radius;
    double rate;
    float acc;
    VarString expr;
    void *ps;
    int count;
    float unkFC;
    float unk100;
    Vec3f unk104;
    int blend;
    float unk114;
    int sprite;
    float unk11C;
    float unk120;
    float unk124;
    float lastUnk124;
    int unk12C;
    Vec3d local;
    Matrix localMatrix;
    char enabled;
    char moved;
} Smoke;

/* sent by a moving parent each frame */
typedef struct MoveArg {
    Vec3d *pos;
    float *m;
} MoveArg;

_Static_assert(offsetof(Smoke, expr) == 0xA4, "Smoke.expr");
_Static_assert(offsetof(Smoke, local) == 0x130, "Smoke.local");
_Static_assert(offsetof(Smoke, enabled) == 0x170, "Smoke.enabled");
_Static_assert(sizeof(Smoke) == 0x178, "Smoke");

extern char *gSmokeFireball;
extern char gSmokeRandom[];
extern uint16_t gSmokeType;
extern void *gSmokeParams;
extern void *gSmokeModel;
extern char *gSmokeBlood;
extern char *gSmokeSmoke2;
extern char *gSmokeSmoke1;
extern const char *gSmokeKindNames[];
extern ParamEnum gSmokeKindEnum;
extern RangeF gSmokeUnk84Range;
extern RangeF gSmokeYawRange;
extern RangeF gSmokeUnk88Range;
extern RangeF gSmokePitchRange;
extern RangeF gSmokeRadiusRange;
extern RangeD gSmokeRateRange;
extern RangeF gSmokeUnk124Range;
extern RangeF gSmokeUnk114Range;
extern RangeI gSmokeSpriteRange;
extern RangeI gSmokeCountRange;
extern RangeI gSmokeBlendRange;
extern RangeF gSmokeUnkFCRange;

static const float kTwoPi = 0x1.921fb6p+2f;
static const float kUnit = 4096.0f;
static const float kRate = 30.0f;
static const float kPerFrame = 0x1.111112p-5f;
static const float kSpeed = 136.0f;
static const float kScale = 63.0f;
static const float kOneF = 1.0f;
static const float kZeroF = 0.0f;
static const float kLife = 409600.0f;

static inline long double x87_cos(long double x)
{
    __asm__("fcos" : "+t"(x));
    return x;
}

static inline long double x87_sin(long double x)
{
    __asm__("fsin" : "+t"(x));
    return x;
}

void Smoke_Create(Smoke *t);
void Smoke_Run(Smoke *t);
void Smoke_Delete(Smoke *t);
void Smoke_GetParams(void *t, void **out);
void Smoke_Changed(Smoke *t);
void Smoke_Activate(Smoke *t);
void Smoke_Deactivate(Smoke *t);
void Smoke_Move(Smoke *t, MoveArg *a);

void Smoke_Init(void)
{
    Smoke s;

    gSmokeKindEnum.count = 3;
    gSmokeKindEnum.names = gSmokeKindNames;
    gSmokeUnk88Range.min = 0.0f;
    gSmokeUnk88Range.max = 14.6484375f;
    gSmokeRateRange.min = 0.0;
    gSmokeRateRange.max = 150.0;
    gSmokeCountRange.min = 1;
    gSmokeCountRange.max = 255;
    gSmokeUnkFCRange.min = 0.0f;
    gSmokeUnkFCRange.max = 4.0f;
    gSmokeRadiusRange.min = 0.0f;
    gSmokeRadiusRange.max = 5.0f;
    gSmokeUnk84Range.min = 0.0f;
    gSmokeUnk84Range.max = 5.0f;
    gSmokeUnk114Range.min = -150.0f;
    gSmokeUnk114Range.max = 150.0f;
    gSmokeSpriteRange.min = 0;
    gSmokeSpriteRange.max = 3;
    gSmokeBlendRange.min = 0;
    gSmokeBlendRange.max = 2;
    gSmokePitchRange.min = 0.0f;
    gSmokePitchRange.max = 0x1.921fb6p+1f;
    gSmokeYawRange.min = -0x1.921fb6p+1f;
    gSmokeYawRange.max = 0x1.921fb6p+1f;
    gSmokeUnk124Range.min = 0.0f;
    gSmokeUnk124Range.max = 1.0f;
    gSmokeModel = VirModel_Add("1729_01_1");
    gSmokeType = TaskType_New("Smoke", sizeof(Smoke), EditorRigidObj_GetType(), 0);
    TaskType_SetHandler(gSmokeType, 0, Smoke_Create);
    TaskType_SetHandler2(gSmokeType, 1, Smoke_Run);
    TaskType_SetHandler2(gSmokeType, 2, Smoke_Delete);
    TaskType_SetHandler(gSmokeType, Param_GetTableEvent(), Smoke_GetParams);
    TaskType_SetHandler(gSmokeType, Param_GetChangedEvent(), Smoke_Changed);
    TaskType_SetHandler2(gSmokeType, Level_GetEventDA8(), Smoke_Activate);
    TaskType_SetHandler2(gSmokeType, Level_GetEventDAC(), Smoke_Deactivate);
    TaskType_SetHandler(gSmokeType, RigidDynCubeObj_GetEvent4(), Smoke_Move);
    TaskType_SetHandler(gSmokeType, 4, NULL);
    gSmokeParams = ParamTable_New(21);
    ParamTable_Add(gSmokeParams, &s, &s.origin, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSmokeParams, &s, &s.pitch, Param_Angle(), 7, &gSmokePitchRange);
    ParamTable_Add(gSmokeParams, &s, &s.yaw, Param_Angle(), 7, &gSmokeYawRange);
    ParamTable_Add(gSmokeParams, &s, &s.count, Param_Int32(), 7, &gSmokeCountRange);
    ParamTable_Add(gSmokeParams, &s, &s.radius, Param_Real32(), 7, &gSmokeRadiusRange);
    ParamTable_Add(gSmokeParams, &s, &s.spread, Param_Angle(), 7, NULL);
    ParamTable_Add(gSmokeParams, &s, &s.unk88, Param_Real32(), 7, &gSmokeUnk88Range);
    ParamTable_Add(gSmokeParams, &s, &s.unk8C, Param_Real32(), 7, &gSmokeUnk88Range);
    ParamTable_Add(gSmokeParams, &s, &s.unk104, Param_RGB(), 7, NULL);
    ParamTable_Add(gSmokeParams, &s, &s.unkFC, Param_Real32(), 7, &gSmokeUnkFCRange);
    ParamTable_Add(gSmokeParams, &s, &s.unk100, Param_Real32(), 7, &gSmokeUnkFCRange);
    ParamTable_Add(gSmokeParams, &s, &s.blend, Param_Int32(), 7, &gSmokeBlendRange);
    ParamTable_Add(gSmokeParams, &s, &s.sprite, Param_Int32(), 7, &gSmokeSpriteRange);
    ParamTable_Add(gSmokeParams, &s, &s.unk84, Param_Real32(), 7, &gSmokeUnk84Range);
    ParamTable_Add(gSmokeParams, &s, &s.unk114, Param_Real32(), 7, &gSmokeUnk114Range);
    ParamTable_Add(gSmokeParams, &s, &s.unk120, Param_Angle(), 7, NULL);
    ParamTable_Add(gSmokeParams, &s, &s.unk11C, Param_Angle(), 7, NULL);
    ParamTable_Add(gSmokeParams, &s, &s.unk124, Param_Real32(), 7, &gSmokeUnk124Range);
    ParamTable_Add(gSmokeParams, &s, &s.rate, Param_Real64(), 7, &gSmokeRateRange);
    ParamTable_Add(gSmokeParams, &s, &s.expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gSmokeParams, &s, &s.enabled, Param_Bool8(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_SMOKE", gSmokeType);
    gSmokeFireball = Game_AddSprite("fireball.spr");
    gSmokeBlood = Game_AddSprite("blood1.spr");
    gSmokeSmoke1 = Game_AddSprite("smoke1.spr");
    gSmokeSmoke2 = Game_AddSprite("smoke2.spr");
}
HOOK(0x437510, Smoke_Init);

/* follows the parent: pos = parent pos + parent rotation * local */
void Smoke_Move(Smoke *t, MoveArg *a)
{
    const float *m = a->m;
    Vec3d v;

    v.x = m[2] * t->local.z + m[1] * t->local.y + m[0] * t->local.x;
    v.y = m[5] * t->local.z + m[4] * t->local.y + m[3] * t->local.x;
    v.z = m[8] * t->local.z + m[7] * t->local.y + m[6] * t->local.x;
    t->pos = v;
    t->pos.x = a->pos->x + t->pos.x;
    t->pos.y = a->pos->y + t->pos.y;
    t->pos.z = a->pos->z + t->pos.z;
}
HOOK(0x437AD0, Smoke_Move);

void Smoke_Create(Smoke *t)
{
    t->pos.x = 0.0;
    t->pos.y = 0.0;
    t->pos.z = 0.0;
    t->origin.x = 0.0;
    t->origin.y = 0.0;
    t->origin.z = 0.0;
    t->registered = 0;
    t->ps = NULL;
    t->pitch = 0.0f;
    t->yaw = 0.0f;
    t->unk88 = 4.5f;
    t->unk8C = 6.0f;
    t->spread = 0x1.921fb6p-1f;
    t->rate = 30.0;
    t->acc = 0.0f;
    t->count = 100;
    t->unk100 = 1.0f;
    t->unkFC = 0.5f;
    t->unk104.x = 2.0f;
    t->unk104.y = 2.0f;
    t->unk104.z = 2.0f;
    t->blend = 0;
    t->unk114 = 0x1.555556p+1f;
    t->unk84 = 0.5f;
    t->sprite = 3;
    t->unk11C = 0.0f;
    t->unk120 = 0.0f;
    t->unk124 = 1.0f;
    t->lastUnk124 = 1.0f;
    t->enabled = 0;
    t->moved = 0;
    VarString_Init(&t->expr);
    Random_Init(gSmokeRandom);
}
HOOK(0x437B90, Smoke_Create);

void Smoke_Activate(Smoke *t)
{
    char name[64];
    char buf[128];

    if (t->registered)
        return;
    t->registered = 1;
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.vGenerateFactor", name);
    Script_RegisterReal(buf, &t->rate);
}
HOOK(0x437CA0, Smoke_Activate);

void Smoke_Deactivate(Smoke *t)
{
    char name[64];
    char buf[128];

    if (!t->registered)
        return;
    Task_GetName(name, t);
    t->registered = 0;
    crt_sprintf(buf, "%s.vGenerateFactor", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x437D00, Smoke_Deactivate);

void Smoke_Delete(Smoke *t)
{
    if (Task_GetId(t) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), t->task.type);

        if (fn)
            ((void (*)(void *))fn)(t);
    }
    VarString_Free(&t->expr);
}
HOOK(0x437D50, Smoke_Delete);

void Smoke_GetParams(void *t, void **out)
{
    (void)t;
    *out = gSmokeParams;
}
HOOK(0x437DA0, Smoke_GetParams);

/* moves the emitter along and emits rate / 30 particles a frame */
void Smoke_Run(Smoke *t)
{
    long double acc;
    void *ps;

    if (t->moved && t->enabled) {
        Vec3d d;

        d.x = t->pos.x - t->lastPos.x;
        d.y = t->pos.y - t->lastPos.y;
        d.z = t->pos.z - t->lastPos.z;
        Particle2D_Translate(t->ps, &d);
    }
    t->moved = 1;
    t->lastPos = t->pos;
    CALL_CHILDREN(&t->task, 1);
    if (VarString_HasExpr(&t->expr))
        t->rate = VarString_EvalReal(&t->expr, t);
    acc = t->rate + t->acc;
    ps = t->ps;
    t->acc = acc;
    while (acc > kRate) {
        float angle = Random_Float(gSmokeRandom) * kTwoPi;
        float r = Random_Float(gSmokeRandom) * t->radius * kUnit;
        long double f = Random_Float(gSmokeRandom);
        long double x = (t->unk11C - t->unk120) * f + t->unk120;
        float c = t->unk124;
        Vec3d p;
        int n1, n2;

        p.z = t->pos.z;
        p.x = x87_cos(angle) * r + t->pos.x;
        p.y = x87_sin(angle) * r + t->pos.y;
        n1 = round_int(t->unk100 * kRate / c);
        n2 = round_int(t->unkFC * kRate);
        Particle2D_Emit(ps, &p, &t->dir, t->spread, t->unk8C * kSpeed, t->unk88 * kSpeed, c, x * kPerFrame, 0, kOneF,
                   n2, n1);
        acc = t->acc - kRate;
        t->acc = acc;
    }
}
HOOK(0x437DB0, Smoke_Run);

void Smoke_UpdateLocal(Smoke *t);

/* rebuilds the emitter unless only unk124 changed */
void Smoke_Changed(Smoke *t)
{
    t->pos = t->origin;
    if (t->ps && EQ_OR_NAN(t->unk124, t->lastUnk124)) {
        Task_Delete(t->ps);
        t->ps = NULL;
    }
    t->moved = 0;
    t->lastUnk124 = t->unk124;
    if (!t->ps) {
        Matrix id = {{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
        Matrix rz, rx;
        Vec3f zero, scale;
        float size[2]; /* start and end size */
        void *sprite, *world, *root;
        int blend;

        Matrix_RotateZ(&rz, &id, t->yaw);
        Matrix_RotateX(&rx, &rz, t->pitch);
        t->dir.z = rx.m[8];
        t->dir.x = rx.m[2];
        t->dir.y = rx.m[5];
        switch (t->blend) {
        case 0:
            blend = 0x1080000;
            break;
        case 1:
            blend = 0x1000008;
            break;
        case 2:
            blend = 0x1000010;
            break;
        default:
            blend = 0; /* the original passes stack garbage */
            break;
        }
        switch (t->sprite) {
        case 0:
            sprite = gSmokeFireball + 0x18;
            break;
        case 1:
            sprite = gSmokeBlood + 0x18;
            break;
        case 2:
            sprite = gSmokeSmoke2 + 0x18;
            break;
        default:
            sprite = gSmokeSmoke1 + 0x18;
            break;
        }
        size[0] = size[1] = t->unk84 * kUnit;
        zero.x = kZeroF;
        zero.y = kZeroF;
        zero.z = kZeroF;
        scale.x = t->unk104.x * kScale;
        scale.y = t->unk104.y * kScale;
        scale.z = t->unk104.z * kScale;
        world = Game_GetWorld();
        root = Level_GetDynamic();
        t->ps = Particle2D_New(root, world, sprite, &zero, &scale, size, kLife, blend, 0, t->unk114 * kPerFrame, 0,
                           t->count, 0);
        Particle2D_SetAutoDelete(t->ps, 0);
    }
    VarString_Bind(&t->expr, t);
    Smoke_UpdateLocal(t);
}
HOOK(0x438000, Smoke_Changed);

/* the offset from the nearest moving ancestor, in its frame */
void Smoke_UpdateLocal(Smoke *t)
{
    RigidDynCubeObj *p = Task_FindAncestor(t, RigidDynCubeObj_GetType());
    const float *m;
    long double z;
    Vec3d v;

    if (!p) {
        static const Matrix id = {{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};

        t->local.x = 0.0;
        t->local.y = 0.0;
        t->local.z = 0.0;
        t->localMatrix = id;
        return;
    }
    m = p->matrix.m;
    t->local.x = t->origin.x - p->pos.x;
    t->local.y = t->origin.y - p->pos.y;
    z = t->origin.z - p->pos.z;
    t->local.z = z;
    v.x = m[6] * z + m[3] * t->local.y + m[0] * t->local.x;
    v.y = m[7] * z + m[4] * t->local.y + m[1] * t->local.x;
    v.z = m[8] * z + m[5] * t->local.y + m[2] * t->local.x;
    t->local = v;
}
HOOK(0x438240, Smoke_UpdateLocal);

void Smoke_Close(void)
{
    QTaskList_Free(gSmokeParams);
    TaskType_Free(gSmokeType);
}
HOOK(0x438370, Smoke_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 53C114, 53C114, 0xC8) INIT({0x28, 0xC1, 0x53, 0x00, 0x00, 0x8D, 0x53, 0x00, 0x20, 0xC1, 0x53, 0x00, 0x53, 0x74, 0x65, 0x61, 0x6D, 0x00, 0x00, 0x00, 0x53, 0x6D, 0x6F, 0x6B, 0x65, 0x00, 0x00, 0x00, 0x73, 0x6D, 0x6F, 0x6B, 0x65, 0x32, 0x2E, 0x73, 0x70, 0x72, 0x00, 0x00, 0x73, 0x6D, 0x6F, 0x6B, 0x65, 0x31, 0x2E, 0x73, 0x70, 0x72, 0x00, 0x00, 0x62, 0x6C, 0x6F, 0x6F, 0x64, 0x31, 0x2E, 0x73, 0x70, 0x72, 0x00, 0x00, 0x66, 0x69, 0x72, 0x65, 0x62, 0x61, 0x6C, 0x6C, 0x2E, 0x73, 0x70, 0x72, 0x00, 0x00, 0x00, 0x00, 0x54, 0x41, 0x53, 0x4B, 0x54, 0x59, 0x50, 0x45, 0x5F, 0x53, 0x4D, 0x4F, 0x4B, 0x45, 0x00, 0x00, 0x31, 0x37, 0x32, 0x39, 0x5F, 0x30, 0x31, 0x5F, 0x31, 0x00, 0x00, 0x00, 0x25, 0x73, 0x2E, 0x76, 0x47, 0x65, 0x6E, 0x65, 0x72, 0x61, 0x74, 0x65, 0x46, 0x61, 0x63, 0x74, 0x6F, 0x72, 0x00, 0x00, 0x63, 0x6F, 0x6E, 0x63, 0x72, 0x65, 0x74, 0x65, 0x32, 0x2E, 0x73, 0x70, 0x72, 0x00, 0x00, 0x00, 0x63, 0x6F, 0x6E, 0x63, 0x72, 0x65, 0x74, 0x65, 0x31, 0x2E, 0x73, 0x70, 0x72, 0x00, 0x00, 0x00, 0x54, 0x41, 0x53, 0x4B, 0x54, 0x59, 0x50, 0x45, 0x5F, 0x47, 0x55, 0x4E, 0x53, 0x48, 0x4F, 0x54, 0x45, 0x46, 0x46, 0x45, 0x43, 0x54, 0x00, 0x00, 0x47, 0x75, 0x6E, 0x53, 0x68, 0x6F, 0x74, 0x45, 0x66, 0x66, 0x65, 0x63, 0x74});
ALIAS(gSmokeKindNames, pad_53C114, 0);
BSS(57BDC8) char *gSmokeFireball;
PAD(igb, 57BDC8, 57BDCC, 0x4);
BSS(57BDC8) RangeF gSmokeUnk84Range;
BSS(57BDC8) RangeF gSmokeYawRange;
BSS(57BDC8) ParamEnum gSmokeKindEnum;
PAD(igb, 57BDC8, 57BDE8, 0x10);
ALIAS(gSmokeRandom, pad_57BDE8, 0);
BSS(57BDC8) RangeF gSmokeUnk88Range;
BSS(57BDC8) uint16_t gSmokeType;
PAD(igb, 57BDC8, 57BE02, 0x6);
BSS(57BDC8) RangeF gSmokePitchRange;
BSS(57BDC8) void *gSmokeParams;
PAD(igb, 57BDC8, 57BE14, 0x4);
BSS(57BDC8) RangeF gSmokeRadiusRange;
BSS(57BDC8) RangeD gSmokeRateRange;
BSS(57BDC8) RangeF gSmokeUnk124Range;
BSS(57BDC8) RangeF gSmokeUnk114Range;
BSS(57BDC8) void *gSmokeModel;
BSS(57BDC8) char *gSmokeBlood;
BSS(57BDC8) char *gSmokeSmoke2;
BSS(57BDC8) char *gSmokeSmoke1;
BSS(57BDC8) RangeI gSmokeSpriteRange;
BSS(57BDC8) RangeI gSmokeCountRange;
PAD(igb, 57BDC8, 57BE60, 0x8);
BSS(57BDC8) RangeI gSmokeBlendRange;
BSS(57BDC8) RangeF gSmokeUnkFCRange;
