#ifndef RPG_SOLDIER_H
#define RPG_SOLDIER_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "loadscreen/loadscreentypes.h"
#include "soldier/soldiertypes.h"
#include "humanplayer/humanplayertypes.h"

typedef struct Soldier Soldier;
typedef struct SoldierSM SoldierSM;
typedef void (*SoldierStateFn)(SoldierSM *sm, Soldier *h);
typedef void (*TaskHandler)(void *task, void *arg);

extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((TaskHandler *)gTaskHandlers)[(slot) * 0x180 + (type)])

/* the subtypes continue right after the 0xD04 byte base */
#pragma pack(push, 4)

/* SoldierSM.bits */
enum {
    SB_ANIM_DONE = 0x1,
    SB_ANIM_EVENT = 0x2,
    SB_ON_GROUND = 0x4,
    SB_HIT = 0x10000,
    SB_DEAD = 0x20000,
    SB_RUN = 0x40000,
    SB_ANIM = 0x100000,
};

/* The AI brain that drives a soldier (Soldier.ai). */
typedef struct SoldierAI {
    char unk0[0x3AA8];
    void *unk3AA8;
    char unk3AAC[0x10];
    char unk3ABC;
    char unk3ABD;
    char unk3ABE[0x12];
    int deathAnim;
    char unk3AD4[0x264];
    int unk3D38;
    char unk3D3C[0x6C];
    float unk3DA8;
    int unk3DAC;
} SoldierAI;

/* Base of all HumanSoldier task types. */
struct Soldier {
    char task[0x1C];
    uint16_t type;
    char unk1E[2];
    double pos[3];
    char unk38[0x30];
    char anim[8];
    HumanMotion *motion;
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
    HumanDamage damage;
    SoldierSM sm;
    void *animSlots;
    char unk33C;
    char unk33D;
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
    char unk52D[0xB];
    int unk538;
    int unk53C;
    char unk540[0x110];
    int unk650[5];
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
    char unk770[0x90];
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

/* HumanSoldierFemale and HumanSoldierPistol, 0xD50 bytes */
typedef struct SoldierF {
    Soldier s;
    float hitLimit;
    float hitSum;
    char unkD0C[4];
    char unkD10[8];
    union {
        int deathKind;
        struct {
            char unkD18;
            char unkD19;
        };
    };
    char unkD1C[4];
    double deathDir[3];
    char unkD38[0x18];
} SoldierF;

/* HumanSoldierRPG, 0xD40 bytes */
typedef struct SoldierRPG {
    Soldier s;
    int anim;
    int deathKind;
    char unkD0C[4];
    double deathDir[3];
    double deathVel[3];
} SoldierRPG;

void SoldierFemale_Update(Soldier *h);
void SoldierRPG_Update(Soldier *h);

#pragma pack(pop)

#define SCHECK(t, f, off) _Static_assert(offsetof(t, f) == (off), #f)
SCHECK(Soldier, motion, 0x70);
SCHECK(Soldier, radius, 0xA0);
SCHECK(Soldier, unkCC, 0xCC);
SCHECK(Soldier, unkE0, 0xE0);
SCHECK(Soldier, prevInput, 0x110);
SCHECK(Soldier, input, 0x1B0);
SCHECK(Soldier, damage, 0x254);
SCHECK(Soldier, damage.hitZone, 0x2E8);
SCHECK(Soldier, sm, 0x2EC);
SCHECK(Soldier, animSlots, 0x338);
SCHECK(Soldier, unk33D, 0x33D);
SCHECK(Soldier, weapons, 0x4C4);
SCHECK(Soldier, fire, 0x4D1);
SCHECK(Soldier, view, 0x4EC);
SCHECK(Soldier, delta, 0x520);
SCHECK(Soldier, unk538, 0x538);
SCHECK(Soldier, unk650, 0x650);
SCHECK(Soldier, vel, 0x664);
SCHECK(Soldier, unk688, 0x688);
SCHECK(Soldier, unk696, 0x696);
SCHECK(Soldier, soundPos, 0x6A8);
SCHECK(Soldier, ground, 0x6EC);
SCHECK(Soldier, groundDistance, 0x710);
SCHECK(Soldier, movePos, 0x728);
SCHECK(Soldier, moveAngle, 0x75C);
SCHECK(Soldier, attach, 0x76C);
SCHECK(Soldier, ai, 0x800);
SCHECK(Soldier, footstep, 0x84C);
SCHECK(Soldier, dying, 0xCF5);
SCHECK(Soldier, model, 0xCF8);
SCHECK(Soldier, spawned, 0xD01);
_Static_assert(sizeof(Soldier) == 0xD04, "Soldier");
SCHECK(SoldierF, hitSum, 0xD08);
SCHECK(SoldierF, deathKind, 0xD18);
SCHECK(SoldierF, deathDir, 0xD20);
_Static_assert(sizeof(SoldierF) == 0xD50, "SoldierF");
SCHECK(SoldierRPG, deathDir, 0xD10);
SCHECK(SoldierRPG, deathVel, 0xD28);
_Static_assert(sizeof(SoldierRPG) == 0xD40, "SoldierRPG");
SCHECK(SoldierAI, unk3ABC, 0x3ABC);
SCHECK(SoldierAI, deathAnim, 0x3AD0);
SCHECK(SoldierAI, unk3D38, 0x3D38);
SCHECK(SoldierAI, unk3DAC, 0x3DAC);
#undef SCHECK

#endif
