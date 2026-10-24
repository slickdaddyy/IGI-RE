#ifndef PISTOL_PISTOL_H
#define PISTOL_PISTOL_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "loadscreen/loadscreentypes.h"

typedef struct Soldier Soldier;
typedef struct SoldierSM SoldierSM;
typedef void (*SoldierStateFn)(SoldierSM *sm, Soldier *h);
typedef void (*TaskHandler)(void *task, void *arg);

extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((TaskHandler *)gTaskHandlers)[(slot) * 0x180 + (type)])

#pragma pack(push, 4)

/* Movement state machine of a soldier. input() turns the controls into
   bits, then run() is called. */
struct SoldierSM {
    SoldierStateFn input;
    SoldierStateFn *states;
    uint32_t bits;
    int prev;
    int state;
    SoldierStateFn run;
    char unk18[0x34];
};

/* SoldierSM.bits */
enum {
    SB_ANIM_DONE = 0x1,
    SB_ANIM_EVENT = 0x2,
    SB_ON_GROUND = 0x4,
    SB_FIRE = 0x200,
    SB_HIT = 0x10000,
    SB_DEAD = 0x20000,
    SB_RUN = 0x40000,
};

typedef struct SoldierMotion {
    int unk0;
    uint32_t flags;
    char unk8[0x850];
    float throwPos[3];
} SoldierMotion;

/* Damage per body part. Hits add mult * damage to a part and to total. */
typedef struct SoldierDamage {
    float total;
    float max;
    float unk08;
    struct {
        float dmg;
        float mult;
    } part[17];
    int hitZone;
} SoldierDamage;

typedef struct AITarget {
    char unk0[0xC];
    void **task;
} AITarget;

/* The AI brain that drives a soldier (Soldier.ai). */
typedef struct SoldierAI {
    char unk0[0x3AA8];
    AITarget *target;
    char unk3AAC[0x10];
    char unk3ABC;
    char unk3ABD;
    char unk3ABE[0x12];
    int deathAnim;
    char unk3AD4[0x2C4];
    float throwSpeedMin;
    float throwSpeedRange;
    float throwCountMin;
    float throwCountRange;
    float meleeChance;
    float meleeDamage;
    char unk3DB0[0x54];
    const char *grenade;
} SoldierAI;

struct Soldier {
    char task[0x1C];
    uint16_t type;
    char unk1E[2];
    double pos[3];
    char unk38[0x30];
    char anim[8];
    SoldierMotion *motion;
    char unk74[4];
    float matrix[9];
    char unk9C[4];
    float radius;
    char unkA4[0x28];
    int unkCC;
    char unkD0[8];
    char unkD8;
    char unkD9[7];
    char unkE0[0x30];
    float prevInput[40];
    float input[40];
    char inputLocked;
    char unk251[3];
    SoldierDamage damage;
    SoldierSM sm;
    void *animSlots;
    char unk33C;
    char moving;
    char unk33E[0x186];
    void *weapons[3];
    char unk4D0;
    char fire;
    char unk4D2[0x1A];
    void *view;
    char unk4F0[0x24];
    char unk514[0xC];
    float delta[3];
    char deltaScaled;
    char unk52D[7];
    int unk534;
    int unk538;
    int unk53C;
    char unk540[0x124];
    float vel[3];
    char unk670[0x18];
    float unk688[3];
    char unk694[2];
    char unk696;
    char unk697[0x11];
    double soundPos[3];
    char unk6C0[0x2C];
    char *ground;
    char unk6F0[0x20];
    double groundDistance;
    float groundAngle;
    char unk71C[0xC];
    float movePos[3];
    char unk734[0x28];
    float moveAngle[3];
    char unk768[4];
    void *attach;
    char unk770[0x4C];
    int uses;
    char unk7C0[0x40];
    SoldierAI *ai;
    char unk804[0x48];
    int footstep;
    char unk850[0x4A5];
    char dying;
    char vulnerable;
    char unkCF7;
    int model;
    int idleAnim;
    char run;
    char spawned;
    char unkD02[2];
};

/* HumanSoldier and HumanSoldierPistol, 0xD50 bytes */
typedef struct SoldierP {
    Soldier s;
    float hitLimit;
    float hitSum;
    int anim;
    int useTarget;
    int unkD14;
    char thrown;
    char meleeDone;
    char unkD1A[2];
    int deathKind;
    double deathDir[3];
    double deathVel[3];
} SoldierP;

typedef struct SoldierHit {
    int unk00;
    float damage;
    float dir[3];
    void *attacker;
    int *weapon;
} SoldierHit;

typedef struct SoldierBlast {
    int unk00;
    double *pos;
    int unk08;
    float radius;
    float unk10;
    int shakeFrames;
} SoldierBlast;

typedef struct SoldierPunch {
    float damage;
    void *attacker;
} SoldierPunch;

/* message 0xF sent to the AI when the soldier is hurt */
typedef struct SoldierAIMsg {
    int type;
    int unk4;
    int unk8;
    int unkC;
    void *data;
    void *attacker;
    void *attacker2;
} SoldierAIMsg;

typedef struct SoldierInputMsg {
    int unk0;
    float *input;
} SoldierInputMsg;

#pragma pack(pop)

#define SCHECK(t, f, off) _Static_assert(offsetof(t, f) == (off), #f)
SCHECK(SoldierMotion, throwPos, 0x858);
SCHECK(Soldier, motion, 0x70);
SCHECK(Soldier, radius, 0xA0);
SCHECK(Soldier, unkCC, 0xCC);
SCHECK(Soldier, prevInput, 0x110);
SCHECK(Soldier, input, 0x1B0);
SCHECK(Soldier, damage, 0x254);
SCHECK(Soldier, damage.hitZone, 0x2E8);
SCHECK(Soldier, sm, 0x2EC);
SCHECK(Soldier, animSlots, 0x338);
SCHECK(Soldier, moving, 0x33D);
SCHECK(Soldier, weapons, 0x4C4);
SCHECK(Soldier, fire, 0x4D1);
SCHECK(Soldier, view, 0x4EC);
SCHECK(Soldier, delta, 0x520);
SCHECK(Soldier, unk534, 0x534);
SCHECK(Soldier, vel, 0x664);
SCHECK(Soldier, unk688, 0x688);
SCHECK(Soldier, unk696, 0x696);
SCHECK(Soldier, soundPos, 0x6A8);
SCHECK(Soldier, ground, 0x6EC);
SCHECK(Soldier, groundDistance, 0x710);
SCHECK(Soldier, movePos, 0x728);
SCHECK(Soldier, moveAngle, 0x75C);
SCHECK(Soldier, attach, 0x76C);
SCHECK(Soldier, uses, 0x7BC);
SCHECK(Soldier, ai, 0x800);
SCHECK(Soldier, footstep, 0x84C);
SCHECK(Soldier, dying, 0xCF5);
SCHECK(Soldier, model, 0xCF8);
SCHECK(Soldier, spawned, 0xD01);
_Static_assert(sizeof(Soldier) == 0xD04, "Soldier");
SCHECK(SoldierP, anim, 0xD0C);
SCHECK(SoldierP, thrown, 0xD18);
SCHECK(SoldierP, deathKind, 0xD1C);
SCHECK(SoldierP, deathDir, 0xD20);
SCHECK(SoldierP, deathVel, 0xD38);
_Static_assert(sizeof(SoldierP) == 0xD50, "SoldierP");
SCHECK(SoldierAI, deathAnim, 0x3AD0);
SCHECK(SoldierAI, throwSpeedMin, 0x3D98);
SCHECK(SoldierAI, meleeDamage, 0x3DAC);
SCHECK(SoldierAI, grenade, 0x3E04);
#undef SCHECK

/* fcomp/fnstsw/test ah,0x40: equal, or unordered */
static inline int is_zero(long double x)
{
    return !(x < 0 || x > 0);
}

static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* the way every state switches: remember the old one, then enter the new */
static inline void sm_go(SoldierSM *sm, Soldier *h, int n)
{
    sm->prev = sm->state;
    sm->state = n;
    sm->run = sm->states[n];
    sm->run(sm, h);
}

/* falling, hit and death come first in every state; hit < 0 skips it */
static inline int sm_common(SoldierSM *sm, Soldier *h, uint32_t b, int hit, int die)
{
    if (!(b & SB_ON_GROUND))
        sm_go(sm, h, 29);
    else if (hit >= 0 && (b & SB_HIT))
        sm_go(sm, h, hit);
    else if (b & SB_DEAD)
        sm_go(sm, h, die);
    else
        return 0;
    return 1;
}

void SoldierPistol_Update(Soldier *h);
void SoldierPistol_UseTarget(Soldier *h);

#endif
