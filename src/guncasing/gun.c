/* Gun: the hand weapon task, and GunCasing, the empty cartridges it throws. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "guncasing/guncasing.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401780, void, Task_DeleteChildren, (void *task));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x413B50, uint8_t, Weapon_GetTypeEvent, (void));
ORIG(0x413B60, uint8_t, Weapon_GetEvent7, (void));
ORIG(0x413BD0, WeaponType *, WeaponType_Find, (int id));
ORIG(0x413DE0, void *, WeaponType_GetIdEnum, (void));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416880, uint8_t, Game_GetEvent13, (void));
ORIG(0x413D10, void, Weapon_Fire, (Gun *g, Task *owner, Vec3d *muzzle, Matrix *matrix, Vec3f *vel, Matrix *aim,
                                    WeaponType *wt, void *ammoType, int unk));
ORIG(0x414A60, void, Matrix_SetYAxis, (Matrix *m, const Vec3f *dir));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416A70, Task *, Game_GetPlayer, (int unk));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x460BF0, uint8_t, Human_GetEvent0, (void));
ORIG(0x460C10, uint8_t, Human_GetEvent1, (void));
ORIG(0x47B100, uint8_t, Shots_GetEvent, (void));
ORIG(0x47B110, Task *, Shots_New, (void *parent, Task *gun, Task *owner));
ORIG(0x47B170, void, Shots_Release, (Task *shots, int unk));
ORIG(0x47B270, void, Shots_SetPos, (Task *shots, Vec3d *pos));
ORIG(0x47CD70, void *, AmmoType_Find, (int id));
ORIG(0x47E6F0, uint8_t, sub_47E6F0, (void));
ORIG(0x487190, uint8_t, Human_GetHudEvent, (void));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4B3BA0, void, Matrix_SetRotY, (Matrix *m, float a));
ORIG(0x4B4740, unsigned, Random_Next, (void *rng));
ORIG(0x4B4770, long double, Random_Float, (void *rng));
ORIG(0x4B47C0, int, Random_Int, (void *rng, int n));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C4950, void, sub_4C4950, (void *a, void *b, void *c));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6840, void, DynCube_ListRemove, (void *world, void *obj, int list));
ORIG(0x4C6890, void, DynCube_ListAdd, (void *world, void *obj, int list));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D9850, uint8_t, MagicObj_GetHideEvent, (void));
ORIG(0x4D9860, void *, MagicObj_GetList, (void));
ORIG(0x4E0930, Task *, Coll_NewTask, (void *parent, void *obj));
ORIG(0x4E81F0, char *, Render_GetView, (void));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (void *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, Vec3d *pos));
ORIG(0x4E75A0, int, sub_4E75A0, (void));
ORIG(0x4F0EE0, void *, Sound_GetOwner, (void));
ORIG(0x4F0FB0, void *, Param_Int16, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F1E20, void *, Param_Enum, (void));
ORIG(0x4F1FD0, uint8_t, Weapon_GetStateEvent, (void));
ORIG(0x4F1FE0, uint8_t, Weapon_GetMoveEvent, (void));
ORIG(0x4F1FF0, uint8_t, Weapon_GetTriggerEvent, (void));
ORIG(0x4F2000, uint8_t, Weapon_GetEvent3, (void));
ORIG(0x4F2010, uint8_t, Weapon_GetEvent4, (void));
ORIG(0x4F2020, uint8_t, Weapon_GetEvent5, (void));
ORIG(0x4F2030, uint8_t, Weapon_GetEvent6, (void));
ORIG(0x4F2040, uint8_t, Weapon_GetEvent7b, (void));

#define gGunParams VAR(void *, 0x5BE3C0)
#define gGunCasingType VAR(uint16_t, 0x5BE3C4)
#define gGunType VAR(uint16_t, 0x5BE3C6)
#define gGunEvents ((uint8_t *)0x5407B8)

#define kCasingGravity VAR(float, 0x53346C)
#define kTwoPi VAR(float, 0x5334A8)
#define kSpinStep VAR(float, 0x5334BC)    /* 1/45 */

#define Task_NullHandler ((void *)0x4D3710)
#define Task_FalseHandler ((void *)0x45F920)
#define Human_ClearResult ((void *)0x45E3D0)

void Gun_CreateHandler(Gun *g);
void Gun_RunHandler(Gun *g);
void Gun_DeleteHandler(Gun *g);
void Gun_HideHandler(Gun *g, int *hide);
void Gun_DrawHandler(Gun *g);
void Gun_GetParamTableHandler(Gun *g, void **out);
void Gun_ParamChangedHandler(Gun *g);
void Gun_GetStateHandler(Gun *g, char **out);
void Gun_MoveHandler(Gun *g, MoveMsg *msg);
void GunCasing_CreateHandler(GunCasing *c);
void GunCasing_RunHandler(GunCasing *c);
void GunCasing_DeleteHandler(GunCasing *c);
void GunCasing_DrawHandler(GunCasing *c);

void Gun_FireHandler(Gun *g, GunFire *msg);

uint16_t Gun_GetType(void)
{
    return gGunType;
}
HOOK(0x477BF0, Gun_GetType);

uint8_t Gun_GetEvent0(void)
{
    return gGunEvents[0];
}
HOOK(0x477C00, Gun_GetEvent0);

uint8_t Gun_GetEvent1(void)
{
    return gGunEvents[1];
}
HOOK(0x477C10, Gun_GetEvent1);

uint8_t Gun_GetEvent2(void)
{
    return gGunEvents[2];
}
HOOK(0x477C20, Gun_GetEvent2);

uint8_t Gun_GetEvent3(void)
{
    return gGunEvents[3];
}
HOOK(0x477C30, Gun_GetEvent3);

uint8_t Gun_GetEvent4(void)
{
    return gGunEvents[4];
}
HOOK(0x477C40, Gun_GetEvent4);

Gun *Gun_New(void *parent, int16_t count, int id, int rounds, char unkF1)
{
    WeaponType *wt;
    uint16_t type;
    TaskFn fn;
    Gun *g;

    Mem_PushMode(1);
    wt = WeaponType_Find(id);
    if (wt)
        type = wt->gunType;
    else {
        Warning_Show("Unknown weapon type: %d", id);
        type = gGunType;
    }
    g = (Gun *)Task_New(parent, type, 0);
    g->unkF1 = unkF1;
    g->unk124 = count;
    crt_sprintf(g->name, "%d", id);
    fn = TASK_HANDLER(Param_GetChangedEvent(), g->o.task.type);
    if (fn)
        fn(g, 0);
    g->rounds = rounds;
    Mem_PopMode();
    return g;
}
HOOK(0x477C50, Gun_New);

GunCasing *GunCasing_New(void *parent, const Vec3d *pos, const Vec3f *vel, const Matrix *matrix, const char *model)
{
    GunCasing *c = (GunCasing *)Task_New(parent, gGunCasingType, 0);
    float ax, ay, az;

    RigidDynCubeObj_SetModel(c, VirModel_Find(model));
    c->o.pos = *pos;
    c->vel = *vel;
    c->o.matrix = *matrix;
    c->life = 19;
    ax = (long double)(uint32_t)Random_Int(Game_GetRandom(), 2) * kDegToRad;
    ay = (long double)(uint32_t)Random_Int(Game_GetRandom(), 5) * kDegToRad;
    az = (long double)(uint32_t)Random_Int(Game_GetRandom(), 5) * kDegToRad;
    Matrix_SetEuler(&c->spin, ax, ay, az);
    return c;
}
HOOK(0x477D10, GunCasing_New);

void Gun_Open(void)
{
    Gun p;
    void *e;
    int i;

    gGunType = TaskType_New("Gun", 0x1B0, RigidDynCubeObj_GetType(), 0, 0);
    for (i = 0; i < 5; i++)
        gGunEvents[i] = TaskEvent_Alloc(0);
    TaskType_SetHandler(gGunType, 0, Gun_CreateHandler);
    TaskType_SetHandler2(gGunType, 1, Gun_RunHandler);
    TaskType_SetHandler2(gGunType, 2, Gun_DeleteHandler);
    TaskType_SetHandler(gGunType, MagicObj_GetHideEvent(), Gun_HideHandler);
    TaskType_SetHandler(gGunType, 4, Gun_DrawHandler);
    TaskType_SetHandler(gGunType, 7, NULL);
    TaskType_SetHandler(gGunType, Param_GetTableEvent(), Gun_GetParamTableHandler);
    TaskType_SetHandler(gGunType, Param_GetChangedEvent(), Gun_ParamChangedHandler);
    TaskType_SetHandler(gGunType, Game_GetEvent7(), Human_ClearResult);
    TaskType_SetHandler(gGunType, Game_GetEvent9(), Task_NullHandler);
    TaskType_SetHandler(gGunType, Game_GetEvent8(), Task_FalseHandler);
    TaskType_SetHandler(gGunType, Game_GetEvent13(), Task_FalseHandler);
    TaskType_SetHandler(gGunType, Weapon_GetStateEvent(), Gun_GetStateHandler);
    TaskType_SetHandler(gGunType, Weapon_GetMoveEvent(), Gun_MoveHandler);
    TaskType_SetHandler(gGunType, Weapon_GetTriggerEvent(), Gun_FireHandler);
    TaskType_SetHandler(gGunType, Weapon_GetEvent3(), (void *)0x4798C0);
    TaskType_SetHandler(gGunType, Weapon_GetEvent4(), (void *)0x479AA0);
    TaskType_SetHandler2(gGunType, Weapon_GetEvent5(), (void *)0x479B20);
    TaskType_SetHandler2(gGunType, Weapon_GetEvent6(), (void *)0x479C10);
    TaskType_SetHandler(gGunType, Weapon_GetEvent7b(), (void *)0x479C20);
    TaskType_SetHandler(gGunType, Weapon_GetTypeEvent(), (void *)0x479C80);
    TaskType_SetHandler(gGunType, Weapon_GetEvent7(), (void *)0x479CA0);
    TaskType_SetHandler(gGunType, Human_GetHudEvent(), (void *)0x479D20);
    TaskType_SetHandler(gGunType, AIDetection_GetEvent0(), (void *)0x47A8E0);
    TaskType_SetHandler(gGunType, Shots_GetEvent(), (void *)0x47A940);
    TaskType_SetHandler(gGunType, Human_GetEvent0(), (void *)0x479780);
    TaskType_SetHandler(gGunType, Human_GetEvent1(), (void *)0x479990);
    TaskType_SetHandler(gGunType, World_GetEvent1(), (void *)0x47A960);
    TaskType_SetHandler(gGunType, World_GetEvent0(), (void *)0x47A9E0);
    TaskType_SetHandler(gGunType, sub_47E6F0(), NULL);
    gGunParams = ParamTable_New(3);
    e = WeaponType_GetIdEnum();
    ParamTable_Add(gGunParams, &p, p.name, Param_Enum(), 7, e);
    ParamTable_Add(gGunParams, &p, &p.unk124, Param_Int16(), 7, NULL);
    gGunCasingType = TaskType_New("GunCasing", 0x128, RigidDynCubeObj_GetType(), 0x32, 0);
    TaskType_SetHandler(gGunCasingType, 0, GunCasing_CreateHandler);
    TaskType_SetHandler2(gGunCasingType, 1, GunCasing_RunHandler);
    TaskType_SetHandler2(gGunCasingType, 2, GunCasing_DeleteHandler);
    TaskType_SetHandler(gGunCasingType, 4, GunCasing_DrawHandler);
    TaskType_SetHandler(gGunCasingType, Game_GetEvent7(), Human_ClearResult);
    Script_RegisterConstant("TASKTYPE_GUN", gGunType);
}
HOOK(0x477E30, Gun_Open);

void GunCasing_CreateHandler(GunCasing *c)
{
    RigidDynCubeObj_Init(Game_GetWorld(), c, NULL);
    c->o.hidden = 0;
    DynCube_ListAdd(Game_GetWorld(), c, 0);
}
HOOK(0x478290, GunCasing_CreateHandler);

/* falls and spins for a while, then goes away */
void GunCasing_RunHandler(GunCasing *c)
{
    const float *m = c->o.matrix.m;
    const float *s = c->spin.m;
    long double vx;
    float vy, vz;
    Matrix r;

    if (!c->life) {
        Task_Delete(c);
        return;
    }
    vx = c->vel.x * kDrag;
    c->life--;
    c->vel.x = vx;
    vy = c->vel.y * kDrag;
    c->vel.y = vy;
    vz = (c->vel.z - kCasingGravity) * kDrag;
    c->vel.z = vz;
    c->o.pos.x = vx + c->o.pos.x;
    c->o.pos.y = vy + c->o.pos.y;
    c->o.pos.z = vz + c->o.pos.z;
    /* the term added last differs per element */
    r.m[0] = (m[2] * s[6] + m[1] * s[3]) + m[0] * s[0];
    r.m[1] = (m[2] * s[7] + m[0] * s[1]) + m[1] * s[4];
    r.m[2] = (m[2] * s[8] + m[0] * s[2]) + m[1] * s[5];
    r.m[3] = (m[5] * s[6] + m[3] * s[0]) + m[4] * s[3];
    r.m[4] = (m[4] * s[4] + m[5] * s[7]) + m[3] * s[1];
    r.m[5] = (m[4] * s[5] + m[5] * s[8]) + m[3] * s[2];
    r.m[6] = (m[8] * s[6] + m[7] * s[3]) + m[6] * s[0];
    r.m[7] = (m[7] * s[4] + m[8] * s[7]) + m[6] * s[1];
    r.m[8] = (m[7] * s[5] + m[8] * s[8]) + m[6] * s[2];
    r.unk24 = c->spin.unk24 + c->o.matrix.unk24 + 1;
    c->o.matrix = r;
}
HOOK(0x4782D0, GunCasing_RunHandler);

void GunCasing_DeleteHandler(GunCasing *c)
{
    DynCube_ListRemove(Game_GetWorld(), c, 0);
}
HOOK(0x4784F0, GunCasing_DeleteHandler);

void GunCasing_DrawHandler(GunCasing *c)
{
    if (*(int *)(Render_GetView() + 0x5C))
        TASK_HANDLER(4, RigidDynCubeObj_GetType())(c, 0);
}
HOOK(0x478510, GunCasing_DrawHandler);

void Gun_Reset(Gun *g);

void Gun_CreateHandler(Gun *g)
{
    Task *shots;

    QTaskList_Add(MagicObj_GetList(), g);
    RigidDynCubeObj_Init(Game_GetWorld(), g, NULL);
    g->unkF1 = 1;
    g->id = 0;
    g->unk124 = 0;
    g->name[0] = 0;
    Gun_Reset(g);
    g->remove = 0;
    g->rounds = -2;
    g->lastShot = -1;
    g->loopSound = NULL;
    g->unk190 = NULL;
    g->wt = NULL;
    g->ammoType = NULL;
    shots = Shots_New(Sound_GetOwner(), &g->o.task, g->o.task.parent);
    g->shots = TaskRef_Add(shots);
    g->o.coll = Coll_NewTask(NULL, g);
}
HOOK(0x478540, Gun_CreateHandler);

void Gun_Reset(Gun *g)
{
    g->inWorld = 0;
    g->firing = 0;
    g->cooldown = 0;
    g->burst = 0;
    *(int *)&g->spin = 0;
    g->unk1A4 = 0;
    g->unk1A8 = 0;
    g->unk19C = 0;
    g->unk1A0 = 0;
    *(int *)&g->recoil = 0;
    g->reload = 0;
}
HOOK(0x4785F0, Gun_Reset);

void Gun_RunHandler(Gun *g)
{
    WeaponType *wt = g->wt;
    long double step, v;

    if (g->rounds == -2)
        g->rounds = wt->rounds;
    if (g->remove) {
        Task_Delete(g);
        return;
    }
    if (g->cooldown)
        g->cooldown--;
    BROADCAST(g->o.coll->head, 1);
    if (!(g->recoil >= kZero)) {
        step = kMinute / ((kMinute / (long double)wt->rate) * kFpsD);
        v = step + g->recoil;
        g->recoil = v;
        if (v > kZero)
            *(int *)&g->recoil = 0;
    } else if (g->recoil > kZero) {
        step = kMinute / ((kMinute / (long double)wt->rate) * kFpsD);
        v = g->recoil - step;
        g->recoil = v;
        if (!(v >= kZero))
            *(int *)&g->recoil = 0;
    }
    if (g->unk1A4) {
        if (g->unk1A8)
            g->unk1A8--;
        else {
            g->unk1A4++;
            g->unk1A8 = round_int((kMinute / (long double)wt->rate) * kFpsD) / 3;
        }
    }
    if (g->unk19C) {
        if (g->unk1A0) {
            g->unk1A0--;
            return;
        }
        g->unk19C++;
        g->unk1A0 = round_int((kMinute / (long double)wt->rate) * kFpsD) / 10;
    }
}
HOOK(0x478640, Gun_RunHandler);

void Gun_RemoveFromWorld(Gun *g);

void Gun_DeleteHandler(Gun *g)
{
    Task **ref;

    QTaskList_Remove(MagicObj_GetList(), g);
    Gun_RemoveFromWorld(g);
    if (*g->shots)
        Shots_Release(*g->shots, 1);
    TaskRef_Remove(g->shots);
    ref = g->loopSound;
    if (ref) {
        Task *t = *ref;

        TaskRef_Remove(ref);
        if (t)
            Task_Delete(t);
    }
    if (g->unk190)
        TaskRef_Remove(g->unk190);
    Task_Delete(g->o.coll);
}
HOOK(0x478840, Gun_DeleteHandler);

void Gun_RemoveFromWorld(Gun *g)
{
    if (!g->inWorld)
        return;
    DynCube_Remove(Game_GetWorld(), g);
    g->inWorld = 0;
}
HOOK(0x4788D0, Gun_RemoveFromWorld);

/* asks the owner whether the gun shows */
void Gun_HideHandler(Gun *g, int *hide)
{
    Task *owner = g->o.task.parent;
    int v;

    if (!TASK_HANDLER(gGunEvents[1], owner->type))
        return;
    v = *hide;
    TASK_HANDLER(gGunEvents[1], owner->type)(owner, &v);
    g->o.hidden = v ? 0 : 1;
}
HOOK(0x478900, Gun_HideHandler);

/* binoculars are only drawn while they have a charge */
void Gun_DrawHandler(Gun *g)
{
    if (g->rounds || g->wt->category != 5)
        TASK_HANDLER(4, RigidDynCubeObj_GetType())(g, 0);
}
HOOK(0x478970, Gun_DrawHandler);

void Gun_GetParamTableHandler(Gun *g, void **out)
{
    *out = gGunParams;
}
HOOK(0x4789B0, Gun_GetParamTableHandler);

void Gun_ParamChangedHandler(Gun *g)
{
    WeaponType *wt;
    void **model;

    g->unkF4 = 0;
    g->unkF8 = g->unk124;
    Mem_PushMode(1);
    g->id = Script_EvalExpr(g->name);
    Mem_PopMode();
    wt = WeaponType_Find(g->id);
    g->wt = wt;
    if (!wt) {
        g->ammoType = NULL;
        return;
    }
    g->ammoType = AmmoType_Find(wt->ammo);
    model = VirModel_Find(wt->key);
    if (model)
        RigidDynCubeObj_SetModel(g, model);
    if (g->o.coll)
        Task_DeleteChildren(g->o.coll);
    sub_4C4950(g->o.coll, g, g);
}
HOOK(0x4789C0, Gun_ParamChangedHandler);

void Gun_GetStateHandler(Gun *g, char **out)
{
    *out = &g->unkF4;
}
HOOK(0x478A80, Gun_GetStateHandler);

void Gun_MoveHandler(Gun *g, MoveMsg *msg)
{
    if (g->inWorld) {
        float s = max_scale(&g->o);
        float radius = VirModel_GetRadius(g->o.model) * s;

        DynCube_Move(Game_GetWorld(), g, msg->pos, radius);
    } else
        g->o.pos = *msg->pos;
    g->o.matrix = *msg->matrix;
    Model_GetPoint(&g->point, g, 2);
    g->point.x = g->o.pos.x + g->point.x;
    g->point.y = g->o.pos.y + g->point.y;
    g->point.z = g->o.pos.z + g->point.z;
    Shots_SetPos(*g->shots, &g->point);
}
HOOK(0x478A90, Gun_MoveHandler);

/* one trigger message: sounds, bullets with spread, empty casings for the
   player's gun, then the recoil and cooldown counters */
void Gun_FireHandler(Gun *g, GunFire *msg)
{
    WeaponType *wt = g->wt;
    char trigger = msg->trigger;
    char fire = trigger;
    void *snd;

    if (msg->muzzle)
        g->muzzle = *msg->muzzle;
    else {
        Model_GetPoint(&g->muzzle, g, 2);
        g->muzzle.x = g->o.pos.x + g->muzzle.x;
        g->muzzle.y = g->o.pos.y + g->muzzle.y;
        g->muzzle.z = g->o.pos.z + g->muzzle.z;
    }
    if (msg->aim) {
        g->dir.x = msg->aim->m[1];
        g->dir.y = msg->aim->m[4];
        g->dir.z = msg->aim->m[7];
    } else {
        g->dir.x = g->o.matrix.m[1];
        g->dir.y = g->o.matrix.m[4];
        g->dir.z = g->o.matrix.m[7];
    }
    if (!trigger)
        g->burst = 0;
    else if (g->burst == wt->burst)
        trigger = fire = 0;
    if (g->cooldown)
        fire = 0;
    if (g->ammoType && !g->rounds)
        trigger = fire = 0;

    snd = NULL;
    if (wt->burst > 1) {
        /* automatic fire loops one sound while the trigger is held */
        if (trigger) {
            if (!g->loopSound) {
                snd = Sound_Play(Sound_GetOwner(), wt->sound);
                if (snd) {
                    g->loopSound = TaskRef_Add(snd);
                    Sound_SetPos(*g->loopSound, &g->o.pos);
                }
                snd = NULL;
            } else
                snd = *g->loopSound;
        } else if (g->loopSound) {
            if (*g->loopSound)
                Sound_Stop(*g->loopSound);
            TaskRef_Remove(g->loopSound);
            g->loopSound = NULL;
            snd = Sound_Play(Sound_GetOwner(), wt->sound2);
        }
    } else if (fire)
        snd = Sound_Play(Sound_GetOwner(), Random_Next(Game_GetRandom()) & 1 ? wt->sound : wt->sound2);
    if (snd)
        Sound_SetPos(snd, &g->o.pos);

    msg->fired = fire;
    if (fire) {
        Matrix aim, rot, out;
        const float *a = aim.m, *e = rot.m;
        Vec3f vel;
        Vec3d eject, top;
        long double s;
        float spread;
        int i;

        Matrix_SetYAxis(&aim, &g->dir);
        out = aim;
        if (msg->vel)
            vel = *msg->vel;
        else
            vel.x = vel.y = vel.z = 0;
        s = (g->burst > 3 ? wt->spread[1] : wt->spread[0]) * kDegToRad;
        if (!(s >= kZero))
            s = -s;
        spread = s;
        for (i = 0; i < wt->pellets; i++) {
            if (spread > kZero) {
                float k = 1.0f, turn;
                int n;

                for (n = 3; n; n--)
                    k = Random_Float(Game_GetRandom()) * k;
                turn = Random_Float(Game_GetRandom()) * kTwoPi;
                Matrix_SetEuler(&rot, k * spread, turn, 0.0f);
                out.m[0] = a[2] * e[6] + a[1] * e[3] + a[0] * e[0];
                out.m[1] = a[2] * e[7] + a[1] * e[4] + a[0] * e[1];
                out.m[2] = a[2] * e[8] + a[1] * e[5] + a[0] * e[2];
                out.m[3] = a[5] * e[6] + a[4] * e[3] + a[3] * e[0];
                out.m[4] = a[5] * e[7] + a[4] * e[4] + a[3] * e[1];
                out.m[5] = a[5] * e[8] + a[4] * e[5] + a[3] * e[2];
                out.m[6] = a[8] * e[6] + a[7] * e[3] + a[6] * e[0];
                out.m[7] = a[8] * e[7] + a[7] * e[4] + a[6] * e[1];
                out.m[8] = a[8] * e[8] + a[7] * e[5] + a[6] * e[2];
                out.unk24 = rot.unk24 + aim.unk24 + 1;
                /* the original then re-orthonormalises a copy once unk24
                   passes 1000, but never uses the result */
            }
            Weapon_Fire(g, g->o.task.parent, &g->muzzle, &g->o.matrix, &vel, &out, g->wt, g->ammoType, msg->unk28);
        }
        Model_GetPoint(&eject, g, 3);
        eject.x = eject.x + g->o.pos.x;
        eject.y = eject.y + g->o.pos.y;
        eject.z = eject.z + g->o.pos.z;
        if (wt->casing[0] && Game_GetPlayer(0) == g->o.task.parent) {
            const float *m = g->o.matrix.m;
            float angle, u0, u1, u2;
            Vec3f v, cv;
            long double w0, w1, w2;
            int r;

            angle = (long double)Random_Int(Game_GetRandom(), 30) * kDegToRad;
            r = Random_Int(Game_GetRandom(), 4);
            v.x = (r + 4) * 4096000 / 108000;
            v.y = 0;
            v.z = 0;
            Matrix_SetRotY(&rot, -angle);
            w0 = e[2] * v.z + e[1] * v.y + e[0] * v.x;
            w1 = e[5] * v.z + e[4] * v.y + e[3] * v.x;
            w2 = e[8] * v.z + e[7] * v.y + e[6] * v.x;
            u0 = w2 * m[2] + w1 * m[1] + w0 * m[0];
            u1 = w2 * m[5] + w1 * m[4] + w0 * m[3];
            u2 = w2 * m[8] + w1 * m[7] + w0 * m[6];
            cv.x = u0 + vel.x;
            cv.y = u1 + vel.y;
            cv.z = u2 + vel.z;
            GunCasing_New(Sound_GetOwner(), &eject, &cv, &g->o.matrix, wt->casing);
        }
        Model_GetPoint(&top, g, 2); /* never used */
        if (g->unk190) {
            Task *t = *g->unk190;

            if (t)
                Task_Delete(t);
            TaskRef_Remove(g->unk190);
            g->unk190 = NULL;
        }
        g->burst++;
        g->cooldown = round_int((kMinute / (long double)wt->rate) * kFpsD);
        if (g->ammoType && g->rounds > 0)
            g->rounds--;
        g->lastShot = sub_4E75A0();
        g->recoil = 60.0f;
        g->unk1A4 = -2;
        g->unk19C = -9;
        g->unk1A8 = round_int((kMinute / (long double)wt->rate) * kFpsD) / 3;
        g->unk1A0 = round_int((kMinute / (long double)wt->rate) * kFpsD) / 10;
    }
    g->firing = msg->trigger;
    if (msg->trigger) {
        long double v = g->spin + kSpinStep;

        g->spin = v;
        if (v > kOne)
            g->spin = 1.0f;
    } else
        *(int *)&g->spin = 0;
}
HOOK(0x478BA0, Gun_FireHandler);
