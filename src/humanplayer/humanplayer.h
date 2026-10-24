#ifndef HUMANPLAYER_H
#define HUMANPLAYER_H

#include <stddef.h>
#include <stdint.h>

#include "script/scripttypes.h"
#include "humanplayer/humanplayertypes.h"
#include "weapon/weapontypes.h"
#include "ammotype/ammotypetypes.h"
#include "viewcam/viewcamtypes.h"

typedef void (*StateFn)(HumanPlayerSM *sm, HumanPlayer *h);

enum {
    SM_ANIM_DONE = 0x1,
    SM_ANIM_EVENT = 0x2,
    SM_HIT = 0x10000,
    SM_DEAD = 0x20000,
};

/* controls, indices into HumanPlayer.input */
enum {
    INPUT_LEFT = 2,
    INPUT_RIGHT = 3,
    INPUT_FORWARD = 4,
    INPUT_BACK = 5,
    INPUT_FIRE = 6,
    INPUT_ALT_FIRE = 7,
    INPUT_RELOAD = 8,
    INPUT_USE = 9,
    INPUT_LEAN = 12,
    INPUT_WEAPON = 13,
    INPUT_TOGGLE_D45 = 20,
    INPUT_NEXT_WEAPON = 21,
    INPUT_PREV_WEAPON = 22,
    INPUT_MAP = 32,
    INPUT_FLY = 33,
    INPUT_BINOCULARS = 34,
    INPUT_COUNT = 40,
};

typedef struct WeaponAmmo {
    int clip;
    int reserve;
} WeaponAmmo;

#endif
