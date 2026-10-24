#ifndef SOLDIER_H
#define SOLDIER_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "loadscreen/loadscreentypes.h"
#include "soldier/soldiertypes.h"

#pragma pack(push, 4)

typedef void (*TaskHandler)(void *task, void *arg);
typedef void (*TaskHandler1)(void *task);

extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])

typedef struct Human Human;
typedef struct HumanSM HumanSM;
typedef void (*HumanStateFn)(HumanSM *sm, Human *h);

/* Movement state machine, same layout as the player's. input() turns the
   controls into bits, then run() is called. */
struct HumanSM {
    HumanStateFn input;
    HumanStateFn *states;
    uint32_t bits;
    int prev;
    int state;
    HumanStateFn run;
    char unk18[0x34];
};

/* HumanSM.bits of a HumanSoldier */
enum {
    SB_ANIM_DONE = 0x1,
    SB_ANIM_EVENT = 0x2,
    SB_ON_GROUND = 0x4,
    SB_FIRE = 0x200,
    SB_HIT = 0x10000,
    SB_DEAD = 0x20000,
    SB_RUN = 0x40000,
    SB_AIM = 0x100000,
    SB_CROUCH_AIM = 0x2000000,
};

typedef struct HumanMotion {
    int unk0;
    uint32_t flags;
} HumanMotion;

/* Damage per body part. Hits add mult * damage to a part and to total. */
typedef struct HumanDamage {
    float total;
    float max;
    float unk08;
    struct {
        float dmg;
        float mult;
    } part[17];
    int hitZone;
} HumanDamage;

/* The AI brain that drives a soldier (Human.ai). */
typedef struct HumanAI {
    char unk0[0x3ABC];
    char invulnerable;
    char fullHeadDamage;
    char unk3ABE[0x12];
    int deathAnim;
    char unk3AD4[0x264];
    int kind;
} HumanAI;

/* Base of the player and the AI soldiers, 0xCF8 bytes. */
struct Human {
    Task task;
    double pos[3];
    char unk38[0x30];
    char anim[8];
    HumanMotion *motion;
    char unk74[4];
    float matrix[9];
    char unk9C[0x3C];
    char unkD8;
    char unkD9[7];
    double unkE0[3];
    int unkF8;
    int unkFC;
    char unk100[0x10];
    float prevInput[40];
    float input[40];
    char inputLocked;
    char unk251[3];
    HumanDamage damage;
    HumanSM sm;
    void *animSlots;
    char dead;
    char crouch;
    char unk33E[2];
    char ammo[0x184];
    void *weapons[3];
    char weaponReady;
    char fire;
    char altFire;
    char fired;
    float unk4D4;
    char unk4D8[0x14];
    void *view;
    int unk4F0;
    char unk4F4;
    char unk4F5[3];
    int unk4F8[5];
    float yaw;
    char unk510;
    char unk511[3];
    int unk514[3];
    float delta[3];
    char unk52C;
    char unk52D[3];
    float unk530;
    char unk534[4];
    int unk538;
    char unk53C[0x110];
    char unk64C;
    char unk64D[0x17];
    float vel[3];
    float unk670[3];
    float unk67C[3];
    float unk688[3];
    char unk694;
    char unk695;
    char unk696;
    char unk697;
    char unk698;
    char unk699[7];
    int unk6A0[2];
    double soundPos[3];
    char unk6C0[0x2C];
    char *ground;
    char unk6F0[0x20];
    double groundDistance;
    float groundAngle;
    char unk71C[4];
    double unk720;
    float movePos[3];
    char unk734[0x28];
    float moveAngle[3];
    void *ref768;
    void *attach;
    char unk770[0x40];
    void *ref7B0;
    void *ref7B4;
    char unk7B8;
    char unk7B9;
    char unk7BA[2];
    int unk7BC;
    int index;
    int unk7C4;
    char deleteMe;
    char unk7C9[3];
    void *shadow;
    char unk7D0[0x18];
    int unk7E8;
    int unk7EC;
    char unk7F0[0x10];
    HumanAI *ai;
    char unk804[4];
    int unk808[12];
    void *ref838;
    void *ref83C;
    void *ref840;
    int unk844;
    int unk848;
    int footstep;
    int textCount;
    void *texts[32];
    char textBuf[32][0x21];
    char unkCF4;
    char dying;
    char vulnerable;
    char unkCF7;
};

/* HumanSoldier, 0xD50 bytes */
typedef struct Soldier {
    Human h;
    int model;
    int idleAnim;
    char run;
    char spawned;
    char unkD02[2];
    float hitLimit;
    float hitSum;
    int scriptAnim;
    int useTarget;
    int hitFrames;
    char unkD18[4];
    int deathKind;
    double deathDir[3];
    double deathVel[3];
} Soldier;

typedef struct HumanInputMsg {
    int unk0;
    float *input;
} HumanInputMsg;

#pragma pack(pop)

#define SCHECK(t, f, off) _Static_assert(offsetof(t, f) == (off), #f)
SCHECK(Human, motion, 0x70);
SCHECK(Human, unkD8, 0xD8);
SCHECK(Human, unkE0, 0xE0);
SCHECK(Human, prevInput, 0x110);
SCHECK(Human, input, 0x1B0);
SCHECK(Human, damage, 0x254);
SCHECK(Human, damage.hitZone, 0x2E8);
SCHECK(Human, sm, 0x2EC);
SCHECK(Human, animSlots, 0x338);
SCHECK(Human, crouch, 0x33D);
SCHECK(Human, ammo, 0x340);
SCHECK(Human, weapons, 0x4C4);
SCHECK(Human, fire, 0x4D1);
SCHECK(Human, view, 0x4EC);
SCHECK(Human, yaw, 0x50C);
SCHECK(Human, delta, 0x520);
SCHECK(Human, unk538, 0x538);
SCHECK(Human, unk64C, 0x64C);
SCHECK(Human, vel, 0x664);
SCHECK(Human, unk688, 0x688);
SCHECK(Human, soundPos, 0x6A8);
SCHECK(Human, ground, 0x6EC);
SCHECK(Human, groundDistance, 0x710);
SCHECK(Human, unk720, 0x720);
SCHECK(Human, movePos, 0x728);
SCHECK(Human, moveAngle, 0x75C);
SCHECK(Human, ref768, 0x768);
SCHECK(Human, ref7B0, 0x7B0);
SCHECK(Human, index, 0x7C0);
SCHECK(Human, shadow, 0x7CC);
SCHECK(Human, unk7E8, 0x7E8);
SCHECK(Human, ai, 0x800);
SCHECK(Human, unk808, 0x808);
SCHECK(Human, ref838, 0x838);
SCHECK(Human, footstep, 0x84C);
SCHECK(Human, texts, 0x854);
SCHECK(Human, textBuf, 0x8D4);
SCHECK(Human, unkCF4, 0xCF4);
_Static_assert(sizeof(Human) == 0xCF8, "Human");
SCHECK(Soldier, hitLimit, 0xD04);
SCHECK(Soldier, scriptAnim, 0xD0C);
SCHECK(Soldier, hitFrames, 0xD14);
SCHECK(Soldier, deathKind, 0xD1C);
SCHECK(Soldier, deathDir, 0xD20);
SCHECK(Soldier, deathVel, 0xD38);
_Static_assert(sizeof(Soldier) == 0xD50, "Soldier");
SCHECK(HumanAI, deathAnim, 0x3AD0);
SCHECK(HumanAI, kind, 0x3D38);
#undef SCHECK

#endif
