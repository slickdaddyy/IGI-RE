#ifndef WEAPON_WEAPON_H
#define WEAPON_WEAPON_H

#include <stdint.h>

#include "igi.h"
#include "script/scripttypes.h"
#include "weapon/weapontypes.h"

#define WEAPON_TYPE_MAX 30
#define TASK_NO_TYPE 0x181

typedef void (*TaskHandler)(void *task, void *arg);

extern int gWeaponTypeCount;

WeaponType *Weapon_GetType(void *task);
void *Task_FindWeapon(void *task, int id);

#endif
