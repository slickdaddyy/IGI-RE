#ifndef HUMANAI_HUMANAITYPES_H
#define HUMANAI_HUMANAITYPES_H

#include "graphnode/graphnodetypes.h"
#include "patrol/patroltypes.h"

/* a point of the walk queue */
typedef struct MoveItem {
    Node node;
    Vec3d pos;
    int door;
    int id;
} MoveItem;

/* an enemy or friend an AIDetection knows of */
typedef struct AISeen {
    Node node;
    Task **ref;
    char unkC[0x1C];
    char unk28;
    char visible;
    char valid;
    char unk2B[5];
} AISeen;
_Static_assert(sizeof(AISeen) == 0x30, "AISeen");

typedef struct AIDetection {
    char unk0[0x18];
    List enemies;       /* AISeen */
    List friends;
    char unk30[0x2400];
} AIDetection;
_Static_assert(sizeof(AIDetection) == 0x2430, "AIDetection");

/* a message to the AI around: a noise, a shot, a call */
typedef struct AIMessage {
    int type;
    int team;
    unsigned flags;
    float radius;
    Vec3d *pos;
    Task *from;
    Task *about;
} AIMessage;
_Static_assert(sizeof(AIMessage) == 0x1C, "AIMessage");

typedef struct NearQuery {
    Vec3d pos;
    double best;
    Task *found;
} NearQuery;

/* the AIAction_Combat parameter block, after its list node */
typedef struct CombatData {
    List *events;
    int unk4;
    double dist;
    int unk10;
    int timer;
    int count;
    int frame;
    char first;
    char spoke;
} CombatData;

typedef struct AIParam {
    Node node;
    CombatData d;
} AIParam;

typedef struct AIEvent {
    Node node;
    int type;
    int team;
    Vec3d pos;
    Task **from;
    Task **about;
} AIEvent;
_Static_assert(sizeof(AIEvent) == 0x30, "AIEvent");

typedef struct AITarget {
    Node node;
    char used;
    char unk09[3];
    Task **ref;
    Task *humans[32];
    int states[32];
    Vec3d pos;
    int count;
    int removed;
} AITarget;
_Static_assert(sizeof(AITarget) == 0x130, "AITarget");

struct HumanAI;

/* a queued action of a HumanAI */
typedef struct AIAction {
    Node node;
    int event;
    int delay;
    unsigned flags;
    void (*start)(struct HumanAI *ai, struct AIAction *a);
    void (*update)(struct HumanAI *ai, struct AIAction *a);
    void (*resume)(struct HumanAI *ai, struct AIAction *a);
    void (*end)(struct HumanAI *ai, struct AIAction *a);
    char (*step)(struct HumanAI *ai, struct AIAction *a);
    void *param;        /* parameter block, an AIParam for combat */
    char used;
    char unk2D[3];
} AIAction;
_Static_assert(sizeof(AIAction) == 0x30, "AIAction");

typedef struct AnimEntry {
    int anim;
    int unk4;
    double weight;
} AnimEntry;

#define AI_EVENT_TYPES 24
#define AI_POOL 32

/* what the AI sees with: position, eye, body and view rotation, cone sizes */
typedef struct AIView {
    Vec3d pos;
    Vec3d eye;
    float body[10];
    float view[10];
    double alpha;
    double gamma;
    double length;
    double alarmLength;
    double secAlpha;
    double secGamma;
    double secLength;
    double secAlarmLength;
} AIView;
_Static_assert(sizeof(AIView) == 0xC0, "AIView");

/* the view handed to the detection code each frame */
typedef struct AIDetectView {
    Vec3d eye;
    float body[10];
    double alpha;
    double gamma;
    double length;
    double secAlpha;
    double secGamma;
    double secLength;
    char active;
    char unk71[7];
} AIDetectView;
_Static_assert(sizeof(AIDetectView) == 0x78, "AIDetectView");

/* HumanAI task (0x3ED0), the brain of an enemy, child of its human */
typedef struct HumanAI {
    Task task;
    AIView view;
    List moves;
    int unkEC;
    AIEvent eventPool[AI_POOL];
    List events[AI_EVENT_TYPES];
    List freeEvents;
    int eventOrder[AI_EVENT_TYPES];
    int eventCounts[AI_EVENT_TYPES];
    int eventOrderCount;
    int eventCount;
    char eventEnabled[AI_EVENT_TYPES];
    List actions;
    AIAction actionPool[AI_POOL];
    AIParam paramPool[AI_POOL];
    List freeParams;
    int routeCount;
    PatrolRoute routes[8];
    AIDetection detection;
    char detectionActive;
    char unk3A29[7];
    AIDetectView dview;
    AITarget *target;
    void *targetSeen;
    void *claim;
    void *alarmClaim;
    char symbolsAdded;
    char isDetection;
    char deadDone;
    char alarmed;
    char invulnerable;
    char noInstantDeath;
    char unk3ABE[2];
    int alarmTriggerId;
    int alarmControlId;
    int statGunId;
    int alarmAccess;
    int deathAnim;
    int scriptInts[5];
    double scriptReals[2];
    AnimEntry anims[6];         /* entries start at anims[1] */
    int animBase;               /* frames */
    int animRandom;
    int animTimer;
    int animEnabled;
    int graphId;
    Graph *graph;
    char scriptPath[0x100];
    void *script;
    float ctrl[0x28];
    char started;
    char aiTypeExpr[0x23];
    int aiType;
    int unk3D3C;
    int unk3D40;
    int unk3D44;
    int unk3D48;
    int maxBurst;
    int maxTurns;
    float evadeChance;
    float lieChance;
    float unk3D5C;
    float rollChance;
    float unk3D64;
    float errorNear;
    float errorFar;
    float burstMin;
    float burstRandom;
    float guardMin;
    float guardRandom;
    float shotDelayMin;
    float shotDelayRandom;
    float grenadeChance;
    float grenadeChance2;
    float grenadeMin;
    float grenadeMax;
    float throwSpeedMin;
    float throwSpeedRange;
    float throwCountMin;
    float throwCountRange;
    float meleeChance;
    float meleeDamage;
    double closeRange;
    double fallFlatRange;
    double coverRange;
    double aimSpeed;
    int unk3DD0;
    int unk3DD4;
    int unk3DD8;
    int unk3DDC;
    int unk3DE0;
    AISeen *seen;
    int followNode;
    int targetNode;
    char targetMoved;
    char reloadToggle;
    char unk3DF2[2];
    int rollDir;
    int nextEvade;
    char hasGun;
    char hasGrenade;
    char unk3DFE[2];
    int gunId;
    int grenadeId;
    int grenadeTime;
    int unk3E0C;
    int unk3E10;
    char unk3E14[0x14];
    int unk3E28;
    int node;
    int destNode;
    char unk3E34[8];
    int unk3E3C;
    int unk3E40;
    int unk3E44;
    char unk3E48;
    char unk3E49;
    char doorWait;
    char unk3E4B;
    Task **door;
    int unk3E50;
    int shotsLeft;
    int lastAmmo;
    int unk3E5C;
    int shotDelay;
    int unk3E64;
    char unk3E68;
    char unk3E69;
    char unk3E6A;
    char unk3E6B;
    int unk3E6C;
    Vec3d aim;
    Vec3d aimTarget;
    NodeGrid *grid;
    int gridX;
    int gridY;
    char unk3EAC[4];
    Vec3d cover;
    float maxAlpha;
    float maxGamma;
} HumanAI;
_Static_assert(offsetof(HumanAI, dview) == 0x3A30, "HumanAI");
_Static_assert(offsetof(HumanAI, deathAnim) == 0x3AD0, "HumanAI");
_Static_assert(offsetof(HumanAI, ctrl) == 0x3C74, "HumanAI");
_Static_assert(offsetof(HumanAI, aiType) == 0x3D38, "HumanAI");
_Static_assert(offsetof(HumanAI, node) == 0x3E2C, "HumanAI");
_Static_assert(offsetof(HumanAI, cover) == 0x3EB0, "HumanAI");
_Static_assert(sizeof(HumanAI) == 0x3ED0, "HumanAI");

#endif
