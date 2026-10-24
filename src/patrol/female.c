/* HumanSoldierFemale: footsteps, movement states, task handlers. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "patrol/patrol.h"
#include "loadscreen/loadscreentypes.h"
#include "soldier/soldiertypes.h"

FN_EXTERN(Human_DeadRun);
FN_EXTERN(Human_UseRun);
FN_EXTERN(SoldierFemale_BlastHandler);
FN_EXTERN(SoldierFemale_HitHandler);
FN_EXTERN(SoldierFemale_InputHandler);
FN_EXTERN(SoldierFemale_PunchHandler);
FN_EXTERN(Soldier_DeleteHandler);
FN_EXTERN(Task_Nothing);

typedef struct Soldier Soldier;
typedef struct SoldierSM SoldierSM;
typedef void (*SoldierStateFn)(SoldierSM *sm, Soldier *h);

enum {
    SB_ANIM_DONE = 0x1,
    SB_ANIM_EVENT = 0x2,
    SB_ON_GROUND = 0x4,
    SB_FORWARD = 0x80,
    SB_FIRE = 0x200,
    SB_RELOAD = 0x800,
    SB_HIT = 0x10000,
    SB_DEAD = 0x20000,
    SB_RUN = 0x40000,
    SB_ANIM = 0x80000,
    SB_USE = 0x100000,
    SB_AIM = 0x200000,
};

enum {
    SF_STAND,
    SF_WALK,
    SF_RUN,
    SF_FIRE,
    SF_RELOAD,
    SF_AIM,
    SF_RAISE,
    SF_LOWER,
    SF_FALL,
    SF_HIT,
    SF_DIE,
    SF_ANIM,
    SF_USE,
};

typedef struct SoldierMotion {
    int unk0;
    uint32_t flags;
    char unk8[0x44];
    int unk4C[5];
} SoldierMotion;

typedef struct SoldierAI {
    char unk0[0x3AD0];
    int deathAnim;
} SoldierAI;

#define SPAD(from, to) char pad##from[(to) - (from)]

/* HumanSoldierFemale, 0xD50 bytes */
struct Soldier {
    Task task;
    double pos[3];
    SPAD(0x38, 0x68);
    char anim[8];
    SoldierMotion *motion;
    SPAD(0x74, 0x78);
    float matrix[9];
    SPAD(0x9C, 0xA0);
    float radius;
    SPAD(0xA4, 0xCC);
    int unkCC;
    SPAD(0xD0, 0xE0);
    double cubePos[3];
    SPAD(0xF8, 0x110);
    float prevInput[40];
    float input[40];
    SPAD(0x250, 0x254);
    char damage[0x94];
    int hitZone;
    SoldierSM sm;
    void *animSlots;
    char unk33C;
    char unk33D;
    SPAD(0x33E, 0x4C4);
    void *weapons[3];
    char unk4D0;
    char fire;
    SPAD(0x4D2, 0x4EC);
    void *view;
    SPAD(0x4F0, 0x534);
    int unk534[7];
    SPAD(0x550, 0x650);
    int unk650[5];
    float vel[3];
    SPAD(0x670, 0x688);
    float unk688[3];
    SPAD(0x694, 0x696);
    char unk696;
    SPAD(0x697, 0x6A8);
    double soundPos[3];
    SPAD(0x6C0, 0x6EC);
    char *ground;
    SPAD(0x6F0, 0x7BC);
    int uses;
    SPAD(0x7C0, 0x800);
    SoldierAI *ai;
    SPAD(0x804, 0x84C);
    unsigned footstep;
    SPAD(0x850, 0xCF8);
    int model;
    int idleAnim;
    char run;
    char spawned;
    SPAD(0xD02, 0xD04);
    float hitLimit;
    int unkD08;
    int scriptAnim;
    int useTarget;
    SPAD(0xD14, 0xD38);
    double deathVel[3];
};

#define SCHECK(f, off) _Static_assert(offsetof(Soldier, f) == (off), #f)
SCHECK(motion, 0x70);
SCHECK(cubePos, 0xE0);
SCHECK(input, 0x1B0);
SCHECK(hitZone, 0x2E8);
SCHECK(sm, 0x2EC);
SCHECK(animSlots, 0x338);
SCHECK(unk33D, 0x33D);
SCHECK(weapons, 0x4C4);
SCHECK(fire, 0x4D1);
SCHECK(unk650, 0x650);
SCHECK(soundPos, 0x6A8);
SCHECK(ground, 0x6EC);
SCHECK(ai, 0x800);
SCHECK(footstep, 0x84C);
SCHECK(model, 0xCF8);
SCHECK(spawned, 0xD01);
SCHECK(useTarget, 0xD10);
SCHECK(deathVel, 0xD38);
_Static_assert(sizeof(Soldier) == 0xD50, "Soldier");
#undef SCHECK

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x413C80, void *, Weapon_GetType, (void *weapon));
ORIG(0x413CC0, void *, Task_FindWeapon, (Soldier *h, int id));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416FB0, int, Game_IsRaining, (void));
ORIG(0x444BC0, void, Switch_Press, (void *task));
ORIG(0x447DA0, void, Door_Toggle, (void *task));
ORIG(0x45D5B0, void, Soldier_GetDeathVelocity, (double *out, Soldier *h, int model, int anim));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460C40, uint8_t, Human_GetPunchEvent, (void));
ORIG(0x460C50, void *, Human_GetParamTable, (void));
ORIG(0x463180, char, Human_IsCrouching, (Soldier *h));
ORIG(0x4635F0, void, Human_FreezeView, (Soldier *h));
ORIG(0x4636A0, void, Human_SetClimbObject, (Soldier *h, void *task));
ORIG(0x463760, void, HumanViewCam, (Soldier *h, int mode));
ORIG(0x463770, void, Human_ApplyGravity, (Soldier *h));
ORIG(0x4638A0, void, Human_Say, (Soldier *h, const char *name, int n));
ORIG(0x464700, void, HumanView_LockPitch, (void *view));
ORIG(0x47E990, uint8_t, FlashBang_GetEvent, (void));
ORIG(0x489C30, void, Human_InitDamage, (Soldier *h, void *speeds));
ORIG(0x489E10, HitAnims *, HumanDamage_GetZone, (void *damage, int zone));
ORIG(0x489E70, void, Human_PlayAnimSlot, (Soldier *h, int anim, float blend));
ORIG(0x489EA0, void, Human_PlayAnim, (Soldier *h, int anim, int loop, float blend));
ORIG(0x48A320, void, Human_EnableLook, (Soldier *h));
ORIG(0x48A330, void, Human_DisableLook, (Soldier *h));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, double *pos, float radius, int category));
ORIG(0x4D38C0, void, BoneObj_Update, (float *matrix, void *anim));
ORIG(0x4D61D0, void, Anim_Play, (void *anim, int track, int id, int flag, float blend));
ORIG(0x4D64C0, int, Anim_GetId, (void *anim, int track));
ORIG(0x4D64E0, long double, Anim_GetLength, (void *anim, int track));
ORIG(0x4D6520, long double, Anim_GetPhase, (void *anim, int track));
ORIG(0x4D65B0, void, Anim_SetTime, (void *anim, int track, float time));
ORIG(0x4D74F0, int, Anim_Exists, (void *anim, int track, int id));
ORIG(0x4D9620, void, BoneDynCubeObj_Init, (void *world, Soldier *h, int16_t model, int b));
ORIG(0x4D96F0, void, BoneDynCubeObj_Deinit, (Soldier *h));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, double *pos));
ORIG(0x4E6C60, void, Sound_SetVolume, (void *sound, float unk, float level));
ORIG(0x4E6CA0, void, Sound_SetPitch, (void *sound, float pitch));
ORIG(0x4EC190, void, StateMachine_Init, (SoldierSM *sm, SoldierStateFn input, SoldierStateFn *states, int state,
                                         int combo_count, void *combos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *table, int extra));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));
ORIG(0x4F2060, void *, WeaponSlots_Get, (void **slots, int i));
ORIG(0x4F2070, void, WeaponSlots_Set, (void **slots, int i, void *weapon));
ORIG(0x4F2290, void, Weapon_DeleteAll, (Soldier *h));
ORIG(0x4F2350, void, Weapon_UseAmmo, (void *weapon, char unlimited));
ORIG(0x4F2390, void, Weapon_Drop, (void *weapon));
ORIG(0x4F23D0, void, Weapon_GetAmmo, (void *weapon, int *out));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));

/* handlers that live outside this file */
#define SoldierFemale_DeleteHandler (FN(0x459EC0, Soldier_DeleteHandler))
#define SoldierFemale_InputHandler (FN(0x455070, SoldierFemale_InputHandler))
#define SoldierFemale_Handler4553E0 (FN(0x4553E0, SoldierFemale_HitHandler))
#define SoldierFemale_Handler455570 (FN(0x455570, SoldierFemale_BlastHandler))
#define SoldierFemale_Handler455780 (FN(0x455780, SoldierFemale_PunchHandler))
#define Human_Handler4D3710 (FN(0x4D3710, Task_Nothing))
#define Human_DeadRun ((SoldierStateFn)FN(0x45C740, Human_DeadRun))
#define Human_UseRun ((SoldierStateFn)FN(0x45CA80, Human_UseRun))

extern const int gFemaleFootstepSounds[];
extern SoldierStateFn gSoldierFemaleStates[];
extern char gSoldierFemaleCombos[];
extern char gSoldierFemaleAnimSlots[];
extern char gSoldierFemaleSpeeds[];
extern void *gSoldierFemaleParams;
extern uint16_t gSoldierFemaleType;
extern int gSoldierFemaleUnk5BDA90;

#define ANIM_WALK_CYCLE 0x3F2
#define ANIM_DEFAULT 6

static const float kHalf = 0.5f;
static const float kOne = 1.0f;
static const float kPitchDrop = 0x1.99999ap-3f;
static const float kStepLevel = 0x1.51eb86p-1f;
static const float kHitLimit = 0x1.51eb86p-2f;

/* fcomp/fnstsw/test ah,0x40: equal, or unordered */
static int is_zero(long double x)
{
    return !(x < 0 || x > 0);
}

static void SM_Set(SoldierSM *sm, Soldier *h, int state)
{
    sm->prev = sm->state;
    sm->state = state;
    sm->run = sm->states[state];
    sm->run(sm, h);
}

/* falling, hit and dead come first in every state */
static int SM_Common(SoldierSM *sm, Soldier *h, uint32_t bits)
{
    if (!(bits & SB_ON_GROUND)) {
        SM_Set(sm, h, SF_FALL);
        return 1;
    }
    if (bits & SB_HIT) {
        SM_Set(sm, h, SF_HIT);
        return 1;
    }
    if (bits & SB_DEAD) {
        SM_Set(sm, h, SF_DIE);
        return 1;
    }
    return 0;
}

void SoldierFemale_Footstep(Soldier *h)
{
    char name[128];
    char *ground;
    float pitch;
    void *s;
    int foot;

    if (h->unk534[1] != ANIM_WALK_CYCLE)
        return;
    if (h->unk534[2] != Anim_GetId(h->anim, 0))
        return;
    ground = h->ground;
    foot = gFemaleFootstepSounds[h->footstep];
    h->footstep = (h->footstep + 1) % 12;
    if (!ground)
        return;
    pitch = Random_Float(Game_GetRandom()) * kHalf + kOne - kPitchDrop;
    s = Sound_Play(Level_GetDynamic(), ground + 0x54 + foot * 0x20);
    if (s) {
        if (Human_IsCrouching(h))
            Sound_SetVolume(s, -1.0f, kStepLevel);
        Sound_SetPos(s, h->soundPos);
        Sound_SetPitch(s, pitch);
    }
    if (Game_IsRaining()) {
        crt_sprintf(name, "walk_rain_%1d", foot + 1);
        s = Sound_Play(Level_GetDynamic(), name);
        if (s) {
            Sound_SetPos(s, h->soundPos);
            Sound_SetPitch(s, pitch);
        }
    }
}
HOOK(0x4538F0, SoldierFemale_Footstep);

/* plays the idle animation set by script, or the state's own */
static void SoldierFemale_PlayIdle(SoldierSM *sm, Soldier *h)
{
    if (h->idleAnim == -1) {
        Human_PlayAnimSlot(h, sm->state, 0.0f);
    } else if (Anim_Exists(h->anim, 0, h->idleAnim)) {
        Anim_Play(h->anim, 0, h->idleAnim, 0, 0.0f);
    } else {
        Warning_Show("Animation #%d not found", h->idleAnim);
        Human_PlayAnimSlot(h, sm->state, 0.0f);
    }
}

void SoldierFemale_StandUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_StandEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 0;
    h->unk696 = 0;
    SoldierFemale_PlayIdle(sm, h);
    sm->run = SoldierFemale_StandUpdate;
}
HOOK(0x453A30, SoldierFemale_StandEnter);

/* the checks every moving state starts with */
static int SM_Moving(SoldierSM *sm, Soldier *h, uint32_t bits)
{
    if (SM_Common(sm, h, bits))
        return 1;
    if (bits & SB_AIM) {
        SM_Set(sm, h, SF_RAISE);
        return 1;
    }
    if (bits & SB_RELOAD) {
        SM_Set(sm, h, SF_RELOAD);
        return 1;
    }
    return 0;
}

void SoldierFemale_StandUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (SM_Moving(sm, h, bits))
        return;
    if (bits & SB_ANIM)
        SM_Set(sm, h, SF_ANIM);
    else if (bits & SB_RUN) {
        if (bits & SB_FORWARD)
            SM_Set(sm, h, SF_RUN);
    } else if (bits & SB_FORWARD)
        SM_Set(sm, h, SF_WALK);
}
HOOK(0x453AE0, SoldierFemale_StandUpdate);

/* walk and run keep the animation phase when switching between them */
static void SoldierFemale_MoveEnter(SoldierSM *sm, Soldier *h, int other, SoldierStateFn update)
{
    float phase;

    h->unk33D = 0;
    h->unk696 = 0;
    phase = Anim_GetPhase(h->anim, 0);
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    if (sm->prev == other)
        Anim_SetTime(h->anim, 0, (float)(Anim_GetLength(h->anim, 0) * phase));
    sm->run = update;
}

void SoldierFemale_WalkUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_WalkEnter(SoldierSM *sm, Soldier *h)
{
    SoldierFemale_MoveEnter(sm, h, SF_RUN, SoldierFemale_WalkUpdate);
}
HOOK(0x453C40, SoldierFemale_WalkEnter);

void SoldierFemale_WalkUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (SM_Moving(sm, h, bits))
        return;
    if (bits & SB_RUN)
        SM_Set(sm, h, SF_RUN);
    else if (!(bits & SB_FORWARD))
        SM_Set(sm, h, SF_STAND);
}
HOOK(0x453CB0, SoldierFemale_WalkUpdate);

void SoldierFemale_RunUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_RunEnter(SoldierSM *sm, Soldier *h)
{
    SoldierFemale_MoveEnter(sm, h, SF_WALK, SoldierFemale_RunUpdate);
}
HOOK(0x453DE0, SoldierFemale_RunEnter);

void SoldierFemale_RunUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (SM_Moving(sm, h, bits))
        return;
    if (!(bits & SB_RUN))
        SM_Set(sm, h, SF_WALK);
    else if (!(bits & SB_FORWARD))
        SM_Set(sm, h, SF_STAND);
}
HOOK(0x453E50, SoldierFemale_RunUpdate);

static void SoldierFemale_EnterAnim(SoldierSM *sm, Soldier *h, SoldierStateFn update)
{
    h->unk33D = 0;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = update;
}

void SoldierFemale_FireUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_FireEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 1;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = SoldierFemale_FireUpdate;
}
HOOK(0x453F80, SoldierFemale_FireEnter);

void SoldierFemale_FireUpdate(SoldierSM *sm, Soldier *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    int ammo[2];
    uint32_t bits;

    Weapon_GetType(w);
    Weapon_GetAmmo(w, ammo);
    bits = sm->bits;
    if (SM_Common(sm, h, bits))
        return;
    if (!is_zero(h->input[6]))
        h->fire = 1;
    if ((bits & SB_ANIM_DONE) && !(bits & SB_FIRE))
        SM_Set(sm, h, (bits & SB_AIM) ? SF_AIM : SF_LOWER);
}
HOOK(0x453FB0, SoldierFemale_FireUpdate);

void SoldierFemale_ReloadUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_ReloadEnter(SoldierSM *sm, Soldier *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = SoldierFemale_ReloadUpdate;
}
HOOK(0x4540D0, SoldierFemale_ReloadEnter);

void SoldierFemale_ReloadUpdate(SoldierSM *sm, Soldier *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    uint32_t bits = sm->bits;

    if (SM_Common(sm, h, bits))
        return;
    if (bits & SB_ANIM_DONE) {
        Weapon_UseAmmo(w, 1);
        SM_Set(sm, h, (bits & SB_AIM) ? SF_AIM : SF_STAND);
    }
}
HOOK(0x454100, SoldierFemale_ReloadUpdate);

void SoldierFemale_AimUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_AimEnter(SoldierSM *sm, Soldier *h)
{
    SoldierFemale_EnterAnim(sm, h, SoldierFemale_AimUpdate);
}
HOOK(0x4541F0, SoldierFemale_AimEnter);

void SoldierFemale_AimUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (SM_Common(sm, h, bits))
        return;
    if (!(bits & SB_AIM))
        SM_Set(sm, h, SF_LOWER);
    else if (bits & SB_FIRE)
        SM_Set(sm, h, SF_FIRE);
}
HOOK(0x454220, SoldierFemale_AimUpdate);

void SoldierFemale_RaiseUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_RaiseEnter(SoldierSM *sm, Soldier *h)
{
    SoldierFemale_EnterAnim(sm, h, SoldierFemale_RaiseUpdate);
}
HOOK(0x454300, SoldierFemale_RaiseEnter);

void SoldierFemale_RaiseUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (SM_Common(sm, h, bits))
        return;
    if (bits & SB_ANIM_DONE)
        SM_Set(sm, h, SF_AIM);
}
HOOK(0x454330, SoldierFemale_RaiseUpdate);

void SoldierFemale_LowerUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_LowerEnter(SoldierSM *sm, Soldier *h)
{
    SoldierFemale_EnterAnim(sm, h, SoldierFemale_LowerUpdate);
}
HOOK(0x4543E0, SoldierFemale_LowerEnter);

void SoldierFemale_LowerUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (SM_Common(sm, h, bits))
        return;
    if (bits & SB_ANIM_DONE)
        SM_Set(sm, h, SF_STAND);
}
HOOK(0x454410, SoldierFemale_LowerUpdate);

void SoldierFemale_FallUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_FallEnter(SoldierSM *sm, Soldier *h)
{
    h->unk33D = 0;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    memcpy(h->vel, h->unk688, sizeof(h->vel));
    sm->run = SoldierFemale_FallUpdate;
}
HOOK(0x4544C0, SoldierFemale_FallEnter);

void SoldierFemale_FallUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits;

    Human_ApplyGravity(h);
    bits = sm->bits;
    if (bits & SB_HIT) {
        SM_Set(sm, h, SF_HIT);
        return;
    }
    if (bits & SB_DEAD) {
        SM_Set(sm, h, SF_DIE);
        return;
    }
    if (!(bits & SB_ON_GROUND))
        return;
    if (h->ground) {
        void *s = Sound_Play(Level_GetDynamic(), h->ground + 0x114);

        if (s)
            Sound_SetPos(s, h->soundPos);
    }
    memset(h->vel, 0, sizeof(h->vel));
    SM_Set(sm, h, SF_STAND);
}
HOOK(0x454510, SoldierFemale_FallUpdate);

void SoldierFemale_HitUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_HitEnter(SoldierSM *sm, Soldier *h)
{
    HitAnims *a;

    h->unk33D = 0;
    h->unk696 = 1;
    a = HumanDamage_GetZone(h->damage, h->hitZone);
    Anim_Play(h->anim, 0, a->hit, 0, 0.0f);
    Human_DisableLook(h);
    sm->run = SoldierFemale_HitUpdate;
}
HOOK(0x4545E0, SoldierFemale_HitEnter);

void SoldierFemale_HitUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (!(bits & SB_ON_GROUND)) {
        SM_Set(sm, h, SF_FALL);
        return;
    }
    if (bits & SB_DEAD) {
        SM_Set(sm, h, SF_DIE);
        return;
    }
    if (bits & SB_ANIM_DONE) {
        Human_EnableLook(h);
        SM_Set(sm, h, SF_STAND);
    }
}
HOOK(0x454630, SoldierFemale_HitUpdate);

void SoldierFemale_DieUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_DieEnter(SoldierSM *sm, Soldier *h)
{
    SoldierAI *ai = h->ai;
    void *w = WeaponSlots_Get(h->weapons, 0);
    HitAnims *a;
    int anim;

    if (w) {
        Weapon_Drop(w);
        WeaponSlots_Set(h->weapons, 0, NULL);
    }
    Weapon_DeleteAll(h);
    h->unk33D = 0;
    h->unk696 = 1;
    a = HumanDamage_GetZone(h->damage, h->hitZone);
    if (ai && ai->deathAnim != -1)
        anim = ai->deathAnim;
    else
        anim = a->death[0];
    Anim_Play(h->anim, 0, anim, 0, 0.0f);
    Soldier_GetDeathVelocity(h->deathVel, h, h->model, anim);
    HumanViewCam(h, 3);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_Say(h, "ai_death_", 4);
    Human_DisableLook(h);
    sm->run = SoldierFemale_DieUpdate;
}
HOOK(0x4546C0, SoldierFemale_DieEnter);

void SoldierFemale_DieUpdate(SoldierSM *sm, Soldier *h)
{
    h->pos[0] = h->deathVel[0] + h->pos[0];
    h->pos[1] = h->deathVel[1] + h->pos[1];
    h->pos[2] = h->deathVel[2] + h->pos[2];
    if (sm->bits & SB_ANIM_DONE)
        sm->run = Human_DeadRun;
}
HOOK(0x4547B0, SoldierFemale_DieUpdate);

void SoldierFemale_AnimUpdate(SoldierSM *sm, Soldier *h);

void SoldierFemale_AnimEnter(SoldierSM *sm, Soldier *h)
{
    h->unk696 = 0;
    if (h->scriptAnim == -1) {
        Human_PlayAnimSlot(h, ANIM_DEFAULT, 0.0f);
    } else if (Anim_Exists(h->anim, 0, h->scriptAnim)) {
        Human_PlayAnim(h, h->scriptAnim, 1, 0.0f);
    } else {
        Warning_Show("Animation #%d not found", h->scriptAnim);
        Human_PlayAnimSlot(h, ANIM_DEFAULT, 0.0f);
    }
    h->scriptAnim = 0;
    Human_DisableLook(h);
    sm->run = SoldierFemale_AnimUpdate;
}
HOOK(0x4547F0, SoldierFemale_AnimEnter);

void SoldierFemale_AnimUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits;

    if (SM_Common(sm, h, bits))
        return;
    if ((bits & 0x301FE1) || h->scriptAnim == -1) {
        Human_EnableLook(h);
        SM_Set(sm, h, SF_STAND);
    }
}
HOOK(0x454880, SoldierFemale_AnimUpdate);

void SoldierFemale_UseEnter(SoldierSM *sm, Soldier *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    sm->run = Human_UseRun;
}
HOOK(0x454940, SoldierFemale_UseEnter);

void SoldierFemale_Create(Soldier *h, void *arg);
void Human_RunBase(void *task);
void SoldierFemale_GetParams(Soldier *h, void **out);
void SoldierFemale_Changed(Soldier *h, void *arg);

void SoldierFemale_Open(void)
{
    Soldier tmp;

    gSoldierFemaleType = TaskType_New("HumanSoldierFemale", sizeof(Soldier), Human_GetType(), 0);
    TaskType_SetFlags(gSoldierFemaleType, 1);
    TaskType_SetHandler(gSoldierFemaleType, 0, SoldierFemale_Create);
    TaskType_SetHandler2(gSoldierFemaleType, 1, Human_RunBase);
    TaskType_SetHandler2(gSoldierFemaleType, 2, SoldierFemale_DeleteHandler);
    TaskType_SetHandler(gSoldierFemaleType, Param_GetTableEvent(), SoldierFemale_GetParams);
    TaskType_SetHandler(gSoldierFemaleType, Param_GetChangedEvent(), SoldierFemale_Changed);
    TaskType_SetHandler(gSoldierFemaleType, Vehicle_GetControlEvent(), SoldierFemale_InputHandler);
    TaskType_SetHandler(gSoldierFemaleType, Game_GetEvent9(), SoldierFemale_Handler4553E0);
    TaskType_SetHandler(gSoldierFemaleType, Game_GetEvent11(), SoldierFemale_Handler455570);
    TaskType_SetHandler(gSoldierFemaleType, Human_GetPunchEvent(), SoldierFemale_Handler455780);
    TaskType_SetHandler(gSoldierFemaleType, FlashBang_GetEvent(), Human_Handler4D3710);
    gSoldierFemaleParams = ParamTable_Extend(Human_GetParamTable(), 2);
    ParamTable_Add(gSoldierFemaleParams, &tmp, &tmp.model, Param_Int32(), 7, NULL);
    ParamTable_Add(gSoldierFemaleParams, &tmp, &tmp.idleAnim, Param_Int32(), 7, NULL);
    gSoldierFemaleUnk5BDA90 = 0;
}
HOOK(0x454970, SoldierFemale_Open);

void SoldierFemale_UpdateInput(SoldierSM *sm, Soldier *h);

void SoldierFemale_Create(Soldier *h, void *arg)
{
    TASK_HANDLER(0, Human_GetType())(h, arg);
    StateMachine_Init(&h->sm, SoldierFemale_UpdateInput, gSoldierFemaleStates, SF_STAND, 1, gSoldierFemaleCombos);
    h->animSlots = gSoldierFemaleAnimSlots;
    Human_InitDamage(h, gSoldierFemaleSpeeds);
    h->model = 6;
    h->idleAnim = -1;
    h->run = 1;
    h->spawned = 0;
    h->unkD08 = 0;
    h->hitLimit = kHitLimit;
}
HOOK(0x454B10, SoldierFemale_Create);

void SoldierFemale_UseTarget(Soldier *h);

/* turns the script controls (input floats) into state machine bits */
void SoldierFemale_UpdateInput(SoldierSM *sm, Soldier *h)
{
    uint32_t bits = sm->bits & 0xFFC3E01C;
    uint32_t flags = h->motion->flags;
    int i;

    if (flags & 0x100)
        bits |= SB_ANIM_DONE;
    if (flags & 0x10000)
        bits |= SB_ANIM_EVENT;
    if (h->run)
        bits |= SB_RUN;
    else
        bits &= ~SB_RUN;
    for (i = 2; i < 10; i++)
        if (!is_zero(h->input[i]))
            bits |= 0x20 << (i - 2);
    if (!is_zero(h->input[21])) {
        h->scriptAnim = x87_fistp(h->input[21]);
        bits |= SB_ANIM;
    }
    if (!is_zero(h->input[22])) {
        h->useTarget = x87_fistp(h->input[22]);
        bits |= SB_USE;
        SoldierFemale_UseTarget(h);
        h->input[22] = 0.0f;
    }
    if (!is_zero(h->input[23]))
        bits |= SB_AIM;
    if (!is_zero(h->input[24])) {
        int id = x87_fistp(h->input[24]);
        void *w = Task_FindWeapon(h, id);

        if (w) {
            WeaponSlots_Set(h->weapons, 0, w);
            sm->bits = bits;
            return;
        }
        Warning_Show("#%d is trying to change to invalid weapon #%d", Task_GetId(h), id);
    }
    sm->bits = bits;
}
HOOK(0x454BA0, SoldierFemale_UpdateInput);

void SoldierFemale_UseTarget(Soldier *h)
{
    Task *t = Task_FindById(h->useTarget);
    TaskHandler fn;
    UseMsg msg;
    int result;

    if (!t)
        return;
    Human_SetClimbObject(h, t);
    result = 0;
    fn = TASK_HANDLER(Game_GetEvent3(), t->type);
    if (fn)
        fn(t, &result);
    if (result == 2) {
        Switch_Press(t);
    } else if (result == 3) {
        Door_Toggle(t);
        SM_Set(&h->sm, h, SF_USE);
    }
    msg.h = h;
    memcpy(msg.pos, h->pos, sizeof(msg.pos));
    memcpy(msg.unk534, h->unk534, sizeof(msg.unk534));
    msg.input = h->input;
    msg.prevInput = h->prevInput;
    msg.unk44 = 0;
    TASK_HANDLER(Game_GetEvent4(), t->type)(t, &msg);
    h->uses++;
}
HOOK(0x454DA0, SoldierFemale_UseTarget);

/* calls the Human type's run handler with only the task */
void Human_RunBase(void *task)
{
    ((void (*)(void *))TASK_HANDLER(1, Human_GetType()))(task);
}
HOOK(0x454EE0, Human_RunBase);

void SoldierFemale_GetParams(Soldier *h, void **out)
{
    *out = gSoldierFemaleParams;
}
HOOK(0x454F00, SoldierFemale_GetParams);

void SoldierFemale_Changed(Soldier *h, void *arg)
{
    uint8_t ev;
    uint16_t type;

    if (h->spawned)
        BoneDynCubeObj_Deinit(h);
    BoneDynCubeObj_Init(Game_GetWorld(), h, h->model, 0);
    h->unkCC = 2;
    if (h->idleAnim == -1) {
        Human_PlayAnim(h, ANIM_DEFAULT, 0, 0.0f);
    } else if (Anim_Exists(h->anim, 0, h->idleAnim)) {
        Anim_Play(h->anim, 0, h->idleAnim, 0, 0.0f);
    } else {
        Warning_Show("Animation #%d not found", h->idleAnim);
        Human_PlayAnim(h, ANIM_DEFAULT, 0, 0.0f);
    }
    if (!h->spawned) {
        DynCube_Add(Game_GetWorld(), h, h->cubePos, h->radius, 2);
        h->spawned = 1;
    }
    ev = Param_GetChangedEvent();
    type = Human_GetType();
    TASK_HANDLER(ev, type)(h, arg);
    BoneObj_Update(h->matrix, h->anim);
    memcpy(h->unk650, h->motion->unk4C, sizeof(h->unk650));
}
HOOK(0x454F10, SoldierFemale_Changed);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 53E400, 53E400, 0x68) INIT({0x30, 0x3A, 0x45, 0x00, 0x40, 0x3C, 0x45, 0x00, 0xE0, 0x3D, 0x45, 0x00, 0x80, 0x3F, 0x45, 0x00, 0xD0, 0x40, 0x45, 0x00, 0xF0, 0x41, 0x45, 0x00, 0x00, 0x43, 0x45, 0x00, 0xE0, 0x43, 0x45, 0x00, 0xC0, 0x44, 0x45, 0x00, 0xE0, 0x45, 0x45, 0x00, 0xC0, 0x46, 0x45, 0x00, 0xF0, 0x47, 0x45, 0x00, 0x40, 0x49, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02});
ALIAS(gSoldierFemaleStates, pad_53E400, 0);
PAD(igd, 53E400, 53E468, 0x8) INIT({0x04, 0x00, 0x00, 0x00, 0x38, 0xE4, 0x53});
ALIAS(gSoldierFemaleCombos, pad_53E468, 0);
PAD_CONST(igd, 53E470, 53E470, 0x30) INIT({0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05});
ALIAS(gFemaleFootstepSounds, pad_53E470, 0);
PAD(igd, 53E4A0, 53E4A0, 0x68) INIT({0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06});
ALIAS(gSoldierFemaleAnimSlots, pad_53E4A0, 0);
PAD(igd, 53E4A0, 53E508, 0x1A0) INIT({0x00, 0x00, 0x00, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x42, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x42, 0x06, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x77, 0x61, 0x6C, 0x6B, 0x5F, 0x72, 0x61, 0x69, 0x6E, 0x5F, 0x25, 0x31, 0x64, 0x00, 0x00, 0x00, 0x41, 0x6E, 0x69, 0x6D, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x23, 0x25, 0x64, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x66, 0x6F, 0x75, 0x6E, 0x64, 0x00, 0x61, 0x69, 0x5F, 0x64, 0x65, 0x61, 0x74, 0x68, 0x5F, 0x00, 0x00, 0x00, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x53, 0x6F, 0x6C, 0x64, 0x69, 0x65, 0x72, 0x46, 0x65, 0x6D, 0x61, 0x6C, 0x65, 0x00, 0x00, 0x23, 0x25, 0x64, 0x20, 0x69, 0x73, 0x20, 0x74, 0x72, 0x79, 0x69, 0x6E, 0x67, 0x20, 0x74, 0x6F, 0x20, 0x63, 0x68, 0x61, 0x6E, 0x67, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x69, 0x6E, 0x76, 0x61, 0x6C, 0x69, 0x64, 0x20, 0x77, 0x65, 0x61, 0x70, 0x6F, 0x6E, 0x20, 0x23, 0x25, 0x64, 0x00, 0x00, 0x00, 0x61, 0x69, 0x5F, 0x68, 0x69, 0x74, 0x5F, 0x00, 0x61, 0x69, 0x5F, 0x78, 0x70, 0x6C, 0x68, 0x69, 0x74, 0x5F});
ALIAS(gSoldierFemaleSpeeds, pad_53E508, 0);
BSS(5BDA84) void *gSoldierFemaleParams;
BSS(5BDA84) uint16_t gSoldierFemaleType;
PAD(igb, 5BDA84, 5BDA8A, 0x6);
BSS(5BDA84) int gSoldierFemaleUnk5BDA90;
ALIAS(gSoldierFemaleList, gSoldierFemaleParams, 0);
