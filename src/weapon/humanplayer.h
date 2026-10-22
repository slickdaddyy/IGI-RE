#ifndef WEAPON_HUMANPLAYER_H
#define WEAPON_HUMANPLAYER_H

#include <stdint.h>

#include "igi.h"

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct HumanPlayer HumanPlayer;

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

/* The player's movement state machine. Changing state sets prev = state,
   state = n, update = enter[n] and calls it. */
typedef struct HumanState {
    int unk00;
    void (**enter)(struct HumanState *s, HumanPlayer *h);
    uint32_t input;
    int prev;
    int state;
    void (*update)(struct HumanState *s, HumanPlayer *h);
} HumanState;

#define HUMAN_DEAD 0x20000

enum {
    HSTATE_UNK29 = 0x1D,
    HSTATE_LADDER = 0x25,
    HSTATE_LADDER_FROM_TOP = 0x26,
    HSTATE_UNK41 = 0x29,
    HSTATE_WIRE = 0x2A,
    HSTATE_ZIPLINE = 0x2B,
    HSTATE_DIE = 0x2F,
};

/* An object the player can attach to (ladder, wire...): position and matrix. */
typedef struct HumanAttach {
    char unk00[0x20];
    Vec3d pos;
    char unk38[0x38];
    float matrix[9];
    char unk94[0x9C];
    char unk130;
    char unk131[7];
    int steps;
} HumanAttach;

typedef struct HumanView {
    char unk00[0x68];
    char anim[0x90];
    float matrix[9];
} HumanView;

/* The "HumanPlayer" task, 0xDF0 bytes. */
struct HumanPlayer {
    char task[0x20];
    Vec3d pos;
    char unk038[0x40];
    float matrix[9];
    char unk09C[0x4];
    float radius;
    char unk0A4[0x58];
    int unk0FC;
    char unk100[0x10];
    float prevInput[40];
    float input[40];
    char unk250[0x4];
    HumanDamage damage;
    HumanState state;
    char unk304[0x1C0];
    void *weapons[3];
    char unk4D0[0x18];
    int meleeCount;
    HumanView *view;
    char unk4F0[0x3C];
    char noclip;
    char unk52D[0x137];
    float vel[3];
    float push[3];
    char unk67C[0x679];
    char quiet;
    char vulnerable;
    char unkCF7[0x59];
    int weaponCategory;
    int weaponCycle;
    int weaponAnim;
    int weaponAnimReq;
    int barMax;
    int barValue;
    char showBar;
    char unkD69[0xF];
    char ladderEnter;
    char onLadder;
    char ladderLeave;
    char ladderFromTop;
    int ladderTop;
    int ladderPos;
    char unkD84[0xC];
    char unkD90;
    char unkD91;
    char unkD92;
    char wireEnter;
    char onWire;
    char unkD95[0xB];
    char ziplineEnter;
    char onZipline;
    char unkDA2[0x2];
    float ziplineDir[3];
    float ziplineSpeed;
    int ziplineSound;
    char unkDB8[0xC];
    int earSound;
    float earLevel;
    float hitFlash[4];
    float flash;
    int fenceTimer;
    void *savedWeapon;
    void *attacker;
    char unkDEC[0x4];
};

/* input channels 23 - 31 are the weapon category keys */
#define INPUT_WEAPON 13
#define INPUT_CATEGORY 23

_Static_assert(sizeof(HumanPlayer) == 0xDF0, "HumanPlayer");
_Static_assert(__builtin_offsetof(HumanPlayer, damage.hitZone) == 0x2E8, "hitZone");
_Static_assert(__builtin_offsetof(HumanPlayer, weapons) == 0x4C4, "weapons");
_Static_assert(__builtin_offsetof(HumanPlayer, vel) == 0x664, "vel");
_Static_assert(__builtin_offsetof(HumanPlayer, ladderTop) == 0xD7C, "ladderTop");
_Static_assert(__builtin_offsetof(HumanAttach, steps) == 0x138, "HumanAttach");
_Static_assert(__builtin_offsetof(HumanView, matrix) == 0xF8, "HumanView");

#endif
