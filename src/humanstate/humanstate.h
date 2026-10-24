#ifndef HUMANSTATE_H
#define HUMANSTATE_H

#include <stddef.h>
#include <stdint.h>

#include "humanbase/humanbasetypes.h"
#include "humanplayer/humanplayertypes.h"

typedef void (*HumanStateFn)(HumanPlayerSM *sm, HumanPlayer *h);

/* HumanPlayerSM.input */
#define HUMAN_ANIM_DONE 0x1
#define HUMAN_ON_GROUND 0x4
#define HUMAN_RUN       0x2010
#define HUMAN_RIGHT     0x20
#define HUMAN_LEFT      0x40
#define HUMAN_FORWARD   0x80
#define HUMAN_BACK      0x100
#define HUMAN_UNK10000  0x10000
#define HUMAN_DEAD      0x20000
#define HUMAN_CROUCH    0x40000

enum {
    HSTATE_STAND = 0,
    HSTATE_WALK_FORWARD = 1,
    HSTATE_WALK_FORWARD_LEFT = 2,
    HSTATE_WALK_FORWARD_RIGHT = 3,
    HSTATE_WALK_BACK = 4,
    HSTATE_WALK_BACK_LEFT = 5,
    HSTATE_WALK_BACK_RIGHT = 6,
    HSTATE_WALK_LEFT = 7,
    HSTATE_WALK_RIGHT = 8,
    HSTATE_CROUCH_FORWARD = 9,
    HSTATE_CROUCH_FORWARD_LEFT = 10,
    HSTATE_CROUCH_FORWARD_RIGHT = 11,
    HSTATE_CROUCH_BACK = 12,
    HSTATE_CROUCH_BACK_LEFT = 13,
    HSTATE_CROUCH_BACK_RIGHT = 14,
    HSTATE_CROUCH_LEFT = 15,
    HSTATE_CROUCH_RIGHT = 16,
    HSTATE_RUN_STAND = 19,
    HSTATE_RUN_FORWARD = 20,
    HSTATE_RUN_FORWARD_LEFT = 21,
    HSTATE_RUN_FORWARD_RIGHT = 22,
    HSTATE_RUN_BACK = 23,
    HSTATE_RUN_BACK_LEFT = 24,
    HSTATE_RUN_BACK_RIGHT = 25,
    HSTATE_RUN_LEFT = 26,
    HSTATE_RUN_RIGHT = 27,
    HSTATE_JUMP = 28,
    HSTATE_FALL = 29,
    HSTATE_PICKLOCK = 30,
    HSTATE_USE31 = 31,
    HSTATE_TYPE_COMPUTER = 32,
    HSTATE_USE33 = 33,
    HSTATE_USE34 = 34,
    HSTATE_USE35 = 35,
    HSTATE_USE36 = 36,
    HSTATE_LADDER = 37,
    HSTATE_LADDER_ON = 38,
    HSTATE_LADDER_OFF_TOP = 39,
    HSTATE_LADDER_SLIDE = 40,
    HSTATE_UNK41 = 41,
    HSTATE_WIRE_DRAG = 42,
    HSTATE_ZIPLINE = 43,
    HSTATE_UNK46 = 46,
    HSTATE_DIE = 47,
};

void HumanPlayer_SetFlagD44(HumanPlayer *h);
void HumanPlayer_SetFlagD68(HumanPlayer *h);
void HumanPlayer_ClearFlagD44(HumanPlayer *h);
void HumanPlayer_ClearFlagD68(HumanPlayer *h);
void HumanPlayer_PicklockNoise(void *obj, HumanPlayer *h);
void HumanPlayer_StartAction(HumanPlayer *h, int action);

#endif
