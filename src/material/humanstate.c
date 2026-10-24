#include <string.h>

#include "igi.h"
#include "hook.h"
#include "humanai/humanaitypes.h"
#include "humanplayer/humanplayertypes.h"

typedef void (*HumanStateFn)(HumanPlayerSM *sm, HumanPlayer *p);

ORIG(0x416D80, char, PlayerXPHit, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4C1780, void, Debug_Log, (HumanPlayer *p));
ORIG(0x413A60, void, HumanPlayer_RequestWeaponAnim, (HumanPlayer *p, int mode));
ORIG(0x413C80, char *, Weapon_GetType, (void *task));
ORIG(0x4F23D0, void, Weapon_GetAmmo, (void *task, int *out));
ORIG(0x489E70, void, Human_PlayAnimSlot, (HumanPlayer *p, int anim, float blend));
ORIG(0x489E10, void, HumanDamage_GetZone, (HumanDamage *damage, int kind));
ORIG(0x4D6520, long double, Anim_GetPhase, (void *model, int track));
ORIG(0x4D64E0, long double, Anim_GetLength, (void *model, int track));
ORIG(0x4D65B0, void, Anim_SetTime, (void *model, int track, float time));
ORIG(0x4659B0, void, HumanView_SetDip, (void *obj, float v));
ORIG(0x4E6B00, void, Sound_Play, (void *obj, const char *sound));
ORIG(0x4C3440, int, DynCubeObj_GetPosition, (HumanPlayer *p));
ORIG(0x450CE0, void, HumanAI_Broadcast, (AIMessage *e));
ORIG(0x40AEF0, void, HumanPlayer_RunForwardLeftUpdate, (HumanPlayerSM *sm, HumanPlayer *p));

extern float gHumanAirSpeed;

void HumanPlayer_CheckDeath(HumanPlayer *p)
{
    if (p->damage.total >= p->damage.max && !PlayerXPHit()) {
        p->dead = 1;
        p->sm.input |= 0x20000;
    }
}
HOOK(0x408420, HumanPlayer_CheckDeath);

void HumanPlayer_UpdateFall(HumanPlayer *p)
{
    float fwd = 0, ax, ay, speed, d, dmg;
    long double vz, side, t, a, b;
    const char *sound;
    char buf[32];
    AIMessage e;

    vz = (long double)p->vel[2] - 84.74169158935547f;
    p->vel[2] = vz;
    if (p->input[2] != 0)
        fwd = -gHumanAirSpeed;
    if (p->input[3] != 0)
        fwd = gHumanAirSpeed + fwd;
    side = 0;
    if (p->input[4] != 0)
        side = gHumanAirSpeed;
    if (p->input[5] != 0)
        side = side - gHumanAirSpeed;
    ax = side * p->matrix.m[1] + (long double)fwd * p->matrix.m[0];
    ay = side * p->matrix.m[4] + (long double)fwd * p->matrix.m[3];
    t = side * p->matrix.m[7] + (long double)fwd * p->matrix.m[6];
    p->vel[0] = ax + (long double)p->vel[0];
    p->vel[1] = ay + (long double)p->vel[1];
    vz += t;
    p->vel[2] = vz;
    if (!(p->sm.input & 4))
        return;
    if (vz < 0)
        vz = -vz;
    a = vz * (1.0f / 4096) * 30.0f;
    speed = a;
    b = a * 0.125f;
    if (b < 0)
        b = 0;
    else if (b > 1)
        b = 1;
    HumanView_SetDip(p->view, b * -1024.0);
    d = speed - 14.0f;
    e.radius = 20480.0f;
    if (d > 0) {
        p->damage.hitZone = 13;
        HumanDamage_GetZone(&p->damage, 13);
        dmg = (long double)d * 0.07692307978868484f * p->damage.max;
        if (!PlayerXPHit()) {
            p->damage.part[13].dmg = dmg + (long double)p->damage.part[13].dmg;
            p->damage.total = dmg + (long double)p->damage.total;
        }
        if (dmg != 0) {
            if ((long double)p->damage.max * 0.1f > dmg)
                sound = "player_fall_1";
            else if ((long double)p->damage.max * 0.2f > dmg)
                sound = "player_fall_2";
            else
                sound = "player_fall_3";
            crt_sprintf(buf, sound);
            Sound_Play(p->view, buf);
            e.radius = 40960.0f;
        }
    }
    e.type = 11;
    e.team = p->team;
    e.flags = 0x46;
    e.pos = (Vec3d *)(DynCubeObj_GetPosition(p));
    e.from = (Task *)p;
    e.about = (Task *)p;
    HumanAI_Broadcast(&e);
}
HOOK(0x408460, HumanPlayer_UpdateFall);

int Weapon_HasAmmo(void *task)
{
    int v[2];

    Weapon_GetAmmo(task, v);
    if (v[0] + v[1] == 0)
        return Weapon_GetType(task)[0x6DC];
    return 1;
}
HOOK(0x408720, Weapon_HasAmmo);

static void HumanState_Goto(HumanPlayerSM *sm, HumanPlayer *p, int state)
{
    sm->prev = sm->state;
    sm->state = state;
    sm->run = sm->states[state];
    sm->run(sm, p);
}

static int HumanState_Landed(HumanPlayer *p)
{
    return p->input[11] != 0 && p->prevInput[11] == 0;
}

static float HumanState_Blend(HumanPlayerSM *sm)
{
    return sm->prev == 28 ? 0.0f : 0.2f;
}

/* keeps the animation at the same relative position */
static void HumanState_KeepPhase(HumanPlayer *p, float phase)
{
    Anim_SetTime(p->anim, 0, Anim_GetLength(p->anim, 0) * phase);
}

static int HumanState_Is(HumanPlayerSM *sm, int a, int b, int c)
{
    return sm->prev == a || sm->prev == b || sm->prev == c;
}

void HumanState_Update00(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update01(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update02(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update03(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update04(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update05(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update06(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update07(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update08(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update09(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update10(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update11(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update12(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update13(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update14(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update15(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update16(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update17(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update18(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update19(HumanPlayerSM *sm, HumanPlayer *p);
void HumanState_Update20(HumanPlayerSM *sm, HumanPlayer *p);

void HumanState_Enter00(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 0;
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    HumanPlayer_RequestWeaponAnim(p, 1);
    sm->run = HumanState_Update00;
}
HOOK(0x408770, HumanState_Enter00);

void HumanState_Update00(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in;
    int next;

    if (p->unkDC0 < 0) {
        p->unkDC0 += 1.0f / 300;
        if (p->unkDC0 > 0)
            p->unkDC0 = 0;
    } else if (p->unkDC0 > 0) {
        p->unkDC0 -= 1.0f / 300;
        if (p->unkDC0 < 0)
            p->unkDC0 = 0;
    }
    in = sm->input;
    if (!(in & 4))
        next = 29;
    else if (in & 0x10000)
        next = 46;
    else if (in & 0x20000)
        next = 47;
    else if ((in & 0x40000) && (in & 0x80))
        next = 9;
    else if ((in & 0x40000) && (in & 0x100))
        next = 12;
    else if ((in & 0x40000) && (in & 0x40))
        next = 15;
    else if ((in & 0x40000) && (in & 0x20))
        next = 16;
    else if (!(in & 0x40000) && (in & 0x80))
        next = 1;
    else if (!(in & 0x40000) && (in & 0x100))
        next = 4;
    else if (!(in & 0x40000) && (in & 0x40))
        next = 7;
    else if (!(in & 0x40000) && (in & 0x20))
        next = 8;
    else if (HumanState_Landed(p))
        next = 28;
    else if (in & 0x2000)
        next = 19;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x4087B0, HumanState_Update00);

void HumanState_Enter01(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 0;
    Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, HumanState_Blend(sm));
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update01;
}
HOOK(0x408A40, HumanState_Enter01);

/* Checks shared by the standing move states, n is the crouched version. */
static int HumanState_Common(HumanPlayerSM *sm, HumanPlayer *p, int crouched, int *next)
{
    uint32_t in = sm->input;

    if (!(in & 4))
        *next = 29;
    else if (in & 0x10000)
        *next = 46;
    else if (in & 0x20000)
        *next = 47;
    else if (in & 0x40000)
        *next = crouched;
    else if (HumanState_Landed(p))
        *next = 28;
    else
        return 0;
    return 1;
}

/* Same for the crouched states, n is the standing version. */
static int HumanState_CommonCrouched(HumanPlayerSM *sm, HumanPlayer *p, int standing, int *next)
{
    uint32_t in = sm->input;

    if (!(in & 4))
        *next = 29;
    else if (in & 0x10000)
        *next = 46;
    else if (in & 0x20000)
        *next = 47;
    else if (!(in & 0x40000))
        *next = standing;
    else if (HumanState_Landed(p))
        *next = 28;
    else
        return 0;
    return 1;
}

void HumanState_Update01(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 9, &next))
        ;
    else if (in & 0x40)
        next = 2;
    else if (in & 0x20)
        next = 3;
    else if (!(in & 0x80))
        next = 0;
    else if (in & 0x2000)
        next = 20;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x408AA0, HumanState_Update01);

void HumanState_Enter02(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 0;
    Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update02;
}
HOOK(0x408C30, HumanState_Enter02);

void HumanState_Update02(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 10, &next))
        ;
    else if (!(in & 0x80))
        next = 7;
    else if (!(in & 0x40))
        next = 1;
    else if (in & 0x2000)
        next = 21;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x408C80, HumanState_Update02);

void HumanState_Enter03(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 1, 11, 22))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update03;
}
HOOK(0x408DF0, HumanState_Enter03);

void HumanState_Update03(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 11, &next))
        ;
    else if (!(in & 0x80))
        next = 8;
    else if (!(in & 0x20))
        next = 1;
    else if (in & 0x2000)
        next = 22;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x408E70, HumanState_Update03);

void HumanState_Enter04(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, HumanState_Blend(sm));
    switch (sm->prev) {
    case 5:
    case 6:
    case 12:
    case 23:
        HumanState_KeepPhase(p, phase);
    }
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update04;
}
HOOK(0x408FE0, HumanState_Enter04);

void HumanState_Update04(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 12, &next))
        ;
    else if (in & 0x40)
        next = 5;
    else if (in & 0x20)
        next = 6;
    else if (!(in & 0x100))
        next = 0;
    else if (in & 0x2000)
        next = 23;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x409090, HumanState_Update04);

void HumanState_Enter05(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 4, 13, 24))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update05;
}
HOOK(0x409220, HumanState_Enter05);

void HumanState_Update05(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 13, &next))
        ;
    else if (!(in & 0x100))
        next = 7;
    else if (!(in & 0x40))
        next = 4;
    else if (in & 0x2000)
        next = 24;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x4092A0, HumanState_Update05);

void HumanState_Enter06(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 4, 14, 25))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update06;
}
HOOK(0x409410, HumanState_Enter06);

void HumanState_Update06(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 14, &next))
        ;
    else if (!(in & 0x100))
        next = 8;
    else if (!(in & 0x20))
        next = 4;
    else if (in & 0x2000)
        next = 25;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x409490, HumanState_Update06);

void HumanState_Enter07(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 0;
    Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, HumanState_Blend(sm));
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update07;
}
HOOK(0x409600, HumanState_Enter07);

void HumanState_Update07(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 15, &next))
        ;
    else if (!(in & 0x40))
        next = 0;
    else if (in & 0x80)
        next = 2;
    else if (in & 0x100)
        next = 5;
    else if (in & 0x2000)
        next = 26;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x409660, HumanState_Update07);

void HumanState_Enter08(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 0;
    Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, HumanState_Blend(sm));
    HumanPlayer_RequestWeaponAnim(p, 3);
    sm->run = HumanState_Update08;
}
HOOK(0x4097F0, HumanState_Enter08);

void HumanState_Update08(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    Debug_Log(p);
    if (HumanState_Common(sm, p, 16, &next))
        ;
    else if (!(in & 0x20))
        next = 0;
    else if (in & 0x80)
        next = 3;
    else if (in & 0x100)
        next = 6;
    else if (in & 0x2000)
        next = 27;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x409850, HumanState_Update08);

void HumanState_Enter09(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    switch (sm->prev) {
    case 1:
    case 10:
    case 11:
    case 20:
        HumanState_KeepPhase(p, phase);
    }
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update09;
}
HOOK(0x4099E0, HumanState_Enter09);

void HumanState_Update09(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 1, &next))
        ;
    else if (in & 0x40)
        next = 2;
    else if (in & 0x20)
        next = 3;
    else if (!(in & 0x80))
        next = 0;
    else if (in & 0x2000)
        next = 20;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x409A80, HumanState_Update09);

void HumanState_Enter10(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 2, 9, 21))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update10;
}
HOOK(0x409C10, HumanState_Enter10);

void HumanState_Update10(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 2, &next))
        ;
    else if (!(in & 0x80))
        next = 15;
    else if (!(in & 0x40))
        next = 9;
    else if (in & 0x2000)
        next = 21;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x409C90, HumanState_Update10);

void HumanState_Enter11(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 3, 9, 22))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update11;
}
HOOK(0x409E00, HumanState_Enter11);

void HumanState_Update11(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 3, &next))
        ;
    else if (!(in & 0x80))
        next = 16;
    else if (!(in & 0x20))
        next = 9;
    else if (in & 0x2000)
        next = 22;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x409E80, HumanState_Update11);

void HumanState_Enter12(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    switch (sm->prev) {
    case 4:
    case 13:
    case 14:
    case 23:
        HumanState_KeepPhase(p, phase);
    }
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update12;
}
HOOK(0x409FF0, HumanState_Enter12);

void HumanState_Update12(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 4, &next))
        ;
    else if (in & 0x40)
        next = 13;
    else if (in & 0x20)
        next = 14;
    else if (!(in & 0x100))
        next = 0;
    else if (in & 0x2000)
        next = 23;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x40A090, HumanState_Update12);

void HumanState_Enter13(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 5, 12, 24))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update13;
}
HOOK(0x40A220, HumanState_Enter13);

void HumanState_Update13(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 5, &next))
        ;
    else if (!(in & 0x100))
        next = 7;
    else if (!(in & 0x40))
        next = 12;
    else if (in & 0x2000)
        next = 23;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x40A2A0, HumanState_Update13);

void HumanState_Enter14(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 6, 12, 25))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update14;
}
HOOK(0x40A410, HumanState_Enter14);

void HumanState_Update14(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 6, &next))
        ;
    else if (!(in & 0x100))
        next = 15;
    else if (!(in & 0x20))
        next = 12;
    else if (in & 0x2000)
        next = 23;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x40A490, HumanState_Update14);

void HumanState_Enter15(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (sm->prev == 26)
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update15;
}
HOOK(0x40A600, HumanState_Enter15);

void HumanState_Update15(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 7, &next))
        ;
    else if (!(in & 0x40))
        next = 0;
    else if (in & 0x80)
        next = 10;
    else if (in & 0x100)
        next = 13;
    else if (in & 0x2000)
        next = 26;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x40A670, HumanState_Update15);

void HumanState_Enter16(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 0;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (sm->prev == 27)
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 2);
    sm->run = HumanState_Update16;
}
HOOK(0x40A800, HumanState_Enter16);

void HumanState_Update16(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (HumanState_CommonCrouched(sm, p, 8, &next))
        ;
    else if (!(in & 0x20))
        next = 0;
    else if (in & 0x80)
        next = 11;
    else if (in & 0x100)
        next = 14;
    else if (in & 0x2000)
        next = 27;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x40A870, HumanState_Update16);

void HumanState_Enter17(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 0;
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    sm->run = HumanState_Update17;
}
HOOK(0x40AA00, HumanState_Enter17);

void HumanState_Update17(HumanPlayerSM *sm, HumanPlayer *p)
{
    if (!(sm->input & 4))
        HumanState_Goto(sm, p, 29);
    else if (sm->input & 1)
        HumanState_Goto(sm, p, 19);
}
HOOK(0x40AA30, HumanState_Update17);

void HumanState_Enter18(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 0;
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    sm->run = HumanState_Update18;
}
HOOK(0x40AA90, HumanState_Enter18);

void HumanState_Update18(HumanPlayerSM *sm, HumanPlayer *p)
{
    if (!(sm->input & 4))
        HumanState_Goto(sm, p, 29);
    else if (sm->input & 1)
        HumanState_Goto(sm, p, 0);
}
HOOK(0x40AAC0, HumanState_Update18);

void HumanState_Enter19(HumanPlayerSM *sm, HumanPlayer *p)
{
    p->crouch = 1;
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    HumanPlayer_RequestWeaponAnim(p, 4);
    sm->run = HumanState_Update19;
}
HOOK(0x40AB20, HumanState_Enter19);

void HumanState_Update19(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (!(in & 4))
        next = 29;
    else if (in & 0x10000)
        next = 46;
    else if (in & 0x20000)
        next = 47;
    else if (in & 0x40)
        next = 26;
    else if (in & 0x20)
        next = 27;
    else if (!(in & 0x2010))
        next = 0;
    else if (in & 0x80)
        next = 20;
    else if (in & 0x100)
        next = 23;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x40AB60, HumanState_Update19);

void HumanState_Enter20(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 1;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (sm->prev == 1)
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 5);
    sm->run = HumanState_Update20;
}
HOOK(0x40ACB0, HumanState_Enter20);

void HumanState_Update20(HumanPlayerSM *sm, HumanPlayer *p)
{
    uint32_t in = sm->input;
    int next;

    if (!(in & 4))
        next = 29;
    else if (in & 0x10000)
        next = 46;
    else if (in & 0x20000)
        next = 47;
    else if (in & 0x40)
        next = 21;
    else if (in & 0x20)
        next = 22;
    else if (!(in & 0x80))
        next = 19;
    else if (!(in & 0x2010))
        next = in & 0x40000 ? 9 : 1;
    else
        return;
    HumanState_Goto(sm, p, next);
}
HOOK(0x40AD20, HumanState_Update20);

void HumanState_Enter21(HumanPlayerSM *sm, HumanPlayer *p)
{
    float phase;

    p->crouch = 1;
    phase = Anim_GetPhase(p->anim, 0);
    Human_PlayAnimSlot(p, sm->state, 0.2f);
    if (HumanState_Is(sm, 2, 20, 26))
        HumanState_KeepPhase(p, phase);
    HumanPlayer_RequestWeaponAnim(p, 5);
    sm->run = HumanPlayer_RunForwardLeftUpdate;
}
HOOK(0x40AE70, HumanState_Enter21);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(56E1F4) float gHumanAirSpeed;
