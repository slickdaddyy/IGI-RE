#ifndef STATGUN_STATGUNTYPES_H
#define STATGUN_STATGUNTYPES_H

#include "types.h"
#include "task/tasktypes.h"
#include "magicobj/magicobjtypes.h"

typedef struct StatGun {
    RigidDynCubeObj obj;
    Vec3d basePos;          /* 0xF0, script parameter */
    Matrix baseMatrix;      /* 0x108, script parameter */
    Vec3d relPos;           /* 0x130, base in the frame of the carrier */
    Matrix relMatrix;       /* 0x148 */
    Vec3d muzzlePos;        /* 0x170 */
    Matrix muzzleMatrix;    /* 0x188 */
    char model[0x10];       /* 0x1B0 */
    char weapon[0x20];      /* 0x1C0, weapon id expression */
    float maxPitch;         /* 0x1E0, degrees */
    float minPitch;
    float yawRange;
    int ammo;
    float recoilPitch;      /* 0x1F0 */
    float recoilYaw;
    float pitch;            /* 0x1F8, radians */
    float yaw;
    float outPitch;         /* 0x200, pitch and yaw with recoil */
    float outYaw;
    struct Human *user;            /* 0x208 */
    Task *slots[3];         /* 0x20C */
    int lastTick;           /* 0x218 */
    char used;              /* 0x21C */
    char pad21D[3];
} StatGun;
_Static_assert(sizeof(StatGun) == 0x220, "StatGun");

typedef struct WeaponUseMsg {
    char fire;
    char shot;
    char unk2[2];
    double *eye;
    Matrix *view;
    float *vel;
    int unk10;
    int active;
    int alt;
    int zoomed;
    float recoilPitch;
    float recoilYaw;
    float aimError;
    struct Human *h;
} WeaponUseMsg;

#endif
