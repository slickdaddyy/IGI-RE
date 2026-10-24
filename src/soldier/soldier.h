#ifndef SOLDIER_H
#define SOLDIER_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "loadscreen/loadscreentypes.h"
#include "soldier/soldiertypes.h"
#include "humanplayer/humanplayertypes.h"
#include "humanbase/humanbasetypes.h"
#include "humanai/humanaitypes.h"

#pragma pack(push, 4)

typedef void (*TaskHandler)(void *task, void *arg);
typedef void (*TaskHandler1)(void *task);

extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])

typedef void (*HumanStateFn)(HumanSM *sm, Human *h);

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

typedef struct HumanInputMsg {
    int unk0;
    float *input;
} HumanInputMsg;

#pragma pack(pop)

#endif
