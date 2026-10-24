#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "fxobj/fxobj.h"

ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x497F20, void, TransContext_Project, (float *screen, float *w, Vec3d *pos));
ORIG(0x498040, void, TransContext_ScaleSize, (float *out, float *size, float w));
ORIG(0x4B3B60, void, Matrix_SetRotX, (Matrix *m, float a));
ORIG(0x4B53B0, void, QVertex_DrawQuadEx, (void *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                          float x3, float y3, float r, float g, float b, float z, float a, int frame,
                                          unsigned int flags, int unk20));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *o, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *o, Vec3d *pos, float radius, int unk));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4E0830, uint16_t, SectorInteriorObject_GetType, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));

#define AlarmLight_Create ((void *)0x43A0E0)
#define AlarmLight_Run ((void *)0x43A270)
#define AlarmLight_Delete ((void *)0x43A3C0)
#define AlarmLight_GetParams ((void *)0x43A440)
#define AlarmLight_Changed ((void *)0x43A450)
#define AlarmLight_Hit ((void *)0x43A5E0)
#define AlarmLight_Blast ((void *)0x43A660)
#define AlarmLight_Activate ((void *)0x43A710)
#define AlarmLight_Deactivate ((void *)0x43A790)
#define VehiclePart_Delete ((void *)0x426860)

/* a rotating warning light ("AlarmLight", 0x1F8 bytes) */
typedef struct AlarmLight {
    RigidObj obj;
    Vec3d pos;
    Matrix local;
    char offModel[16];
    char onModel[16];
    char deadModel[16];
    char onSound[32];
    char deadSound[32];
    char on;
    char destroyed;
    char unk1A2[2];
    VarString cond;
} AlarmLight;

/* the turning reflector inside an alarm light ("AlarmLightReflector", 0x140 bytes) */
typedef struct Reflector {
    RigidObj obj;
    Vec3d offset;
    Matrix local;
    AlarmLight *light;
    int unk134;
    float angle;
    int unk13C;
} Reflector;

_Static_assert(offsetof(AlarmLight, on) == 0x1A0, "AlarmLight.on");
_Static_assert(offsetof(AlarmLight, cond) == 0x1A4, "AlarmLight.cond");
_Static_assert(offsetof(Reflector, light) == 0x130, "Reflector.light");
_Static_assert(sizeof(Reflector) == 0x140, "Reflector");

#define gAlarmLightParams VAR(void *, 0x57BE9C)
#define gAlarmLightReflectorType VAR(uint16_t, 0x57BEA0)
#define gAlarmLightType VAR(uint16_t, 0x57BEA2)
#define gAlarmLightSprite VAR(char *, 0x57BEA4)
#define gViewPos VAR(Vec3d, 0xBCAB08)

static const float kTurn = 0x1.aceeap-4f;
static const double kZero = 0.0;
static const float kZeroF = 0.0f;
static const double kPi = 0x1.921fb54442d1bp+1;
static const float kInvPi = 0x1.45f306p-2f;
static const float kHalfF = 0.5f;
static const float kFlareScale = 128.0f;

/* the C runtime's fmod helper, which takes its arguments on the x87 stack */
static inline long double crt_fmod(long double x, long double y)
{
    long double r;

    __asm__ volatile("call *%3" : "=t"(r) : "0"(y), "u"(x), "r"(0x4A56DA) : "st(1)", "eax", "ecx", "edx", "memory");
    return r;
}

void AlarmLightReflector_Create(Reflector *t, PartCreateArg *a);
void AlarmLightReflector_Run(Reflector *t);
void AlarmLightReflector_Draw(Reflector *t, void *arg);

void AlarmLight_Init(void)
{
    AlarmLight l;
    int i;

    gAlarmLightType = TaskType_New("AlarmLight", sizeof(AlarmLight), SectorInteriorObject_GetType(), 0);
    TaskType_SetHandler(gAlarmLightType, 0, AlarmLight_Create);
    TaskType_SetHandler2(gAlarmLightType, 1, AlarmLight_Run);
    TaskType_SetHandler2(gAlarmLightType, 2, AlarmLight_Delete);
    TaskType_SetHandler(gAlarmLightType, Param_GetTableEvent(), AlarmLight_GetParams);
    TaskType_SetHandler(gAlarmLightType, Param_GetChangedEvent(), AlarmLight_Changed);
    TaskType_SetHandler(gAlarmLightType, Game_GetEvent9(), AlarmLight_Hit);
    TaskType_SetHandler(gAlarmLightType, Game_GetEvent11(), AlarmLight_Blast);
    TaskType_SetHandler2(gAlarmLightType, Level_GetEventDA8(), AlarmLight_Activate);
    TaskType_SetHandler2(gAlarmLightType, Level_GetEventDAC(), AlarmLight_Deactivate);
    Script_RegisterConstant("TASKTYPE_ALARMLIGHT", gAlarmLightType);
    gAlarmLightParams = ParamTable_New(8);
    ParamTable_Add(gAlarmLightParams, &l, &l.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gAlarmLightParams, &l, &l.local, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gAlarmLightParams, &l, &l.cond, Param_VarString(), 7, NULL);
    for (i = 0; i < 3; i++)
        ParamTable_Add(gAlarmLightParams, &l, l.offModel + 0x10 * i, Param_String16(), 7, NULL);
    for (i = 0; i < 2; i++)
        ParamTable_Add(gAlarmLightParams, &l, l.onSound + 0x20 * i, Param_String16(), 7, NULL);
    gAlarmLightReflectorType = TaskType_New("AlarmLightReflector", sizeof(Reflector), MagicObj_GetType(), 0);
    TaskType_SetHandler(gAlarmLightReflectorType, 0, AlarmLightReflector_Create);
    TaskType_SetHandler2(gAlarmLightReflectorType, 1, AlarmLightReflector_Run);
    TaskType_SetHandler2(gAlarmLightReflectorType, 2, VehiclePart_Delete);
    TaskType_SetHandler(gAlarmLightReflectorType, 4, AlarmLightReflector_Draw);
    Script_RegisterConstant("TASKTYPE_ALARMLIGHTREFLECTOR", gAlarmLightReflectorType);
    gAlarmLightSprite = Game_AddSprite("alarmlight.spr");
}
HOOK(0x4397C0, AlarmLight_Init);

void AlarmLightReflector_Create(Reflector *t, PartCreateArg *a)
{
    void **model;
    long double r;

    t->offset = a->offset;
    t->local = a->local;
    model = VirModel_Find(a->model);
    if (!model) {
        Error_Show("VirModel \"%s\" not available", a->model);
        for (;;)
            ;
    }
    RigidDynCubeObj_Init((void *)(intptr_t)a->unk4, t, model);
    {
        float s = max_scale(&t->obj);

        r = VirModel_GetRadius(t->obj.model) * s;
    }
    DynCube_Add((void *)(intptr_t)a->unk4, t, &a->pos, r, 1);
    t->obj.matrix = a->matrix;
    t->light = (AlarmLight *)a->owner;
    RigidDynCubeObj_CreateMagicObjs(t, t, a->owner);
    t->angle = 0.0f;
}
HOOK(0x439A50, AlarmLightReflector_Create);

/* turns the reflector while the light is on: matrix = light * (local * rotx) */
void AlarmLightReflector_Run(Reflector *t)
{
    AlarmLight *l = t->light;

    if (l->on && !l->destroyed) {
        const float *L = l->obj.matrix.m, *T = t->local.m, *R;
        long double x0, x3, x6;
        float x1, x2, x4, x5, x7, x8;
        float a = t->angle + kTurn;
        Matrix r;
        int stamp;

        t->angle = a;
        Matrix_SetRotX(&r, a);
        R = r.m;
        x0 = R[3] * T[1] + R[6] * T[2] + R[0] * T[0];
        x1 = R[4] * T[1] + R[7] * T[2] + R[1] * T[0];
        x2 = R[5] * T[1] + R[8] * T[2] + R[2] * T[0];
        x3 = R[6] * T[5] + R[0] * T[3] + R[3] * T[4];
        x4 = R[7] * T[5] + R[1] * T[3] + R[4] * T[4];
        x5 = R[8] * T[5] + R[2] * T[3] + R[5] * T[4];
        x6 = R[3] * T[7] + R[6] * T[8] + R[0] * T[6];
        stamp = t->local.stamp + r.stamp + 1;
        x7 = R[4] * T[7] + R[7] * T[8] + R[1] * T[6];
        x8 = R[5] * T[7] + R[8] * T[8] + R[2] * T[6];
        r.m[0] = x0 * L[0] + x6 * L[2] + x3 * L[1];
        r.m[1] = x1 * L[0] + x7 * L[2] + x4 * L[1];
        r.m[2] = x2 * L[0] + x8 * L[2] + x5 * L[1];
        r.m[3] = x0 * L[3] + x6 * L[5] + x3 * L[4];
        r.m[4] = x1 * L[3] + x7 * L[5] + x4 * L[4];
        r.m[5] = x2 * L[3] + x8 * L[5] + x5 * L[4];
        r.m[6] = x6 * L[8] + x3 * L[7] + x0 * L[6];
        r.stamp = l->obj.matrix.stamp + stamp + 1;
        r.m[7] = x7 * L[8] + x4 * L[7] + x1 * L[6];
        r.m[8] = x8 * L[8] + x5 * L[7] + x2 * L[6];
        t->obj.matrix = r;
    }
    CALL_CHILDREN(&t->obj.task, 1);
}
HOOK(0x439B70, AlarmLightReflector_Run);

void AlarmLightReflector_DrawFlare(Reflector *t, int point);

void AlarmLightReflector_Draw(Reflector *t, void *arg)
{
    if (t->light->destroyed)
        return;
    if (t->light->on)
        AlarmLightReflector_DrawFlare(t, 0);
    gTaskHandlers[4][RigidDynCubeObj_GetType()](t, arg);
}
HOOK(0x439EC0, AlarmLightReflector_Draw);

/* a glow sprite on screen when the reflector faces the camera */
void AlarmLightReflector_DrawFlare(Reflector *t, int point)
{
    AlarmLight *l = t->light;
    float scr[2], size[2], w;
    float x0, y0, x1, y1;
    long double k;
    Vec3d p;

    Model_GetPoint(&p, t, point);
    p.x = p.x + t->obj.pos.x;
    p.y = p.y + t->obj.pos.y;
    p.z = p.z + t->obj.pos.z;
    {
        float m1 = l->local.m[1], m4 = l->local.m[4], m7 = l->local.m[7];
        long double dx = gViewPos.x - l->obj.pos.x;
        long double dy = gViewPos.y - l->obj.pos.y;
        long double dz = gViewPos.z - l->obj.pos.z;

        if (!(dz * m7 + dy * m4 + dx * m1 > kZero))
            return;
    }
    TransContext_Project(scr, &w, &p);
    if (!(w > kZeroF))
        return;
    size[0] = 6144.0f;
    size[1] = 6144.0f;
    TransContext_ScaleSize(size, size, w);
    k = crt_fmod(t->angle, kPi) * kInvPi - kHalfF;
    if (k < kZeroF || k != k)
        k = -k;
    x0 = scr[0] - size[0];
    y0 = scr[1] - size[1];
    x1 = scr[0] + size[0];
    y1 = scr[1] + size[1];
    QVertex_DrawQuadEx(gAlarmLightSprite + 0x18, x0, y0, x1, y0, x0, y1, x1, y1, (k + k) * kFlareScale, 0.0f, 0.0f,
                       w, 0.5f, 0, 0x1040008, 0);
}
HOOK(0x439F10, AlarmLightReflector_DrawFlare);
