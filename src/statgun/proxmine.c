#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "statgun/statgun.h"
#include "coll/colltypes.h"

FN_EXTERN(Missile_GetModelSize);

/* the private data of the proximity mine weapon type */
typedef struct ProxData {
    float armTime;      /* seconds */
    float countdown;
    float activate;
    float explode;
} ProxData;

/* the move message of event 0x4F1FE0 */
typedef struct GunMoveMsg {
    Vec3d *pos;
    Matrix *matrix;
} GunMoveMsg;

/* the filter Coll_Lines passes to the objects it meets */
typedef struct ProxQuery {
    float unk0;
    void *self;
    Task *owner;
    int unkC;
    float unk10;
} ProxQuery;

extern uint16_t gProxMineType;

extern float kZero;
extern double kOneD;
extern float kRangeScale;
extern float kTicksPerSecond;

#define HANDLER_429910 (FN(0x429910, Missile_GetModelSize))

ORIG(0x4F1FE0, uint8_t, Weapon_GetMoveEvent, (void));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x449D20, uint16_t, Glass_GetType, (void));
ORIG(0x447D20, uint16_t, Door_GetType, (void));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *results, Vec3d *starts, Vec3d *ends, int count, char event,
                                  ProxQuery *query, unsigned int flags));
ORIG(0x414A60, void, Matrix_SetYAxis, (Matrix *m, const Vec3f *dir));
ORIG(0x476040, void, Mine_New, (void *a, Vec3d *pos, Matrix *m, char *key, int armTicks, float countdown,
                                  float activate, float explode, char alt));

void ProxMine_Create(Gun *g, void *arg);
void ProxMine_Move(Gun *g, GunMoveMsg *m);
void ProxMine_Use(Gun *g, WeaponUseMsg *msg);
void ProxMine_ReadArgs(void *task, WeaponInitArgs *a);
void ProxMine_Config(void *task, WeaponConfigMsg *m);

void ProxMine_Open(void)
{
    gProxMineType = TaskType_New("ProximityMine", sizeof(Gun), Gun_GetType(), 0);
    TaskType_SetHandler(gProxMineType, 0, ProxMine_Create);
    TaskType_SetHandler2(gProxMineType, 2, Gun_Delete);
    TaskType_SetHandler(gProxMineType, Weapon_GetMoveEvent(), ProxMine_Move);
    TaskType_SetHandler(gProxMineType, Human_GetEvent0(), ProxMine_Use);
    TaskType_SetHandler(gProxMineType, WeaponType_GetInitEvent2(), ProxMine_ReadArgs);
    TaskType_SetHandler(gProxMineType, WeaponType_GetSizeEvent2(), HANDLER_429910);
    TaskType_SetHandler(gProxMineType, WeaponConfig_GetEvent(), ProxMine_Config);
    Script_RegisterConstant("TASKTYPE_PROXIMITYMINE", gProxMineType);
}
HOOK(0x475380, ProxMine_Open);

void ProxMine_Create(Gun *g, void *arg)
{
    HANDLER(0, Gun_GetType())(g, arg);
    crt_sprintf(g->name, "WEAPON_ID_PROXIMITYMINE");
}
HOOK(0x475460, ProxMine_Create);

void Gun_Delete(Gun *g)
{
    HANDLER(2, Gun_GetType())(g);
}
HOOK(0x4754A0, Gun_Delete);

void ProxMine_Move(Gun *g, GunMoveMsg *m)
{
    float radius;

    if (g->inWorld) {
        radius = VirModel_GetRadius(g->o.model) * max_scale(&g->o);
        DynCube_Move(Game_GetWorld(), g, m->pos, radius);
        g->o.matrix = *m->matrix;
        return;
    }
    g->o.pos = *m->pos;
    g->o.matrix = *m->matrix;
}
HOOK(0x4754C0, ProxMine_Move);

/* fire puts a mine on the surface in front of the user, if it is in range
   and not on a human, glass, a door or a vehicle */
void ProxMine_Use(Gun *g, WeaponUseMsg *msg)
{
    WeaponType *info = g->wt;
    ProxData *d = info->data2;
    long double s, inv;
    ProxQuery q;
    CollHit hit;
    Vec3d start, end;
    Vec3f dir;
    Matrix m;
    int onHuman, onHumanPart, blocked;

    if (msg->eye)
        g->muzzle = *(Vec3d *)msg->eye;
    if (msg->view) {
        MOV32(g->dir.x, msg->view->m[1]);
        MOV32(g->dir.y, msg->view->m[4]);
        MOV32(g->dir.z, msg->view->m[7]);
    } else {
        MOV32(g->dir.x, g->o.matrix.m[1]);
        MOV32(g->dir.y, g->o.matrix.m[4]);
        MOV32(g->dir.z, g->o.matrix.m[7]);
    }
    if (msg->fire && !g->firing) {
        s = info->range * kRangeScale;
        dir = g->dir;
        q.unk0 = s * dir.x;
        start = g->muzzle;
        dir.y = s * dir.y;
        end.x = q.unk0 + start.x;
        end.y = dir.y + start.y;
        q.self = g;
        q.owner = g->o.task.parent;
        q.unkC = 0;
        q.unk10 = 1.0f;
        end.z = s * dir.z + start.z;
        Coll_Lines(Game_GetWorld(), &hit, &start, &end, 1, Game_GetEvent7(), &q, 0xFFFFFFFF);
        if (hit.found) {
            onHuman = hit.task && TaskType_IsA(hit.task->type, Human_GetType());
            onHumanPart = hit.task && hit.task->parent && TaskType_IsA(hit.task->parent->type, Human_GetType());
            blocked = hit.task && (hit.task->type == Glass_GetType() || hit.task->type == Door_GetType() ||
                                   TaskType_IsA(hit.task->type, Vehicle_GetType()));
            if (!onHuman && !onHumanPart && !blocked) {
                if (hit.normal.x != kZero || hit.normal.y != kZero || hit.normal.z != kZero) {
                    inv = kOneD / x87_sqrt(hit.normal.x * hit.normal.x + hit.normal.y * hit.normal.y +
                                        hit.normal.z * hit.normal.z);
                    hit.normal.x = hit.normal.x * inv;
                    hit.normal.y = hit.normal.y * inv;
                    hit.normal.z = hit.normal.z * inv;
                }
                Matrix_SetYAxis(&m, &hit.normal);
                Mine_New(Level_GetDynamic(), &hit.pos, &m, info->key, (int)round_ll(d->armTime * kTicksPerSecond),
                           d->countdown, d->activate, d->explode, msg->alt);
                if (g->rounds > 0)
                    g->rounds--;
                msg->shot = 1;
            }
        }
    }
    g->firing = msg->fire;
}
HOOK(0x4755A0, ProxMine_Use);

void ProxMine_ReadArgs(void *task, WeaponInitArgs *a)
{
    int i = a->next;
    float *out = a->data;

    out[0] = Script_GetArgReal(a->args, i);
    out[1] = Script_GetArgReal(a->args, i + 1);
    out[2] = Script_GetArgReal(a->args, i + 2);
    out[3] = Script_GetArgReal(a->args, i + 3);
    a->next = i + 4;
}
HOOK(0x475900, ProxMine_ReadArgs);

void ProxMine_Config(void *task, WeaponConfigMsg *m)
{
    ConfigVar *arm, *count, *activate, *explode;
    ProxData *d;

    if (m->info->data2) {
        Mem_Free(m->info->data2);
        m->info->data2 = NULL;
    }
    arm = TagItem_Find(m->config, "Proximitymine - Time before arming (seconds)");
    if (!arm)
        arm = TagItem_AddReal32(m->config, "Proximitymine - Time before arming (seconds)", 2.0f);
    count = TagItem_Find(m->config, "Proximitymine - Countdown time (seconds)");
    if (!count)
        count = TagItem_AddReal32(m->config, "Proximitymine - Countdown time (seconds)", 0.5f);
    activate = TagItem_Find(m->config, "Proximitymine - Activate radius (meters)");
    if (!activate)
        activate = TagItem_AddReal32(m->config, "Proximitymine - Activate radius (meters)", 1.5f);
    explode = TagItem_Find(m->config, "Proximitymine - Explosion radius (meters)");
    if (!explode)
        explode = TagItem_AddReal32(m->config, "Proximitymine - Explosion radius (meters)", 3.0f);
    m->info->data2 = d = Mem_Alloc(sizeof(ProxData), 4);
    MOV32(d->armTime, arm->v.f);
    MOV32(d->countdown, count->v.f);
    MOV32(d->activate, activate->v.f);
    MOV32(d->explode, explode->v.f);
}
HOOK(0x475960, ProxMine_Config);

void ProxMine_Close(void)
{
    TaskType_Free(gProxMineType);
}
HOOK(0x475AB0, ProxMine_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5333BC) float kTicksPerSecond INIT(30.0f);
ALIAS(gFramesPerSecond, kTicksPerSecond, 0);
ALIAS(kFps, kTicksPerSecond, 0);
ALIAS(kThirty, kTicksPerSecond, 0);
BSS(5BE394) uint16_t gProxMineType;
PAD(igb, 5BE394, 5BE396, 0x2);
