#ifndef HUMANPLAYER_HUMANPLAYERTYPES_H
#define HUMANPLAYER_HUMANPLAYERTYPES_H

#include <stdint.h>

#include "types.h"
#include "task/tasktypes.h"
#include "coll/colltypes.h"
#include "loadscreen/loadscreentypes.h"
#include "humanbase/humanbasetypes.h"

typedef struct HumanMotion {
    int unk0;
    uint32_t flags;
    char unk8[0x10];
    Vec3f bones[1];     /* 0x84 bytes apart */
    char unk24[0x28];
    int unk4C[5];
    char unk60[0x24C];
    float eye[3];
    float matrix[10];
    char unk2E0[0x578];
    float throwPos[3];
} HumanMotion;
_Static_assert(offsetof(HumanMotion, eye) == 0x2AC, "HumanMotion");
_Static_assert(offsetof(HumanMotion, throwPos) == 0x858, "HumanMotion");
_Static_assert(sizeof(HumanMotion) == 0x864, "HumanMotion");

struct HumanPlayer;

/* State machine used for the player's body and weapon. update() turns the
   controls into input bits, then the current state's run function is called. */
typedef struct HumanPlayerSM {
    void (*update)(struct HumanPlayerSM *sm, struct HumanPlayer *h);
    void (**states)(struct HumanPlayerSM *sm, struct HumanPlayer *h);
    uint32_t input;
    int prev;
    int state;
    void (*run)(struct HumanPlayerSM *sm, struct HumanPlayer *h);
    int combo_count;
    int combo;
    void *combos;
    uint8_t combo_progress[0x28];
} HumanPlayerSM;
_Static_assert(sizeof(HumanPlayerSM) == 0x4C, "HumanPlayerSM");

/* the "HumanPlayer" task (0xDF0 bytes): a Human with the player's state machines */
typedef struct HumanPlayer {
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
    HumanPlayerSM sm;
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
    HumanPlayerSM weapon;
    char weaponLock;
    char unkD45;
    char unkD46[2];
    int cycleIndex;
    int nextWeapon;
    int weaponCategory;
    int weaponCycle;
    int weaponAnim;
    int weaponAnimReq;
    int actionFrames;           /* shown as a bar */
    int actionFrame;
    char showBar;
    char unkD69[3];
    int medipackDelay;
    void *actionSound;
    int useStage;
    char ladderEnter;
    char onLadder;
    char ladderLeave;
    char ladderFromTop;
    int ladderTop;
    int ladderPos;
    int ladderDir;
    float ladderStep;
    int ladderSound;
    char unkD90;
    char unkD91;
    char unkD92;
    char wireEnter;
    char onWire;
    char unkD95[3];
    float wireStep;
    int wireSound;
    char ziplineEnter;
    char onZipline;
    char unkDA2[2];
    float ziplineDir[3];
    float ziplineSpeed;
    void *ziplineSound;
    char unkDB8[4];
    Task **unkDBC;
    float unkDC0;
    int earSound;
    float earLevel;
    float hitFlash[4];
    float flash;
    int fenceTimer;
    Task *savedWeapon;
    Task **attacker;
    char unkDEC[4];
} HumanPlayer;
_Static_assert(offsetof(HumanPlayer, weapon) == 0xCF8, "HumanPlayer");
_Static_assert(offsetof(HumanPlayer, actionSound) == 0xD70, "HumanPlayer");
_Static_assert(offsetof(HumanPlayer, ziplineDir) == 0xDA4, "HumanPlayer");
_Static_assert(sizeof(HumanPlayer) == 0xDF0, "HumanPlayer");

#endif
