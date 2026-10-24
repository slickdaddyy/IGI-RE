#ifndef WEAPON_WEAPON_H
#define WEAPON_WEAPON_H

#include <stdint.h>

#include "igi.h"
#include "script/scripttypes.h"
#include "weapon/weapontypes.h"

#define WEAPON_TYPE_MAX 30
#define TASK_NO_TYPE 0x181

/* One DefineWeaponType() call from weapons/<name>/weapon.qsc. The fields
   are in script argument order from 0x610 on, see the notes. */
typedef struct WeaponType {
    int id;
    int unk004;
    int unk008;
    int unk00C;
    char name[0x100];
    char unk110[0x100];
    char unk210[0x100];
    char unk310[0x100];
    char unk410[0x100];
    char unk510[0x100];
    int category;
    int sightDisplay;
    int ammoDisplay;
    char key[0x10];
    char unk62C[0x10];
    int unk63C;
    int unk640[4];
    float unk650;
    float unk654[4];
    float unk664[6];
    int anim[6];
    int anim2[7];
    char unk6B0[0x10];
    char unk6C0[0x10];
    float detectRange;
    int taskType;
    int taskType2;
    char unk6DC;
    char pad6DD[3];
    void *data;
    void *data2;
} WeaponType;

_Static_assert(sizeof(WeaponType) == 0x6E8, "WeaponType");

typedef void (*TaskHandler)(void *task, void *arg);

extern TaskHandler gTaskHandlers[][0x180];
extern WeaponType gWeaponTypes[];
extern int gWeaponTypeCount;

WeaponType *Weapon_GetType(void *task);
void *Task_FindWeapon(void *task, int id);

#endif
