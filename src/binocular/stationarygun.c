#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "binocular/binocular.h"

FN_EXTERN(VehiclePart_Delete);

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x413C00, int *, WeaponType_FindByKey, (const char *key));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int index));
ORIG(0x460BF0, uint8_t, Human_GetEvent0, (void));
ORIG(0x477C50, Task *, Gun_New, (void *owner, int a, int id, int ammo, char b));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x489630, uint8_t, AIDetection_GetEvent2, (void));
ORIG(0x4B2760, long double, Math_ACosF, (float x));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3160, long double, Vec3d_LengthSq, (const Vec3d *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3BA0, void, Matrix_SetRotY, (Matrix *m, float a));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (Matrix *m, float a));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C56D0, uint8_t, RigidDynCubeObj_GetEvent0, (void));
ORIG(0x4C56E0, uint8_t, RigidDynCubeObj_GetEvent1, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (void *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, Vec3d *pos));
ORIG(0x4F2050, void, WeaponSlots_Init, (void **slots));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (void **slots, int i));
ORIG(0x4F2140, void, WeaponSlots_SelectNext, (void *owner, void **slots, int a));
ORIG(0x4F2310, void, Weapon_Move, (Task *w, Vec3d *pos, Matrix *m));

#define VEHICLEPART_DELETE (FN(0x426860, VehiclePart_Delete))

extern void *gGunHolderList;
extern uint16_t gAIStationaryGunType;

/* parsed "AIStationaryGun" MagicObj arguments (0x60 bytes) */
typedef struct GunDef {
    char weapon[0x20];
    float pitchMax;
    float pitchMin;
    float yawRange;
    int ammo;
    float pitchSpeed;
    float yawSpeed;
    char sound[0x20];
    float spinSpeed;
    float range;
} GunDef;

/* "AIStationaryGun" (0x1E8 bytes), a turning gun mounted on a vehicle */
typedef struct AIStationaryGun {
    RigidObj obj;
    Vec3d offset;           /* 0xF0 */
    Matrix local;           /* 0x108 */
    RigidObj *owner;        /* 0x130 */
    int unk134;
    Vec3d pos;              /* 0x138 */
    Matrix matrix;          /* 0x150 */
    Vec3d muzzlePos;        /* 0x178 */
    Matrix muzzleMatrix;    /* 0x190 */
    GunDef *def;            /* 0x1B8 */
    float pitch;            /* 0x1BC */
    float yaw;              /* 0x1C0 */
    void *weapons[3];       /* 0x1C4 */
    void *sound;            /* 0x1D0 */
    int hold;               /* 0x1D4, frames to keep aiming at the lost target */
    char hasTarget;         /* 0x1D8 */
    char pad1D9[3];
    float holdPitch;        /* 0x1DC */
    float holdYaw;          /* 0x1E0 */
    float spin;             /* 0x1E4 */
} AIStationaryGun;

/* what the owner returns through the AIDetection event 1 slot */
typedef struct Detection {
    char unk0[0x18];
    RefNode *targets;
} Detection;

/* AIDetection event 0: may the detector see this pair of tasks */
typedef struct DetectQuery {
    char result;
    char pad[7];
    Task *a;
    Task *b;
} DetectQuery;

_Static_assert(sizeof(GunDef) == 0x60, "GunDef");
_Static_assert(offsetof(AIStationaryGun, def) == 0x1B8, "AIStationaryGun");
_Static_assert(offsetof(AIStationaryGun, hold) == 0x1D4, "AIStationaryGun");
_Static_assert(sizeof(AIStationaryGun) == 0x1E8, "AIStationaryGun");

F32(kPi, 0x40490FDB);
F32(kTwoPi, 0x40C90FDB);
F32(kZero, 0x00000000);
F32(kAimAngle, 0x3DB2B8C3);    /* 5 degrees */
F32(kSoundAngle, 0x3D0EFA35);  /* 2 degrees */
F32(kDegToRad, 0x3C8EFA35);
F32(kPerFrame, 0x3D088889);    /* 1/30 */
F32(kRangeScale, 0x45800000);  /* 4096 */
static const double kTwoPiD = 0x1.921fb54442d18p+2;

void *GunHolder_GetList(void)
{
    return gGunHolderList;
}
HOOK(0x46EAD0, GunHolder_GetList);

void AIStationaryGun_PlaceInitial(AIStationaryGun *g, Matrix *parent);

void AIStationaryGun_Create(AIStationaryGun *g, PartCreateArg *arg)
{
    void **model = VirModel_Find(arg->model);
    float s;

    if (!model) {
        Error_Show("VirModel \"%s\" not available", arg->model);
        for (;;)
            ;
    }
    g->offset = arg->offset;
    g->local = arg->local;
    g->owner = arg->owner;
    RigidDynCubeObj_Init(arg->world, g, model);
    s = max_scale(g->obj.scale);
    DynCube_Add(arg->world, g, &arg->pos, (float)(VirModel_GetRadius(g->obj.model) * s), 1);
    g->obj.matrix = arg->matrix;
    g->def = arg->def;
    g->pos = arg->pos;
    g->matrix = arg->matrix;
    g->pitch = 0;
    g->yaw = 0;
    WeaponSlots_Init(g->weapons);
    Gun_New(g, 0, *WeaponType_FindByKey(g->def->weapon), g->def->ammo, 0);
    WeaponSlots_SelectNext(g, g->weapons, 0);
    AIStationaryGun_PlaceInitial(g, &g->owner->matrix);
    RigidDynCubeObj_CreateMagicObjs(g, g, g->owner);
    g->sound = NULL;
    g->hold = 0;
    g->spin = 0;
    g->hasTarget = 0;
}
HOOK(0x46EAE0, AIStationaryGun_Create);

/* sets the gun and its weapon from the owner's matrix (when created) */
void AIStationaryGun_PlaceInitial(AIStationaryGun *g, Matrix *parent)
{
    Task *w = WeaponSlots_Get(g->weapons, 0);
    Matrix r, a, b;
    long double col0[3];
    Vec3d pos;

    if (!w)
        return;
    Matrix_SetRotZ(&r, g->yaw);
    mat_mul(&a, &g->local, &r, 1, 1, 1);
    mat_mul(&b, parent, &a, 0, 1, 1);
    g->obj.matrix = b;
    Model_GetPoint(&pos, g, 1);
    pos.x += g->obj.pos.x;
    pos.y += g->obj.pos.y;
    pos.z += g->obj.pos.z;
    Matrix_SetEuler(&r, g->pitch, g->spin, g->yaw + kPi.f);
    mat_mul(&a, &g->local, &r, 1, 1, 1);
    mat_mul_col0_ext(col0, &g->local, &r, 1, 1, 1);
    mat_mul_col0(&r, parent, &a, col0, 1, 0, 1);
    Weapon_Move(w, &pos, &r);
    g->muzzlePos = pos;
    g->muzzleMatrix = r;
}
HOOK(0x46EC90, AIStationaryGun_PlaceInitial);

void AIStationaryGun_Run(AIStationaryGun *g);
void AIStationaryGun_ParseDef(void *unused, ScriptArg *a);
void AIStationaryGun_GetDefSize(void *unused, int *size);
void AIStationaryGun_CanSee(AIStationaryGun *g, DetectQuery *q);
void AIStationaryGun_Move(AIStationaryGun *g, PartMoveArg *a);

void AIStationaryGun_Open(void)
{
    gAIStationaryGunType = TaskType_New("AIStationaryGun", 0x1E8, MagicObj_GetType(), 0);
    TaskType_SetHandler(gAIStationaryGunType, 0, AIStationaryGun_Create);
    TaskType_SetHandler2(gAIStationaryGunType, 1, AIStationaryGun_Run);
    TaskType_SetHandler2(gAIStationaryGunType, 2, VEHICLEPART_DELETE);
    TaskType_SetHandler(gAIStationaryGunType, RigidDynCubeObj_GetEvent0(), AIStationaryGun_ParseDef);
    TaskType_SetHandler(gAIStationaryGunType, RigidDynCubeObj_GetEvent1(), AIStationaryGun_GetDefSize);
    TaskType_SetHandler(gAIStationaryGunType, AIDetection_GetEvent0(), AIStationaryGun_CanSee);
    TaskType_SetHandler(gAIStationaryGunType, MagicObj_GetMoveEvent(), AIStationaryGun_Move);
    Script_RegisterConstant("TASKTYPE_AISTATIONARYGUN", gAIStationaryGunType);
}
HOOK(0x46F2B0, AIStationaryGun_Open);

void AIStationaryGun_Fire(AIStationaryGun *g, char fire);
void AIStationaryGun_Clamp(AIStationaryGun *g);

/* aims at the first target the owner's detection has and fires when on it */
void AIStationaryGun_Run(AIStationaryGun *g)
{
    float yaw = 0, pitch = 0, distSq = 0, dp;
    long double dy, t;
    char found = 0;
    Detection *det;
    TaskFn fn;
    GunDef *def;
    int fire;

    WeaponSlots_Get(g->weapons, 0);
    det = NULL;
    fn = HANDLER(AIDetection_GetEvent2(), g->owner->task.type);
    if (fn)
        fn(g->owner, &det);
    if (!det)
        return;
    {
        Matrix m = g->owner->matrix;
        RefNode *n = det->targets;
        RefNode *next;

        for (next = n->next; next; n = next, next = next->next) {
            Vec3d d, v;
            long double dz;

            if (!n->active || !*n->ref)
                continue;
            d.x = DynCubeObj_GetPosition(*n->ref)->x - g->obj.pos.x;
            d.y = DynCubeObj_GetPosition(*n->ref)->y - g->obj.pos.y;
            dz = DynCubeObj_GetPosition(*n->ref)->z - g->obj.pos.z;
            d.z = dz;
            v.x = m.m[6] * dz + (long double)m.m[3] * d.y + (long double)m.m[0] * d.x;
            v.y = m.m[7] * dz + (long double)m.m[4] * d.y + (long double)m.m[1] * d.x;
            v.z = m.m[8] * dz + (long double)m.m[5] * d.y + (long double)m.m[2] * d.x;
            d = v;
            yaw = (float)Vec3d_YawAngle(v);
            distSq = (float)Vec3d_LengthSq(&d);
            v = d;
            v.z = 0;
            Vec3d_Normalize(&v);
            Vec3d_Normalize(&d);
            pitch = (float)Math_ACosF((float)((long double)v.z * d.z + (long double)v.y * d.y
                                             + (long double)v.x * d.x));
            if (d.z < 0.0)
                pitch = -pitch;
            found = 1;
            break;
        }
    }

    if (g->hasTarget) {
        if (!found)
            g->hold = 90;
        else
            g->hold = 0;
    } else if (found) {
        g->hold = 0;
    }
    g->hasTarget = found;
    if (g->hold) {
        pitch = g->holdPitch;
        yaw = g->holdYaw;
        g->hold--;
    } else {
        g->holdPitch = pitch;
        g->holdYaw = yaw;
    }

    def = g->def;
    if (def->yawRange > kTwoPi.f) {
        t = (long double)yaw - g->yaw;
        if (t < kZero.f)
            t = -t;
        if (t > kPi.f)
            yaw = yaw + kTwoPi.f;
    }
    if (g->pitch < pitch) {
        t = (long double)def->pitchSpeed + g->pitch;
        g->pitch = t;
        if (t > pitch)
            g->pitch = pitch;
    } else if (g->pitch > pitch) {
        t = (long double)g->pitch - def->pitchSpeed;
        g->pitch = t;
        if (t < pitch)
            g->pitch = pitch;
    }
    if (g->yaw < yaw) {
        t = (long double)def->yawSpeed + g->yaw;
        g->yaw = t;
        if (t > yaw)
            g->yaw = yaw;
    } else if (g->yaw > yaw) {
        t = (long double)g->yaw - def->yawSpeed;
        g->yaw = t;
        if (t < yaw)
            g->yaw = yaw;
    }
    AIStationaryGun_Clamp(g);

    t = (long double)g->pitch - pitch;
    if (t < kZero.f)
        t = -t;
    dp = t;
    dy = (long double)g->yaw - yaw;
    if (dy < kZero.f)
        dy = -dy;
    if (found) {
        fire = dp < kAimAngle.f && dy < kAimAngle.f
               && distSq > (long double)g->def->range * g->def->range;
        AIStationaryGun_Fire(g, fire);
        if (fire)
            g->spin = (long double)g->def->spinSpeed + g->spin;
    } else {
        AIStationaryGun_Fire(g, 0);
    }

    def = g->def;
    if (pitch < def->pitchMin)
        pitch = def->pitchMin;
    else if (pitch > def->pitchMax)
        pitch = def->pitchMax;
    if (yaw < -def->yawRange)
        yaw = -def->yawRange;
    else if (def->yawRange < yaw)
        yaw = def->yawRange;
    t = (long double)g->pitch - pitch;
    if (t < kZero.f)
        t = -t;
    dp = t;
    dy = (long double)g->yaw - yaw;
    if (dy < kZero.f)
        dy = -dy;
    if (dp > kSoundAngle.f || dy > kSoundAngle.f) {
        if (!g->sound)
            g->sound = Sound_Play(g, def->sound);
        if (g->sound)
            Sound_SetPos(g->sound, &g->obj.pos);
    } else if (g->sound) {
        Sound_Stop(g->sound);
        g->sound = NULL;
    }
    RUN_CHILDREN(&g->obj.task);
}
HOOK(0x46F390, AIStationaryGun_Run);

void AIStationaryGun_Fire(AIStationaryGun *g, char fire)
{
    Task *w = WeaponSlots_Get(g->weapons, 0);
    WeaponUseArg a;

    if (!w)
        return;
    a.fire = fire;
    a.unk1 = 0;
    a.pos = NULL;
    a.matrix = NULL;
    a.unkC = 0;
    a.unk10 = 0;
    a.unk20 = 0;
    a.unk24 = 0;
    a.unk28 = 1.0f;
    a.human = NULL;
    HANDLER(Human_GetEvent0(), w->type)(w, &a);
}
HOOK(0x46F950, AIStationaryGun_Fire);

/* keeps pitch and yaw inside the limits of the definition */
void AIStationaryGun_Clamp(AIStationaryGun *g)
{
    GunDef *def = g->def;

    if (def->pitchMin > g->pitch)
        g->pitch = def->pitchMin;
    else if (def->pitchMax < g->pitch)
        g->pitch = def->pitchMax;
    if (def->yawRange < kTwoPiD) {
        if (g->yaw < -def->yawRange)
            g->yaw = -def->yawRange;
        else if (def->yawRange < g->yaw)
            g->yaw = def->yawRange;
    }
}
HOOK(0x46F9D0, AIStationaryGun_Clamp);

void AIStationaryGun_ParseDef(void *unused, ScriptArg *a)
{
    GunDef *d = a->out;
    int i = a->index;

    Script_GetArgString(a->args, i, d->weapon, 0x20);
    d->yawRange = Script_GetArgReal(a->args, i + 1) * kDegToRad.f;
    d->pitchMax = Script_GetArgReal(a->args, i + 2) * kDegToRad.f;
    d->pitchMin = Script_GetArgReal(a->args, i + 3) * kDegToRad.f;
    d->ammo = Script_GetArgInt(a->args, i + 4);
    d->pitchSpeed = Script_GetArgReal(a->args, i + 5) * kDegToRad.f * kPerFrame.f;
    d->yawSpeed = Script_GetArgReal(a->args, i + 6) * kDegToRad.f * kPerFrame.f;
    Script_GetArgString(a->args, i + 7, d->sound, 0x20);
    d->spinSpeed = Script_GetArgReal(a->args, i + 8) * kDegToRad.f * kPerFrame.f;
    d->range = Script_GetArgReal(a->args, i + 9) * kRangeScale.f;
}
HOOK(0x46FA50, AIStationaryGun_ParseDef);

void AIStationaryGun_GetDefSize(void *unused, int *size)
{
    *size = sizeof(GunDef);
}
HOOK(0x46FB50, AIStationaryGun_GetDefSize);

/* the gun does not see itself, its vehicle or what is inside it */
void AIStationaryGun_CanSee(AIStationaryGun *g, DetectQuery *q)
{
    if (q->a == &g->obj.task || q->a == g->obj.task.parent || q->b == &g->obj.task
        || q->b == g->obj.task.parent || q->a == &g->owner->task) {
        q->result = 0;
        return;
    }
    q->result = 1;
}
HOOK(0x46FB60, AIStationaryGun_CanSee);

void AIStationaryGun_PlaceWeapon(AIStationaryGun *g);

void AIStationaryGun_Move(AIStationaryGun *g, PartMoveArg *a)
{
    Matrix *p = a->matrix;
    Matrix r, m, b;
    Vec3d pos;
    float s;
    PartMoveArg out;

    pos.x = (long double)p->m[2] * g->offset.z + (long double)p->m[1] * g->offset.y
            + (long double)p->m[0] * g->offset.x;
    pos.y = (long double)p->m[5] * g->offset.z + (long double)p->m[4] * g->offset.y
            + (long double)p->m[3] * g->offset.x;
    pos.z = (long double)p->m[8] * g->offset.z + (long double)p->m[7] * g->offset.y
            + (long double)p->m[6] * g->offset.x;
    pos.x = pos.x + a->pos->x;
    pos.y = pos.y + a->pos->y;
    pos.z = pos.z + a->pos->z;
    s = max_scale(g->obj.scale);
    DynCube_Move(g->obj.world, g, &pos, (float)(VirModel_GetRadius(g->obj.model) * s));
    Matrix_SetRotZ(&r, g->yaw);
    mat_mul(&m, &g->local, &r, 1, 1, 1);
    mat_mul(&b, a->matrix, &m, 2, 0, 0);
    g->obj.matrix = b;
    AIStationaryGun_PlaceWeapon(g);
    out.pos = &g->obj.pos;
    out.matrix = &g->obj.matrix;
    EACH_CHILD(&g->obj.task, MagicObj_GetMoveEvent(), &out);
}
HOOK(0x46FBA0, AIStationaryGun_Move);

/* moves the weapon to the gun's barrel point, turned by the aim and spin */
void AIStationaryGun_PlaceWeapon(AIStationaryGun *g)
{
    Task *w = WeaponSlots_Get(g->weapons, 0);
    Matrix r, m, t;
    Vec3d pos;
    float spin;

    Model_GetPoint(&pos, g, 1);
    pos.x += g->obj.pos.x;
    pos.y += g->obj.pos.y;
    pos.z += g->obj.pos.z;
    Matrix_SetEuler(&r, g->pitch, 0, g->yaw);
    mat_mul(&m, &g->owner->matrix, &r, 0, 1, 0);
    spin = g->spin;
    if (spin != kZero.f) {
        Matrix_SetRotY(&r, spin);
        mat_mul(&t, &m, &r, 0, 0, 0);
        m = t;
    }
    Weapon_Move(w, &pos, &m);
    g->muzzlePos = pos;
    g->muzzleMatrix = m;
}
HOOK(0x470050, AIStationaryGun_PlaceWeapon);

void AIStationaryGun_Close(void)
{
    TaskType_Free(gAIStationaryGunType);
}
HOOK(0x4703F0, AIStationaryGun_Close);

uint16_t AIStationaryGun_GetType(void)
{
    return gAIStationaryGunType;
}
HOOK(0x470400, AIStationaryGun_GetType);
