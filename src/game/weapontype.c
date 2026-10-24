#include "hook.h"
#include "igi.h"
#include "game/game.h"

ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4F1D80, void, ParamEnum_Free, (void *e));

/* only the tail of the 0x6E8 byte weapon type record from weapon.qsc */
typedef struct WeaponType {
    char unk0[0x6E0];
    void *data;
    void *data2;
} WeaponType;

extern WeaponType gWeaponTypes[];
extern int gWeaponTypeCount;
extern uint8_t gWeaponEvents[];
extern char gWeaponCategoryEnum[];
extern char gWeaponSightEnum[];
extern char gWeaponAmmoEnum[];
extern char gWeaponIdEnum[];

void WeaponType_Close(void)
{
    int i;

    ParamEnum_Free(gWeaponCategoryEnum);
    ParamEnum_Free(gWeaponSightEnum);
    ParamEnum_Free(gWeaponAmmoEnum);
    ParamEnum_Free(gWeaponIdEnum);
    for (i = 0; i < gWeaponTypeCount; i++) {
        if (gWeaponTypes[i].data2) {
            Mem_Free(gWeaponTypes[i].data2);
            gWeaponTypes[i].data2 = NULL;
        }
        if (gWeaponTypes[i].data) {
            Mem_Free(gWeaponTypes[i].data);
            gWeaponTypes[i].data = NULL;
        }
    }
    for (i = 0; i < 7; i++)
        TaskEvent_Free(gWeaponEvents[i]);
    gWeaponTypeCount = 0;
}
HOOK(0x414810, WeaponType_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, 56E258, 56E258, 0xCF34);
ALIAS(gWeaponTypes, pad_56E258, 0);
