#ifndef HUMANBASE_HUMANBASETYPES_H
#define HUMANBASE_HUMANBASETYPES_H

#include "coll/colltypes.h"
#include "loadscreen/loadscreentypes.h"

typedef struct HumanAnimEvent {
    int unk0;
    int sound;
    int anim;
    float param;
    float offset[3];
} HumanAnimEvent;

typedef struct AnimSlot {
    int anim;
    int loop;
} AnimSlot;

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

#pragma pack(push, 4)
/* the "Human" task (0xCF8 bytes), base of the soldiers and the player */
typedef struct Human {
    Task task;
    double pos[3];
    char unk38[0x30];
    char anim[8];
    struct HumanMotion *motion;
    int unk74;
    Matrix matrix;
    float radius;
    char unkA4[0x28];
    int unkCC;
    char unkD0[8];
    char frozen;
    char unkD9[7];
    double placePos[3];
    float yaw;
    int team;
    char modelName[0x10];
    float prevInput[40];
    float input[40];
    char inputLocked;
    char unk251[3];
    HumanDamage damage;
    HumanSM sm;
    struct AnimSlot *animSlots;
    char deadState;             /* set in the dead state */
    char crouch;
    char unk33E[2];
    int ammo[0x61];
    Task *weapons[3];
    char weaponReady;
    char fire;
    char altFire;
    char fired;
    float aimError;
    char grenadeTicking;
    char unk4D9[3];
    int grenadeFuse;
    int cookTime;
    int grenadeSound;
    int meleeCount;
    struct HumanView *view;
    int camMode;
    char camToggle;
    char unk4F5[3];
    float rotX, baseX, rotY, baseY, rotZ, baseZ;
    char frozenView;
    char unk511[3];
    float boneOffset[3];
    float delta[3];
    char deltaScaled;
    char unk52D[3];
    float unk530;
    HumanAnimEvent animEvent;
    char unk550[0xFC];
    char look;
    char unk64D[3];
    int unk650[5];
    float vel[3];
    float push[3];
    float slide[3];
    float outVel[3];
    char unk694;
    char unk695;
    char unk696;
    char unk697;
    char unk698;
    char unk699[7];
    CollHit ground;
    double groundDistance;
    float slopeAngle;
    float slope;
    double roofDistance;
    float movePos[3];
    Matrix moveMatrix;
    float moveAngle[3];
    Task **groundRef;
    Task *attach;
    double attachPos[3];
    Matrix attachMatrix;
    Task **attachedRef;
    Task **climbRef;            /* ladder, wire or stationary gun in use */
    char unk7B8;
    char unk7B9;
    char unk7BA[2];
    int unk7BC;
    int index;
    int unk7C4;
    char deleteMe;
    char unk7C9[3];
    void *shadow;
    double groundPos[3];
    int unk7E8;
    int groundValid;
    float groundDist;
    float groundUp[3];
    struct HumanAI *ai;
    char unk804[4];
    double pathPrev[3];
    double pathCur[3];
    Task **bloodRef;
    Task **dropRef;
    Task **voiceRef;
    int voiceTime;
    int missSoundTime;
    unsigned footstep;
    int textCount;
    struct HumanText *texts[32];
    char lines[32][0x21];
    char scriptVars;
    char dead;
    char vulnerable;
    char unkCF7;
} Human;
#pragma pack(pop)
_Static_assert(offsetof(Human, damage) == 0x254, "Human");
_Static_assert(offsetof(Human, ground) == 0x6A0, "Human");
_Static_assert(offsetof(Human, ai) == 0x800, "Human");
_Static_assert(sizeof(Human) == 0xCF8, "Human");

#endif
