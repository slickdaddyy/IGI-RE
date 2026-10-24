#ifndef GUNCASING_GUNCASINGTYPES_H
#define GUNCASING_GUNCASINGTYPES_H

#include "magicobj/magicobjtypes.h"
#include "weapon/weapontypes.h"

typedef struct Mine {
    RigidDynCubeObj o;
    Vec3d placePos;     /* 0xF0 */
    Matrix placeMatrix; /* 0x108 */
    char unk130[0x40];
    char model[0x20];   /* 0x170 */
    char exploded;      /* 0x190 */
    char unk191[0x53];
    char explode;       /* 0x1E4 */
    char unk1E5[0x53];
    int armDelay;       /* 0x238, frames */
    int countdown;      /* 0x23C */
    float delay;        /* 0x240, seconds */
    float radius;       /* 0x244, meters */
    char triggered;     /* 0x248 */
    char remote;        /* 0x249 */
    char unk24A[6];
} Mine;
_Static_assert(sizeof(Mine) == 0x250, "Mine");

typedef struct Gun {
    RigidDynCubeObj o;
    char inWorld;       /* 0xF0 */
    char unkF1;
    char unkF2[2];
    char unkF4;
    char unkF5[3];
    int unkF8;
    char remove;        /* 0xFC */
    char unkFD[3];
    int id;             /* 0x100 */
    char name[0x20];    /* 0x104 */
    int16_t unk124;
    char unk126[2];
    WeaponType *wt;     /* 0x128 */
    struct AmmoType *ammoType; /* 0x12C */
    Task **shots;       /* 0x130 */
    char firing;        /* 0x134 */
    char unk135[3];
    int lastShot;       /* 0x138, frame */
    int cooldown;       /* 0x13C */
    int burst;          /* 0x140 */
    int rounds;         /* 0x144 */
    float spin;         /* 0x148 */
    char unk14C[4];
    Vec3d muzzle;       /* 0x150 */
    Vec3f dir;          /* 0x168 */
    char unk174[4];
    Vec3d point;        /* 0x178 */
    Task **unk190;
    Task **loopSound;   /* 0x194 */
    float recoil;       /* 0x198 */
    int unk19C;
    int unk1A0;
    int unk1A4;
    int unk1A8;
    int reload;         /* 0x1AC */
} Gun;
_Static_assert(sizeof(Gun) == 0x1B0, "Gun");

#endif
