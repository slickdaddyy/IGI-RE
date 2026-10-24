#ifndef SOLDIER_SOLDIERTYPES_H
#define SOLDIER_SOLDIERTYPES_H

#include <stdint.h>

#include "loadscreen/loadscreentypes.h"
#include "humanbase/humanbasetypes.h"

typedef struct SoldierSM SoldierSM;
typedef struct Soldier Soldier;
typedef void (*SoldierStateFn)(SoldierSM *sm, Soldier *h);
/* same layout as the player's state machine */
typedef struct SoldierSM {
    SoldierStateFn input;
    SoldierStateFn *states;
    uint32_t bits;
    int prev;
    int state;
    SoldierStateFn run;
    char unk18[0x34];
} SoldierSM;

typedef struct SoldierHit {
    int unk00;
    float damage;
    float dir[3];
    void *attacker;
    int *weapon;
} SoldierHit;

/* message 0xF sent to the AI when the soldier is hurt */
typedef struct SoldierAIMsg {
    int type;
    int unk4;
    int unk8;
    int unkC;
    void *data;
    void *attacker;
    void *attacker2;
} SoldierAIMsg;

typedef struct SoldierBlast {
    int unk00;
    double *pos;
    int unk08;
    float radius;
    float unk10;
    int shakeFrames;
} SoldierBlast;

typedef struct SoldierPunch {
    float damage;
    void *attacker;
} SoldierPunch;

typedef struct SoldierInputMsg {
    int unk0;
    float *input;
} SoldierInputMsg;

/* sent to the used task through the slot of 0x416800 */
typedef struct UseMsg {
    Soldier *h;
    int unk4;
    double pos[3];
    int unk534[7];
    float *input;
    float *prevInput;
    int unk44;
} UseMsg;

/* same layout as the player's state machine */
struct Human;

/* the part every soldier type shares */
typedef struct Soldier {
    Human h;
    int model;
    int idleAnim;
    char run;
    char spawned;
    char unkD02[2];
} Soldier;
_Static_assert(sizeof(Soldier) == 0xD04, "Soldier");

#pragma pack(push, 4)
/* HumanSoldier and HumanSoldierPistol, 0xD50 bytes */
typedef struct HumanSoldier {
    Soldier s;
    float hitLimit;
    float hitSum;
    int scriptAnim;
    int useTarget;
    int hitFrames;
    char thrown;
    char meleeDone;
    char unkD1A[2];
    int deathKind;
    double deathDir[3];
    double deathVel[3];
} HumanSoldier;

/* HumanSoldierFemale, 0xD50 bytes */
typedef struct SoldierF {
    Soldier s;
    float hitLimit;
    float hitSum;
    int scriptAnim;
    int useTarget;
    char unkD14[4];
    int deathKind;
    char unkD1C[4];
    double deathDir[3];
    double deathVel[3];
} SoldierF;
#pragma pack(pop)
_Static_assert(offsetof(HumanSoldier, scriptAnim) == 0xD0C, "HumanSoldier");
_Static_assert(offsetof(HumanSoldier, thrown) == 0xD18, "HumanSoldier");
_Static_assert(offsetof(HumanSoldier, deathDir) == 0xD20, "HumanSoldier");
_Static_assert(offsetof(HumanSoldier, deathVel) == 0xD38, "HumanSoldier");
_Static_assert(sizeof(HumanSoldier) == 0xD50, "HumanSoldier");
_Static_assert(offsetof(SoldierF, deathKind) == 0xD18, "SoldierF");
_Static_assert(offsetof(SoldierF, deathVel) == 0xD38, "SoldierF");
_Static_assert(sizeof(SoldierF) == 0xD50, "SoldierF");

#endif
