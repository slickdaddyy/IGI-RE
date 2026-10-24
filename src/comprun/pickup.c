#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "comprun/comprun.h"

FN_EXTERN(Task_FalseHandler);
FN_EXTERN(Terminal_MapHandler);

ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, int, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x413BD0, char *, WeaponType_Find, (int id));
ORIG(0x413DE0, void *, WeaponType_GetIdEnum, (void));
ORIG(0x4167E0, uint8_t, Game_GetEvent1, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x4168C0, void *, Game_GetTaskList2, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x47CD40, char *, AmmoType_Find, (int id));
ORIG(0x47CE00, void, AmmoList_Add, (void *ammo, int id, int count));
ORIG(0x47CF10, void *, AmmoType_GetEnum, (void));
ORIG(0x485970, void, StatusMessageShow, (void *owner, const char *text, const char *sprite, const char *unk));
ORIG(0x4A53B3, int, crt_sprintf, (char *out, const char *fmt, ...));
ORIG(0x4B0D60, void, Mem_PushMode, (int mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const Matrix *m, float *ax, float *ay, float *az));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *results, Vec3d *starts, Vec3d *ends, int count, char unk,
                                  void *query, unsigned int flags));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4DCCB0, void, Sector_AddObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));
ORIG(0x4DCF00, void, Sector_MoveObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E0830, uint16_t, SectorInteriorObject_GetType, (void));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, Vec3d *pos));
ORIG(0x4E7200, void, Sound_PlayDefined, (const char *name, int a, void *b));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FB0, void *, Param_Int16, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1E20, void *, Param_EnumString32, (void));

#define Task_HandlerNothing45F920 (FN(0x45F920, Task_FalseHandler))
#define ComputerObject_Draw443DC0 (FN(0x443DC0, Terminal_MapHandler))

/* "GenericPickup" (0x120 bytes), a model that disappears when touched */
typedef struct GenericPickup {
    RigidDynCubeObj obj;
    Vec3d pos;      /* 0xF0 */
    char model[16]; /* 0x108 */
    char registered;
    char pickedUp;
    char inWorld;
    char unk11B[5];
} GenericPickup;

/* "GunPickup" (0x158 bytes), a weapon lying around or falling to the ground */
typedef struct GunPickup {
    RigidDynCubeObj obj;
    Vec3d pos;    /* 0xF0 */
    Vec3d ground; /* 0x108 */
    double fall;  /* 0x120 */
    char registered;
    char pickedUp;
    char unk12A[2];
    int weapon;       /* 0x12C */
    char weaponName[0x20];
    int ammo;         /* 0x150 */
    int unk154;
} GunPickup;

/* "AmmoPickup" (0x138 bytes) */
typedef struct AmmoPickup {
    RigidDynCubeObj obj;
    Vec3d pos; /* 0xF0 */
    char registered;
    char pickedUp;
    char unk10A[2];
    int ammo;           /* 0x10C */
    char ammoName[0x20];
    int16_t count;      /* 0x130 */
    char unk132[6];
} AmmoPickup;

/* "AddAmmo" (0x48 bytes), gives its parent human some ammo and goes away */
typedef struct AddAmmo {
    Task task;
    int ammo;
    char ammoName[0x20];
    int16_t count;
    char unk46[2];
} AddAmmo;

/* what a touched pickup hands to the one picking it up */
typedef struct PickupMsg {
    int id;
    int count;
} PickupMsg;

_Static_assert(sizeof(GenericPickup) == 0x120, "GenericPickup");
_Static_assert(offsetof(GunPickup, weaponName) == 0x130, "weaponName");
_Static_assert(sizeof(GunPickup) == 0x158, "GunPickup");
_Static_assert(offsetof(AmmoPickup, count) == 0x130, "count");
_Static_assert(sizeof(AmmoPickup) == 0x138, "AmmoPickup");
_Static_assert(offsetof(AddAmmo, count) == 0x44, "AddAmmo.count");
_Static_assert(sizeof(AddAmmo) == 0x48, "AddAmmo");

extern uint16_t gGenericPickupType;
extern void *gGenericPickupParams;
extern uint16_t gGunPickupType;
extern uint8_t gGunPickupEvent;
extern void *gGunPickupParams;
extern uint16_t gAmmoPickupType;
extern uint8_t gAmmoPickupEvent;
extern void *gAmmoPickupParams;
extern uint16_t gAddAmmoType;
extern void *gAddAmmoParams;

static const double kMinTouch = 2867.2;
static const double kTouchHeight = 4096.0;
static const double kZero = 0.0;
static const double kHalf = 0.5;
static const double kBounce = -0.5;
static const double kGravity = 0x1.64ce9ed57275ep+5;
static const double kRest = 409.6;
static const double kDropAbove = 2048.0;
static const double kDropBelow = 204800.0;
static const float kZeroF = 0.0f;
static const float kRight = 90.0f;
static const float kLeft = -90.0f;

static long double radius(RigidDynCubeObj *o)
{
    float s = max_scale(o);

    return VirModel_GetRadius(o->model) * s;
}

/* whether a toucher is close enough to pick o up */
static int touches(RigidDynCubeObj *o, Toucher *by)
{
    long double r = radius(o);
    double range = r;
    double dz;
    Vec3d d;

    if (!(r >= kMinTouch))
        range = kMinTouch;
    d.x = o->pos.x - by->pos.x;
    d.y = o->pos.y - by->pos.y;
    {
        long double z = o->pos.z - by->pos.z;

        if (z < kZero)
            z = -z;
        dz = z;
    }
    d.z = 0.0;
    return !(Vec3d_Length(&d) >= range) && !(dz >= kTouchHeight);
}

/* registers "<name>.isPickedUp" for the scripts */
static void register_flag(void *t, char *registered, char *flag)
{
    char name[64], buf[128];

    if (*registered)
        return;
    *registered = 1;
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.isPickedUp", name);
    Script_RegisterBool8(buf, flag);
}

static void unregister_flag(void *t, char *registered)
{
    char name[64], buf[128];

    if (!*registered)
        return;
    Task_GetName(name, t);
    *registered = 0;
    crt_sprintf(buf, "%s.isPickedUp", name);
    Script_RemoveSymbol(buf);
}

static void call_dac(Task *t)
{
    if (Task_GetId(t) >= 0) {
        TaskFn fn = gTaskHandlers[Level_GetEventDAC()][t->type];

        if (fn)
            ((void (*)(void *))fn)(t);
    }
}

/* GenericPickup */

void GenericPickup_Create(GenericPickup *t);
void GenericPickup_Run(GenericPickup *t);
void GenericPickup_Destroy(GenericPickup *t);
void GenericPickup_GetParams(void *t, void **out);
void GenericPickup_Changed(GenericPickup *t);
void GenericPickup_Register(GenericPickup *t);
void GenericPickup_Unregister(GenericPickup *t);
void GenericPickup_Touch(GenericPickup *t, Toucher *by);
void GenericPickup_Show(GenericPickup *t, void *arg);
void GenericPickup_Hide(GenericPickup *t, void *arg);

void GenericPickup_Init(void)
{
    GenericPickup p;

    gGenericPickupType = TaskType_New("GenericPickup", sizeof(GenericPickup), SectorInteriorObject_GetType(), 0);
    TaskType_SetHandler(gGenericPickupType, 0, GenericPickup_Create);
    TaskType_SetHandler2(gGenericPickupType, 1, GenericPickup_Run);
    TaskType_SetHandler2(gGenericPickupType, 2, GenericPickup_Destroy);
    TaskType_SetHandler(gGenericPickupType, Param_GetTableEvent(), GenericPickup_GetParams);
    TaskType_SetHandler(gGenericPickupType, Param_GetChangedEvent(), GenericPickup_Changed);
    TaskType_SetHandler2(gGenericPickupType, Level_GetEventDA8(), GenericPickup_Register);
    TaskType_SetHandler2(gGenericPickupType, Level_GetEventDAC(), GenericPickup_Unregister);
    TaskType_SetHandler(gGenericPickupType, Game_GetEvent8(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gGenericPickupType, Game_GetEvent13(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gGenericPickupType, Game_GetEvent1(), GenericPickup_Touch);
    TaskType_SetHandler(gGenericPickupType, ComputerObject_GetEvent0(), ComputerObject_Draw443DC0);
    TaskType_SetHandler(gGenericPickupType, World_GetEvent1(), GenericPickup_Hide);
    TaskType_SetHandler(gGenericPickupType, World_GetEvent0(), GenericPickup_Show);
    gGenericPickupParams = ParamTable_New(3);
    ParamTable_Add(gGenericPickupParams, &p, &p.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gGenericPickupParams, &p, &p.obj.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gGenericPickupParams, &p, p.model, Param_String16(), 7, NULL);
}
HOOK(0x46C0C0, GenericPickup_Init);

static void generic_add(GenericPickup *t)
{
    long double r = radius(&t->obj);
    Vec3d *p;

    DynCube_Add(Game_GetWorld(), t, &t->pos, r, 2);
    p = DynCubeObj_GetPosition(t);
    Sector_AddObject(Game_GetWorld(), t, p);
}

static void generic_remove(GenericPickup *t)
{
    Sector_RemoveObject(t);
    DynCube_Remove(Game_GetWorld(), t);
}

void GenericPickup_Create(GenericPickup *t)
{
    QTaskList_Add(Game_GetTaskList2(), t);
    t->pos.x = 0.0;
    t->pos.y = 0.0;
    t->pos.z = 0.0;
    RigidDynCubeObj_Init(Game_GetWorld(), t, NULL);
    generic_add(t);
    t->registered = 0;
    t->pickedUp = 0;
    t->inWorld = 1;
    t->model[0] = 0;
}
HOOK(0x46C2B0, GenericPickup_Create);

void GenericPickup_Run(GenericPickup *t)
{
    if (t->pickedUp && t->inWorld) {
        Sound_PlayDefined("pickup", 0, NULL);
        generic_remove(t);
        t->inWorld = 0;
    }
}
HOOK(0x46C3C0, GenericPickup_Run);

void GenericPickup_Destroy(GenericPickup *t)
{
    QTaskList_Remove(Game_GetTaskList2(), t);
    if (t->inWorld)
        generic_remove(t);
    call_dac(&t->obj.task);
}
HOOK(0x46C410, GenericPickup_Destroy);

void GenericPickup_GetParams(void *t, void **out)
{
    (void)t;
    *out = gGenericPickupParams;
}
HOOK(0x46C480, GenericPickup_GetParams);

void GenericPickup_Changed(GenericPickup *t)
{
    void **m = VirModel_Find(t->model);

    if (m)
        RigidDynCubeObj_SetModel(t, m);
    if (t->inWorld) {
        long double r = radius(&t->obj);
        Vec3d *p;

        DynCube_Move(Game_GetWorld(), t, &t->pos, r);
        p = DynCubeObj_GetPosition(t);
        Sector_MoveObject(Game_GetWorld(), t, p);
    }
}
HOOK(0x46C490, GenericPickup_Changed);

void GenericPickup_Register(GenericPickup *t)
{
    register_flag(t, &t->registered, &t->pickedUp);
}
HOOK(0x46C570, GenericPickup_Register);

void GenericPickup_Unregister(GenericPickup *t)
{
    unregister_flag(t, &t->registered);
}
HOOK(0x46C5D0, GenericPickup_Unregister);

void GenericPickup_Touch(GenericPickup *t, Toucher *by)
{
    if (touches(&t->obj, by))
        t->pickedUp = 1;
}
HOOK(0x46C630, GenericPickup_Touch);

void GenericPickup_Show(GenericPickup *t, void *arg)
{
    if (!t->inWorld && !t->pickedUp) {
        generic_add(t);
        t->inWorld = 1;
    }
    CALL_CHILDREN_EV(t, World_GetEvent0(), arg);
}
HOOK(0x46C750, GenericPickup_Show);

void GenericPickup_Hide(GenericPickup *t, void *arg)
{
    if (t->inWorld) {
        generic_remove(t);
        t->inWorld = 0;
    }
    CALL_CHILDREN_EV(t, World_GetEvent1(), arg);
}
HOOK(0x46C8A0, GenericPickup_Hide);

void GenericPickup_Close(void)
{
    QTaskList_Free(gGenericPickupParams);
    TaskType_Free(gGenericPickupType);
}
HOOK(0x46C950, GenericPickup_Close);

/* GunPickup */

uint8_t GunPickup_GetEvent(void)
{
    return gGunPickupEvent;
}
HOOK(0x46C970, GunPickup_GetEvent);

/* drops a weapon at pos, falling to the ground below it */
GunPickup *GunPickup_New(void *parent, Vec3d *pos, Matrix *m, int weapon)
{
    struct {
        int unk0;
        Task *ignore;
    } query;
    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    char hit[0x70];
    Vec3d start, end;
    GunPickup *t;
    TaskFn fn;

    Mem_PushMode(1);
    t = (GunPickup *)Task_New(parent, gGunPickupType, NULL);
    t->pos = *pos;
    crt_sprintf(t->weaponName, "%d", weapon);
    fn = gTaskHandlers[Param_GetChangedEvent()][t->obj.task.type];
    if (fn)
        fn(t, NULL);
    Matrix_GetEuler(m, &ax, &ay, &az);
    if (ax < kLeft || !(ax <= kRight))
        ax = 0x1.921fb6p+1f;
    else
        ax = 0.0f;
    ay = ay >= kZeroF ? 0x1.921fb6p+0f : -0x1.921fb6p+0f;
    Matrix_SetEuler(&t->obj.matrix, ax, ay, az);
    start = *pos;
    start.z += kDropAbove;
    end = *pos;
    end.z -= kDropBelow;
    query.ignore = &t->obj.task;
    {
        uint8_t ev = Game_GetEvent8();

        Coll_Lines(Game_GetWorld(), hit, &start, &end, 1, ev, &query, 0xFFFFFFFF);
    }
    if (*(int *)(hit + 0x58)) {
        t->ground = *(Vec3d *)(hit + 8);
        t->ground.z += kRest;
    } else {
        t->ground = *pos;
    }
    Mem_PopMode();
    return t;
}
HOOK(0x46C980, GunPickup_New);

void GunPickup_Create(GunPickup *t);
void GunPickup_Run(GunPickup *t);
void Pickup_Destroy(RigidDynCubeObj *t);
void GunPickup_GetParams(void *t, void **out);
void GunPickup_Changed(GunPickup *t);
void GunPickup_Register(GunPickup *t);
void GunPickup_Unregister(GunPickup *t);
void GunPickup_Touch(GunPickup *t, Toucher *by);

void GunPickup_Init(void)
{
    GunPickup p;

    gGunPickupType = TaskType_New("GunPickup", sizeof(GunPickup), SectorInteriorObject_GetType(), 0);
    gGunPickupEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gGunPickupType, 0, GunPickup_Create);
    TaskType_SetHandler2(gGunPickupType, 1, GunPickup_Run);
    TaskType_SetHandler2(gGunPickupType, 2, Pickup_Destroy);
    TaskType_SetHandler(gGunPickupType, Param_GetTableEvent(), GunPickup_GetParams);
    TaskType_SetHandler(gGunPickupType, Param_GetChangedEvent(), GunPickup_Changed);
    TaskType_SetHandler2(gGunPickupType, Level_GetEventDA8(), GunPickup_Register);
    TaskType_SetHandler2(gGunPickupType, Level_GetEventDAC(), GunPickup_Unregister);
    TaskType_SetHandler(gGunPickupType, Game_GetEvent8(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gGunPickupType, Game_GetEvent13(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gGunPickupType, Game_GetEvent1(), GunPickup_Touch);
    gGunPickupParams = ParamTable_New(3);
    ParamTable_Add(gGunPickupParams, &p, &p.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gGunPickupParams, &p, &p.obj.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gGunPickupParams, &p, p.weaponName, Param_EnumString32(), 7, WeaponType_GetIdEnum());
}
HOOK(0x46CB70, GunPickup_Init);

/* creates a pickup lying at the origin until its parameters arrive */
static void pickup_create(RigidDynCubeObj *t)
{
    Vec3d zero = {0.0, 0.0, 0.0};
    long double r;

    QTaskList_Add(Game_GetTaskList2(), t);
    RigidDynCubeObj_Init(Game_GetWorld(), t, NULL);
    r = radius(t);
    DynCube_Add(Game_GetWorld(), t, &zero, r, 1);
    Sector_AddObject(t->world, t, DynCubeObj_GetPosition(t));
}

/* moves a pickup to pos after a parameter change */
static void pickup_move(RigidDynCubeObj *t, Vec3d *pos)
{
    long double r = radius(t);

    DynCube_Move(Game_GetWorld(), t, pos, r);
    Sector_MoveObject(t->world, t, DynCubeObj_GetPosition(t));
}

void GunPickup_Create(GunPickup *t)
{
    pickup_create(&t->obj);
    t->pos.x = 0.0;
    t->pos.y = 0.0;
    t->pos.z = 0.0;
    t->ground.x = 0.0;
    t->ground.y = 0.0;
    t->ground.z = 0.0;
    t->fall = 0.0;
    t->registered = 0;
    t->pickedUp = 0;
    t->weapon = 12;
    t->weaponName[0] = 0;
    t->ammo = 0;
}
HOOK(0x46CD20, GunPickup_Create);

void GunPickup_Run(GunPickup *t)
{
    if (!(t->pos.z >= t->ground.z)) {
        long double f = t->fall * kBounce;
        const char *name;
        Task *s;

        t->pos.z = t->ground.z;
        t->fall = f;
        name = Random_Float(Game_GetRandom()) > kHalf ? "weapondrop_01" : "weapondrop_02";
        s = Sound_Play(Level_GetDynamic(), name);
        if (s)
            Sound_SetPos(s, &t->pos);
    }
    if (!(t->pos.z > t->ground.z)) {
        long double f = t->fall;

        if (f < kZero)
            f = -f;
        if (!(f >= kRest)) {
            t->fall = 0.0;
            goto moved;
        }
    }
    t->pos.z = t->pos.z - t->fall;
    t->fall = t->fall + kGravity;
moved:
    {
        long double r = radius(&t->obj);

        DynCube_Move(Game_GetWorld(), t, &t->pos, r);
    }
    if (t->pickedUp)
        Task_Delete(t);
}
HOOK(0x46CEA0, GunPickup_Run);

void GunPickup_GetParams(void *t, void **out)
{
    (void)t;
    *out = gGunPickupParams;
}
HOOK(0x46D040, GunPickup_GetParams);

void GunPickup_Changed(GunPickup *t)
{
    char *w;
    void **m;

    Mem_PushMode(1);
    t->weapon = Script_EvalExpr(t->weaponName);
    Mem_PopMode();
    w = WeaponType_Find(t->weapon);
    if (!w)
        return;
    t->ground = t->pos;
    m = VirModel_Find(w + 0x61C);
    if (m)
        RigidDynCubeObj_SetModel(t, m);
    pickup_move(&t->obj, &t->pos);
    t->ammo = *(int *)(w + 0x648);
}
HOOK(0x46D050, GunPickup_Changed);

void GunPickup_Register(GunPickup *t)
{
    register_flag(t, &t->registered, &t->pickedUp);
}
HOOK(0x46D170, GunPickup_Register);

void GunPickup_Unregister(GunPickup *t)
{
    unregister_flag(t, &t->registered);
}
HOOK(0x46D1D0, GunPickup_Unregister);

void GunPickup_Touch(GunPickup *t, Toucher *by)
{
    char buf[260];
    PickupMsg msg;
    TaskFn fn;

    if (!touches(&t->obj, by))
        return;
    msg.id = t->weapon;
    msg.count = t->ammo;
    fn = gTaskHandlers[gGunPickupEvent][by->task->type];
    if (fn)
        fn(by->task, &msg);
    t->pickedUp = 1;
    crt_sprintf(buf, "PICKEDUPWEAPON%s", WeaponType_Find(t->weapon) + 0x110);
    StatusMessageShow(Level_GetDynamic(), buf, gEmptyString, gEmptyString);
}
HOOK(0x46D230, GunPickup_Touch);

void GunPickup_Close(void)
{
    QTaskList_Free(gGunPickupParams);
    TaskEvent_Free(gGunPickupEvent);
    TaskType_Free(gGunPickupType);
}
HOOK(0x46D3E0, GunPickup_Close);

/* AmmoPickup */

uint8_t AmmoPickup_GetEvent(void)
{
    return gAmmoPickupEvent;
}
HOOK(0x46D410, AmmoPickup_GetEvent);

void AmmoPickup_Create(AmmoPickup *t);
void AmmoPickup_Run(AmmoPickup *t);
void AmmoPickup_GetParams(void *t, void **out);
void AmmoPickup_Changed(AmmoPickup *t);
void AmmoPickup_Register(AmmoPickup *t);
void AmmoPickup_Unregister(AmmoPickup *t);
void AmmoPickup_Touch(AmmoPickup *t, Toucher *by);

void AmmoPickup_Init(void)
{
    AmmoPickup p;

    gAmmoPickupType = TaskType_New("AmmoPickup", sizeof(AmmoPickup), SectorInteriorObject_GetType(), 0);
    gAmmoPickupEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gAmmoPickupType, 0, AmmoPickup_Create);
    TaskType_SetHandler2(gAmmoPickupType, 1, AmmoPickup_Run);
    TaskType_SetHandler2(gAmmoPickupType, 2, Pickup_Destroy);
    TaskType_SetHandler(gAmmoPickupType, Param_GetTableEvent(), AmmoPickup_GetParams);
    TaskType_SetHandler(gAmmoPickupType, Param_GetChangedEvent(), AmmoPickup_Changed);
    TaskType_SetHandler2(gAmmoPickupType, Level_GetEventDA8(), AmmoPickup_Register);
    TaskType_SetHandler2(gAmmoPickupType, Level_GetEventDAC(), AmmoPickup_Unregister);
    TaskType_SetHandler(gAmmoPickupType, Game_GetEvent8(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gAmmoPickupType, Game_GetEvent13(), Task_HandlerNothing45F920);
    TaskType_SetHandler(gAmmoPickupType, Game_GetEvent1(), AmmoPickup_Touch);
    gAmmoPickupParams = ParamTable_New(4);
    ParamTable_Add(gAmmoPickupParams, &p, &p.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gAmmoPickupParams, &p, &p.obj.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gAmmoPickupParams, &p, p.ammoName, Param_EnumString32(), 7, AmmoType_GetEnum());
    ParamTable_Add(gAmmoPickupParams, &p, &p.count, Param_Int16(), 7, NULL);
}
HOOK(0x46D420, AmmoPickup_Init);

void AmmoPickup_Create(AmmoPickup *t)
{
    pickup_create(&t->obj);
    t->pos.x = 0.0;
    t->pos.y = 0.0;
    t->pos.z = 0.0;
    t->registered = 0;
    t->pickedUp = 0;
    t->ammo = 0;
    t->ammoName[0] = 0;
    t->count = 0;
}
HOOK(0x46D5F0, AmmoPickup_Create);

void AmmoPickup_Run(AmmoPickup *t)
{
    if (t->pickedUp)
        Task_Delete(t);
}
HOOK(0x46D740, AmmoPickup_Run);

/* the delete handler of GunPickup and AmmoPickup */
void Pickup_Destroy(RigidDynCubeObj *t)
{
    QTaskList_Remove(Game_GetTaskList2(), t);
    Sector_RemoveObject(t);
    DynCube_Remove(Game_GetWorld(), t);
    call_dac(&t->task);
}
HOOK(0x46D760, Pickup_Destroy);

void AmmoPickup_GetParams(void *t, void **out)
{
    (void)t;
    *out = gAmmoPickupParams;
}
HOOK(0x46D7C0, AmmoPickup_GetParams);

void AmmoPickup_Changed(AmmoPickup *t)
{
    char *a;
    void **m;

    Mem_PushMode(1);
    t->ammo = Script_EvalExpr(t->ammoName);
    Mem_PopMode();
    a = AmmoType_Find(t->ammo);
    if (!a)
        return;
    m = VirModel_Find(a + 0x148);
    if (m)
        RigidDynCubeObj_SetModel(t, m);
    pickup_move(&t->obj, &t->pos);
}
HOOK(0x46D7D0, AmmoPickup_Changed);

void AmmoPickup_Register(AmmoPickup *t)
{
    register_flag(t, &t->registered, &t->pickedUp);
}
HOOK(0x46D8D0, AmmoPickup_Register);

void AmmoPickup_Unregister(AmmoPickup *t)
{
    unregister_flag(t, &t->registered);
}
HOOK(0x46D930, AmmoPickup_Unregister);

void AmmoPickup_Touch(AmmoPickup *t, Toucher *by)
{
    PickupMsg msg;
    TaskFn fn;

    if (!touches(&t->obj, by))
        return;
    msg.id = t->ammo;
    msg.count = t->count;
    fn = gTaskHandlers[gAmmoPickupEvent][by->task->type];
    if (fn)
        fn(by->task, &msg);
    t->pickedUp = 1;
    StatusMessageShow(Level_GetDynamic(), "PICKEDUPAMMO", "STATUSSCREEN:ammo.spr", gEmptyString);
}
HOOK(0x46D990, AmmoPickup_Touch);

void AmmoPickup_Close(void)
{
    QTaskList_Free(gAmmoPickupParams);
    TaskEvent_Free(gAmmoPickupEvent);
    TaskType_Free(gAmmoPickupType);
}
HOOK(0x46DB10, AmmoPickup_Close);

/* AddAmmo */

void AddAmmo_Create(AddAmmo *t);
void AddAmmo_Run(AddAmmo *t);
void AddAmmo_GetParams(void *t, void **out);
void AddAmmo_Changed(AddAmmo *t);

void AddAmmo_Init(void)
{
    AddAmmo p;

    gAddAmmoType = TaskType_New("AddAmmo", sizeof(AddAmmo), Task_GetRootType(), 0);
    TaskType_SetHandler(gAddAmmoType, 0, AddAmmo_Create);
    TaskType_SetHandler2(gAddAmmoType, 1, AddAmmo_Run);
    TaskType_SetHandler2(gAddAmmoType, 2, TASK_NOTHING);
    TaskType_SetHandler(gAddAmmoType, Param_GetTableEvent(), AddAmmo_GetParams);
    TaskType_SetHandler(gAddAmmoType, Param_GetChangedEvent(), AddAmmo_Changed);
    gAddAmmoParams = ParamTable_New(2);
    ParamTable_Add(gAddAmmoParams, &p, p.ammoName, Param_EnumString32(), 7, AmmoType_GetEnum());
    ParamTable_Add(gAddAmmoParams, &p, &p.count, Param_Int16(), 7, NULL);
}
HOOK(0x46DB40, AddAmmo_Init);

void AddAmmo_Create(AddAmmo *t)
{
    t->ammo = 0;
    t->ammoName[0] = 0;
    t->count = 0;
}
HOOK(0x46DC20, AddAmmo_Create);

void AddAmmo_Run(AddAmmo *t)
{
    char *h = (char *)t->task.parent;

    if (TaskType_IsA(((Task *)h)->type, Human_GetType()) && AmmoType_Find(t->ammo))
        AmmoList_Add(h + 0x340, t->ammo, t->count);
    Task_Delete(t);
}
HOOK(0x46DC40, AddAmmo_Run);

void AddAmmo_GetParams(void *t, void **out)
{
    (void)t;
    *out = gAddAmmoParams;
}
HOOK(0x46DCA0, AddAmmo_GetParams);

void AddAmmo_Changed(AddAmmo *t)
{
    Mem_PushMode(1);
    t->ammo = Script_EvalExpr(t->ammoName);
    Mem_PopMode();
}
HOOK(0x46DCB0, AddAmmo_Changed);

void AddAmmo_Close(void)
{
    QTaskList_Free(gAddAmmoParams);
    TaskType_Free(gAddAmmoType);
}
HOOK(0x46DCE0, AddAmmo_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53FDE0) uint8_t gGunPickupEvent INIT(255);
PAD(igd, 53FDE0, 53FDE1, 0x3);
DATA(53FE28) uint8_t gAmmoPickupEvent INIT(255);
PAD(igd, 53FE28, 53FE29, 0x3);
BSS(5BE2D0) uint16_t gGenericPickupType;
PAD(igb, 5BE2D0, 5BE2D2, 0x2);
BSS(5BE2D0) void *gGenericPickupParams;
PAD(igb, 5BE2D0, 5BE2D8, 0x8);
BSS(5BE2D0) uint16_t gGunPickupType;
PAD(igb, 5BE2D0, 5BE2E2, 0x16);
BSS(5BE2D0) void *gGunPickupParams;
PAD(igb, 5BE2D0, 5BE2FC, 0x4);
BSS(5BE2D0) uint16_t gAmmoPickupType;
PAD(igb, 5BE2D0, 5BE302, 0x2);
BSS(5BE2D0) void *gAmmoPickupParams;
PAD(igb, 5BE2D0, 5BE308, 0x4);
BSS(5BE2D0) uint16_t gAddAmmoType;
PAD(igb, 5BE2D0, 5BE30E, 0x2);
BSS(5BE2D0) void *gAddAmmoParams;
