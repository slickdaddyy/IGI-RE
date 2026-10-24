#ifndef SOLDIER_SOLDIERTYPES_H
#define SOLDIER_SOLDIERTYPES_H

#include <stdint.h>

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

/* Damage per body part. Hits add mult * damage to a part and to total. */
typedef struct SoldierDamage {
    float total;
    float max;
    float unk08;
    struct {
        float dmg;
        float mult;
    } part[17];
    int hitZone;
} SoldierDamage;

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

typedef struct HumanSM {
    void (*input)(struct HumanSM *sm, struct Human *h);
    void (**states)(struct HumanSM *sm, struct Human *h);
    uint32_t bits;
    int prev;
    int state;
    void (*run)(struct HumanSM *sm, struct Human *h);
    char unk18[0x34];
} HumanSM;

#endif
