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

/* a weapon type of weapons.qsc (0x6E8 bytes) */
typedef struct WeaponType {
    int id;
    int unk004;
    int unk008;
    int unk00C;
    char name[0x100];
    char unk110[0x100];     /* text shown on the HUD */
    char unk210[0x100];
    char unk310[0x100];
    char unk410[0x100];
    char unk510[0x100];
    int category;
    int sightDisplay;
    int ammoDisplay;
    char key[0x10];         /* model and physics type name */
    char casing[0x10];      /* empty casing model */
    int caliber;
    union {                 /* the script fills the arrays in loops */
        int unk640[4];
        struct {
            int pellets;
            int rate;       /* shots per minute */
            int clipSize;
            int burst;
        };
    };
    float range;
    union {
        float unk654[4];
        struct {
            float pad654[3];
            float throwSpeed;
        };
    };
    union {
        float unk664[6];
        struct {
            float spread[2];        /* degrees: first shots, then the rest */
            float recoilPitch;      /* degrees */
            float recoilYaw;
            float recoilPitchRand;
            float recoilYawRand;
        };
    };
    int anim[6];
    int anim2[7];
    char sound[0x10];
    char sound2[0x10];
    float detectRange;
    int taskType;
    int taskType2;
    char unk6DC;
    char pad6DD[3];
    void *data;
    void *data2;
} WeaponType;
_Static_assert(offsetof(WeaponType, spread) == 0x664, "WeaponType");
_Static_assert(offsetof(WeaponType, sound2) == 0x6C0, "WeaponType");
_Static_assert(sizeof(WeaponType) == 0x6E8, "WeaponType");

#endif
