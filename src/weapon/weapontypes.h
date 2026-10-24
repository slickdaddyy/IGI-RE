#ifndef WEAPON_WEAPONTYPES_H
#define WEAPON_WEAPONTYPES_H

typedef struct WeaponCategory {
    int count;
    int ids[8];
} WeaponCategory;

/* What a weapon task type's init handler gets: its private data and the
   rest of the DefineWeaponType() arguments, starting at index next. */
typedef struct WeaponInitArgs {
    void *data;
    void *args;
    int next;
} WeaponInitArgs;

#endif
