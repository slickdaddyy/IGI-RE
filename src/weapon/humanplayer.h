#ifndef WEAPON_HUMANPLAYER_H
#define WEAPON_HUMANPLAYER_H

#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "loadscreen/loadscreentypes.h"
#include "humanplayer/humanplayertypes.h"

typedef struct HumanPlayer HumanPlayer;

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
typedef struct ClimbObj {
    char unk00[0x20];
    Vec3d pos;
    char unk38[0x38];
    float matrix[9];
    char unk94[0x9C];
    char unk130;
    char unk131[7];
    int steps;
} ClimbObj;

typedef struct HumanView {
    char unk00[0x68];
    char anim[0x90];
    float matrix[9];
} HumanView;

/* input channels 23 - 31 are the weapon category keys */
#define INPUT_WEAPON 13
#define INPUT_CATEGORY 23

_Static_assert(__builtin_offsetof(HumanPlayer, damage.hitZone) == 0x2E8, "hitZone");
_Static_assert(__builtin_offsetof(HumanPlayer, weapons) == 0x4C4, "weapons");
_Static_assert(__builtin_offsetof(HumanPlayer, vel) == 0x664, "vel");
_Static_assert(__builtin_offsetof(HumanPlayer, ladderTop) == 0xD7C, "ladderTop");
_Static_assert(__builtin_offsetof(ClimbObj, steps) == 0x138, "ClimbObj");
_Static_assert(__builtin_offsetof(HumanView, matrix) == 0xF8, "HumanView");

#endif
