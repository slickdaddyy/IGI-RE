#ifndef PISTOL_PISTOL_H
#define PISTOL_PISTOL_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "loadscreen/loadscreentypes.h"
#include "soldier/soldiertypes.h"
#include "humanplayer/humanplayertypes.h"
#include "humanai/humanaitypes.h"

typedef void (*TaskHandler)(void *task, void *arg);

extern void *gTaskHandlers[][0x180];
#define TASK_HANDLER(slot, type) (((TaskHandler *)gTaskHandlers)[(slot) * 0x180 + (type)])

#pragma pack(push, 4)

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

#pragma pack(pop)

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
