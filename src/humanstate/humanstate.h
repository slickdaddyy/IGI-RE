#ifndef HUMANSTATE_H
#define HUMANSTATE_H

#include <stddef.h>
#include <stdint.h>

#include "humanstate/humanstatetypes.h"

typedef struct Human Human;
typedef struct HumanState HumanState;
typedef void (*HumanStateFn)(HumanState *sm, Human *h);

/* HumanState.input */
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

#define HPAD(from, to) char pad##from[(to) - (from)]

struct Human {
    HPAD(0x0, 0x20);
    double pos[3];
    HPAD(0x38, 0x68);
    char anim[0x10];
    float rot[9];
    HPAD(0x9C, 0xFC);
    int unkFC;
    HPAD(0x100, 0x134);
    float unk134;
    HPAD(0x138, 0x1D4);
    float unk1D4;
    HPAD(0x1D8, 0x2EC);
    HumanState sm;
    HPAD(0x304, 0x33D);
    char unk33D;
    HPAD(0x33E, 0x4D0);
    char unk4D0;
    HPAD(0x4D1, 0x4EC);
    void *view;
    HPAD(0x4F0, 0x514);
    char unk514[0xC];
    float move[3];
    HPAD(0x52C, 0x538);
    int unk538;
    int unk53C;
    float unk540;
    HPAD(0x544, 0x664);
    float vel[3];
    float unk670[3];
    HPAD(0x67C, 0x6A8);
    double unk6A8[3];
    HPAD(0x6C0, 0x6EC);
    char *ground;
    HPAD(0x6F0, 0x728);
    float unk728[3];
    HPAD(0x734, 0x768);
    void **unk768;
    HPAD(0x76C, 0x7BC);
    int unk7BC;
    HPAD(0x7C0, 0xD44);
    char unkD44;
    HPAD(0xD45, 0xD60);
    int actionFrames;
    int actionFrame;
    char unkD68;
    HPAD(0xD69, 0xD70);
    void *actionSound;
    int useStage;
    char unkD78;
    char unkD79;
    char unkD7A;
    char unkD7B;
    int ladderTop;
    int ladderPos;
    int ladderDir;
    float ladderStep;
    int ladderSound;
    char unkD90;
    char unkD91;
    char unkD92;
    char unkD93;
    HPAD(0xD94, 0xD98);
    float wireStep;
    int wireSound;
    char unkDA0;
    HPAD(0xDA1, 0xDA4);
    float wireDir[3];
    float wireSpeed;
    void *wireSlideSound;
};

void HumanPlayer_SetFlagD44(Human *h);
void HumanPlayer_SetFlagD68(Human *h);
void HumanPlayer_ClearFlagD44(Human *h);
void HumanPlayer_ClearFlagD68(Human *h);
void HumanPlayer_PicklockNoise(void *obj, Human *h);
void HumanPlayer_StartAction(Human *h, int action);

#define HCHECK(f, off) _Static_assert(offsetof(Human, f) == (off), #f)
HCHECK(pos, 0x20);
HCHECK(rot, 0x78);
HCHECK(unk1D4, 0x1D4);
HCHECK(sm, 0x2EC);
HCHECK(unk33D, 0x33D);
HCHECK(view, 0x4EC);
HCHECK(move, 0x520);
HCHECK(unk540, 0x540);
HCHECK(vel, 0x664);
HCHECK(ground, 0x6EC);
HCHECK(unk768, 0x768);
HCHECK(unk7BC, 0x7BC);
HCHECK(unkD44, 0xD44);
HCHECK(actionFrames, 0xD60);
HCHECK(actionSound, 0xD70);
HCHECK(ladderTop, 0xD7C);
HCHECK(ladderSound, 0xD8C);
HCHECK(unkD93, 0xD93);
HCHECK(wireStep, 0xD98);
HCHECK(unkDA0, 0xDA0);
HCHECK(wireSlideSound, 0xDB4);
#undef HCHECK

#endif
