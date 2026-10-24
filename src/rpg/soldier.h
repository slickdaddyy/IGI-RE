#ifndef RPG_SOLDIER_H
#define RPG_SOLDIER_H

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
SCHECK(SoldierRPG, deathDir, 0xD10);
SCHECK(SoldierRPG, deathVel, 0xD28);
_Static_assert(sizeof(SoldierRPG) == 0xD40, "SoldierRPG");
#undef SCHECK

#endif
