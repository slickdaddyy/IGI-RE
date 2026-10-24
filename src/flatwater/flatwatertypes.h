#ifndef FLATWATER_FLATWATERTYPES_H
#define FLATWATER_FLATWATERTYPES_H

#include "magicobj/magicobjtypes.h"
#include "param/paramtypes.h"

typedef struct MoveRigidObj {
    RigidDynCubeObj obj;
    Vec3d keyPos[2];
    Vec3d savedKey[2];
    float alpha[2];
    float beta[2];
    float gamma[2];
    float savedAlpha[2];
    float savedBeta[2];
    float savedGamma[2];
    char model[16];
    char unk190[16];
    double value;
    double last;        /* value of the last placement */
    char unk1B0;
    char pad1B1[3];
    VarString expr;
    char registered;
    char pad205[3];
} MoveRigidObj;
_Static_assert(sizeof(MoveRigidObj) == 0x208, "MoveRigidObj");

typedef struct SoundManager {
    Task task;
    char name[32]; /* 0x20 */
    Task **sound;  /* 0x40 */
} SoundManager;

#endif
