#ifndef HUMANBASE_H
#define HUMANBASE_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

typedef void (*TaskHandler)(void *task, void *arg);

/* gTaskHandlers[slot][type] */
#define gTaskHandlers ((TaskHandler (*)[0x180])0xA96AE0)

typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *children;
    void *unkC;
    void *unk10;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef struct HumanText {
    void *grid;
    float x, y, z;
    char unk10[0x14];
    unsigned int flags;
} HumanText;

typedef struct HumanView {
    char unk0[0x20];
    double pos[3];
    char unk38[0x38];
    char *model;
    char unk74[0x65];
    char firstPerson;
    char unkDA[0x5A];
    float unk134;
    char unk138[2];
    char camOverride;
    char unk13B[5];
    double camPos[3];
    float camMatrix[10];
    char unk180[0x64];
    float fov;
    int unk1E8;
} HumanView;

/* a hit of the collision queries, 0x70 bytes */
typedef struct CollHit {
    Task *task;
    int unk4;
    double pos[3];
    char unk20[0x18];
    float normal[3];
    int unk44;
    int unk48;
    void *material;
    void *material2;
    int unk54;
    int found;
    int unk5C;
    double dist;
    char unk68[8];
} CollHit;

typedef struct HumanPath {
    char unk0[0x14];
    void *unk14;
    char unk18[0x4C];
    char unk64;
} HumanPath;

/* the AI brain of a soldier (Human.ai) */
typedef struct HumanAI {
    char unk0[0xE0];
    Task *route[3];
    char unkEC[0x39D0];
    char immortal;
    char unk3ABD[0xAF];
    HumanPath *path;
    char unk3B70[0x2BC];
    int node;
} HumanAI;

/* damage per body part; hits add mult * damage to a part and to total */
typedef struct HumanDamage {
    float total;
    float max;
    float unk8;
    struct {
        float dmg;
        float mult;
    } part[17];
    int hitZone;
} HumanDamage;

typedef struct HumanSM {
    void *update;
    void *states;
    uint32_t bits;
    int prev;
    int state;
    void *run;
    char unk18[0x34];
} HumanSM;

#define HUMAN_ON_GROUND 0x4
#define HUMAN_ROOF      0x10
#define HUMAN_DEAD      0x20000

/* filled by the animation code (0x4D3210) when a sound event is due */
typedef struct HumanAnimEvent {
    int unk0;
    int sound;
    int unk8[2];
    float offset[3];
} HumanAnimEvent;

/* Base of every Human task type (players and AI), 0xCF8 bytes. */
typedef struct Human {
    Task task;
    double pos[3];
    char unk38[0x30];
    char anim[8];
    char *motion;
    int unk74;
    float matrix[10];
    float radius;
    char unkA4[0x34];
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
    void *animSlots;
    char unk33C;
    char crouch;
    char unk33E[2];
    char ammo[0x184];
    void *weapons[3];
    char unk4D0[0x1C];
    HumanView *view;
    int camMode;
    char camToggle;
    char unk4F5[3];
    float rotX, baseX, rotY, baseY, rotZ, baseZ;
    char frozenView;
    char unk511[0xF];
    float delta[3];
    char unk52C[8];
    HumanAnimEvent animEvent;
    char unk550[0x114];
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
    float moveMatrix[10];
    float moveAngle[3];
    char unk768[4];
    Task *attach;
    char unk770[0x18];
    float attachMatrix[9];
    int unk7AC;
    char unk7B0[4];
    Task **unk7B4;
    char unk7B8;
    char unk7B9[3];
    int unk7BC;
    char unk7C0[4];
    int unk7C4;
    char unk7C8;
    char unk7C9[7];
    double groundPos[3];
    int unk7E8;
    int groundValid;
    float groundDist;
    float groundUp[3];
    HumanAI *ai;
    char unk804[4];
    double pathPrev[3];
    double pathCur[3];
    char unk838[0x10];
    int missSoundTime;
    char unk84C[8];
    HumanText *texts[32];
    char lines[32][0x21];
    char scriptVars;
    char dead;
    char active;
    char unkCF7;
} Human;

_Static_assert(offsetof(Human, pos) == 0x20, "pos");
_Static_assert(offsetof(Human, motion) == 0x70, "motion");
_Static_assert(offsetof(Human, radius) == 0xA0, "radius");
_Static_assert(offsetof(Human, frozen) == 0xD8, "frozen");
_Static_assert(offsetof(Human, team) == 0xFC, "team");
_Static_assert(offsetof(Human, input) == 0x1B0, "input");
_Static_assert(offsetof(Human, damage) == 0x254, "damage");
_Static_assert(offsetof(Human, damage.hitZone) == 0x2E8, "hitZone");
_Static_assert(offsetof(Human, sm.bits) == 0x2F4, "bits");
_Static_assert(offsetof(Human, crouch) == 0x33D, "crouch");
_Static_assert(offsetof(Human, weapons) == 0x4C4, "weapons");
_Static_assert(offsetof(Human, view) == 0x4EC, "view");
_Static_assert(offsetof(Human, rotX) == 0x4F8, "rotX");
_Static_assert(offsetof(Human, frozenView) == 0x510, "frozenView");
_Static_assert(offsetof(Human, delta) == 0x520, "delta");
_Static_assert(offsetof(Human, animEvent) == 0x534, "animEvent");
_Static_assert(offsetof(Human, vel) == 0x664, "vel");
_Static_assert(offsetof(Human, outVel) == 0x688, "outVel");
_Static_assert(offsetof(Human, unk698) == 0x698, "unk698");
_Static_assert(offsetof(Human, ground) == 0x6A0, "ground");
_Static_assert(offsetof(Human, ground.normal) == 0x6D8, "ground.normal");
_Static_assert(offsetof(Human, ground.found) == 0x6F8, "ground.found");
_Static_assert(offsetof(Human, slope) == 0x71C, "slope");
_Static_assert(offsetof(Human, groundPos) == 0x7D0, "groundPos");
_Static_assert(offsetof(Human, groundValid) == 0x7EC, "groundValid");
_Static_assert(offsetof(Human, groundUp) == 0x7F4, "groundUp");
_Static_assert(offsetof(Human, pathPrev) == 0x808, "pathPrev");
_Static_assert(offsetof(Human, pathCur) == 0x820, "pathCur");
_Static_assert(offsetof(Human, groundDistance) == 0x710, "groundDistance");
_Static_assert(offsetof(Human, roofDistance) == 0x720, "roofDistance");
_Static_assert(offsetof(Human, movePos) == 0x728, "movePos");
_Static_assert(offsetof(Human, moveAngle) == 0x75C, "moveAngle");
_Static_assert(offsetof(Human, attach) == 0x76C, "attach");
_Static_assert(offsetof(Human, attachMatrix) == 0x788, "attachMatrix");
_Static_assert(offsetof(Human, unk7B4) == 0x7B4, "unk7B4");
_Static_assert(offsetof(Human, unk7C8) == 0x7C8, "unk7C8");
_Static_assert(offsetof(Human, ai) == 0x800, "ai");
_Static_assert(offsetof(Human, missSoundTime) == 0x848, "missSoundTime");
_Static_assert(offsetof(Human, texts) == 0x854, "texts");
_Static_assert(offsetof(Human, lines) == 0x8D4, "lines");
_Static_assert(offsetof(Human, scriptVars) == 0xCF4, "scriptVars");
_Static_assert(sizeof(Human) == 0xCF8, "Human size");
_Static_assert(offsetof(HumanView, firstPerson) == 0xD9, "firstPerson");
_Static_assert(offsetof(HumanView, unk134) == 0x134, "unk134");
_Static_assert(offsetof(HumanView, camOverride) == 0x13A, "camOverride");
_Static_assert(offsetof(HumanView, camPos) == 0x140, "camPos");
_Static_assert(offsetof(HumanView, camMatrix) == 0x158, "camMatrix");
_Static_assert(offsetof(HumanView, fov) == 0x1E4, "fov");
_Static_assert(offsetof(HumanAI, route) == 0xE0, "route");
_Static_assert(offsetof(HumanAI, immortal) == 0x3ABC, "immortal");
_Static_assert(offsetof(HumanAI, path) == 0x3B6C, "path");
_Static_assert(offsetof(HumanAI, node) == 0x3E2C, "node");
_Static_assert(offsetof(HumanPath, unk64) == 0x64, "unk64");
_Static_assert(sizeof(CollHit) == 0x70, "CollHit");
_Static_assert(offsetof(HumanText, flags) == 0x24, "flags");

/* where a weapon sits: hand bone of the body, or of the view model */
typedef struct HumanBoneMsg {
    int firstPerson;
    int unk4;
    double pos[3];
    float matrix[10];
} HumanBoneMsg;

/* slot 9: segments against everything; filter is an event asked first */
typedef struct CollLinesMsg {
    double *starts;
    double *ends;
    int count;
    uint8_t filter;
    char unkD[3];
    char *result;
    int unk14;
    int unk18;
    int magic;
    CollHit *hits;
} CollLinesMsg;

/* slot 8: one segment (line of sight) */
typedef struct CollLineMsg {
    double *start;
    double *end;
    uint8_t filter;
    char unk9[3];
    char *result;
} CollLineMsg;

typedef struct HumanCameraInfo {
    double pos[3];
    float matrix[10];
    float fov;
    float bodyMatrix[10];
    int unk6C;
    int mode;
} HumanCameraInfo;

typedef struct HumanNoise {
    int type;
    int team;
    int unk8;
    float radius;
    double *pos;
    void *unk14;
    Task *source;
} HumanNoise;

void Human_WakeIfVisible(Human *h);
void Human_Wake(Human *h);
int Human_LinesHitBounds(Human *h, const double *starts, const double *ends, int count);
uint16_t Human_GetType(void);
void *Human_GetList(void);
void Human_printf(Human *h, const char *fmt, ...);

#endif
