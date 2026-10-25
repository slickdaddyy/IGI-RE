#ifndef HUMANPLAYER_HUMANPLAYERVARS_H
#define HUMANPLAYER_HUMANPLAYERVARS_H

#include <stdint.h>

#include "weapon/weapontypes.h"

extern uint16_t gHumanPlayerType;
extern void *gHumanPlayerParams;
extern WeaponCategory gWeaponCategories[];
extern float gMoveScale;
extern struct QTaskList *gHumanPlayerList;

#endif
