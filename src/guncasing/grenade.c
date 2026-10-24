/* Grenade: a thrown physics object that explodes or flashes when its fuse
   runs out. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "guncasing/guncasing.h"

FN_EXTERN(HitZone_GetArgSize);
FN_EXTERN(Task_FalseHandler);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x4071D0, uint8_t, WeaponConfig_GetEvent, (void));
ORIG(0x413B20, uint8_t, Weapon_GetFireEvent, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x4168F0, void *, Game_GetCutsceneList, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (void *blast, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x450CE0, void, HumanAI_Broadcast, (Noise *noise));
ORIG(0x47E9A0, void, FlashBang_Explode, (Vec3d *pos, CollHit *hit, float radius, float falloff));
ORIG(0x480FE0, void, Explosion_Spawn, (Vec3d *pos, int a, int b, float c, float d, float e, float f, int g, int h,
                                       CollHit *hit, const char *sprite, const char *sound, int j, int k, float l,
                                       int m));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CABF0, void, Coll_DropPoints, (void *world, CollHit *hit, Vec3d *points, int count, double *dz, int event,
                                       CollIgnore *ignore, int flags));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, Vec3d *pos));
ORIG(0x4ECDB0, void, Body_Update, (Body *body));
ORIG(0x4EE0E0, uint8_t, PhysicsObjType_GetParseEvent, (void));
ORIG(0x4EE0F0, uint8_t, PhysicsObjType_GetSizeEvent, (void));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4FB5E0, uint16_t, PhysicsObj_GetType, (void));
ORIG(0x4FB5F0, void, PhysicsObj_Init, (void *obj));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *obj, PhysType *phys));
ORIG(0x4FB980, void, PhysicsObj_Free, (void *obj));
ORIG(0x4FC0E0, void, PhysicsObj_Collide, (void *obj, float a, float b, float c, void *world, int event,
                                          CollIgnore *msg));
ORIG(0x4FC7B0, int, PhysicsObj_IsResting, (void *obj));
ORIG(0x4FE650, float *, TagItem_AddReal32, (void *table, const char *name, float value));
ORIG(0x4FE730, float *, TagItem_Find, (void *table, const char *name));

extern uint16_t gGrenadeType;
extern uint8_t gGrenadeEvent;

extern float kFuseFrames; /* -30 */

#define Task_FalseHandler (FN(0x45F920, Task_FalseHandler))
#define HitZone_GetArgSize (FN(0x427F80, HitZone_GetArgSize))

/* a value of a weapon config table */
#define CONFIG_VALUE(c) (*(float *)((char *)(c) + 0x124))

typedef struct GrenadeConfigMsg {
    void *table;
    WeaponType *wt;
} GrenadeConfigMsg;

typedef struct GrenadeArgsMsg {
    GrenadeType *cfg;
    void *args;
    int next;
} GrenadeArgsMsg;

void Grenade_CreateHandler(Grenade *g);
void Grenade_RunHandler(Grenade *g);
void Grenade_DeleteHandler(Grenade *g);
void Grenade_HitHandler(Grenade *g);
void Grenade_BlastHandler(Grenade *g, void *blast);
void Grenade_ParseArgsHandler(void *type, GrenadeArgsMsg *msg);
void Grenade_FireHandler(void *type, Task **msg);
void Grenade_WeaponConfigHandler(void *type, GrenadeConfigMsg *msg);

void Grenade_Open(void)
{
    gGrenadeType = TaskType_New("Grenade", 0x738, PhysicsObj_GetType(), 0, 0);
    gGrenadeEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gGrenadeType, 0, Grenade_CreateHandler);
    TaskType_SetHandler2(gGrenadeType, 1, Grenade_RunHandler);
    TaskType_SetHandler2(gGrenadeType, 2, Grenade_DeleteHandler);
    TaskType_SetHandler(gGrenadeType, Game_GetEvent9(), Grenade_HitHandler);
    TaskType_SetHandler(gGrenadeType, Game_GetEvent11(), Grenade_BlastHandler);
    TaskType_SetHandler(gGrenadeType, Game_GetEvent8(), Task_FalseHandler);
    TaskType_SetHandler(gGrenadeType, Game_GetEvent13(), Task_FalseHandler);
    TaskType_SetHandler(gGrenadeType, PhysicsObjType_GetParseEvent(), Grenade_ParseArgsHandler);
    TaskType_SetHandler(gGrenadeType, PhysicsObjType_GetSizeEvent(), HitZone_GetArgSize);
    TaskType_SetHandler(gGrenadeType, Weapon_GetFireEvent(), Grenade_FireHandler);
    TaskType_SetHandler(gGrenadeType, WeaponConfig_GetEvent(), Grenade_WeaponConfigHandler);
    Script_RegisterConstant("TASKTYPE_GRENADE", gGrenadeType);
    Script_RegisterConstant("GRENADE_EXPLOSIONTYPE_NORMAL", 0);
    Script_RegisterConstant("GRENADE_EXPLOSIONTYPE_FLASHBANG", 1);
}
HOOK(0x476730, Grenade_Open);

/* reads the three explosion settings from the weapon config, adding them
   with their defaults when missing */
void Grenade_WeaponConfigHandler(void *type, GrenadeConfigMsg *msg)
{
    static const char kFalloff[] = "Grenade - Explosion falloff (meters)";
    static const char kRadius[] = "Grenade - Explosion radius (meters)";
    static const char kDamage[] = "Grenade - Explosion damage factor";
    float *falloff, *radius, *damage;
    uint32_t f, r, d;
    GrenadeConfig *c;

    if (msg->wt->grenade) {
        Mem_Free(msg->wt->grenade);
        msg->wt->grenade = NULL;
    }
    falloff = TagItem_Find(msg->table, kFalloff);
    if (!falloff)
        falloff = TagItem_AddReal32(msg->table, kFalloff, 0.0f);
    radius = TagItem_Find(msg->table, kRadius);
    if (!radius)
        radius = TagItem_AddReal32(msg->table, kRadius, 5.0f);
    damage = TagItem_Find(msg->table, kDamage);
    if (!damage)
        damage = TagItem_AddReal32(msg->table, kDamage, 1.0f);
    f = *(uint32_t *)&CONFIG_VALUE(falloff);
    r = *(uint32_t *)&CONFIG_VALUE(radius);
    d = *(uint32_t *)&CONFIG_VALUE(damage);
    msg->wt->grenade = Mem_Alloc(sizeof(GrenadeConfig), 4);
    c = msg->wt->grenade;
    *(uint32_t *)&c->damage = d;
    *(uint32_t *)&c->radius = r;
    *(uint32_t *)&c->falloff = f;
}
HOOK(0x4768A0, Grenade_WeaponConfigHandler);

void Grenade_CreateHandler(Grenade *g)
{
    Vec3d zero = {0, 0, 0};
    float s, radius;

    g->bounced = 0;
    g->bounceTick = Flow_GetFrames();
    g->owner = NULL;
    g->weapon = NULL;
    RigidDynCubeObj_Init(Game_GetWorld(), g, NULL);
    s = max_scale(&g->o);
    radius = VirModel_GetRadius(g->o.model) * s;
    DynCube_Add(Game_GetWorld(), g, &zero, radius, 2);
    QTaskList_Add(Game_GetCutsceneList(), g);
}
HOOK(0x4769B0, Grenade_CreateHandler);

void Grenade_UpdatePhysics(Grenade *g);
void Grenade_UpdateBounce(Grenade *g);

void Grenade_RunHandler(Grenade *g)
{
    GrenadeType *gt = g->phys->data;
    GrenadeConfig *c;
    CollIgnore ignore;
    CollHit hit;
    double dz;
    uint8_t ev;

    if (g->fuse) {
        g->fuse--;
        Grenade_UpdatePhysics(g);
        Grenade_UpdateBounce(g);
        BROADCAST(g->o.task.head, 1);
        return;
    }
    ignore.task = &g->o.task;
    dz = -2048.0;
    ev = Game_GetEvent13();
    Coll_DropPoints(Game_GetWorld(), &hit, &g->o.pos, 1, &dz, ev, &ignore, -1);
    if (g->weapon && (c = g->weapon->grenade) != NULL) {
        if (gt->explosion == 0)
            Explosion_Spawn(&g->o.pos, 1, 1, c->damage, c->radius * kMeter, c->falloff * kMeter, 6144.0f, 3, 1,
                            &hit, "1009_01_1", "explo_02_m", 0x15, 2, 40960.0f, 0);
        else if (gt->explosion == 1) {
            FlashBang_Explode(&g->o.pos, &hit, c->radius * kMeter, c->falloff * kMeter);
            Task_Delete(g);
            return;
        }
    } else if (gt->explosion == 0)
        Explosion_Spawn(&g->o.pos, 1, 1, 1.0f, 20480.0f, 0, 6144.0f, 3, 1, &hit, "1009_01_1", "explo_02_m", 0x15, 2,
                        40960.0f, 0);
    else if (gt->explosion == 1)
        FlashBang_Explode(&g->o.pos, &hit, 20480.0f, 40960.0f);
    Task_Delete(g);
}
HOOK(0x476AC0, Grenade_RunHandler);

void Grenade_MakeNoise(Grenade *g, int kind, int unk8, float radius);

/* plays the bounce sound once per landing; a new one needs 10 ticks in the air */
void Grenade_UpdateBounce(Grenade *g)
{
    if (!g->hit) {
        if (Flow_GetFrames() - g->bounceTick > 10)
            g->bounced = 0;
        return;
    }
    if (g->bounced)
        return;
    {
        void *snd = Sound_Play(Level_GetDynamic(), "grenade_bnc_1");

        if (snd)
            Sound_SetPos(snd, &g->o.pos);
    }
    Grenade_MakeNoise(g, 0x12, 2, 20480.0f);
    g->bounced = 1;
    g->bounceTick = Flow_GetFrames();
}
HOOK(0x476CC0, Grenade_UpdateBounce);

void Grenade_MakeNoise(Grenade *g, int kind, int unk8, float radius)
{
    Noise n;

    n.kind = kind;
    n.unk4 = -1;
    n.unk8 = unk8;
    n.radius = radius;
    n.pos = &g->o.pos;
    n.source = &g->o.task;
    n.unk18 = 0;
    HumanAI_Broadcast(&n);
}
HOOK(0x476D40, Grenade_MakeNoise);

void Grenade_UpdatePhysics(Grenade *g)
{
    CollIgnore msg;
    uint8_t ev;

    if (PhysicsObj_IsResting(g))
        return;
    msg.task = &g->o.task;
    ev = Game_GetEvent8();
    PhysicsObj_Collide(g, 0x1.64ce9ep+5f, 1.0f, 0.92f, Game_GetWorld(), ev, &msg);
    if (g->hit)
        return;
    g->body.vel.x = g->body.vel.x * kDrag;
    g->body.vel.y = g->body.vel.y * kDrag;
    g->body.vel.z = g->body.vel.z * kDrag;
    Body_Update(&g->body);
}
HOOK(0x476D90, Grenade_UpdatePhysics);

void Grenade_DeleteHandler(Grenade *g)
{
    if (g->owner) {
        TaskRef_Remove(g->owner);
        g->owner = NULL;
    }
    PhysicsObj_Free(g);
    DynCube_Remove(Game_GetWorld(), g);
    QTaskList_Remove(Game_GetCutsceneList(), g);
}
HOOK(0x476E50, Grenade_DeleteHandler);

/* a bullet makes it go off next frame */
void Grenade_HitHandler(Grenade *g)
{
    g->fuse = 0;
}
HOOK(0x476EA0, Grenade_HitHandler);

/* so does a nearby explosion */
void Grenade_BlastHandler(Grenade *g, void *blast)
{
    float s, radius;
    long double v;

    if (!g->fuse)
        return;
    s = max_scale(&g->o);
    radius = VirModel_GetRadius(g->o.model) * s;
    v = Game_BlastFalloff(blast, g, &g->o.pos, radius, 0);
    if (v < kZero || v > kZero)
        g->fuse = 0;
}
HOOK(0x476EB0, Grenade_BlastHandler);

void Grenade_ParseArgsHandler(void *type, GrenadeArgsMsg *msg)
{
    GrenadeType *cfg = msg->cfg;
    void *args = msg->args;
    int i = msg->next;

    cfg->fuse = Script_GetArgReal(args, i);
    cfg->explosion = Script_GetArgInt(args, i + 1);
}
HOOK(0x476F70, Grenade_ParseArgsHandler);

Grenade *Grenade_Throw(void *parent, Task *owner, Vec3d *pos, const Matrix *matrix, const Vec3f *vel,
                       WeaponType *wt, int cooked);

/* msg is the weapon's fire message; the thrower says how long it held the
   grenade by answering the grenade event */
void Grenade_FireHandler(void *type, Task **msg)
{
    Task *thrower = msg[0]->parent;
    TaskFn fn = TASK_HANDLER(gGrenadeEvent, thrower->type);
    void *slot = msg;
    int cooked = 0;

    if (fn) {
        fn(thrower, &slot);
        cooked = (int)(intptr_t)slot;
    }
    Grenade_Throw(Level_GetDynamic(), msg[1], (Vec3d *)msg[2], (Matrix *)msg[3], (Vec3f *)msg[4],
                  (WeaponType *)msg[8], cooked);
}
HOOK(0x476FA0, Grenade_FireHandler);

void Grenade_Close(void)
{
    TaskEvent_Free(gGrenadeEvent);
    TaskType_Free(gGrenadeType);
}
HOOK(0x477010, Grenade_Close);

uint8_t Grenade_GetEvent(void)
{
    return gGrenadeEvent;
}
HOOK(0x477030, Grenade_GetEvent);

Grenade *Grenade_Throw(void *parent, Task *owner, Vec3d *pos, const Matrix *matrix, const Vec3f *vel,
                       WeaponType *wt, int cooked)
{
    Grenade *g;
    void **model;
    PhysType *phys;
    float s, radius;
    int n;

    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        g = (Grenade *)Task_New(parent, gGrenadeType, 0);
        Mem_ClearMode();
    } else
        g = (Grenade *)Task_New(parent, gGrenadeType, 0);
    g->weapon = wt;
    g->owner = owner ? TaskRef_Add(owner) : NULL;
    model = VirModel_Find(wt->key);
    if (!model) {
        Error_Show("VirModel is null");
        for (;;)
            ;
    }
    RigidDynCubeObj_SetModel(g, model);
    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        PhysicsObj_Init(g);
        Mem_ClearMode();
    } else
        PhysicsObj_Init(g);
    phys = PhysicsObjType_Find(wt->key);
    n = (int)(0u - (unsigned)cooked - (unsigned)round_int(((GrenadeType *)phys->data)->fuse * kFuseFrames));
    g->fuse = n;
    if (n < 0)
        g->fuse = 0;
    s = max_scale(&g->o);
    radius = VirModel_GetRadius(g->o.model) * s;
    DynCube_Move(Game_GetWorld(), g, pos, radius);
    PhysicsObj_SetPhysType(g, phys);
    g->body.vel = *vel;
    Body_Update(&g->body);
    g->o.matrix = *matrix;
    Grenade_MakeNoise(g, 0x11, 1, -1.0f);
    return g;
}
HOOK(0x477040, Grenade_Throw);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54066C) uint16_t gGrenadeType INIT(385);
PAD(igd, 54066C, 54066E, 0x2);
BSS(5BE3B8) uint8_t gGrenadeEvent;
PAD(igb, 5BE3B8, 5BE3B9, 0x3);
