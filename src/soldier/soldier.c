/* HumanSoldier: the AI soldier task type, its later movement states, input,
   update and damage. */
#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "soldier/soldier.h"
#include "material/materialtypes.h"

FN_EXTERN(SoldierPistol_BlastHandler);
FN_EXTERN(SoldierPistol_HitUpdate);
FN_EXTERN(SoldierPistol_PunchHandler);
FN_EXTERN(SoldierPistol_State19Update);
FN_EXTERN(SoldierPistol_State22Update);
FN_EXTERN(SoldierPistol_State28Update);
FN_EXTERN(SoldierRPG_ParamsChanged);
FN_EXTERN(Soldier_DeleteHandler);
FN_EXTERN(Task_Nothing);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x413BC0, int, WeaponType_GetMaxId, (void));
ORIG(0x413CC0, void *, Task_FindWeapon, (Human *h, int id));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x444B30, void, Obj_GetUsePoint, (void *obj, double *pos, float *matrix));
ORIG(0x450D50, void, HumanAI_Send, (HumanAI *ai, SoldierAIMsg *msg));
ORIG(0x459D50, void, SoldierPistol_UseTarget, (Human *h));
ORIG(0x45A7F0, void, Soldier_Footstep, (Human *h));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460C40, uint8_t, Human_GetPunchEvent, (void));
ORIG(0x460C50, void *, Human_GetParamTable, (void));
ORIG(0x460C80, void, Human_printf, (Human *h, const char *fmt, ...));
ORIG(0x460CF0, void, Human_UpdatePlatformMove, (Human *h));
ORIG(0x4610C0, void, Human_UpdateBody, (Human *h));
ORIG(0x4613D0, void, Human_FireAnimTrigger, (Human *h));
ORIG(0x4614B0, void, Human_UpdateMove, (Human *h));
ORIG(0x462CF0, void, Human_MoveOnPlatform, (Human *h));
ORIG(0x462E50, void, Human_UpdateWeapon, (Human *h));
ORIG(0x463080, void, Human_SendPosition, (Human *h));
ORIG(0x463110, int, Human_GetState, (Human *h));
ORIG(0x4631A0, void, Human_SetPosition, (Human *h, double *pos));
ORIG(0x463220, void, Human_SetYaw, (Human *h, float yaw));
ORIG(0x463310, void, Human_SetPlatform, (Human *h, void *obj));
ORIG(0x463550, void, Human_ClearPlatform, (Human *h));
ORIG(0x4635F0, void, Human_FreezeView, (Human *h));
ORIG(0x463600, void, Human_UnfreezeView, (Human *h));
ORIG(0x463620, void, Human_EndUse, (Human *h));
ORIG(0x463680, int, Human_GetGround, (Human *h));
ORIG(0x4636A0, void, Human_SetClimbObject, (Human *h, void *obj));
ORIG(0x4636E0, void *, Human_GetClimbObject, (Human *h));
ORIG(0x463760, void, HumanViewCam, (Human *h, int mode));
ORIG(0x463770, void, Human_ApplyGravity, (Human *h));
ORIG(0x4637C0, void, Human_SayTimed, (Human *h, const char *sound, int count));
ORIG(0x4638A0, void, Human_Say, (Human *h, const char *sound, int count));
ORIG(0x463AD0, void, Human_Bleed, (Human *h, float *dir, int unk));
ORIG(0x464700, void, HumanView_LockPitch, (void *view));
ORIG(0x464710, void, HumanView_UnlockPitch, (void *view));
ORIG(0x464720, void, HumanView_ClearAngles, (void *view));
ORIG(0x464800, void, HumanView_UpdateView, (void *view, Human *h));
ORIG(0x464BD0, void, HumanView_UpdateBody, (void *view, Human *h));
ORIG(0x475340, void, StationaryGun_Release, (void *obj));
ORIG(0x47E990, uint8_t, FlashBang_GetEvent, (void));
ORIG(0x489C30, void, Human_InitDamage, (Human *h, void *speeds));
ORIG(0x489C60, int, Human_GetHitZone, (Human *h, SoldierHit *hit));
ORIG(0x489E10, HitAnims *, HumanDamage_GetZone, (HumanDamage *d, int zone));
ORIG(0x489E70, void, Human_PlayAnimSlot, (Human *h, int slot, float blend));
ORIG(0x489EA0, void, Human_PlayAnim, (Human *h, int anim, int loop, float blend));
ORIG(0x48A320, void, Human_EnableLook, (Human *h));
ORIG(0x48A330, void, Human_DisableLook, (Human *h));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3320, char, Vec3d_IsZero, (const Vec3d *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (float *m, float a));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (void *a, float *delta, void *anim));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, CollHit *results, Vec3d *starts, Vec3d *ends, int count, char event,
                                  void *query, unsigned int flags));
ORIG(0x4D61D0, void, Anim_Play, (void *anim, int track, int id, int flag, float blend));
ORIG(0x4D6FC0, void, Anim_GetPosKey, (float *out, int track, void *anim, int id, int frame));
ORIG(0x4D7020, int, Anim_GetPosKeyCount, (int model, int id));
ORIG(0x4D74D0, long double, Anim_GetLengthSeconds, (int model, int id));
ORIG(0x4D74F0, int, Anim_Exists, (void *anim, int track, int id));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, double *pos));
ORIG(0x4EC190, void, StateMachine_Init, (HumanSM *sm, HumanStateFn input, HumanStateFn *states, int state,
                                         int combo_count, void *combos));
ORIG(0x4EC1E0, void, StateMachine_Update, (HumanSM *sm, Human *h));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *table, int extra));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F2060, void *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4F2070, void, WeaponSlots_Set, (Task **slots, int i, void *weapon));
ORIG(0x4F2290, void, Weapon_DeleteAll, (Human *h));
ORIG(0x4F2350, void, Weapon_UseAmmo, (void *weapon, char unlimited));
ORIG(0x4F2390, void, Weapon_Drop, (void *weapon));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));

/* handlers of the other soldier types this one shares */
#define Soldier_DeleteHandler (FN(0x459EC0, Soldier_DeleteHandler))
#define SoldierRPG_ParamsChanged (FN(0x456E50, SoldierRPG_ParamsChanged))
#define SoldierPistol_BlastHandler (FN(0x45A3D0, SoldierPistol_BlastHandler))
#define SoldierPistol_PunchHandler (FN(0x45A5E0, SoldierPistol_PunchHandler))
#define Human_Handler4D3710 (FN(0x4D3710, Task_Nothing))

extern void *gSoldierParams;
extern uint16_t gSoldierType;
extern int gSoldierUnusedInt;
extern HumanStateFn gSoldierStates[];
extern char gSoldierCombos[];
extern char gSoldierAnimSlots[];
extern char gSoldierAnimSlotsCrouch[];
extern char gSoldierAnimSlots5[];
extern char gSoldierAnimSlots6[];
extern char gSoldierSpeeds[];

/* states entered from code in this file whose handlers live elsewhere */
#define Soldier_CrouchRun ((HumanStateFn)FN(0x458BF0, SoldierPistol_State19Update))
#define Soldier_CrouchFireRun ((HumanStateFn)FN(0x458EB0, SoldierPistol_State22Update))
#define Soldier_ReloadRun ((HumanStateFn)FN(0x459180, SoldierPistol_State28Update))
#define Soldier_HitRun ((HumanStateFn)FN(0x4592F0, SoldierPistol_HitUpdate))

static const double kUseOffset = 2867.2;
static const float kPi = 0x1.921fb6p+1f;
static const double kZero = 0.0;
static const float kUnitsToMetersF = 0x1p-12f;
static const double kUnitsToMeters = 0x1p-12;
static const float kFps = 30.0f;
static const float kRadToDeg = 0x1.ca5dc0p+5f;
static const float kHitLimit = 0x1.51eb86p-2f;
static const float kSlideReach = 4096.0f;
static const float kSlideLow = -3276.8f;
static const float kSlideStep = 0x1.99999ap+10f;
static const float kSlideFan = 22.0f;
static const float kDegToRad = 0x1.1df46ap-6f;
static const double kOne = 1.0;
static const int kDeathAnims[2] = {0x17, 0x16};

enum {
    S_STAND = 0,
    S_CROUCH = 19,
    S_CROUCH_FIRE = 22,
    S_AIM = 26,
    S_FIRE = 27,
    S_FALL = 29,
    S_HIT = 30,
    S_AIM_HIT = 31,
    S_DIE = 32,
    S_CROUCH_DIE = 33,
    S_AIM_DIE = 34,
    S_RISE_AIM = 25,
    S_CROUCH_AIM = 24,
    S_LOWER = 16,
    S_RELOAD = 28,
};

/* fcomp/fnstsw/test ah,0x40: equal, or unordered */
static int is_zero(long double x)
{
    return !(x < 0 || x > 0);
}

/* fistp with the current rounding mode */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

static void sm_go(HumanSM *sm, Human *h, int state)
{
    sm->prev = sm->state;
    sm->state = state;
    sm->run = sm->states[state];
    sm->run(sm, h);
}

/* falling, hit and dead come first in most states; hit and die vary */
static int sm_common(HumanSM *sm, Human *h, uint32_t bits, int hit, int die)
{
    if (!(bits & SB_ON_GROUND)) {
        sm_go(sm, h, S_FALL);
        return 1;
    }
    if (hit && (bits & SB_HIT)) {
        sm_go(sm, h, hit);
        return 1;
    }
    if (bits & SB_DEAD) {
        sm_go(sm, h, die);
        return 1;
    }
    return 0;
}

static void enter_anim(HumanSM *sm, Human *h, int crouch, HumanStateFn run)
{
    h->crouch = crouch;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = run;
}

void Soldier_ToCrouchRun(HumanSM *sm, Human *h);
void Soldier_StandUpRun(HumanSM *sm, Human *h);
void Soldier_RaiseRun(HumanSM *sm, Human *h);
void Soldier_LowerRun(HumanSM *sm, Human *h);
void Soldier_CrouchReloadRun(HumanSM *sm, Human *h);
void Soldier_CrouchAimRun(HumanSM *sm, Human *h);
void Soldier_RiseAimRun(HumanSM *sm, Human *h);
void Soldier_AimRun(HumanSM *sm, Human *h);
void Soldier_FireRun(HumanSM *sm, Human *h);
void Soldier_FallRun(HumanSM *sm, Human *h);
void Soldier_AimHitRun(HumanSM *sm, Human *h);
void Soldier_DieRun(HumanSM *sm, Human *h);
void Human_DeadRun(HumanSM *sm, Human *h);
void Soldier_AnimRun(HumanSM *sm, Human *h);
void Soldier_MountRun(HumanSM *sm, Human *h);
void Human_UseRun(HumanSM *sm, Human *h);
char Soldier_TurnToHit(Human *h, double *dir);
void Soldier_GetDeathVelocity(double *out, Human *h, int model, int anim);

void Soldier_CrouchDownEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 0, Soldier_ToCrouchRun);
}
HOOK(0x45B8D0, Soldier_CrouchDownEnter);

/* the short moves that end crouched */
void Soldier_ToCrouchRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, 0, S_CROUCH_DIE))
        return;
    if (bits & SB_ANIM_DONE)
        sm_go(sm, h, S_CROUCH);
}
HOOK(0x45B900, Soldier_ToCrouchRun);

void Soldier_StandUpEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 0, Soldier_StandUpRun);
}
HOOK(0x45B990, Soldier_StandUpEnter);

void Soldier_StandUpRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, 0, S_CROUCH_DIE))
        return;
    if (bits & SB_ANIM_DONE)
        sm_go(sm, h, S_STAND);
}
HOOK(0x45B9C0, Soldier_StandUpRun);

void Soldier_RaiseEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 0, Soldier_RaiseRun);
}
HOOK(0x45BA50, Soldier_RaiseEnter);

void Soldier_RaiseRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, 0, S_AIM_DIE))
        return;
    if (bits & SB_ANIM_DONE)
        sm_go(sm, h, S_AIM);
}
HOOK(0x45BA80, Soldier_RaiseRun);

void Soldier_LowerEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 0, Soldier_LowerRun);
}
HOOK(0x45BB10, Soldier_LowerEnter);

void Soldier_LowerRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, 0, S_AIM_DIE))
        return;
    if (bits & SB_ANIM_DONE)
        sm_go(sm, h, S_STAND);
}
HOOK(0x45BB40, Soldier_LowerRun);

void Soldier_CrouchEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 1, Soldier_CrouchRun);
}
HOOK(0x45BBD0, Soldier_CrouchEnter);

void Soldier_CrouchLeftEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 1, Soldier_ToCrouchRun);
}
HOOK(0x45BC00, Soldier_CrouchLeftEnter);

void Soldier_CrouchRightEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 1, Soldier_ToCrouchRun);
}
HOOK(0x45BC30, Soldier_CrouchRightEnter);

void Soldier_CrouchFireEnter(HumanSM *sm, Human *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = Soldier_CrouchFireRun;
}
HOOK(0x45BC60, Soldier_CrouchFireEnter);

void Soldier_CrouchReloadEnter(HumanSM *sm, Human *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = Soldier_CrouchReloadRun;
}
HOOK(0x45BC90, Soldier_CrouchReloadEnter);

void Soldier_CrouchReloadRun(HumanSM *sm, Human *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, S_HIT, S_CROUCH_DIE))
        return;
    if (bits & SB_ANIM_DONE) {
        Weapon_UseAmmo(w, 1);
        sm_go(sm, h, S_CROUCH);
    }
}
HOOK(0x45BCC0, Soldier_CrouchReloadRun);

void Soldier_CrouchAimEnter(HumanSM *sm, Human *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = Soldier_CrouchAimRun;
}
HOOK(0x45BD90, Soldier_CrouchAimEnter);

void Soldier_CrouchAimRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, S_HIT, S_CROUCH_DIE))
        return;
    if (!(bits & SB_CROUCH_AIM))
        sm_go(sm, h, S_CROUCH);
    else if ((bits & SB_ANIM_DONE) && (bits & SB_FIRE))
        sm_go(sm, h, S_CROUCH_FIRE);
}
HOOK(0x45BDC0, Soldier_CrouchAimRun);

void Soldier_RiseAimEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 1, Soldier_RiseAimRun);
}
HOOK(0x45BEA0, Soldier_RiseAimEnter);

void Soldier_RiseAimRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, 0, S_CROUCH_DIE))
        return;
    if (bits & SB_ANIM_DONE)
        sm_go(sm, h, S_AIM);
}
HOOK(0x45BED0, Soldier_RiseAimRun);

void Soldier_AimEnter(HumanSM *sm, Human *h)
{
    enter_anim(sm, h, 0, Soldier_AimRun);
}
HOOK(0x45BF60, Soldier_AimEnter);

void Soldier_AimRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, S_AIM_HIT, S_AIM_DIE))
        return;
    if (!(bits & SB_AIM))
        sm_go(sm, h, S_LOWER);
    else if (bits & SB_FIRE)
        sm_go(sm, h, S_FIRE);
    else if (bits & 0x800)
        sm_go(sm, h, S_RELOAD);
}
HOOK(0x45BF90, Soldier_AimRun);

void Soldier_FireEnter(HumanSM *sm, Human *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = Soldier_FireRun;
}
HOOK(0x45C090, Soldier_FireEnter);

void Soldier_FireRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, S_AIM_HIT, S_AIM_DIE))
        return;
    if (!is_zero(h->input[6]))
        h->fire = 1;
    if ((bits & SB_ANIM_DONE) && !(bits & SB_FIRE))
        sm_go(sm, h, S_AIM);
}
HOOK(0x45C0C0, Soldier_FireRun);

void Soldier_ReloadEnter(HumanSM *sm, Human *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = Soldier_ReloadRun;
}
HOOK(0x45C190, Soldier_ReloadEnter);

void Soldier_FallEnter(HumanSM *sm, Human *h)
{
    h->crouch = 0;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    memcpy(h->vel, h->outVel, sizeof(h->vel));
    sm->run = Soldier_FallRun;
}
HOOK(0x45C1C0, Soldier_FallEnter);

void Soldier_FallRun(HumanSM *sm, Human *h)
{
    uint32_t bits;

    Human_ApplyGravity(h);
    bits = sm->bits;
    if (bits & SB_HIT) {
        sm_go(sm, h, S_HIT);
        return;
    }
    if (bits & SB_DEAD) {
        sm_go(sm, h, S_DIE);
        return;
    }
    if (!(bits & SB_ON_GROUND))
        return;
    if (h->ground.material) {
        void *s = Sound_Play(Level_GetDynamic(), h->ground.material->landSound);

        if (s)
            Sound_SetPos(s, &h->ground.pos.x);
    }
    memset(h->vel, 0, sizeof(h->vel));
    sm_go(sm, h, S_STAND);
}
HOOK(0x45C210, Soldier_FallRun);

void Soldier_HitEnter(HumanSM *sm, Human *h)
{
    HitAnims *a;

    h->crouch = 0;
    h->unk696 = 1;
    a = HumanDamage_GetZone(&h->damage, h->damage.hitZone);
    Anim_Play(h->anim, 0, a->hit, 0, 0.0f);
    Human_DisableLook(h);
    sm->run = Soldier_HitRun;
}
HOOK(0x45C2E0, Soldier_HitEnter);

void Soldier_AimHitEnter(HumanSM *sm, Human *h)
{
    h->unk696 = 1;
    h->crouch = 0;
    ((HumanSoldier *)h)->hitFrames = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    Human_DisableLook(h);
    sm->run = Soldier_AimHitRun;
}
HOOK(0x45C330, Soldier_AimHitEnter);

/* a new hit restarts the animation, but only after 20 frames */
void Soldier_AimHitRun(HumanSM *sm, Human *h)
{
    HumanSoldier *s = (HumanSoldier *)h;
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, 0, S_AIM_DIE))
        return;
    if (s->hitFrames < 20) {
        s->hitFrames++;
    } else if (bits & SB_HIT) {
        sm_go(sm, h, S_AIM_HIT);
        return;
    }
    if (bits & SB_ANIM_DONE) {
        Human_EnableLook(h);
        sm_go(sm, h, S_AIM);
    }
}
HOOK(0x45C370, Soldier_AimHitRun);

static void die_start(Human *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);

    if (w) {
        Weapon_Drop(w);
        WeaponSlots_Set(h->weapons, 0, NULL);
    }
    Weapon_DeleteAll(h);
    h->crouch = 0;
    h->unk696 = 1;
}

static void die_end(HumanSM *sm, Human *h, int anim)
{
    Anim_Play(h->anim, 0, anim, 0, 0.0f);
    Soldier_GetDeathVelocity(((HumanSoldier *)h)->deathVel, h, ((HumanSoldier *)h)->s.model, anim);
    HumanViewCam(h, 3);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_Say(h, "ai_death_", 4);
    Human_DisableLook(h);
    sm->run = Soldier_DieRun;
}

/* death after a hit or a fall: the AI's own animation, a fall towards or
   away from the shot, or a random one */
void Soldier_DieEnter(HumanSM *sm, Human *h)
{
    HumanSoldier *s = (HumanSoldier *)h;
    HumanAI *ai = h->ai;
    HitAnims *a;
    int zone, anim = -1, r;

    die_start(h);
    zone = h->damage.hitZone;
    a = HumanDamage_GetZone(&h->damage, zone);
    if (ai && ai->deathAnim != -1) {
        anim = ai->deathAnim;
    } else {
        r = Random_Int(Game_GetRandom(), 2);
        if (s->deathKind >= 0) {
            if (s->deathKind <= 1)
                anim = Soldier_TurnToHit(h, s->deathDir) ? 0x1C : 0x47;
            else if (s->deathKind == 2)
                anim = zone == 15 || zone == 16 ? a->death[r] : kDeathAnims[r];
        }
    }
    die_end(sm, h, anim);
}
HOOK(0x45C440, Soldier_DieEnter);

void Soldier_CrouchDieEnter(HumanSM *sm, Human *h)
{
    die_start(h);
    die_end(sm, h, 0x18);
}
HOOK(0x45C5A0, Soldier_CrouchDieEnter);

void Soldier_AimDieEnter(HumanSM *sm, Human *h)
{
    die_start(h);
    die_end(sm, h, 0x19);
}
HOOK(0x45C650, Soldier_AimDieEnter);

void Soldier_DieRun(HumanSM *sm, Human *h)
{
    HumanSoldier *s = (HumanSoldier *)h;

    h->pos[0] = s->deathVel[0] + h->pos[0];
    h->pos[1] = s->deathVel[1] + h->pos[1];
    h->pos[2] = s->deathVel[2] + h->pos[2];
    if (sm->bits & SB_ANIM_DONE)
        sm->run = Human_DeadRun;
}
HOOK(0x45C700, Soldier_DieRun);

void Human_DeadRun(HumanSM *sm, Human *h)
{
    (void)sm;
    h->deadState = 1;
}
HOOK(0x45C740, Human_DeadRun);

/* plays the animation the script asked for */
void Soldier_AnimEnter(HumanSM *sm, Human *h)
{
    HumanSoldier *s = (HumanSoldier *)h;

    h->unk696 = 0;
    if (s->scriptAnim == -1) {
        Human_PlayAnimSlot(h, 2, 0.0f);
    } else if (Anim_Exists(h->anim, 0, s->scriptAnim)) {
        Human_PlayAnim(h, s->scriptAnim, 0, 0.0f);
    } else {
        Warning_Show("Animation #%d not found", s->scriptAnim);
        Human_PlayAnimSlot(h, 2, 0.0f);
    }
    s->scriptAnim = 0;
    Human_DisableLook(h);
    sm->run = Soldier_AnimRun;
}
HOOK(0x45C750, Soldier_AnimEnter);

void Soldier_AnimRun(HumanSM *sm, Human *h)
{
    uint32_t bits = sm->bits;

    if (sm_common(sm, h, bits, S_HIT, S_DIE))
        return;
    if ((bits & 0xFD81FE1) || ((HumanSoldier *)h)->scriptAnim == -1) {
        Human_EnableLook(h);
        sm_go(sm, h, S_STAND);
    }
}
HOOK(0x45C7E0, Soldier_AnimRun);

/* takes position behind a stationary gun */
void Soldier_MountEnter(HumanSM *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3], dy;
    float m[10], ax, ay, az;

    h->unk696 = 0;
    Obj_GetUsePoint(obj, pos, m);
    dy = m[4] * kUseOffset;
    pos[2] = h->pos[2];
    pos[0] = m[1] * kUseOffset + pos[0];
    pos[1] = dy + pos[1];
    Human_SetPosition(h, pos);
    Matrix_GetEuler(m, &ax, &ay, &az);
    az = az + kPi;
    Human_SetYaw(h, az);
    Human_PlayAnim(h, 0x1F, 0, 0.0f);
    HumanView_ClearAngles(h->view);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_SetPlatform(h, obj);
    sm->run = Soldier_MountRun;
}
HOOK(0x45C8B0, Soldier_MountEnter);

/* leaves the gun when the use key is pressed again, or on death */
void Soldier_MountRun(HumanSM *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);

    if ((!is_zero(h->input[24]) && is_zero(h->prevInput[24])) || (sm->bits & SB_DEAD)) {
        HumanView_UnlockPitch(h->view);
        Human_UnfreezeView(h);
        StationaryGun_Release(obj);
        Human_EndUse(h);
        Human_SetClimbObject(h, NULL);
        Human_ClearPlatform(h);
        sm_go(sm, h, S_STAND);
    }
}
HOOK(0x45C9B0, Soldier_MountRun);

void Soldier_UseEnter(HumanSM *sm, Human *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = Human_UseRun;
}
HOOK(0x45CA50, Soldier_UseEnter);

void Human_UseRun(HumanSM *sm, Human *h)
{
    if (sm->bits & SB_ANIM_DONE)
        sm_go(sm, h, S_STAND);
}
HOOK(0x45CA80, Human_UseRun);

void Soldier_CreateHandler(Human *h, void *arg);
void Soldier_RunHandler(Human *h);
void Soldier_GetParams(Human *h, void **out);
void Soldier_InputHandler(Human *h, HumanInputMsg *msg);
void Soldier_HitHandler(Human *h, SoldierHit *hit);
void Soldier_UpdateInput(HumanSM *sm, Human *h);
void Soldier_Update(Human *h);
void Soldier_EnsureWeapon(Human *h);

void Soldier_Open(void)
{
    HumanSoldier tmp;

    gSoldierType = TaskType_New("HumanSoldier", sizeof(HumanSoldier), Human_GetType(), 0);
    TaskType_SetFlags(gSoldierType, 1);
    TaskType_SetHandler(gSoldierType, 0, Soldier_CreateHandler);
    TaskType_SetHandler2(gSoldierType, 1, Soldier_RunHandler);
    TaskType_SetHandler2(gSoldierType, 2, Soldier_DeleteHandler);
    TaskType_SetHandler(gSoldierType, Param_GetTableEvent(), Soldier_GetParams);
    TaskType_SetHandler(gSoldierType, Param_GetChangedEvent(), SoldierRPG_ParamsChanged);
    TaskType_SetHandler(gSoldierType, Vehicle_GetControlEvent(), Soldier_InputHandler);
    TaskType_SetHandler(gSoldierType, Game_GetEvent9(), Soldier_HitHandler);
    TaskType_SetHandler(gSoldierType, Game_GetEvent11(), SoldierPistol_BlastHandler);
    TaskType_SetHandler(gSoldierType, Human_GetPunchEvent(), SoldierPistol_PunchHandler);
    TaskType_SetHandler(gSoldierType, FlashBang_GetEvent(), Human_Handler4D3710);
    gSoldierParams = ParamTable_Extend(Human_GetParamTable(), 2);
    ParamTable_Add(gSoldierParams, &tmp, &tmp.s.model, Param_Int32(), 7, NULL);
    ParamTable_Add(gSoldierParams, &tmp, &tmp.s.idleAnim, Param_Int32(), 7, NULL);
    gSoldierUnusedInt = 0;
}
HOOK(0x45CAB0, Soldier_Open);

void Soldier_CreateHandler(Human *h, void *arg)
{
    HumanSoldier *s = (HumanSoldier *)h;

    memset((char *)s + sizeof(Human), 0, sizeof(HumanSoldier) - sizeof(Human));
    ((TaskHandler)TASK_HANDLER(0, Human_GetType()))(h, arg);
    StateMachine_Init(&h->sm, Soldier_UpdateInput, gSoldierStates, S_STAND, 1, gSoldierCombos);
    h->animSlots = (struct AnimSlot *)gSoldierAnimSlots;
    Human_InitDamage(h, gSoldierSpeeds);
    s->s.idleAnim = -1;
    s->s.run = 1;
    s->s.spawned = 0;
    s->hitSum = 0;
    s->hitLimit = kHitLimit;
    s->s.model = 1;
}
HOOK(0x45CC50, Soldier_CreateHandler);

/* turns the script controls (input floats) into state machine bits */
void Soldier_UpdateInput(HumanSM *sm, Human *h)
{
    HumanSoldier *s = (HumanSoldier *)h;
    uint32_t bits = sm->bits & 0xF003E01C;
    uint32_t flags = h->motion->flags;
    int i;

    if (flags & 0x100)
        bits |= SB_ANIM_DONE;
    if (flags & 0x10000)
        bits |= SB_ANIM_EVENT;
    if (s->s.run)
        bits |= SB_RUN;
    else
        bits &= ~SB_RUN;
    for (i = 2; i < 10; i++)
        if (!is_zero(h->input[i]))
            bits |= 0x20 << (i - 2);
    if (!is_zero(h->input[21]))
        bits |= 0x80000;
    if (!is_zero(h->input[22]))
        bits |= SB_AIM;
    if (!is_zero(h->input[23])) {
        s->scriptAnim = x87_fistp(h->input[23]);
        bits |= 0x200000;
    }
    if (!is_zero(h->input[24])) {
        bits |= 0x400000;
        if (Human_GetState(h) != 0x24) {
            s->useTarget = x87_fistp(h->input[24]);
            SoldierPistol_UseTarget(h);
            h->input[24] = 0.0f;
        }
    }
    if (!is_zero(h->input[25]))
        bits |= 0x800000;
    if (!is_zero(h->input[26]))
        bits |= 0x1000000;
    if (!is_zero(h->input[27]))
        bits |= SB_CROUCH_AIM;
    if (!is_zero(h->input[28])) {
        int id = x87_fistp(h->input[28]);
        void *w = Task_FindWeapon(h, id);

        if (w)
            WeaponSlots_Set(h->weapons, 0, w);
        else
            Warning_Show("#%d is trying to change to invalid weapon #%d", Task_GetId(h), id);
    }
    if (!is_zero(h->input[29]))
        bits |= 0x4000000;
    if (!is_zero(h->input[30]))
        bits |= 0x8000000;
    sm->bits = bits;
}
HOOK(0x45CCF0, Soldier_UpdateInput);

/* picks the animation set for the kind of AI, then runs the Human handler */
void Soldier_RunHandler(Human *h)
{
    switch (h->ai->aiType) {
    case 5:
        h->animSlots = (struct AnimSlot *)gSoldierAnimSlots5;
        break;
    case 6:
        h->animSlots = (struct AnimSlot *)gSoldierAnimSlots6;
        break;
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
        h->animSlots = (struct AnimSlot *)gSoldierAnimSlotsCrouch;
        break;
    default:
        h->animSlots = (struct AnimSlot *)gSoldierAnimSlots;
        break;
    }
    ((TaskHandler1)TASK_HANDLER(1, Human_GetType()))(h);
}
HOOK(0x45CF90, Soldier_RunHandler);

void Soldier_GetParams(Human *h, void **out)
{
    (void)h;
    *out = gSoldierParams;
}
HOOK(0x45D030, Soldier_GetParams);

void Soldier_InputHandler(Human *h, HumanInputMsg *msg)
{
    if (!h->inputLocked) {
        memcpy(h->prevInput, h->input, sizeof(h->input));
        memcpy(h->input, msg->input, sizeof(h->input));
    }
    Soldier_Update(h);
}
HOOK(0x45D040, Soldier_InputHandler);

void Soldier_Update(Human *h)
{
    float *d = h->delta, x, y, z;
    long double speed;

    Soldier_EnsureWeapon(h);
    Human_UpdatePlatformMove(h);
    StateMachine_Update(&h->sm, h);
    if (!is_zero(h->input[20]))
        ((HumanSoldier *)h)->s.run ^= 1;
    h->sm.bits &= ~SB_HIT;
    Human_UpdateBody(h);
    HumanView_UpdateView(h->view, h);
    h->frozen = 1;
    BoneObj_GetRootMotion(h->boneOffset, d, h->anim);
    x = h->matrix.m[2] * d[2] + h->matrix.m[1] * d[1] + h->matrix.m[0] * d[0];
    y = h->matrix.m[5] * d[2] + h->matrix.m[4] * d[1] + h->matrix.m[3] * d[0];
    z = h->matrix.m[8] * d[2] + h->matrix.m[7] * d[1] + h->matrix.m[6] * d[0];
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
    Soldier_Footstep(h);
    speed = h->vel[2];
    if (!(h->vel[2] >= 0))
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
    Human_printf(h, "Ground angle: %.3f\n", (double)(h->slopeAngle * kRadToDeg));
    Human_printf(h, "Delta translation X: %.2f\n", (double)h->delta[0]);
    Human_printf(h, "Delta translation Y: %.2f\n", (double)h->delta[1]);
    Human_printf(h, "Delta translation Z: %.2f\n", (double)h->delta[2]);
    if (h->sm.bits & SB_ON_GROUND)
        Human_printf(h, "Ground!\n");
}
HOOK(0x45D080, Soldier_Update);

/* without a weapon in hand, takes the first one carried, counting up from
   id 1 and wrapping */
void Soldier_EnsureWeapon(Human *h)
{
    int id, n, i;
    void *w;

    if (WeaponSlots_Get(h->weapons, 0))
        return;
    id = 0;
    n = WeaponType_GetMaxId() + 1;
    for (i = n; i != 0; i--) {
        id++;
        if (id >= n)
            id = 0;
        w = Task_FindWeapon(h, id);
        if (w)
            WeaponSlots_Set(h->weapons, 0, w);
    }
}
HOOK(0x45D360, Soldier_EnsureWeapon);

void Soldier_HitHandler(Human *h, SoldierHit *hit)
{
    HumanSoldier *s = (HumanSoldier *)h;
    HumanDamage *d = &h->damage;
    double scratch[3];
    SoldierAIMsg msg;
    char headshot = 0;
    float dmg;
    long double sum;
    uint32_t bits;
    int part;

    if (!h->ai || (headshot = h->ai->noInstantDeath, !h->ai->invulnerable)) {
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
        sum = dmg + (long double)s->hitSum;
        bits = h->sm.bits;
        s->hitSum = sum;
        if (!(d->total >= d->max)) {
            if (sum > s->hitLimit) {
                s->hitSum = 0;
                bits |= SB_HIT;
            }
            Human_SayTimed(h, "ai_hit_", 3);
            if (hit->weapon[0x610 / 4] == 3)
                Sound_Play(h, "stab_01");
        } else {
            s->deathDir[0] = hit->dir[0];
            s->deathDir[1] = hit->dir[1];
            s->deathDir[2] = hit->dir[2];
            s->deathKind = hit->weapon[0x610 / 4] == 2 ? 1 : 2;
            h->dead = 1;
            bits |= SB_DEAD;
        }
        h->sm.bits = bits;
    }
    msg.type = 0xF;
    msg.unk4 = 1;
    msg.unk8 = 0;
    msg.data = scratch;
    msg.attacker = hit->attacker;
    msg.attacker2 = hit->attacker;
    HumanAI_Send(h->ai, &msg);
}
HOOK(0x45D3C0, Soldier_HitHandler);

void Soldier_Close(void)
{
    QTaskList_Free(gSoldierParams);
    TaskType_Free(gSoldierType);
}
HOOK(0x45D580, Soldier_Close);

uint16_t Soldier_GetType(void)
{
    return gSoldierType;
}
HOOK(0x45D5A0, Soldier_GetType);

/* turns the soldier to face along dir (or away from it when dir points
   backwards); returns 1 when facing away or dir is zero */
char Soldier_TurnToHit(Human *h, double *dir)
{
    Vec3d v;

    if (Vec3d_IsZero((Vec3d *)dir))
        return 1;
    memcpy(&v, dir, sizeof(v));
    Vec3d_Normalize(&v);
    if (v.z * h->matrix.m[7] + v.y * h->matrix.m[4] + v.x * h->matrix.m[1] > kZero) {
        h->baseZ = Vec3d_YawAngle(v);
        return 0;
    }
    h->baseZ = Vec3d_YawAngle(v) + kPi;
    return 1;
}
HOOK(0x45DAA0, Soldier_TurnToHit);

/* How far a dying soldier slides each frame: the animation's root motion
   turned to his heading, cut short when the traces in that direction hit
   something close. Zero when nothing is hit. */
void Soldier_GetDeathVelocity(double *out, Human *h, int model, int anim)
{
    float p0[3], p1[3], m0[10], m1[10], dy, nx, ny, zoff, reach, frames;
    long double dx, dz, len, k, nz, X, dist, f;
    double dir[3], rd[3], Y, Z;
    Vec3d to[9], from[9], v;
    CollHit hits[9];
    CollIgnore ignore;
    int best = -1, i, j, row;
    char event;

    Anim_GetPosKey(p0, 0, h->anim, anim, 0);
    p0[2] = 0;
    Anim_GetPosKey(p1, 0, h->anim, anim, Anim_GetPosKeyCount(model, anim) - 1);
    dx = (long double)p1[0] - p0[0];
    dy = p1[1] - p0[1];
    dz = -(long double)p0[2];
    len = sqrtl(dz * dz + dx * dx + (long double)dy * dy);
    if (!(len > 0))
        goto none;
    k = (len + kSlideReach) / len;
    nx = k * dx;
    ny = k * dy;
    nz = k * dz;
    reach = sqrtl(nz * nz + (long double)nx * nx + (long double)ny * ny);
    dir[0] = nx;
    dir[1] = ny;
    dir[2] = nz;
    Matrix_SetRotZ(m0, h->baseZ);
    rd[0] = m0[2] * dir[2] + m0[1] * dir[1] + m0[0] * dir[0];
    rd[1] = m0[5] * dir[2] + m0[4] * dir[1] + m0[3] * dir[0];
    rd[2] = m0[8] * dir[2] + m0[7] * dir[1] + m0[6] * dir[0];
    memcpy(dir, rd, sizeof(dir));
    zoff = kSlideLow;
    j = 0;
    for (row = 3; row != 0; row--) {
        for (i = -1; i < 2; i++, j++) {
            Matrix_SetRotZ(m1, i * kSlideFan * kDegToRad);
            X = m1[2] * rd[2] + m1[1] * rd[1] + m1[0] * rd[0];
            memcpy(&from[j], h->pos, sizeof(Vec3d));
            Y = m1[5] * rd[2] + m1[4] * rd[1] + m1[3] * rd[0];
            memcpy(&to[j], h->pos, sizeof(Vec3d));
            Z = m1[8] * rd[2] + m1[7] * rd[1] + m1[6] * rd[0];
            from[j].z = zoff + from[j].z;
            to[j].x = X + to[j].x;
            to[j].y = Y + to[j].y;
            to[j].z = zoff + to[j].z + Z;
        }
        zoff = zoff + kSlideStep;
    }
    ignore.task = (Task *)h;
    event = Game_GetEvent8();
    Coll_Lines(Game_GetWorld(), hits, from, to, 9, event, &ignore, 0xFFFFFFFF);
    for (j = 0; j < 9; j++)
        if (hits[j].found && (best == -1 || !(hits[j].dist >= hits[best].dist)))
            best = j;
    if (best < 0)
        goto none;
    v.x = from[best].x - hits[best].pos.x;
    v.y = from[best].y - hits[best].pos.y;
    v.z = 0;
    frames = Anim_GetLengthSeconds(model, anim) * kFps;
    if (!Vec3d_IsZero(&v)) {
        dist = Vec3d_Length(&v);
        if (dist > reach)
            goto none;
        f = kOne - dist / reach;
        dir[0] = f * rd[0];
        dir[1] = f * dir[1];
        dir[2] = f * dir[2];
    }
    k = kOne / frames;
    out[0] = -(k * dir[0]);
    out[1] = -(k * dir[1]);
    out[2] = -(k * dir[2]);
    return;
none:
    memset(out, 0, 3 * sizeof(double));
}
HOOK(0x45D5B0, Soldier_GetDeathVelocity);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(Soldier_AimDieEnter);
REF_EXTERN(Soldier_AimEnter);
REF_EXTERN(Soldier_AimHitEnter);
REF_EXTERN(Soldier_AnimEnter);
REF_EXTERN(Soldier_CrouchAimEnter);
REF_EXTERN(Soldier_CrouchDieEnter);
REF_EXTERN(Soldier_CrouchDownEnter);
REF_EXTERN(Soldier_CrouchEnter);
REF_EXTERN(Soldier_CrouchFireEnter);
REF_EXTERN(Soldier_CrouchLeftEnter);
REF_EXTERN(Soldier_CrouchReloadEnter);
REF_EXTERN(Soldier_CrouchRightEnter);
REF_EXTERN(Soldier_DieEnter);
REF_EXTERN(Soldier_FallEnter);
REF_EXTERN(Soldier_FireEnter);
REF_EXTERN(Soldier_HitEnter);
REF_EXTERN(Soldier_LowerEnter);
REF_EXTERN(Soldier_MeleeEnter);
REF_EXTERN(Soldier_MountEnter);
REF_EXTERN(Soldier_RaiseEnter);
REF_EXTERN(Soldier_ReloadEnter);
REF_EXTERN(Soldier_RiseAimEnter);
REF_EXTERN(Soldier_StandEnter);
REF_EXTERN(Soldier_StandUpEnter);
REF_EXTERN(Soldier_State10Enter);
REF_EXTERN(Soldier_State11Enter);
REF_EXTERN(Soldier_State12Enter);
REF_EXTERN(Soldier_State17Enter);
REF_EXTERN(Soldier_State18Enter);
REF_EXTERN(Soldier_State1Enter);
REF_EXTERN(Soldier_State2Enter);
REF_EXTERN(Soldier_State3Enter);
REF_EXTERN(Soldier_State4Enter);
REF_EXTERN(Soldier_State5Enter);
REF_EXTERN(Soldier_State6Enter);
REF_EXTERN(Soldier_State7Enter);
REF_EXTERN(Soldier_ThrowEnter);
REF_EXTERN(Soldier_UseEnter);
STORE(igd, 53EC10, 4) __typeof__(gSoldierStates[0]) store_53EC10[38] INIT({(__typeof__(store_53EC10[0]))ref_Soldier_StandEnter, (__typeof__(store_53EC10[1]))ref_Soldier_State1Enter, (__typeof__(store_53EC10[2]))ref_Soldier_State2Enter, (__typeof__(store_53EC10[3]))ref_Soldier_State3Enter, (__typeof__(store_53EC10[4]))ref_Soldier_State4Enter, (__typeof__(store_53EC10[5]))ref_Soldier_State5Enter, (__typeof__(store_53EC10[6]))ref_Soldier_State6Enter, (__typeof__(store_53EC10[7]))ref_Soldier_State7Enter, (__typeof__(store_53EC10[8]))ref_Soldier_ThrowEnter, (__typeof__(store_53EC10[9]))ref_Soldier_MeleeEnter, (__typeof__(store_53EC10[10]))ref_Soldier_State10Enter, (__typeof__(store_53EC10[11]))ref_Soldier_State11Enter, (__typeof__(store_53EC10[12]))ref_Soldier_State12Enter, (__typeof__(store_53EC10[13]))ref_Soldier_CrouchDownEnter, (__typeof__(store_53EC10[14]))ref_Soldier_StandUpEnter, (__typeof__(store_53EC10[15]))ref_Soldier_RaiseEnter, (__typeof__(store_53EC10[16]))ref_Soldier_LowerEnter, (__typeof__(store_53EC10[17]))ref_Soldier_State17Enter, (__typeof__(store_53EC10[18]))ref_Soldier_State18Enter, (__typeof__(store_53EC10[19]))ref_Soldier_CrouchEnter, (__typeof__(store_53EC10[20]))ref_Soldier_CrouchLeftEnter, (__typeof__(store_53EC10[21]))ref_Soldier_CrouchRightEnter, (__typeof__(store_53EC10[22]))ref_Soldier_CrouchFireEnter, (__typeof__(store_53EC10[23]))ref_Soldier_CrouchReloadEnter, (__typeof__(store_53EC10[24]))ref_Soldier_CrouchAimEnter, (__typeof__(store_53EC10[25]))ref_Soldier_RiseAimEnter, (__typeof__(store_53EC10[26]))ref_Soldier_AimEnter, (__typeof__(store_53EC10[27]))ref_Soldier_FireEnter, (__typeof__(store_53EC10[28]))ref_Soldier_ReloadEnter, (__typeof__(store_53EC10[29]))ref_Soldier_FallEnter, (__typeof__(store_53EC10[30]))ref_Soldier_HitEnter, (__typeof__(store_53EC10[31]))ref_Soldier_AimHitEnter, (__typeof__(store_53EC10[32]))ref_Soldier_DieEnter, (__typeof__(store_53EC10[33]))ref_Soldier_CrouchDieEnter, (__typeof__(store_53EC10[34]))ref_Soldier_AimDieEnter, (__typeof__(store_53EC10[35]))ref_Soldier_AnimEnter, (__typeof__(store_53EC10[36]))ref_Soldier_MountEnter, (__typeof__(store_53EC10[37]))ref_Soldier_UseEnter});
ALIAS(gSoldierStates, store_53EC10, 0);
DATA(53EC10) uint8_t dat_53ECA8[48] INIT({1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 2, 0, 0, 1, 0, 0, 0, 4, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 2});
STORE(igd, 53EC10, 1) char store_53ECD8[8] INIT({4, 0, 0, 0, -88, -20, 83});
ALIAS(gSoldierCombos, store_53ECD8, 0);
STORE(igd, 53ED10, 1) char store_53ED10[304] INIT({2, 0, 0, 0, 1, 0, 0, 0, 34, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 57, 0, 0, 0, 1, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 56, 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 58, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 0, 0, 0, 0, 53, 0, 0, 0, 0, 0, 0, 0, 55, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 1, 0, 0, 0, 52, 0, 0, 0, 1, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 1, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 21, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 1, 0, 0, 0, 2});
ALIAS(gSoldierAnimSlots, store_53ED10, 0);
STORE(igd, 53ED10, 1) char store_53EE40[304] INIT({62, 0, 0, 0, 1, 0, 0, 0, 60, 0, 0, 0, 1, 0, 0, 0, 60, 0, 0, 0, 1, 0, 0, 0, 81, 0, 0, 0, 1, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 63, 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 58, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0, 78, 0, 0, 0, 0, 0, 0, 0, 79, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 1, 0, 0, 0, 52, 0, 0, 0, 1, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 1, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 21, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 1, 0, 0, 0, 2});
ALIAS(gSoldierAnimSlotsCrouch, store_53EE40, 0);
STORE(igd, 53ED10, 1) char store_53EF70[304] INIT({64, 0, 0, 0, 1, 0, 0, 0, 65, 0, 0, 0, 1, 0, 0, 0, 65, 0, 0, 0, 1, 0, 0, 0, 66, 0, 0, 0, 1, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 69, 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 58, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 1, 0, 0, 0, 52, 0, 0, 0, 1, 0, 0, 0, 18, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 1, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 21, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 1, 0, 0, 0, 2});
ALIAS(gSoldierAnimSlots5, store_53EF70, 0);
STORE(igd, 53ED10, 1) char store_53F0A0[304] INIT({77, 0, 0, 0, 1, 0, 0, 0, 76, 0, 0, 0, 1, 0, 0, 0, 76, 0, 0, 0, 1, 0, 0, 0, 75, 0, 0, 0, 1, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 69, 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 58, 0, 0, 0, 0, 0, 0, 0, 70, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 68, 0, 0, 0, 0, 0, 0, 0, 93, 0, 0, 0, 0, 0, 0, 0, 95, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 0, 0, 0, 0, 0, 76, 0, 0, 0, 1, 0, 0, 0, 52, 0, 0, 0, 1, 0, 0, 0, 94, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 1, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 21, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 1, 0, 0, 0, 2});
ALIAS(gSoldierAnimSlots6, store_53F0A0, 0);
STORE(igd, 53ED10, 1) char store_53F1D0[324] INIT({0, 0, 0, 63, 44, 0, 0, 0, 23, 0, 0, 0, 23, 0, 0, 0, 0, 0, 64, 63, 44, 0, 0, 0, 23, 0, 0, 0, 23, 0, 0, 0, 0, 0, -128, 63, 44, 0, 0, 0, 23, 0, 0, 0, 23, 0, 0, 0, 0, 0, 0, 63, 47, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, 64, 63, 47, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, -128, 63, 47, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, 0, 63, 45, 0, 0, 0, 23, 0, 0, 0, 23, 0, 0, 0, 0, 0, 64, 63, 45, 0, 0, 0, 23, 0, 0, 0, 23, 0, 0, 0, 0, 0, -128, 63, 45, 0, 0, 0, 23, 0, 0, 0, 23, 0, 0, 0, 0, 0, 0, 63, 46, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, 64, 63, 46, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, -128, 63, 46, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, -128, 63, 27, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, -128, 63, 27, 0, 0, 0, 28, 0, 0, 0, 28, 0, 0, 0, 0, 0, -128, 63, 27, 0, 0, 0, 29, 0, 0, 0, 29, 0, 0, 0, 0, 0, -56, 66, 27, 0, 0, 0, 30, 0, 0, 0, 30, 0, 0, 0, 0, 0, -56, 66, 27, 0, 0, 0, 22, 0, 0, 0, 22, 0, 0, 0, 72, 117, 109, 97, 110, 83, 111, 108, 100, 105, 101, 114, 0, 0, 0, 0, 115, 116, 97, 98, 95, 48, 49, 0, 72, 117, 109, 97, 110, 80, 108, 97, 121, 101, 114, 73, 110, 112, 117, 116, 0, 0, 0, 0, 76, 79, 67, 65, 76, 58});
ALIAS(gSoldierSpeeds, store_53F1D0, 0);
BSS(5BDAB4) void *gSoldierParams;
PAD(igb, 5BDAB4, 5BDAB8, 0x4);
BSS(5BDAB4) uint16_t gSoldierType;
PAD(igb, 5BDAB4, 5BDABE, 0x2);
BSS(5BDAB4) int gSoldierUnusedInt;
