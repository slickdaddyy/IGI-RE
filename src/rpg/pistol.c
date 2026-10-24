/* The start of HumanSoldierPistol: footsteps and the first states. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "rpg/soldier.h"

FN_EXTERN(SoldierPistol_State13Update);
FN_EXTERN(Soldier_State10Update);
FN_EXTERN(Soldier_State12Update);
FN_EXTERN(Soldier_State18Update);
FN_EXTERN(Soldier_State2Update);
FN_EXTERN(Soldier_State6Update);
FN_EXTERN(Soldier_State7Update);
FN_EXTERN(Soldier_ThrowUpdate);

ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416FB0, int, Game_IsRaining, (void));
ORIG(0x460C40, uint8_t, Human_GetPunchEvent, (void));
ORIG(0x463180, char, Human_IsCrouching, (Soldier *h));
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
ORIG(0x4F0EE0, void *, Level_GetDynamic, (const char *name));

extern const int gFootstepSeq[];
#define SoldierPistol_State2Update ((SoldierStateFn)FN(0x45AD10, Soldier_State2Update))
#define SoldierPistol_State4Update ((SoldierStateFn)FN(0x458DD0, SoldierPistol_State13Update))
#define SoldierPistol_State6Update ((SoldierStateFn)FN(0x45B060, Soldier_State6Update))
#define SoldierPistol_State7Update ((SoldierStateFn)FN(0x45B1C0, Soldier_State7Update))
#define SoldierPistol_State8Update ((SoldierStateFn)FN(0x45B2F0, Soldier_ThrowUpdate))
#define SoldierPistol_State10Update ((SoldierStateFn)FN(0x45B6E0, Soldier_State10Update))
#define SoldierPistol_State12Update ((SoldierStateFn)FN(0x45B820, Soldier_State12Update))
#define SoldierPistol_State18Update ((SoldierStateFn)FN(0x45AB00, Soldier_State18Update))

static const float kPitchRange = 0.5f;
static const float kPitchDown = 0x1.99999ap-3f;

enum {
    PS_STAND,
    PS_HIT_STATE = 13,
    PS_ANIM = 15,
    PS_FALL = 29,
    PS_HIT = 30,
    PS_DIE = 32,
};

void SoldierPistol_Footstep(Soldier *h)
{
    char name[128];
    float pitch;
    void *snd;
    int i;

    if (h->unk538 != 0x3F2 || h->unk53C != Anim_GetId(h->anim, 0))
        return;
    i = gFootstepSeq[h->footstep];
    h->footstep = (unsigned)(h->footstep + 1) % 12;
    if (!h->ground)
        return;
    pitch = Random_Float(Game_GetRandom()) * kPitchRange + 1.0f - kPitchDown;
    snd = Sound_Play(Level_GetDynamic(h->ground + 0x54 + i * 0x20), h->ground + 0x54 + i * 0x20);
    if (snd) {
        if (Human_IsCrouching(h))
            Sound_SetVolume(snd, -1.0f, 0x1.51eb86p-1f);
        Sound_SetPos(snd, h->soundPos);
        Sound_SetPitch(snd, pitch);
    }
    if (!Game_IsRaining())
        return;
    crt_sprintf(name, "walk_rain_%1d", i + 1);
    snd = Sound_Play(Level_GetDynamic(name), name);
    if (snd) {
        Sound_SetPos(snd, h->soundPos);
        Sound_SetPitch(snd, pitch);
    }
}
HOOK(0x457810, SoldierPistol_Footstep);

static void sm_go(SoldierSM *sm, Soldier *h, int n)
{
    sm->prev = sm->state;
    sm->state = n;
    sm->run = sm->states[n];
    sm->run(sm, h);
}

static int ps_common(SoldierSM *sm, Soldier *h, uint32_t b)
{
    if (!(b & SB_ON_GROUND))
        sm_go(sm, h, PS_FALL);
    else if (b & SB_HIT)
        sm_go(sm, h, PS_HIT);
    else if (b & SB_DEAD)
        sm_go(sm, h, PS_DIE);
    else
        return 0;
    return 1;
}

/* the checks shared by the moving states after the common ones */
static int ps_actions(SoldierSM *sm, Soldier *h, uint32_t b)
{
    if (b & 0x80000)
        sm_go(sm, h, PS_HIT_STATE);
    else if (b & SB_ANIM)
        sm_go(sm, h, PS_ANIM);
    else
        return 0;
    return 1;
}

static int ps_weapon(SoldierSM *sm, Soldier *h, uint32_t b)
{
    if (b & 0x2000000)
        sm_go(sm, h, 11);
    else if (b & 0x4000000)
        sm_go(sm, h, 8);
    else if (b & 0x800)
        sm_go(sm, h, 7);
    else
        return 0;
    return 1;
}

/* strafe right / left, or stand when not moving forward */
static void ps_sides(SoldierSM *sm, Soldier *h, uint32_t b)
{
    if (b & 0x20)
        sm_go(sm, h, 4);
    else if (b & 0x40)
        sm_go(sm, h, 5);
    else if (!(b & 0x80))
        sm_go(sm, h, PS_STAND);
}

static void ps_enter(SoldierSM *sm, Soldier *h, char moving, SoldierStateFn run)
{
    h->unk33D = moving;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = run;
}

/* blend from a neighbouring state, keeping the animation phase */
static void ps_enter_phase(SoldierSM *sm, Soldier *h, int keep, SoldierStateFn run)
{
    float phase;

    h->unk33D = 0;
    h->unk696 = 0;
    phase = Anim_GetPhase(h->anim, 0);
    Human_PlayAnimSlot(h, sm->state, 0);
    if (keep)
        Anim_SetTime(h->anim, 0, Anim_GetLength(h->anim, 0) * phase);
    sm->run = run;
}

void SoldierPistol_StandUpdate(SoldierSM *sm, Soldier *h);

void SoldierPistol_StandEnter(SoldierSM *sm, Soldier *h)
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
    sm->run = SoldierPistol_StandUpdate;
}
HOOK(0x457950, SoldierPistol_StandEnter);

void SoldierPistol_StandUpdate(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (ps_common(sm, h, b))
        return;
    if (b & 0x80010)
        sm_go(sm, h, PS_HIT_STATE);
    else if (b & SB_ANIM)
        sm_go(sm, h, PS_ANIM);
    else if (ps_weapon(sm, h, b))
        return;
    else if (b & 0x200000)
        sm_go(sm, h, 35);
    else if (b & 0x8000000)
        sm_go(sm, h, 9);
    else if ((b & 0x1000000) && (b & 0x80))
        sm_go(sm, h, 17);
    else if ((b & 0x800000) && (b & 0x80))
        sm_go(sm, h, 1);
    else if (!(b & SB_RUN) && (b & 0x80))
        sm_go(sm, h, 2);
    else if ((b & SB_RUN) && (b & 0x80))
        sm_go(sm, h, 3);
    else if (b & 0x20)
        sm_go(sm, h, 4);
    else if (b & 0x40)
        sm_go(sm, h, 5);
}
HOOK(0x457A00, SoldierPistol_StandUpdate);

void SoldierPistol_State1Update(SoldierSM *sm, Soldier *h);

void SoldierPistol_State1Enter(SoldierSM *sm, Soldier *h)
{
    int p = sm->prev;

    ps_enter_phase(sm, h, p > 1 && (p < 4 || p == 17), SoldierPistol_State1Update);
}
HOOK(0x457CB0, SoldierPistol_State1Enter);

void SoldierPistol_State1Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (ps_common(sm, h, b) || ps_actions(sm, h, b) || ps_weapon(sm, h, b))
        return;
    if (!(b & 0x800000))
        sm_go(sm, h, b & SB_RUN ? 3 : 2);
    else
        ps_sides(sm, h, b);
}
HOOK(0x457D30, SoldierPistol_State1Update);

void SoldierPistol_State17Update(SoldierSM *sm, Soldier *h);

void SoldierPistol_State17Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_phase(sm, h, sm->prev > 0 && sm->prev < 4, SoldierPistol_State17Update);
}
HOOK(0x457F50, SoldierPistol_State17Enter);

void SoldierPistol_State17Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (ps_common(sm, h, b) || ps_actions(sm, h, b))
        return;
    if (b & 0x200)
        sm_go(sm, h, 18);
    else if (!(b & 0x1000000))
        sm_go(sm, h, b & 0x800000 ? 1 : b & SB_RUN ? 3 : 2);
    else
        ps_sides(sm, h, b);
}
HOOK(0x457FC0, SoldierPistol_State17Update);

void SoldierPistol_State18Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State18Update);
}
HOOK(0x4581B0, SoldierPistol_State18Enter);

void SoldierPistol_State2Enter(SoldierSM *sm, Soldier *h)
{
    int p = sm->prev;

    ps_enter_phase(sm, h, p == 1 || p == 3 || p == 17, SoldierPistol_State2Update);
}
HOOK(0x4581E0, SoldierPistol_State2Enter);

void SoldierPistol_State3Update(SoldierSM *sm, Soldier *h);

void SoldierPistol_State3Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter_phase(sm, h, sm->prev > 0 && sm->prev < 3, SoldierPistol_State3Update);
}
HOOK(0x458250, SoldierPistol_State3Enter);

void SoldierPistol_State3Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (ps_common(sm, h, b) || ps_actions(sm, h, b) || ps_weapon(sm, h, b))
        return;
    if (b & 0x1000000)
        sm_go(sm, h, 17);
    else if (b & 0x800000)
        sm_go(sm, h, 1);
    else if (!(b & SB_RUN))
        sm_go(sm, h, 2);
    else
        ps_sides(sm, h, b);
}
HOOK(0x4582C0, SoldierPistol_State3Update);

void SoldierPistol_State4Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State4Update);
}
HOOK(0x458510, SoldierPistol_State4Enter);

void SoldierPistol_State5Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State4Update);
}
HOOK(0x458540, SoldierPistol_State5Enter);

void SoldierPistol_State6Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 1, SoldierPistol_State6Update);
}
HOOK(0x458570, SoldierPistol_State6Enter);

void SoldierPistol_State7Enter(SoldierSM *sm, Soldier *h)
{
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = SoldierPistol_State7Update;
}
HOOK(0x4585A0, SoldierPistol_State7Enter);

void SoldierPistol_State8Enter(SoldierSM *sm, Soldier *h)
{
    ((SoldierF *)h)->unkD18 = 0;
    h->unk696 = 0;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = SoldierPistol_State8Update;
}
HOOK(0x4585D0, SoldierPistol_State8Enter);

void SoldierPistol_MeleeUpdate(SoldierSM *sm, Soldier *h);

void SoldierPistol_MeleeEnter(SoldierSM *sm, Soldier *h)
{
    h->unk696 = 0;
    ((SoldierF *)h)->unkD19 = 1;
    Human_PlayAnimSlot(h, sm->state, 0);
    sm->run = SoldierPistol_MeleeUpdate;
}
HOOK(0x458600, SoldierPistol_MeleeEnter);

/* on the hit frame of the punch animation the target may get punched */
void SoldierPistol_MeleeUpdate(SoldierSM *sm, Soldier *h)
{
    SoldierF *f = (SoldierF *)h;
    uint32_t b = sm->bits;
    SoldierAI *ai = h->ai;
    SoldierPunch msg;
    TaskHandler fn;
    char *t;

    if (ps_common(sm, h, b))
        return;
    if (h->unk538 == 0xC && h->unk53C == Anim_GetId(h->anim, 0) && !f->unkD19) {
        f->unkD19 = 1;
        if (!(Random_Float(Game_GetRandom()) > ai->unk3DA8) && ai->unk3AA8) {
            char ***list = (char ***)((char *)ai->unk3AA8 + 0xC);

            if (*list && **list) {
                *(int *)&msg.damage = ai->unk3DAC;
                msg.attacker = h;
                t = **list;
                fn = TASK_HANDLER(Human_GetPunchEvent(), *(uint16_t *)(t + 0x1C));
                if (fn)
                    fn(t, &msg);
            }
        }
    }
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, PS_STAND);
}
HOOK(0x458630, SoldierPistol_MeleeUpdate);

void SoldierPistol_State10Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, SoldierPistol_State10Update);
}
HOOK(0x4587B0, SoldierPistol_State10Enter);

void SoldierPistol_State11Update(SoldierSM *sm, Soldier *h);

void SoldierPistol_State11Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, SoldierPistol_State11Update);
}
HOOK(0x4587E0, SoldierPistol_State11Enter);

void SoldierPistol_State11Update(SoldierSM *sm, Soldier *h)
{
    uint32_t b = sm->bits;

    if (ps_common(sm, h, b))
        return;
    if (b & SB_ANIM_DONE)
        sm_go(sm, h, 10);
}
HOOK(0x458810, SoldierPistol_State11Update);

void SoldierPistol_State12Enter(SoldierSM *sm, Soldier *h)
{
    ps_enter(sm, h, 0, SoldierPistol_State12Update);
}
HOOK(0x4588C0, SoldierPistol_State12Enter);
