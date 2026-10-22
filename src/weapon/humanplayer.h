#ifndef WEAPON_HUMANPLAYER_H
#define WEAPON_HUMANPLAYER_H

#include <stdint.h>

#include "igi.h"

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

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
    int lastPart;
} HumanDamage;

typedef struct HumanState {
    void *unk00;
    void (**handlers)(struct HumanState *s, HumanPlayer *hp);
    uint32_t flags;
    int prev;
    int cur;
    void (*fn)(struct HumanState *s, HumanPlayer *hp);
} HumanState;

/* An object the player can attach to (ladder, wire...): position and matrix. */
typedef struct HumanAttach {
    char unk00[0x20];
    Vec3d pos;
    char unk38[0x38];
    float matrix[9];
    char unk94[0x9C];
    char unk130;
    char unk131[7];
    int unk138;
} HumanAttach;

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
    char unk100[0x8];
    float prevInput[40];
    float input[40];
    char unk248[0xC];
    HumanDamage damage;
    HumanState state;
    char unk304[0x1C0];
    char unk4C4[0x24];
    int stance;
    char *body;
    char unk4F0[0x3C];
    char noclip;
    char unk52D[0x137];
    float push[6];
    char unk67C[0x679];
    char quiet;
    char vulnerable;
    char unkCF7[0x59];
    int weaponCategory;
    int weaponCycle;
    int weaponAnim;
    int unkD5C;
    int barMax;
    int barValue;
    char showBar;
    char unkD69[0xF];
    char ladderEnter;
    char onLadder;
    char ladderLeave;
    char ladderFromTop;
    int unkD7C;
    int unkD80;
    char unkD84[0xC];
    char unkD90;
    char unkD91;
    char unkD92;
    char unkD93;
    char unkD94;
    char unkD95[0xB];
    char wireEnter;
    char onWire;
    char unkDA2[0x2];
    float wireDir[3];
    int unkDB0;
    int wireSound;
    char unkDB8[0xC];
    int earSound;
    float earLevel;
    float hitFlash[4];
    float flash;
    int shockTimer;
    int unkDE4;
    void *attacker;
    char unkDEC[0x4];
};

_Static_assert(sizeof(HumanPlayer) == 0xDF0, "HumanPlayer");
_Static_assert(__builtin_offsetof(HumanAttach, unk138) == 0x138, "HumanAttach");

#endif
