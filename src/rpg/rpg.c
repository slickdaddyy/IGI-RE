/* HumanSoldierRPG: an AI soldier with a rocket launcher. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "rpg/soldier.h"

FN_EXTERN(Human_DeadRun);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x413C80, void *, Weapon_GetType, (void *weapon));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (SoldierBlast *e, Soldier *h, double *pos, float radius, int unk));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x450D50, void, HumanAI_Send, (SoldierAI *ai, SoldierAIMsg *msg));
ORIG(0x454EE0, void, Human_RunBase, (void *task, void *arg));
ORIG(0x459EC0, void, Soldier_DeleteHandler, (void *task, void *arg));
ORIG(0x45D360, void, Soldier_EnsureWeapon, (Soldier *h));
ORIG(0x45D5B0, void, Soldier_GetDeathVelocity, (double *vel, Soldier *h, int model, int anim));
ORIG(0x45DAA0, char, Soldier_TurnToHit, (Soldier *h, double *dir));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460C40, uint8_t, Human_GetPunchEvent, (void));
ORIG(0x460C50, void *, Human_GetParamTable, (void));
ORIG(0x460C80, void, Human_printf, (Soldier *h, const char *fmt, ...));
ORIG(0x460CF0, void, Human_UpdatePlatformMove, (Soldier *h));
ORIG(0x4610C0, void, Human_UpdateBody, (Soldier *h));
ORIG(0x4613D0, void, Human_FireAnimTrigger, (Soldier *h));
ORIG(0x4614B0, void, Human_UpdateMove, (Soldier *h));
ORIG(0x462CF0, void, Human_MoveOnPlatform, (Soldier *h));
ORIG(0x462E50, void, Human_UpdateWeapon, (Soldier *h));
ORIG(0x463080, void, Human_SendPosition, (Soldier *h));
ORIG(0x463680, int, Human_GetGround, (Soldier *h));
ORIG(0x463760, void, HumanViewCam, (Soldier *h, int mode));
ORIG(0x463770, void, Human_ApplyGravity, (Soldier *h));
ORIG(0x4635F0, void, Human_FreezeView, (Soldier *h));
ORIG(0x4637C0, void, Human_SayTimed, (Soldier *h, const char *sound, int count));
ORIG(0x4638A0, void, Human_Say, (Soldier *h, const char *sound, int count));
ORIG(0x463920, void, Human_Footstep, (Soldier *h));
ORIG(0x463AD0, void, Human_Bleed, (Soldier *h, float *dir, int unk));
ORIG(0x464700, void, HumanView_LockPitch, (void *view));
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
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, double *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C7140, void, DynCube_Add, (void *world, Soldier *h, void *pos, float radius, int flags));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (void *a, float *delta, void *anim));
ORIG(0x4D3710, void, Task_Nothing, (void *task, void *arg));
ORIG(0x4D38C0, void, BoneObj_Update, (float *matrix, void *anim));
ORIG(0x4D61D0, void, Anim_Play, (void *anim, int track, int id, int a, int b));
ORIG(0x4D64E0, long double, Anim_GetLength, (void *anim, int track));
ORIG(0x4D6520, long double, Anim_GetPhase, (void *anim, int track));
ORIG(0x4D65B0, void, Anim_SetTime, (void *anim, int track, float time));
ORIG(0x4D74F0, int, Anim_Exists, (void *anim, int track, int id));
ORIG(0x4D9620, void, BoneDynCubeObj_Init, (void *world, Soldier *h, short model, int unk));
ORIG(0x4D96F0, void, BoneDynCubeObj_Deinit, (Soldier *h));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, const double *pos));
ORIG(0x4EC190, void, StateMachine_Init, (SoldierSM *sm, SoldierStateFn input, SoldierStateFn *states, int state,
                                         int a, void *combos));
ORIG(0x4EC1E0, void, StateMachine_Update, (SoldierSM *sm, Soldier *h));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (const char *name));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *parent, int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F2060, void *, WeaponSlots_Get, (void **slots, int i));
ORIG(0x4F2070, void, WeaponSlots_Set, (void **slots, int i, void *weapon));
ORIG(0x4F2290, void, Weapon_DeleteAll, (Soldier *h));
ORIG(0x4F2350, void, Weapon_UseAmmo, (void *weapon, int n));
ORIG(0x4F2390, void, Weapon_Drop, (void *weapon));
ORIG(0x4F23D0, void, Weapon_GetAmmo, (void *weapon, int *out));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));

extern uint16_t gSoldierRPGType;
extern void *gSoldierRPGList;
extern int gSoldierRPGUnk;
extern SoldierStateFn gSoldierRPGStates[];
extern char gSoldierRPGCombos[];
extern char gSoldierRPGAnimSlots[];
extern char gSoldierRPGSpeeds[];
#define Human_DeadRun ((SoldierStateFn)FN(0x45C740, Human_DeadRun))

static const float kShakeAmount = 0x1.99999ap+7f;
static const float kShakeAngle = 0x1.aceea0p-6f;
static const float kBlastRadius = 0x1.555556p-2f;
static const double kForward = 4096.0;
static const float kUnitsToMetersF = 0x1p-12f;
static const double kUnitsToMeters = 0x1p-12;
static const float kFps = 30.0f;
static const float kRadToDeg = 0x1.ca5dc0p+5f;

enum {
    RPG_STAND,
    RPG_FALL = 11,
    RPG_HIT,
    RPG_DIE,
    RPG_ANIM,
};

static int is_zero(long double x)
{
    return !(x < 0 || x > 0);
}

static void sm_go(SoldierSM *sm, Soldier *h, int n)
{
    sm->prev = sm->state;
    sm->state = n;
    sm->run = sm->states[n];
    sm->run(sm, h);
}

/* falling, hit and death come first in every state; hit is optional */
static int rpg_common(SoldierSM *sm, Soldier *h, uint32_t b, int hit)
{
    if (!(b & SB_ON_GROUND))
        sm_go(sm, h, RPG_FALL);
    else if (hit && (b & SB_HIT))
        sm_go(sm, h, RPG_HIT);
    else if (b & SB_DEAD)
        sm_go(sm, h, RPG_DIE);
    else
        return 0;
    return 1;
}

static void rpg_enter(SoldierSM *sm, Soldier *h, SoldierStateFn run)
{
    h->unk33D = 0;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = run;
}

void SoldierRPG_StandUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_StandEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 0;
    h->unk696 = 0;
    if (h->idleAnim == -1) {
        Human_PlayAnimSlot(h, sm->state, 0);
    } else if (Anim_Exists(h->anim, 0, h->idleAnim)) {
        Anim_Play(h->anim, 0, h->idleAnim, 0, 0);
    } else {
        Warning_Show("Animation #%d not found", h->idleAnim);
        Human_PlayAnimSlot(h, sm->state, 0);
    }
    sm->run = SoldierRPG_StandUpdate;
}
HOOK(0x455990, SoldierRPG_StandEnter);

void SoldierRPG_StandUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if (b & 0x200)
        sm_go(sm, h, 4);
    else if (b & 0x800)
        sm_go(sm, h, 5);
    else if (b & SB_ANIM)
        sm_go(sm, h, RPG_ANIM);
    else if ((b & SB_RUN) && (b & 0x80))
        sm_go(sm, h, 2);
    else if (!(b & SB_RUN) && (b & 0x80))
        sm_go(sm, h, 1);
    else if (b & 0x80010)
        sm_go(sm, h, 5);
}
HOOK(0x455A40, SoldierRPG_StandUpdate);

void SoldierRPG_WalkUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_WalkEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 0;
    h->unk696 = 0;
    Anim_GetPhase(h->anim, 0);
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = SoldierRPG_WalkUpdate;
}
HOOK(0x455BA0, SoldierRPG_WalkEnter);

void SoldierRPG_WalkUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if (b & 0x200)
        sm_go(sm, h, 4);
    else if (b & 0x800)
        sm_go(sm, h, 5);
    else if (b & SB_RUN)
        sm_go(sm, h, 2);
    else if (!(b & 0x80))
        sm_go(sm, h, RPG_STAND);
    else if (b & 0x80000)
        sm_go(sm, h, 5);
}
HOOK(0x455BE0, SoldierRPG_WalkUpdate);

void SoldierRPG_RunUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_RunEnter(SoldierSM *sm, Soldier *h)
{
    float phase;

    h->unk33D = 0;
    h->unk696 = 0;
    phase = Anim_GetPhase(h->anim, 0);
    Human_PlayAnimSlot(h, sm->state, 0);
    if (sm->prev == 1)
        Anim_SetTime(h->anim, 0, Anim_GetLength(h->anim, 0) * phase);
    sm->run = SoldierRPG_RunUpdate;
}
HOOK(0x455D10, SoldierRPG_RunEnter);

void SoldierRPG_RunUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if (b & 0x200)
        sm_go(sm, h, 4);
    else if (b & 0x800)
        sm_go(sm, h, 5);
    else if (!(b & SB_RUN))
        sm_go(sm, h, 1);
    else if (!(b & 0x80))
        sm_go(sm, h, RPG_STAND);
    else if (b & 0x80000)
        sm_go(sm, h, 5);
}
HOOK(0x455D80, SoldierRPG_RunUpdate);

void SoldierRPG_FireUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_FireEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 1;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = SoldierRPG_FireUpdate;
}
HOOK(0x455EB0, SoldierRPG_FireEnter);

/* weapon up, the fire control shoots */
static int rpg_fire(SoldierSM *sm, Soldier *h, int next)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    int ammo[2];
    uint32_t b;

    Weapon_GetType(w);
    Weapon_GetAmmo(w, ammo);
    b = sm->bits;
    if (rpg_common(sm, h, b, 1))
        return 1;
    if (!is_zero(h->input[6]))
        h->fire = 1;
    if ((b & SB_ANIM_DONE) && !(b & 0x200))
        sm_go(sm, h, next);
    return 0;
}

void SoldierRPG_FireUpdate(SoldierSM *sm, Soldier *h)
{
    rpg_fire(sm, h, RPG_STAND);
}
HOOK(0x455EE0, SoldierRPG_FireUpdate);

void SoldierRPG_RaiseUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_RaiseEnter(SoldierSM *sm, Soldier *h)
{
    rpg_enter(sm, h, SoldierRPG_RaiseUpdate);
}
HOOK(0x455FE0, SoldierRPG_RaiseEnter);

void SoldierRPG_RaiseUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 3);
}
HOOK(0x456010, SoldierRPG_RaiseUpdate);

void SoldierRPG_AimRaiseUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_AimRaiseEnter(SoldierSM *sm, Soldier *h)
{
    rpg_enter(sm, h, SoldierRPG_AimRaiseUpdate);
}
HOOK(0x4560C0, SoldierRPG_AimRaiseEnter);

void SoldierRPG_AimRaiseUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 0))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 7);
}
HOOK(0x4560F0, SoldierRPG_AimRaiseUpdate);

void SoldierRPG_AimLowerUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_AimLowerEnter(SoldierSM *sm, Soldier *h)
{
    rpg_enter(sm, h, SoldierRPG_AimLowerUpdate);
}
HOOK(0x456170, SoldierRPG_AimLowerEnter);

/* also the run of the hit state */
void SoldierRPG_AimLowerUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 0))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, RPG_STAND);
}
HOOK(0x4561A0, SoldierRPG_AimLowerUpdate);

void SoldierRPG_AimUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_AimEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 1;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = SoldierRPG_AimUpdate;
}
HOOK(0x456220, SoldierRPG_AimEnter);

void SoldierRPG_AimUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if (b & 0x200)
        sm_go(sm, h, 10);
    else if (b & 0x800)
        sm_go(sm, h, 9);
    else if (!(b & 0x80010))
        sm_go(sm, h, RPG_STAND);
}
HOOK(0x456250, SoldierRPG_AimUpdate);

void SoldierRPG_AimFireUpdate(SoldierSM *sm, Soldier *h);

static void rpg_enter_keep(SoldierSM *sm, Soldier *h, SoldierStateFn run)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = run;
}

void SoldierRPG_AimFireEnter(SoldierSM *sm, Soldier *h)
{
    rpg_enter_keep(sm, h, SoldierRPG_AimFireUpdate);
}
HOOK(0x456350, SoldierRPG_AimFireEnter);

void SoldierRPG_AimFireUpdate(SoldierSM *sm, Soldier *h)
{
    rpg_fire(sm, h, 7);
}
HOOK(0x456380, SoldierRPG_AimFireUpdate);

void SoldierRPG_ReloadUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_ReloadEnter(SoldierSM *sm, Soldier *h)
{
    rpg_enter_keep(sm, h, SoldierRPG_ReloadUpdate);
}
HOOK(0x456480, SoldierRPG_ReloadEnter);

void SoldierRPG_ReloadUpdate(SoldierSM *sm, Soldier *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if (b & SB_ANIM_DONE) {
        Weapon_UseAmmo(w, 1);
        sm_go(sm, h, 7);
    }
}
HOOK(0x4564B0, SoldierRPG_ReloadUpdate);

void SoldierRPG_AimFireRaiseUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_AimFireRaiseEnter(SoldierSM *sm, Soldier *h)
{
    rpg_enter_keep(sm, h, SoldierRPG_AimFireRaiseUpdate);
}
HOOK(0x456570, SoldierRPG_AimFireRaiseEnter);

void SoldierRPG_AimFireRaiseUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 8);
}
HOOK(0x4565A0, SoldierRPG_AimFireRaiseUpdate);

void SoldierRPG_FallUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_FallEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 0;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    memcpy(h->vel, h->unk688, sizeof(h->vel));
    sm->run = SoldierRPG_FallUpdate;
}
HOOK(0x456650, SoldierRPG_FallEnter);

void SoldierRPG_FallUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b;

    Human_ApplyGravity(h);
    b = sm->bits;
    if (!(b & SB_ON_GROUND)) {
        sm_go(sm, h, RPG_FALL);
        return;
    }
    if (b & SB_DEAD) {
        sm_go(sm, h, RPG_DIE);
        return;
    }
    if (h->ground) {
        const char *name = h->ground + 0x114;
        void *snd = Sound_Play(Level_GetDynamic(name), name);

        if (snd)
            Sound_SetPos(snd, h->soundPos);
    }
    h->vel[0] = 0;
    h->vel[1] = 0;
    h->vel[2] = 0;
    sm_go(sm, h, RPG_STAND);
}
HOOK(0x4566A0, SoldierRPG_FallUpdate);

void SoldierRPG_HitEnter(SoldierSM *sm, Soldier *h)
{
    HitAnims *a;

    h->unk33D = 0;
    h->unk696 = 1;
    a = HumanDamage_GetZone(&h->damage, h->damage.hitZone);
    Anim_Play(h->anim, 0, a->hit, 0, 0);
    sm->run = SoldierRPG_AimLowerUpdate;
}
HOOK(0x456770, SoldierRPG_HitEnter);

void SoldierRPG_DieUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_DieEnter(SoldierSM *sm, Soldier *h)
{
    static const int kDeaths[2] = {14, 12};
    SoldierRPG *s = (SoldierRPG *)h;
    SoldierAI *ai = h->ai;
    void *w = WeaponSlots_Get(h->weapons, 0);
    int part, anim = -1, r;
    HitAnims *a;

    if (w) {
        Weapon_Drop(w);
        WeaponSlots_Set(h->weapons, 0, NULL);
    }
    Weapon_DeleteAll(h);
    h->unk33D = 0;
    h->unk696 = 1;
    part = h->damage.hitZone;
    a = HumanDamage_GetZone(&h->damage, part);
    if (ai && ai->deathAnim != -1) {
        anim = ai->deathAnim;
    } else {
        r = Random_Int(Game_GetRandom(), 2);
        if (s->deathKind >= 0) {
            if (s->deathKind <= 1)
                anim = Soldier_TurnToHit(h, s->deathDir) ? 15 : 16;
            else if (s->deathKind == 2)
                anim = part == 15 || part == 16 ? a->death[r] : kDeaths[r];
        }
    }
    Anim_Play(h->anim, 0, anim, 0, 0);
    Soldier_GetDeathVelocity(s->deathVel, h, h->model, anim);
    HumanViewCam(h, 3);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_Say(h, "ai_death_", 4);
    sm->run = SoldierRPG_DieUpdate;
}
HOOK(0x4567C0, SoldierRPG_DieEnter);

void SoldierRPG_DieUpdate(SoldierSM *sm, Soldier *h)
{
    SoldierRPG *s = (SoldierRPG *)h;

    h->pos[0] = s->deathVel[0] + h->pos[0];
    h->pos[1] = s->deathVel[1] + h->pos[1];
    h->pos[2] = s->deathVel[2] + h->pos[2];
    if (sm->bits & SB_ANIM_DONE)
        sm->run = Human_DeadRun;
}
HOOK(0x456920, SoldierRPG_DieUpdate);

void SoldierRPG_AnimUpdate(SoldierSM *sm, Soldier *h);

void SoldierRPG_AnimEnter(SoldierSM *sm, Soldier *h)
{
    SoldierRPG *s = (SoldierRPG *)h;

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
    sm->run = SoldierRPG_AnimUpdate;
}
HOOK(0x456960, SoldierRPG_AnimEnter);

void SoldierRPG_AnimUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (rpg_common(sm, h, b, 1))
        return;
    if ((b & 0x81FE1) || ((SoldierRPG *)h)->anim == -1)
        sm_go(sm, h, RPG_STAND);
}
HOOK(0x456A10, SoldierRPG_AnimUpdate);

void SoldierRPG_CreateHandler(Soldier *h, void *arg);
void SoldierRPG_GetParamTable(Soldier *h, void **out);
void SoldierRPG_ParamsChanged(Soldier *h, void *arg);
void SoldierRPG_InputHandler(Soldier *h, SoldierInputMsg *msg);
void SoldierRPG_HitHandler(Soldier *h, SoldierHit *hit);
void SoldierRPG_BlastHandler(Soldier *h, SoldierBlast *e);
void SoldierRPG_PunchHandler(Soldier *h, SoldierPunch *p);
void SoldierRPG_UpdateInput(SoldierSM *sm, Soldier *h);

#define FIELD(name) ((void *)offsetof(Soldier, name))

void SoldierRPG_Open(void)
{
    gSoldierRPGType = TaskType_New("HumanSoldierRPG", sizeof(SoldierRPG), Human_GetType(), 0);
    TaskType_SetFlags(gSoldierRPGType, 1);
    TaskType_SetHandler(gSoldierRPGType, 0, SoldierRPG_CreateHandler);
    TaskType_SetHandler2(gSoldierRPGType, 1, Human_RunBase);
    TaskType_SetHandler2(gSoldierRPGType, 2, Soldier_DeleteHandler);
    TaskType_SetHandler(gSoldierRPGType, Param_GetTableEvent(), SoldierRPG_GetParamTable);
    TaskType_SetHandler(gSoldierRPGType, Param_GetChangedEvent(), SoldierRPG_ParamsChanged);
    TaskType_SetHandler(gSoldierRPGType, Vehicle_GetControlEvent(), SoldierRPG_InputHandler);
    TaskType_SetHandler(gSoldierRPGType, Game_GetEvent9(), SoldierRPG_HitHandler);
    TaskType_SetHandler(gSoldierRPGType, Game_GetEvent11(), SoldierRPG_BlastHandler);
    TaskType_SetHandler(gSoldierRPGType, Human_GetPunchEvent(), SoldierRPG_PunchHandler);
    TaskType_SetHandler(gSoldierRPGType, FlashBang_GetEvent(), Task_Nothing);
    gSoldierRPGList = ParamTable_Extend(Human_GetParamTable(), 2);
    /* the original passes a dummy soldier on its stack, only the difference
       of the two pointers is kept */
    ParamTable_Add(gSoldierRPGList, NULL, FIELD(model), Param_Int32(), 7, NULL);
    ParamTable_Add(gSoldierRPGList, NULL, FIELD(idleAnim), Param_Int32(), 7, NULL);
    gSoldierRPGUnk = 0;
}
HOOK(0x456AD0, SoldierRPG_Open);

void SoldierRPG_CreateHandler(Soldier *h, void *arg)
{
    TASK_HANDLER(0, Human_GetType())(h, arg);
    StateMachine_Init(&h->sm, SoldierRPG_UpdateInput, gSoldierRPGStates, 0, 1, gSoldierRPGCombos);
    h->animSlots = gSoldierRPGAnimSlots;
    Human_InitDamage(h, gSoldierRPGSpeeds);
    h->model = 2;
    h->idleAnim = 2;
    h->run = 1;
    h->spawned = 0;
}
HOOK(0x456C70, SoldierRPG_CreateHandler);

static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

void SoldierRPG_UpdateInput(SoldierSM *sm, Soldier *h)
{
    static const uint32_t bits[8] = {0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000};
    uint32_t flags = h->motion->flags;
    uint32_t b = sm->bits & 0xFFE3E01C;
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
    if (!is_zero(h->input[22])) {
        b |= SB_ANIM;
        ((SoldierRPG *)h)->anim = x87_fistp(h->input[22]);
    }
    sm->bits = b;
}
HOOK(0x456CF0, SoldierRPG_UpdateInput);

void SoldierRPG_GetParamTable(Soldier *h, void **out)
{
    (void)h;
    *out = gSoldierRPGList;
}
HOOK(0x456E40, SoldierRPG_GetParamTable);

void SoldierRPG_ParamsChanged(Soldier *h, void *arg)
{
    if (h->spawned)
        BoneDynCubeObj_Deinit(h);
    BoneDynCubeObj_Init(Game_GetWorld(), h, (short)h->model, 0);
    h->unkCC = 2;
    if (h->idleAnim == -1) {
        Human_PlayAnim(h, 2, 0, 0);
    } else if (Anim_Exists(h->anim, 0, h->idleAnim)) {
        Anim_Play(h->anim, 0, h->idleAnim, 0, 0);
    } else {
        Warning_Show("Animation #%d not found", h->idleAnim);
        Human_PlayAnim(h, 2, 0, 0);
    }
    if (!h->spawned) {
        DynCube_Add(Game_GetWorld(), h, h->unkE0, h->radius, 2);
        h->spawned = 1;
    }
    TASK_HANDLER(Param_GetChangedEvent(), Human_GetType())(h, arg);
    BoneObj_Update(h->matrix, h->anim);
    memcpy(h->unk650, h->motion->unk4C, sizeof(h->unk650));
}
HOOK(0x456E50, SoldierRPG_ParamsChanged);

void SoldierRPG_InputHandler(Soldier *h, SoldierInputMsg *msg)
{
    if (!h->inputLocked) {
        memcpy(h->prevInput, h->input, sizeof(h->input));
        memcpy(h->input, msg->input, sizeof(h->input));
    }
    SoldierRPG_Update(h);
}
HOOK(0x456FB0, SoldierRPG_InputHandler);

void SoldierRPG_Update(Soldier *h)
{
    float *d = h->delta, x, y, z;
    long double speed;

    Soldier_EnsureWeapon(h);
    Human_UpdatePlatformMove(h);
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
    Human_FireAnimTrigger(h);
    if (!h->attach)
        Human_UpdateMove(h);
    else
        Human_MoveOnPlatform(h);
    HumanView_UpdateBody(h->view, h);
    Human_UpdateWeapon(h);
    Human_SendPosition(h);
    Human_Footstep(h);
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
HOOK(0x456FF0, SoldierRPG_Update);

static void rpg_tell_ai(Soldier *h, void *data, void *attacker)
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

void SoldierRPG_HitHandler(Soldier *h, SoldierHit *hit)
{
    SoldierRPG *s = (SoldierRPG *)h;
    SoldierDamage *d = &h->damage;
    double scratch[3];
    char headshot = 0;
    long double dmg;
    uint32_t bits;
    int part;

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
        bits = h->sm.bits;
        if (d->total < d->max) {
            bits |= SB_HIT;
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
    rpg_tell_ai(h, scratch, hit->attacker);
}
HOOK(0x4572D0, SoldierRPG_HitHandler);

void SoldierRPG_BlastHandler(Soldier *h, SoldierBlast *e)
{
    SoldierRPG *s = (SoldierRPG *)h;
    SoldierDamage *d = &h->damage;
    double cam[3], v[3];
    long double k, r, total;
    uint32_t bits;

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
    total = r + d->total;
    d->total = total;
    bits = h->sm.bits;
    if (total < d->max) {
        bits |= SB_HIT;
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
HOOK(0x457430, SoldierRPG_BlastHandler);

void SoldierRPG_PunchHandler(Soldier *h, SoldierPunch *p)
{
    SoldierRPG *s = (SoldierRPG *)h;
    SoldierDamage *d = &h->damage;
    double fwd[3], v[3], *a, *b;
    long double dmg, total;
    uint32_t bits;
    int part;

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
        total = dmg + d->total;
        d->total = total;
        bits = h->sm.bits;
        if (total < d->max) {
            bits |= SB_HIT;
            Human_SayTimed(h, "ai_hit_", 3);
        } else {
            memcpy(s->deathDir, v, sizeof(v));
            bits |= SB_DEAD;
            s->deathKind = 2;
            h->dying = 1;
        }
        h->sm.bits = bits;
    }
    rpg_tell_ai(h, fwd, p->attacker);
}
HOOK(0x457620, SoldierRPG_PunchHandler);

void SoldierRPG_Close(void)
{
    QTaskList_Free(gSoldierRPGList);
    TaskType_Free(gSoldierRPGType);
}
HOOK(0x4577E0, SoldierRPG_Close);

uint16_t SoldierRPG_GetType(void)
{
    return gSoldierRPGType;
}
HOOK(0x457800, SoldierRPG_GetType);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 53E6A8, 53E6A8, 0x70) INIT({0x90, 0x59, 0x45, 0x00, 0xA0, 0x5B, 0x45, 0x00, 0x10, 0x5D, 0x45, 0x00, 0xB0, 0x5E, 0x45, 0x00, 0xE0, 0x5F, 0x45, 0x00, 0xC0, 0x60, 0x45, 0x00, 0x70, 0x61, 0x45, 0x00, 0x20, 0x62, 0x45, 0x00, 0x50, 0x63, 0x45, 0x00, 0x80, 0x64, 0x45, 0x00, 0x70, 0x65, 0x45, 0x00, 0x50, 0x66, 0x45, 0x00, 0x70, 0x67, 0x45, 0x00, 0xC0, 0x67, 0x45, 0x00, 0x60, 0x69, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02});
ALIAS(gSoldierRPGStates, pad_53E6A8, 0);
PAD(igd, 53E6A8, 53E718, 0x8) INIT({0x04, 0x00, 0x00, 0x00, 0xE8, 0xE6, 0x53});
ALIAS(gSoldierRPGCombos, pad_53E718, 0);
PAD(igd, 53E6A8, 53E720, 0x78) INIT({0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01});
ALIAS(gSoldierRPGAnimSlots, pad_53E720, 0);
PAD(igd, 53E6A8, 53E798, 0x120) INIT({0x00, 0x00, 0x00, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x0D, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x42, 0x0D, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x42, 0x0D, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x53, 0x6F, 0x6C, 0x64, 0x69, 0x65, 0x72, 0x52, 0x50, 0x47});
ALIAS(gSoldierRPGSpeeds, pad_53E798, 0);
BSS(5BDA94) uint16_t gSoldierRPGType;
PAD(igb, 5BDA94, 5BDA96, 0x2);
BSS(5BDA94) void *gSoldierRPGList;
PAD(igb, 5BDA94, 5BDA9C, 0x4);
BSS(5BDA94) int gSoldierRPGUnk;
