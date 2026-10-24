/* The end of HumanSoldierPistol: the last states, task setup and damage. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "pistol/pistol.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x413CC0, void *, Task_FindWeapon, (Soldier *h, int id));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (SoldierBlast *e, Soldier *h, double *pos, float radius, int unk));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x444B30, void, Obj_GetUsePoint, (void *obj, double *pos, float *matrix));
ORIG(0x444BC0, void, Switch_Press, (void *t));
ORIG(0x447DA0, void, Door_Toggle, (void *t));
ORIG(0x450D50, void, HumanAI_Send, (SoldierAI *ai, SoldierAIMsg *msg));
ORIG(0x454EE0, void, Human_RunBase, (void *task));
ORIG(0x456E50, void, SoldierRPG_ParamsChanged, (Soldier *h, void *arg));
ORIG(0x457810, void, SoldierPistol_Footstep, (Soldier *h));
ORIG(0x45D360, void, Soldier_EnsureWeapon, (Soldier *h));
ORIG(0x45D5B0, void, Soldier_GetDeathVelocity, (double *vel, Soldier *h, int model, int anim));
ORIG(0x45DAA0, char, Soldier_TurnToHit, (Soldier *h, double *dir));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460C40, uint8_t, Human_GetPunchEvent, (void));
ORIG(0x460C50, void *, Human_GetParamTable, (void));
ORIG(0x460C80, void, Human_printf, (Soldier *h, const char *fmt, ...));
ORIG(0x460CF0, void, sub_460CF0, (Soldier *h));
ORIG(0x4610C0, void, Human_UpdateBody, (Soldier *h));
ORIG(0x4613D0, void, sub_4613D0, (Soldier *h));
ORIG(0x4614B0, void, Human_UpdateMove, (Soldier *h));
ORIG(0x462CF0, void, Human_MoveOnPlatform, (Soldier *h));
ORIG(0x462E50, void, Human_UpdateWeapon, (Soldier *h));
ORIG(0x463080, void, Human_SendPosition, (Soldier *h));
ORIG(0x463110, int, Human_GetState, (Soldier *h));
ORIG(0x4631A0, void, Human_SetPosition, (Soldier *h, double *pos));
ORIG(0x463220, void, Human_SetYaw, (Soldier *h, float angle));
ORIG(0x463310, void, Human_SetPlatform, (Soldier *h, void *obj));
ORIG(0x4635F0, void, sub_4635F0, (Soldier *h));
ORIG(0x463680, int, Human_GetGround, (Soldier *h));
ORIG(0x4636A0, void, Human_SetClimbObject, (Soldier *h, void *t));
ORIG(0x4636E0, void *, Human_GetClimbObject, (Soldier *h));
ORIG(0x463760, void, HumanViewCam, (Soldier *h, int mode));
ORIG(0x4637C0, void, Human_SayTimed, (Soldier *h, const char *sound, int count));
ORIG(0x4638A0, void, Human_Say, (Soldier *h, const char *sound, int count));
ORIG(0x463AD0, void, Human_Bleed, (Soldier *h, float *dir, int unk));
ORIG(0x464700, void, HumanView_LockPitch, (void *view));
ORIG(0x464720, void, HumanView_ClearAngles, (void *view));
ORIG(0x464800, void, HumanView_UpdateView, (void *view, Soldier *h));
ORIG(0x464BD0, void, HumanView_UpdateBody, (void *view, Soldier *h));
ORIG(0x465800, void, HumanView_GetEye, (double *pos, void *view));
ORIG(0x465970, void, HumanView_Shake, (void *view, float amount, float angle, int frames));
ORIG(0x47E990, uint8_t, FlashBang_GetEvent, (void));
ORIG(0x489C30, void, Human_InitDamage, (Soldier *h, void *speeds));
ORIG(0x489C60, int, Human_GetHitZone, (Soldier *h, SoldierHit *hit));
ORIG(0x489E10, HitAnims *, HumanDamage_GetZone, (SoldierDamage *d, int part));
ORIG(0x489E70, void, Human_PlayAnimSlot, (Soldier *h, int slot, float blend));
ORIG(0x489EA0, void, Human_PlayAnim, (Soldier *h, int anim, int loop, float blend));
ORIG(0x48A320, void, Human_EnableLook, (Soldier *h));
ORIG(0x48A330, void, Human_DisableLook, (Soldier *h));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *a, float *b, float *c));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, double *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, Soldier *h));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (void *a, float *delta, void *anim));
ORIG(0x4D3710, void, Task_Nothing, (void *task, void *arg));
ORIG(0x4D61D0, void, Anim_Play, (void *anim, int track, int id, int a, int b));
ORIG(0x4D74F0, int, Anim_Exists, (void *anim, int track, int id));
ORIG(0x4D96F0, void, BoneDynCubeObj_Deinit, (Soldier *h));
ORIG(0x4EC190, void, StateMachine_Init, (SoldierSM *sm, SoldierStateFn input, SoldierStateFn *states, int state,
                                         int a, void *combos));
ORIG(0x4EC1E0, void, StateMachine_Update, (SoldierSM *sm, Soldier *h));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1030, void *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (Soldier *h));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *parent, int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F2060, void *, WeaponSlots_Get, (void **slots, int i));
ORIG(0x4F2070, void, WeaponSlots_Set, (void **slots, int i, void *weapon));
ORIG(0x4F2290, void, Weapon_DeleteAll, (Soldier *h));
ORIG(0x4F2350, void, Weapon_UseAmmo, (void *weapon, int n));
ORIG(0x4F2390, void, Weapon_Drop, (void *weapon));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));

#define gSoldierPistolList VAR(void *, 0x5BDAA4)
#define gSoldierPistolType VAR(uint16_t, 0x5BDAA8)
#define gSoldierPistolUnk VAR(int, 0x5BDAB0)
#define gSoldierPistolStates ((SoldierStateFn *)0x53E8B8)
#define gSoldierPistolCombos ((void *)0x53E980)
#define gSoldierPistolAnimSlots ((void *)0x53E9B8)
#define gSoldierPistolSpeeds ((void *)0x53EAE8)

/* updates that live with the HumanSoldier states */
#define Soldier_State23Update ((SoldierStateFn)0x45BCC0)
#define Soldier_State24Update ((SoldierStateFn)0x45BDC0)
#define Soldier_State26Update ((SoldierStateFn)0x45BF90)
#define Soldier_State27Update ((SoldierStateFn)0x45C0C0)
#define Soldier_FallUpdate ((SoldierStateFn)0x45C210)
#define Soldier_State31Update ((SoldierStateFn)0x45C370)
#define Soldier_DeadUpdate ((SoldierStateFn)0x45C740)
#define Soldier_AnimUpdate ((SoldierStateFn)0x45C7E0)
#define Soldier_UseUpdate ((SoldierStateFn)0x45C9B0)
#define Soldier_State37Update ((SoldierStateFn)0x45CA80)

static const double kUseDistance = 2867.2;
static const float kPi = 0x1.921fb6p+1f;
static const float kHitLimit = 0x1.51eb86p-2f;
static const float kShakeAmount = 0x1.99999ap+7f;
static const float kShakeAngle = 0x1.aceea0p-6f;
static const float kBlastRadius = 0x1.555556p-2f;
static const double kForward = 4096.0;
static const float kUnitsToMetersF = 0x1p-12f;
static const double kUnitsToMeters = 0x1p-12;
static const float kFps = 30.0f;
static const float kRadToDeg = 0x1.ca5dc0p+5f;

enum {
    PS_STAND,
    PS_FALL = 29,
};

static void ps_enter(SoldierSM *sm, Soldier *h, char moving, SoldierStateFn run)
{
    h->moving = moving;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = run;
}

static void ps_enter_keep(SoldierSM *sm, Soldier *h, SoldierStateFn run)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = run;
}

void SoldierPistol_State13Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_State14Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_State15Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_State16Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_State19Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_State22Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_State25Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_State28Update(SoldierSM *sm, Soldier *h);
void SoldierPistol_HitUpdate(SoldierSM *sm, Soldier *h);
void SoldierPistol_DieUpdate(SoldierSM *sm, Soldier *h);

void SoldierPistol_State13Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, SoldierPistol_State13Update);
}
HOOK(0x4588F0, SoldierPistol_State13Enter);

void SoldierPistol_State14Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, SoldierPistol_State14Update);
}
HOOK(0x458920, SoldierPistol_State14Enter);

void SoldierPistol_State14Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 33))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, PS_STAND);
}
HOOK(0x458950, SoldierPistol_State14Update);

void SoldierPistol_State15Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, SoldierPistol_State15Update);
}
HOOK(0x458A00, SoldierPistol_State15Enter);

void SoldierPistol_State15Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 31, 34))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 26);
}
HOOK(0x458A30, SoldierPistol_State15Update);

void SoldierPistol_State16Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, SoldierPistol_State16Update);
}
HOOK(0x458AE0, SoldierPistol_State16Enter);

void SoldierPistol_State16Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 31, 34))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, PS_STAND);
}
HOOK(0x458B10, SoldierPistol_State16Update);

void SoldierPistol_State19Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State19Update);
}
HOOK(0x458BC0, SoldierPistol_State19Enter);

void SoldierPistol_State19Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 33))
        return;
    if (b & 0x100000)
        sm_go(sm, h, 25);
    else if (!(b & 0x80010))
        sm_go(sm, h, PS_STAND);
    else if (b & SB_FIRE)
        sm_go(sm, h, 24);
    else if (b & 0x800)
        sm_go(sm, h, 23);
    else if (b & 0x20)
        sm_go(sm, h, 20);
    else if (b & 0x40)
        sm_go(sm, h, 21);
}
HOOK(0x458BF0, SoldierPistol_State19Update);

void SoldierPistol_State20Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State13Update);
}
HOOK(0x458D70, SoldierPistol_State20Enter);

void SoldierPistol_State21Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State13Update);
}
HOOK(0x458DA0, SoldierPistol_State21Enter);

void SoldierPistol_State13Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 33))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 19);
}
HOOK(0x458DD0, SoldierPistol_State13Update);

void SoldierPistol_State22Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_keep(sm, h, SoldierPistol_State22Update);
}
HOOK(0x458E80, SoldierPistol_State22Enter);

void SoldierPistol_State22Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 33))
        return;
    if (!is_zero(h->input[6]))
        h->fire = 1;
    if ((b & SB_ANIM_DONE) && !(b & SB_FIRE))
        sm_go(sm, h, b & 0x2000000 ? 24 : 19);
}
HOOK(0x458EB0, SoldierPistol_State22Update);

void SoldierPistol_State23Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_keep(sm, h, Soldier_State23Update);
}
HOOK(0x458FB0, SoldierPistol_State23Enter);

void SoldierPistol_State24Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_keep(sm, h, Soldier_State24Update);
}
HOOK(0x458FE0, SoldierPistol_State24Enter);

void SoldierPistol_State25Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State25Update);
}
HOOK(0x459010, SoldierPistol_State25Enter);

void SoldierPistol_State25Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 33))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 26);
}
HOOK(0x459040, SoldierPistol_State25Update);

void SoldierPistol_State26Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, Soldier_State26Update);
}
HOOK(0x4590F0, SoldierPistol_State26Enter);

void SoldierPistol_State27Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_keep(sm, h, Soldier_State27Update);
}
HOOK(0x459120, SoldierPistol_State27Enter);

void SoldierPistol_State28Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_keep(sm, h, SoldierPistol_State28Update);
}
HOOK(0x459150, SoldierPistol_State28Enter);

void SoldierPistol_State28Update(SoldierSM *sm, Soldier *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 31, 34))
        return;
    if (b & SB_ANIM_DONE) {
        Weapon_UseAmmo(w, 1);
        sm_go(sm, h, 26);
    }
}
HOOK(0x459180, SoldierPistol_State28Update);

void SoldierPistol_FallEnter(SoldierSM *sm, Soldier *h)
{
    h->moving = 0;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    memcpy(h->vel, h->unk688, sizeof(h->vel));
    sm->run = Soldier_FallUpdate;
}
HOOK(0x459250, SoldierPistol_FallEnter);

void SoldierPistol_HitEnter(SoldierSM *sm, Soldier *h)
{
    HitAnims *a;

    h->moving = 0;
    h->unk696 = 1;
    a = HumanDamage_GetZone(&h->damage, h->damage.hitZone);
    Anim_Play(h->anim, 0, a->hit, 0, 0);
    Human_DisableLook(h);
    sm->run = SoldierPistol_HitUpdate;
}
HOOK(0x4592A0, SoldierPistol_HitEnter);

void SoldierPistol_HitUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, -1, 32))
        return;
    if (b & SB_ANIM_DONE) {
        Human_EnableLook(h);
        sm_go(sm, h, PS_STAND);
    }
}
HOOK(0x4592F0, SoldierPistol_HitUpdate);

void SoldierPistol_State31Enter(SoldierSM *sm, Soldier *h)
{
    h->unk696 = 1;
    h->moving = 0;
    ((SoldierP *)h)->unkD14 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    Human_DisableLook(h);
    sm->run = Soldier_State31Update;
}
HOOK(0x459380, SoldierPistol_State31Enter);

/* drop the weapon, play the death animation and fix the camera */
static void ps_die(SoldierSM *sm, Soldier *h, int anim)
{
    Anim_Play(h->anim, 0, anim, 0, 0);
    Soldier_GetDeathVelocity(((SoldierP *)h)->deathVel, h, h->model, anim);
    HumanViewCam(h, 3);
    HumanView_LockPitch(h->view);
    sub_4635F0(h);
    Human_Say(h, "ai_death_", 4);
    Human_DisableLook(h);
    sm->run = SoldierPistol_DieUpdate;
}

static void ps_drop_weapon(Soldier *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);

    if (w) {
        Weapon_Drop(w);
        WeaponSlots_Set(h->weapons, 0, NULL);
    }
    Weapon_DeleteAll(h);
    h->moving = 0;
    h->unk696 = 1;
}

void SoldierPistol_DieEnter(SoldierSM *sm, Soldier *h)
{
    static const int kDeaths[2] = {26, 24};
    SoldierP *s = (SoldierP *)h;
    SoldierAI *ai = h->ai;
    int part, anim = -1, r;
    HitAnims *a;

    ps_drop_weapon(h);
    part = h->damage.hitZone;
    a = HumanDamage_GetZone(&h->damage, part);
    if (ai && ai->deathAnim != -1) {
        anim = ai->deathAnim;
    } else {
        r = Random_Int(Game_GetRandom(), 2);
        if (s->deathKind >= 0) {
            if (s->deathKind <= 1)
                anim = Soldier_TurnToHit(h, s->deathDir) ? 27 : 28;
            else if (s->deathKind == 2)
                anim = part == 15 || part == 16 ? a->death[r] : kDeaths[r];
        }
    }
    ps_die(sm, h, anim);
}
HOOK(0x4593C0, SoldierPistol_DieEnter);

void SoldierPistol_Die2Enter(SoldierSM *sm, Soldier *h)
{
    ps_drop_weapon(h);
    ps_die(sm, h, 29);
}
HOOK(0x459520, SoldierPistol_Die2Enter);

void SoldierPistol_Die3Enter(SoldierSM *sm, Soldier *h)
{
    ps_drop_weapon(h);
    ps_die(sm, h, 30);
}
HOOK(0x4595D0, SoldierPistol_Die3Enter);

void SoldierPistol_DieUpdate(SoldierSM *sm, Soldier *h)
{
    SoldierP *s = (SoldierP *)h;

    h->pos[0] = s->deathVel[0] + h->pos[0];
    h->pos[1] = s->deathVel[1] + h->pos[1];
    h->pos[2] = s->deathVel[2] + h->pos[2];
    if (sm->bits & SB_ANIM_DONE)
        sm->run = Soldier_DeadUpdate;
}
HOOK(0x459680, SoldierPistol_DieUpdate);

void SoldierPistol_AnimEnter(SoldierSM *sm, Soldier *h)
{
    SoldierP *s = (SoldierP *)h;

    h->unk696 = 0;
    if (s->anim == -1) {
        Human_PlayAnimSlot(h, 2, 0);
    } else if (Anim_Exists(h->anim, 0, s->anim)) {
        Human_PlayAnim(h, s->anim, 0, 0);
    } else {
        Warning_Show("Animation #%d not found", s->anim);
        Human_PlayAnimSlot(h, 2, 0);
    }
    s->anim = 0;
    Human_DisableLook(h);
    sm->run = Soldier_AnimUpdate;
}
HOOK(0x4596C0, SoldierPistol_AnimEnter);

/* step in front of the object being used and face it */
void SoldierPistol_UseEnter(SoldierSM *sm, Soldier *h)
{
    void *obj = Human_GetClimbObject(h);
    float m[9], a, b, angle;
    double pos[3], dy;

    h->unk696 = 0;
    Obj_GetUsePoint(obj, pos, m);
    pos[2] = h->pos[2];
    dy = m[4] * kUseDistance;
    pos[0] = m[1] * kUseDistance + pos[0];
    pos[1] = dy + pos[1];
    Human_SetPosition(h, pos);
    Matrix_GetEuler(m, &a, &b, &angle);
    angle = angle + kPi;
    Human_SetYaw(h, angle);
    Human_PlayAnim(h, 0x27, 0, 0);
    HumanView_ClearAngles(h->view);
    HumanView_LockPitch(h->view);
    sub_4635F0(h);
    Human_SetPlatform(h, obj);
    sm->run = Soldier_UseUpdate;
}
HOOK(0x459750, SoldierPistol_UseEnter);

void SoldierPistol_State37Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_keep(sm, h, Soldier_State37Update);
}
HOOK(0x459850, SoldierPistol_State37Enter);

void SoldierPistol_CreateHandler(Soldier *h, void *arg);
void Soldier_DeleteHandler(Soldier *h);
void SoldierPistol_GetParamTable(Soldier *h, void **out);
void SoldierPistol_InputHandler(Soldier *h, SoldierInputMsg *msg);
void SoldierPistol_HitHandler(Soldier *h, SoldierHit *hit);
void SoldierPistol_BlastHandler(Soldier *h, SoldierBlast *e);
void SoldierPistol_PunchHandler(Soldier *h, SoldierPunch *p);
void SoldierPistol_UpdateInput(SoldierSM *sm, Soldier *h);

#define FIELD(name) ((void *)offsetof(Soldier, name))

void SoldierPistol_Open(void)
{
    gSoldierPistolType = TaskType_New("HumanSoldierPistol", sizeof(SoldierP), Human_GetType(), 0);
    TaskType_SetFlags(gSoldierPistolType, 1);
    TaskType_SetHandler(gSoldierPistolType, 0, SoldierPistol_CreateHandler);
    TaskType_SetHandler2(gSoldierPistolType, 1, Human_RunBase);
    TaskType_SetHandler2(gSoldierPistolType, 2, Soldier_DeleteHandler);
    TaskType_SetHandler(gSoldierPistolType, Param_GetTableEvent(), SoldierPistol_GetParamTable);
    TaskType_SetHandler(gSoldierPistolType, Param_GetChangedEvent(), SoldierRPG_ParamsChanged);
    TaskType_SetHandler(gSoldierPistolType, Vehicle_GetControlEvent(), SoldierPistol_InputHandler);
    TaskType_SetHandler(gSoldierPistolType, Game_GetEvent9(), SoldierPistol_HitHandler);
    TaskType_SetHandler(gSoldierPistolType, Game_GetEvent11(), SoldierPistol_BlastHandler);
    TaskType_SetHandler(gSoldierPistolType, Human_GetPunchEvent(), SoldierPistol_PunchHandler);
    TaskType_SetHandler(gSoldierPistolType, FlashBang_GetEvent(), Task_Nothing);
    gSoldierPistolList = ParamTable_Extend(Human_GetParamTable(), 2);
    /* the original passes a dummy soldier on its stack, only the difference
       of the two pointers is kept */
    ParamTable_Add(gSoldierPistolList, NULL, FIELD(model), Param_Int32(), 7, NULL);
    ParamTable_Add(gSoldierPistolList, NULL, FIELD(idleAnim), Param_Int32(), 7, NULL);
    gSoldierPistolUnk = 0;
}
HOOK(0x459880, SoldierPistol_Open);

void SoldierPistol_CreateHandler(Soldier *h, void *arg)
{
    SoldierP *s = (SoldierP *)h;

    TASK_HANDLER(0, Human_GetType())(h, arg);
    StateMachine_Init(&h->sm, SoldierPistol_UpdateInput, gSoldierPistolStates, 0, 1, gSoldierPistolCombos);
    h->animSlots = gSoldierPistolAnimSlots;
    Human_InitDamage(h, gSoldierPistolSpeeds);
    h->model = 3;
    h->idleAnim = -1;
    h->run = 1;
    h->spawned = 0;
    s->hitSum = 0;
    s->hitLimit = kHitLimit;
}
HOOK(0x459A20, SoldierPistol_CreateHandler);

void SoldierPistol_UpdateInput(SoldierSM *sm, Soldier *h)
{
    static const uint32_t bits[8] = {0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000};
    static const uint32_t more[3] = {0x800000, 0x1000000, 0x2000000};
    SoldierP *s = (SoldierP *)h;
    uint32_t b = sm->bits & 0xF003E01C;
    uint32_t flags = h->motion->flags;
    int i;

    if (flags & 0x100)
        b |= SB_ANIM_DONE;
    if (flags & 0x10000)
        b |= SB_ANIM_EVENT;
    if (h->run)
        b |= SB_RUN;
    else
        b &= ~SB_RUN;
    for (i = 0; i < 8; i++) {
        if (!is_zero(h->input[2 + i]))
            b |= bits[i];
    }
    if (!is_zero(h->input[21]))
        b |= 0x80000;
    if (!is_zero(h->input[22]))
        b |= 0x100000;
    if (!is_zero(h->input[23])) {
        s->anim = x87_fistp(h->input[23]);
        b |= 0x200000;
    }
    if (!is_zero(h->input[24])) {
        b |= 0x400000;
        if (Human_GetState(h) != 0x24) {
            s->useTarget = x87_fistp(h->input[24]);
            SoldierPistol_UseTarget(h);
            h->input[24] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        if (!is_zero(h->input[25 + i]))
            b |= more[i];
    }
    if (!is_zero(h->input[28])) {
        int id = x87_fistp(h->input[28]);
        void *w = Task_FindWeapon(h, id);

        if (w)
            WeaponSlots_Set(h->weapons, 0, w);
        else
            Warning_Show("#%d is trying to change to invalid weapon #%d", Task_GetId(h), id);
    }
    if (!is_zero(h->input[29]))
        b |= 0x4000000;
    if (!is_zero(h->input[30]))
        b |= 0x8000000;
    sm->bits = b;
}
HOOK(0x459AB0, SoldierPistol_UpdateInput);

/* sent to the used task through the slot of Game_GetEvent4 */
typedef struct UseMsg {
    Soldier *h;
    int unk4;
    double pos[3];
    int unk534[7];
    float *input;
    float *prevInput;
    int unk44;
} UseMsg;

void SoldierPistol_UseTarget(Soldier *h)
{
    void *t = Task_FindById(((SoldierP *)h)->useTarget);
    uint16_t type;
    TaskHandler fn;
    UseMsg msg;
    int result;

    if (!t)
        return;
    Human_SetClimbObject(h, t);
    result = 0;
    type = *(uint16_t *)((char *)t + 0x1C);
    fn = TASK_HANDLER(Game_GetEvent3(), type);
    if (fn)
        fn(t, &result);
    if (result == 2) {
        Switch_Press(t);
    } else if (result == 3) {
        Door_Toggle(t);
        sm_go(&h->sm, h, 0x25);
    } else if (result == 6) {
        sm_go(&h->sm, h, 0x24);
    }
    msg.h = h;
    memcpy(msg.pos, h->pos, sizeof(msg.pos));
    memcpy(msg.unk534, &h->unk534, sizeof(msg.unk534));
    msg.input = h->input;
    msg.prevInput = h->prevInput;
    msg.unk44 = 0;
    TASK_HANDLER(Game_GetEvent4(), type)(t, &msg);
    h->uses++;
}
HOOK(0x459D50, SoldierPistol_UseTarget);

/* the delete handler of every HumanSoldier type */
void Soldier_DeleteHandler(Soldier *h)
{
    if (h->spawned) {
        BoneDynCubeObj_Deinit(h);
        DynCube_Remove(Game_GetWorld(), h);
    }
    ((void (*)(Soldier *))TASK_HANDLER(2, Human_GetType()))(h);
}
HOOK(0x459EC0, Soldier_DeleteHandler);

void SoldierPistol_GetParamTable(Soldier *h, void **out)
{
    (void)h;
    *out = gSoldierPistolList;
}
HOOK(0x459F00, SoldierPistol_GetParamTable);

void SoldierPistol_InputHandler(Soldier *h, SoldierInputMsg *msg)
{
    if (!h->inputLocked) {
        memcpy(h->prevInput, h->input, sizeof(h->input));
        memcpy(h->input, msg->input, sizeof(h->input));
    }
    SoldierPistol_Update(h);
}
HOOK(0x459F10, SoldierPistol_InputHandler);

void SoldierPistol_Update(Soldier *h)
{
    float *d = h->delta, x, y, z;
    long double speed;

    Soldier_EnsureWeapon(h);
    sub_460CF0(h);
    StateMachine_Update(&h->sm, h);
    if (!is_zero(h->input[20]))
        h->run ^= 1;
    h->sm.bits &= ~SB_HIT;
    Human_UpdateBody(h);
    HumanView_UpdateView(h->view, h);
    h->unkD8 = 1;
    BoneObj_GetRootMotion(h->unk514, d, h->anim);
    x = h->matrix[2] * d[2] + h->matrix[1] * d[1] + h->matrix[0] * d[0];
    y = h->matrix[5] * d[2] + h->matrix[4] * d[1] + h->matrix[3] * d[0];
    z = h->matrix[8] * d[2] + h->matrix[7] * d[1] + h->matrix[6] * d[0];
    d[0] = x;
    d[1] = y;
    d[2] = z;
    sub_4613D0(h);
    if (!h->attach)
        Human_UpdateMove(h);
    else
        Human_MoveOnPlatform(h);
    HumanView_UpdateBody(h->view, h);
    Human_UpdateWeapon(h);
    Human_SendPosition(h);
    SoldierPistol_Footstep(h);
    speed = h->vel[2];
    if (h->vel[2] < 0)
        speed = -speed;
    Human_printf(h, "Speed Z (m/s): %.2f\n", (double)(speed * kUnitsToMetersF * kFps));
    Human_printf(h, "Ground task: %x\n", Human_GetGround(h));
    Human_printf(h, "Attach task: %x\n", h->attach);
    Human_printf(h, "Delta move pos X: %.2f\n", (double)h->movePos[0]);
    Human_printf(h, "Delta move pos Y: %.2f\n", (double)h->movePos[1]);
    Human_printf(h, "Delta move pos Z: %.2f\n", (double)h->movePos[2]);
    Human_printf(h, "Delta move Alpha: %.2f\n", (double)h->moveAngle[0]);
    Human_printf(h, "Delta move Beta: %.2f\n", (double)h->moveAngle[1]);
    Human_printf(h, "Delta move Gamma: %.2f\n", (double)h->moveAngle[2]);
    Human_printf(h, "Distance to ground from feet: %.2fm\n", (double)(h->groundDistance * kUnitsToMeters));
    Human_printf(h, "Ground angle: %.3f\n", (double)(h->groundAngle * kRadToDeg));
    Human_printf(h, "Delta translation X: %.2f\n", (double)h->delta[0]);
    Human_printf(h, "Delta translation Y: %.2f\n", (double)h->delta[1]);
    Human_printf(h, "Delta translation Z: %.2f\n", (double)h->delta[2]);
    if (h->sm.bits & SB_ON_GROUND)
        Human_printf(h, "Ground!\n");
}
HOOK(0x459F50, SoldierPistol_Update);

/* hit report every damage handler sends to the AI */
static void ps_tell_ai(Soldier *h, void *data, void *attacker)
{
    SoldierAIMsg msg;

    msg.type = 0xF;
    msg.unk4 = 1;
    msg.unk8 = 0;
    msg.data = data;
    msg.attacker = attacker;
    msg.attacker2 = attacker;
    HumanAI_Send(h->ai, &msg);
}

/* Adds dmg to the hit sum; a sum over the limit flinches (SB_HIT) and
   starts over. Returns the new bits. */
static uint32_t ps_flinch(SoldierP *s, long double dmg, uint32_t bits, int *flinched)
{
    long double sum = dmg + s->hitSum;

    s->hitSum = sum;
    *flinched = 0;
    if (s->s.damage.total < s->s.damage.max && sum > s->hitLimit) {
        bits |= SB_HIT;
        s->hitSum = 0;
        *flinched = 1;
    }
    return bits;
}

void SoldierPistol_HitHandler(Soldier *h, SoldierHit *hit)
{
    SoldierP *s = (SoldierP *)h;
    SoldierDamage *d = &h->damage;
    double scratch[3];
    char headshot = 0;
    int part, flinched;
    float dmg;
    uint32_t bits;

    if (!h->ai || (headshot = h->ai->unk3ABD, !h->ai->unk3ABC)) {
        part = Human_GetHitZone(h, hit);
        d->hitZone = part;
        HumanDamage_GetZone(d, part);
        if ((part == 15 || part == 16) && headshot)
            dmg = hit->damage;
        else
            dmg = d->part[part].mult * hit->damage;
        d->part[part].dmg = dmg + d->part[part].dmg;
        d->total = dmg + d->total;
        Human_Bleed(h, hit->dir, hit->unk00);
        bits = ps_flinch(s, dmg, h->sm.bits, &flinched);
        if (d->total < d->max) {
            Human_SayTimed(h, "ai_hit_", 3);
        } else {
            s->deathDir[0] = hit->dir[0];
            s->deathDir[1] = hit->dir[1];
            s->deathDir[2] = hit->dir[2];
            s->deathKind = *(int *)((char *)hit->weapon + 0x610) == 2 ? 1 : 2;
            bits |= SB_DEAD;
            h->dying = 1;
        }
        h->sm.bits = bits;
    }
    ps_tell_ai(h, scratch, hit->attacker);
}
HOOK(0x45A230, SoldierPistol_HitHandler);

void SoldierPistol_BlastHandler(Soldier *h, SoldierBlast *e)
{
    SoldierP *s = (SoldierP *)h;
    SoldierDamage *d = &h->damage;
    double cam[3], v[3];
    long double k, r;
    uint32_t bits;
    int flinched;

    if (!h->vulnerable)
        return;
    if (h->ai && h->ai->unk3ABC)
        return;
    HumanView_GetEye(cam, h->view);
    v[0] = e->pos[0] - cam[0];
    v[1] = e->pos[1] - cam[1];
    v[2] = e->pos[2] - cam[2];
    k = Vec3d_Length(v) - e->radius * 3.0f;
    if (!(k >= 0))
        k = 1.0f;
    else
        k = 1.0f - k / (e->radius * 6.0f);
    if (k > 0)
        HumanView_Shake(h->view, k * kShakeAmount, k * kShakeAngle, e->shakeFrames);
    d->hitZone = 13;
    HumanDamage_GetZone(d, 13);
    r = Game_BlastFalloff(e, h, h->pos, h->radius * kBlastRadius, 1);
    if (is_zero(r))
        return;
    r = r * d->part[13].mult;
    d->part[13].dmg = r + d->part[13].dmg;
    d->total = r + d->total;
    bits = ps_flinch(s, r, h->sm.bits, &flinched);
    if (d->total < d->max) {
        if (flinched)
            Human_SayTimed(h, "ai_xplhit_", 3);
    } else {
        s->deathKind = 0;
        bits |= SB_DEAD;
        s->deathDir[0] = h->pos[0] - e->pos[0];
        s->deathDir[1] = h->pos[1] - e->pos[1];
        h->dying = 1;
        s->deathDir[2] = h->pos[2] - e->pos[2];
    }
    h->sm.bits = bits;
}
HOOK(0x45A3D0, SoldierPistol_BlastHandler);

void SoldierPistol_PunchHandler(Soldier *h, SoldierPunch *p)
{
    SoldierP *s = (SoldierP *)h;
    SoldierDamage *d = &h->damage;
    double fwd[3], v[3], *a, *b;
    long double dmg;
    int part, flinched;
    uint32_t bits;

    if (!h->ai || !h->ai->unk3ABC) {
        fwd[0] = h->matrix[1] * kForward;
        fwd[1] = h->matrix[4] * kForward;
        fwd[2] = h->matrix[7] * kForward;
        a = DynCubeObj_GetPosition(h);
        b = DynCubeObj_GetPosition(p->attacker);
        v[0] = a[0] - b[0];
        a = DynCubeObj_GetPosition(h);
        b = DynCubeObj_GetPosition(p->attacker);
        v[1] = a[1] - b[1];
        a = DynCubeObj_GetPosition(h);
        b = DynCubeObj_GetPosition(p->attacker);
        v[2] = a[2] - b[2];
        Vec3d_Normalize(v);
        part = v[2] * fwd[2] + v[1] * fwd[1] + v[0] * fwd[0] >= 0.0 ? 14 : 13;
        d->hitZone = part;
        HumanDamage_GetZone(d, part);
        dmg = d->part[part].mult * p->damage;
        d->part[part].dmg = dmg + d->part[part].dmg;
        d->total = dmg + d->total;
        bits = ps_flinch(s, dmg, h->sm.bits, &flinched);
        if (d->total < d->max) {
            Human_SayTimed(h, "ai_hit_", 3);
        } else {
            s->deathKind = 2;
            memcpy(s->deathDir, v, sizeof(v));
            bits |= SB_DEAD;
            h->dying = 1;
        }
        h->sm.bits = bits;
    }
    ps_tell_ai(h, fwd, p->attacker);
}
HOOK(0x45A5E0, SoldierPistol_PunchHandler);

void SoldierPistol_Close(void)
{
    QTaskList_Free(gSoldierPistolList);
    TaskType_Free(gSoldierPistolType);
}
HOOK(0x45A7C0, SoldierPistol_Close);

uint16_t SoldierPistol_GetType(void)
{
    return gSoldierPistolType;
}
HOOK(0x45A7E0, SoldierPistol_GetType);
