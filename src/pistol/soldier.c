/* The start of HumanSoldier: footsteps and the first states. HumanSoldierPistol
   uses many of these too. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "pistol/pistol.h"

FN_EXTERN(SoldierPistol_StandUpdate);
FN_EXTERN(SoldierPistol_State11Update);
FN_EXTERN(SoldierPistol_State17Update);
FN_EXTERN(SoldierPistol_State1Update);
FN_EXTERN(SoldierPistol_State3Update);
FN_EXTERN(Soldier_ToCrouchRun);

ORIG(0x413BD0, void *, WeaponType_Find, (const char *name));
ORIG(0x413C80, void *, Weapon_GetType, (void *weapon));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416FB0, int, Game_IsRaining, (void));
ORIG(0x452930, void, AI_AimThrow, (void *out, float *dir, SoldierAI *ai, double *pos, float speed));
ORIG(0x4528F0, int, AI_HasTarget, (SoldierAI *ai));
ORIG(0x460C40, uint8_t, Human_GetPunchEvent, (void));
ORIG(0x463180, char, Human_IsCrouching, (Soldier *h));
ORIG(0x477040, void, Grenade_Throw, (void *owner, Soldier *h, double *pos, void *path, float *dir, void *type,
                                     int count));
ORIG(0x489E70, void, Human_PlayAnimSlot, (Soldier *h, int slot, float blend));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4D61D0, void, Anim_Play, (void *anim, int track, int id, int a, int b));
ORIG(0x4D64C0, int, Anim_GetId, (void *anim, int track));
ORIG(0x4D64E0, long double, Anim_GetLength, (void *anim, int track));
ORIG(0x4D6520, long double, Anim_GetPhase, (void *anim, int track));
ORIG(0x4D65B0, void, Anim_SetTime, (void *anim, int track, float time));
ORIG(0x4D74F0, int, Anim_Exists, (void *anim, int track, int id));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, const double *pos));
ORIG(0x4E6C60, void, Sound_SetVolume, (void *sound, float a, float b));
ORIG(0x4E6CA0, void, Sound_SetPitch, (void *sound, float pitch));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F2060, void *, WeaponSlots_Get, (void **slots, int i));
ORIG(0x4F2350, void, Weapon_UseAmmo, (void *weapon, int n));
ORIG(0x4F23D0, void, Weapon_GetAmmo, (void *weapon, int *out));

extern const int gSoldierFootsteps[];

/* updates that live with the HumanSoldierPistol states */
#define SoldierPistol_StandUpdate ((SoldierStateFn)FN(0x457A00, SoldierPistol_StandUpdate))
#define SoldierPistol_State1Update ((SoldierStateFn)FN(0x457D30, SoldierPistol_State1Update))
#define SoldierPistol_State17Update ((SoldierStateFn)FN(0x457FC0, SoldierPistol_State17Update))
#define SoldierPistol_State3Update ((SoldierStateFn)FN(0x4582C0, SoldierPistol_State3Update))
#define SoldierPistol_State11Update ((SoldierStateFn)FN(0x458810, SoldierPistol_State11Update))
#define Soldier_State4Update ((SoldierStateFn)FN(0x45B900, Soldier_ToCrouchRun))

static const float kPitchRange = 0.5f;
static const float kPitchDown = 0x1.99999ap-3f;
static const float kFps = 30.0f;

void Soldier_Footstep(Soldier *h)
{
    char name[128];
    float pitch;
    void *snd;
    int i;

    if (h->unk538 != 0x3F2 || h->unk53C != Anim_GetId(h->anim, 0))
        return;
    i = gSoldierFootsteps[h->footstep];
    h->footstep = (unsigned)(h->footstep + 1) % 12;
    if (!h->ground)
        return;
    pitch = Random_Float(Game_GetRandom()) * kPitchRange + 1.0f - kPitchDown;
    snd = Sound_Play(Level_GetDynamic(), h->ground + 0x54 + i * 0x20);
    if (snd) {
        if (Human_IsCrouching(h))
            Sound_SetVolume(snd, -1.0f, 0x1.51eb86p-1f);
        Sound_SetPos(snd, h->soundPos);
        Sound_SetPitch(snd, pitch);
    }
    if (!Game_IsRaining())
        return;
    crt_sprintf(name, "walk_rain_%1d", i + 1);
    snd = Sound_Play(Level_GetDynamic(), name);
    if (snd) {
        Sound_SetPos(snd, h->soundPos);
        Sound_SetPitch(snd, pitch);
    }
}
HOOK(0x45A7F0, Soldier_Footstep);

static void s_enter(SoldierSM *sm, Soldier *h, char moving, SoldierStateFn run)
{
    h->moving = moving;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = run;
}

/* walking and running states keep the step phase when coming from one of
   the states in the mask */
static void s_enter_phase(SoldierSM *sm, Soldier *h, uint32_t from, SoldierStateFn run)
{
    float phase;
    unsigned p;

    h->moving = 0;
    h->unk696 = 0;
    phase = Anim_GetPhase(h->anim, 0);
    Human_PlayAnimSlot(h, sm->state, 0);
    p = sm->prev;
    if (p < 32 && (from >> p & 1))
        Anim_SetTime(h->anim, 0, Anim_GetLength(h->anim, 0) * phase);
    sm->run = run;
}

void Soldier_StandEnter(SoldierSM *sm, Soldier *h)
{
    h->moving = 0;
    h->unk696 = 0;
    if (h->idleAnim == -1) {
        Human_PlayAnimSlot(h, sm->state, 0);
    } else if (Anim_Exists(h->anim, 0, h->idleAnim)) {
        Anim_Play(h->anim, 0, h->idleAnim, 0, 0);
    } else {
        Warning_Show("Animation #%d not found", h->idleAnim);
        Human_PlayAnimSlot(h, sm->state, 0);
    }
    sm->run = SoldierPistol_StandUpdate;
}
HOOK(0x45A930, Soldier_StandEnter);

void Soldier_State1Enter(SoldierSM *sm, Soldier *h)
{
    s_enter_phase(sm, h, 1 << 2 | 1 << 3 | 1 << 17, SoldierPistol_State1Update);
}
HOOK(0x45A9E0, Soldier_State1Enter);

void Soldier_State17Enter(SoldierSM *sm, Soldier *h)
{
    s_enter_phase(sm, h, 1 << 1 | 1 << 2 | 1 << 3, SoldierPistol_State17Update);
}
HOOK(0x45AA60, Soldier_State17Enter);

void Soldier_State18Update(SoldierSM *sm, Soldier *h);

void Soldier_State18Enter(SoldierSM *sm, Soldier *h)
{
    s_enter(sm, h, 1, Soldier_State18Update);
}
HOOK(0x45AAD0, Soldier_State18Enter);

/* the weapon is checked first in the aiming states */
static void *s_weapon(Soldier *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    int ammo[2];

    Weapon_GetType(w);
    Weapon_GetAmmo(w, ammo);
    return w;
}

void Soldier_State18Update(SoldierSM *sm, Soldier *h)
{
    void *w = s_weapon(h);
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 32))
        return;
    Weapon_UseAmmo(w, 1);
    if (!is_zero(h->input[6]))
        h->fire = 1;
    if (!(sm->bits & 0x1000000))
        sm_go(sm, h, b & SB_RUN ? 3 : 2);
    else if (!(b & 0x80))
        sm_go(sm, h, 0);
    else if ((b & SB_ANIM_DONE) && !(b & SB_FIRE))
        sm_go(sm, h, 17);
}
HOOK(0x45AB00, Soldier_State18Update);

void Soldier_State2Update(SoldierSM *sm, Soldier *h);

void Soldier_State2Enter(SoldierSM *sm, Soldier *h)
{
    s_enter_phase(sm, h, 1 << 1 | 1 << 3 | 1 << 17, Soldier_State2Update);
}
HOOK(0x45ACA0, Soldier_State2Enter);

void Soldier_State2Update(SoldierSM *sm, Soldier *h)
{
    static const struct {
        uint32_t bit;
        int state;
    } next[] = {
        {0x80000, 13}, {0x100000, 15}, {0x2000000, 11}, {0x4000000, 8}, {0x800, 7},
        {0x1000000, 17}, {0x800000, 1}, {SB_RUN, 3}, {0x20, 4}, {0x40, 5},
    };
    uint32_t b = sm->bits;
    size_t i;

    if (sm_common(sm, h, b, 30, 32))
        return;
    for (i = 0; i < sizeof(next) / sizeof(next[0]); i++) {
        if (b & next[i].bit) {
            sm_go(sm, h, next[i].state);
            return;
        }
    }
    if (!(b & 0x80))
        sm_go(sm, h, 0);
}
HOOK(0x45AD10, Soldier_State2Update);

void Soldier_State3Enter(SoldierSM *sm, Soldier *h)
{
    s_enter_phase(sm, h, 1 << 1 | 1 << 2, SoldierPistol_State3Update);
}
HOOK(0x45AF60, Soldier_State3Enter);

void Soldier_State4Enter(SoldierSM *sm, Soldier *h)
{
    s_enter(sm, h, 1, Soldier_State4Update);
}
HOOK(0x45AFD0, Soldier_State4Enter);

void Soldier_State5Enter(SoldierSM *sm, Soldier *h)
{
    s_enter(sm, h, 1, Soldier_State4Update);
}
HOOK(0x45B000, Soldier_State5Enter);

void Soldier_State6Update(SoldierSM *sm, Soldier *h);

void Soldier_State6Enter(SoldierSM *sm, Soldier *h)
{
    s_enter(sm, h, 1, Soldier_State6Update);
}
HOOK(0x45B030, Soldier_State6Enter);

void Soldier_State6Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b;

    s_weapon(h);
    b = sm->bits;
    if (sm_common(sm, h, b, 30, 32))
        return;
    if (!is_zero(h->input[6]))
        h->fire = 1;
    if ((b & SB_ANIM_DONE) && !(b & SB_FIRE))
        sm_go(sm, h, b & 0x2000000 ? 10 : 12);
}
HOOK(0x45B060, Soldier_State6Update);

static void s_enter_keep(SoldierSM *sm, Soldier *h, SoldierStateFn run)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = run;
}

void Soldier_State7Update(SoldierSM *sm, Soldier *h);

void Soldier_State7Enter(SoldierSM *sm, Soldier *h)
{
    s_enter_keep(sm, h, Soldier_State7Update);
}
HOOK(0x45B190, Soldier_State7Enter);

void Soldier_State7Update(SoldierSM *sm, Soldier *h)
{
    void *w = WeaponSlots_Get(h->weapons, 0);
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 32))
        return;
    if (b & SB_ANIM_DONE) {
        Weapon_UseAmmo(w, 1);
        sm_go(sm, h, b & 0x2000000 ? 10 : 0);
    }
}
HOOK(0x45B1C0, Soldier_State7Update);

void Soldier_ThrowUpdate(SoldierSM *sm, Soldier *h);

void Soldier_ThrowEnter(SoldierSM *sm, Soldier *h)
{
    ((SoldierP *)h)->thrown = 0;
    s_enter_keep(sm, h, Soldier_ThrowUpdate);
}
HOOK(0x45B2C0, Soldier_ThrowEnter);

/* throws a grenade at the release frame of the animation */
void Soldier_ThrowUpdate(SoldierSM *sm, Soldier *h)
{
    SoldierP *s = (SoldierP *)h;
    uint32_t b = sm->bits;
    SoldierAI *ai = h->ai;

    if (sm_common(sm, h, b, 30, 32))
        return;
    if (AI_HasTarget(ai) == 1 && h->unk538 == 0xB && h->unk53C == Anim_GetId(h->anim, 0) && !s->thrown) {
        float *at = h->motion->throwPos, speed, dir[3];
        char path[40];
        double pos[3];
        void *type, *owner;
        int count;

        speed = Random_Float(Game_GetRandom()) * ai->throwSpeedRange + ai->throwSpeedMin;
        count = x87_fistp(Random_Float(Game_GetRandom()) * ai->throwCountRange * kFps + ai->throwCountMin);
        pos[0] = at[0] + h->pos[0];
        pos[1] = at[1] + h->pos[1];
        pos[2] = at[2] + h->pos[2];
        AI_AimThrow(path, dir, ai, pos, speed);
        type = WeaponType_Find(ai->grenade);
        owner = Level_GetDynamic();
        Grenade_Throw(owner, h, pos, path, dir, type, count);
        s->thrown = 1;
    }
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, b & 0x2000000 ? 10 : 0);
}
HOOK(0x45B2F0, Soldier_ThrowUpdate);

void Soldier_MeleeUpdate(SoldierSM *sm, Soldier *h);

void Soldier_MeleeEnter(SoldierSM *sm, Soldier *h)
{
    h->unk696 = 0;
    ((SoldierP *)h)->meleeDone = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = Soldier_MeleeUpdate;
}
HOOK(0x45B500, Soldier_MeleeEnter);

/* at the hit frame, maybe punch the target */
void Soldier_MeleeUpdate(SoldierSM *sm, Soldier *h)
{
    SoldierP *s = (SoldierP *)h;
    uint32_t b = sm->bits;
    SoldierAI *ai = h->ai;

    if (sm_common(sm, h, b, 30, 32))
        return;
    if (h->unk53C == Anim_GetId(h->anim, 0) && h->unk538 == 0xC && !s->meleeDone) {
        s->meleeDone = 1;
        if (!(Random_Float(Game_GetRandom()) > ai->meleeChance) && ai->target && ai->target->task &&
            *ai->target->task) {
            SoldierPunch p;
            void *t;
            TaskHandler fn;

            p.damage = ai->meleeDamage;
            p.attacker = h;
            t = *ai->target->task;
            fn = TASK_HANDLER(Human_GetPunchEvent(), *(uint16_t *)((char *)t + 0x1C));
            if (fn)
                fn(t, &p);
        }
    }
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 0);
}
HOOK(0x45B530, Soldier_MeleeUpdate);

void Soldier_State10Update(SoldierSM *sm, Soldier *h);

void Soldier_State10Enter(SoldierSM *sm, Soldier *h)
{
    s_enter(sm, h, 0, Soldier_State10Update);
}
HOOK(0x45B6B0, Soldier_State10Enter);

void Soldier_State10Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 32))
        return;
    if (!(b & 0x2000000))
        sm_go(sm, h, 12);
    else if (b & SB_FIRE)
        sm_go(sm, h, 6);
}
HOOK(0x45B6E0, Soldier_State10Update);

void Soldier_State11Enter(SoldierSM *sm, Soldier *h)
{
    s_enter(sm, h, 0, SoldierPistol_State11Update);
}
HOOK(0x45B7C0, Soldier_State11Enter);

void Soldier_State12Update(SoldierSM *sm, Soldier *h);

void Soldier_State12Enter(SoldierSM *sm, Soldier *h)
{
    s_enter(sm, h, 0, Soldier_State12Update);
}
HOOK(0x45B7F0, Soldier_State12Enter);

void Soldier_State12Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (sm_common(sm, h, b, 30, 32))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 0);
}
HOOK(0x45B820, Soldier_State12Update);
