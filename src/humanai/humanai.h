#ifndef HUMANAI_HUMANAI_H
#define HUMANAI_HUMANAI_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "qasm/qasmtypes.h"
#include "graphtask/graphtasktypes.h"
#include "sysmisc/sysmisctypes.h"
#include "humanai/humanaitypes.h"
#include "graphnode/graphnodetypes.h"
#include "weapon/weapontypes.h"

typedef void (*TaskFn)(void *task, void *arg);

/* the parts of a human task the AI reads */
typedef struct Human {
    Task task;
    Vec3d pos;
    char unk38[0x38];
    HumanModel *model;
    int unk74;
    float matrix[10];
    char unkA0[0x29C];
    char unk33C;
    char unk33D[0x187];
    Task *weapons[10];
    void *view;
    char unk4F0[0x2D4];
    int unk7C4;
    char unk7C8[0x538];
    char unkD00;
} Human;

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

/* AIMessage.flags */
#define AIMSG_IN_VIEW    0x1
#define AIMSG_IN_RADIUS  0x2
#define AIMSG_HUMAN      0x4
#define AIMSG_ALIVE      0x8
#define AIMSG_DEAD       0x10
#define AIMSG_SAME_TEAM  0x20
#define AIMSG_OTHER_TEAM 0x40

typedef struct HumanAI HumanAI;
typedef struct AIAction AIAction;
typedef void (*AIActionFn)(HumanAI *ai, AIAction *a);

/* AIAction.flags */
#define AIACTION_PUSHABLE 0x1

struct AIAction {
    Node node;
    int event;
    int delay;
    unsigned flags;
    AIActionFn start;
    AIActionFn update;
    int unk1C;
    AIActionFn end;
    int unk24;
    Node *param;
    char used;
    char unk2D[3];
};

#define AI_EVENT_TYPES 24
#define AI_POOL 32

enum {
    EVENT_CREATE,
    EVENT_DELETE,
    EVENT_DEAD,
    EVENT_ANIMATION,
    EVENT_IDLE,
    EVENT_ALERT,
    EVENT_ALERT_RESPONSE,
    EVENT_COMBAT,
    EVENT_ALARMON,
    EVENT_ALARMOFF,
    EVENT_WALK,
    EVENT_GROUNDIMPACT,
    EVENT_DOOR,
    EVENT_FENCE,
    EVENT_LADDER,
    EVENT_TAKINGDAMAGE,
    EVENT_GUNSHOT,
    EVENT_GRENADETHROWN,
    EVENT_GRENADELAND,
    EVENT_FLASHBANG,
    EVENT_GUNSHOTMISS,
    EVENT_EXPLOSION,
    EVENT_ENEMYDETECTION,
    EVENT_FRIENDLYDETECTION,
};

/* HumanAI task (0x3ED0), the brain of an enemy, child of its human */
struct HumanAI {
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
    int unk1514;
    char unk1518[0xE0];
    AIDetection detection;
    char detectionActive;
    char unk3A29[7];
    AIDetectView dview;
    AITarget *target;
    void *targetSeen;
    void *unk3AB0;
    void *unk3AB4;
    char symbolsAdded;
    char isDetection;
    char deadDone;
    char alarmed;
    char unk3ABC;
    char unk3ABD;
    char unk3ABE[2];
    int unk3AC0;
    int alarmControlId;
    int unk3AC8;
    int unk3ACC;
    int unk3AD0;
    char unk3AD4[0x84];
    int animBase;
    int animRandom;
    int animTimer;
    int animEnabled;
    int graphId;
    void *graph;
    char scriptPath[0x100];
    void *script;
    float ctrl[0x28];
    char started;
    char aiTypeExpr[0x23];
    int aiType;
    int unk3D3C;
    int unk3D40;
    char unk3D44[0x6C];
    double unk3DB0;
    char unk3DB8[0x18];
    int unk3DD0;
    int unk3DD4;
    int unk3DD8;
    int unk3DDC;
    int unk3DE0;
    int unk3DE4;
    int unk3DE8;
    int unk3DEC;
    char unk3DF0;
    char unk3DF1;
    char unk3DF2[2];
    int unk3DF4;
    int unk3DF8;
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
    int unk3E54;
    int unk3E58;
    int unk3E5C;
    int unk3E60;
    int unk3E64;
    char unk3E68;
    char unk3E69;
    char unk3E6A;
    char unk3E6B;
    int unk3E6C;
    int unk3E70;
    int unk3E74;
    int unk3E78;
    int unk3E7C;
    int unk3E80;
    int unk3E84;
    char unk3E88[0x18];
    void *path;
    int pathLen;
    char pathA[8];
    char pathB[0x18];
    float maxAlpha;
    float maxGamma;
};

/* HumanAI.ctrl, the input the AI gives its human */
#define CTRL_GAMMA 0
#define CTRL_ALPHA 1
#define CTRL_FORWARD 4
#define CTRL_CHEAT 19

_Static_assert(offsetof(Human, unk33C) == 0x33C, "Human");
_Static_assert(offsetof(Human, weapons) == 0x4C4, "Human");
_Static_assert(offsetof(Human, unk7C4) == 0x7C4, "Human");
_Static_assert(offsetof(Human, unkD00) == 0xD00, "Human");
_Static_assert(sizeof(AIView) == 0xC0, "AIView");
_Static_assert(sizeof(AIDetectView) == 0x78, "AIDetectView");
_Static_assert(sizeof(AIAction) == 0x30, "AIAction");
_Static_assert(offsetof(HumanAI, eventPool) == 0xF0, "eventPool");
_Static_assert(offsetof(HumanAI, events) == 0x6F0, "events");
_Static_assert(offsetof(HumanAI, eventEnabled) == 0x8E4, "eventEnabled");
_Static_assert(offsetof(HumanAI, actionPool) == 0x908, "actionPool");
_Static_assert(offsetof(HumanAI, freeParams) == 0x1508, "freeParams");
_Static_assert(offsetof(HumanAI, detection) == 0x15F8, "detection");
_Static_assert(offsetof(HumanAI, dview) == 0x3A30, "dview");
_Static_assert(offsetof(HumanAI, target) == 0x3AA8, "target");
_Static_assert(offsetof(HumanAI, symbolsAdded) == 0x3AB8, "symbolsAdded");
_Static_assert(offsetof(HumanAI, animBase) == 0x3B58, "animBase");
_Static_assert(offsetof(HumanAI, scriptPath) == 0x3B70, "scriptPath");
_Static_assert(offsetof(HumanAI, ctrl) == 0x3C74, "ctrl");
_Static_assert(offsetof(HumanAI, aiType) == 0x3D38, "aiType");
_Static_assert(offsetof(HumanAI, unk3DB0) == 0x3DB0, "unk3DB0");
_Static_assert(offsetof(HumanAI, unk3DD0) == 0x3DD0, "unk3DD0");
_Static_assert(offsetof(HumanAI, hasGun) == 0x3DFC, "hasGun");
_Static_assert(offsetof(HumanAI, unk3E10) == 0x3E10, "unk3E10");
_Static_assert(offsetof(HumanAI, unk3E28) == 0x3E28, "unk3E28");
_Static_assert(offsetof(HumanAI, door) == 0x3E4C, "door");
_Static_assert(offsetof(HumanAI, unk3E84) == 0x3E84, "unk3E84");
_Static_assert(offsetof(HumanAI, path) == 0x3EA0, "path");
_Static_assert(sizeof(HumanAI) == 0x3ED0, "HumanAI");

extern TaskFn gTaskHandlers[][0x180];
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* event slots allocated by HumanAI */
extern uint8_t gAIEventSlot;
extern uint8_t gAIDoorSlot;
extern uint8_t gAIEvent3Slot;

/* the AI a script is running for, and where its actions go */
extern HumanAI *gAICurrent;
extern List *gAIActionList;
extern List *gAIEventList;
extern int gAIEventIndex;

extern List gAIMoveFree;
extern List gAITargets;
extern AITarget gAITargetPool[];
#define AI_TARGETS 8

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4C1780, void, Debug_Log, (void *task, int level, const char *fmt, ...));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, void *node));
ORIG(0x4AF910, void, List_AddHead, (List *list, void *node));
ORIG(0x4AF960, void, List_Remove, (void *node));
ORIG(0x4AF9B0, int, List_Count, (List *list));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x463120, int, Human_GetIndex, (Task *human));
ORIG(0x463130, int, Human_GetTeam, (Task *human));
ORIG(0x463170, char, Human_IsDead, (Task *human));
ORIG(0x463180, char, Human_IsCrouching, (Task *human));
ORIG(0x463190, char, Human_IsActive, (Task *human));
ORIG(0x4F9720, GraphNode *, Graph_GetNode, (void *graph, int id));
ORIG(0x4F9770, void, Graph_GetNodePos, (Vec3d *out, GraphNode *node));
ORIG(0x4F9A40, char, Graph_IsInNode, (const Vec3d *pos, GraphNode *node, double tol, int unk));
ORIG(0x452C10, void, NodeGrid_Release, (void *path, int a, int b));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x4B4770, long double, Random_Float, (void *r));

void HumanAI_InitEvents(HumanAI *ai);
void HumanAI_FindWeapons(HumanAI *ai);
char HumanAI_RunScript(HumanAI *ai, int type, int index);
void HumanAI_ProcessEvents(HumanAI *ai);
void HumanAI_FreeEvent(HumanAI *ai, AIEvent *e);
void HumanAI_RunAction(HumanAI *ai);
void HumanAI_ClearMove(HumanAI *ai);
void HumanAI_ClearMoveQueue(HumanAI *ai);
void HumanAI_FreeEvents(HumanAI *ai);
char AIView_CanSee(AIView *v, const Vec3d *pos, char alarm);
AIEvent *HumanAI_NewEvent(HumanAI *ai, AIMessage *m);
void HumanAI_UnregisterFunctions(void);
void HumanAI_UnregisterConstants(void);
uint8_t HumanAI_GetEventSlot(void);
uint8_t HumanAI_GetDoorSlot(void);
void HumanAI_Send(Task *t, AIMessage *m);
void HumanAI_SendSimple(Task *t, int type, unsigned flags);
AIAction *HumanAI_GetTopAction(HumanAI *ai);
AIAction *HumanAI_FindFreeAction(HumanAI *ai);
void HumanAI_StartAction(HumanAI *ai, AIAction *a);
void HumanAI_PopActions(HumanAI *ai);
void HumanAI_StartActions(HumanAI *ai, List *list);
void HumanAI_EndAction(HumanAI *ai, AIAction *a);
void HumanAI_FreeParam(HumanAI *ai, Node *p);
void AITarget_Free(AITarget *t);
void AITarget_Remove(AITarget *t, HumanAI *ai, char count);
char HumanAI_IsDead(HumanAI *ai);
long double AIView_GetPitch(AIView *v);
int HumanAI_MoveStep(HumanAI *ai);
MoveItem *HumanAI_NewMoveItem(HumanAI *ai);
void Graph_EdgePoint(Vec3d *out, GraphNode *node, const Vec3d *center, const Vec3d *dir, char add);
char HumanAI_MoveTo(HumanAI *ai, int node, const Vec3d *pos, char replan);
char HumanAI_TurnTo(HumanAI *ai, const Vec3d *pos, float tol, char flat);
char HumanAI_FaceTo(HumanAI *ai, const Vec3d *pos, float tol, char flat);

#endif
