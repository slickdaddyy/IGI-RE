#ifndef HUMANAI_HUMANAI_H
#define HUMANAI_HUMANAI_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "qasm/qasmtypes.h"
#include "graphtask/graphtasktypes.h"
#include "sysmisc/sysmisctypes.h"
#include "humanai/humanaitypes.h"
#include "weapon/weapontypes.h"
#include "humanplayer/humanplayertypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn)(void *task, void *arg);

/* AIMessage.flags */
#define AIMSG_IN_VIEW    0x1
#define AIMSG_IN_RADIUS  0x2
#define AIMSG_HUMAN      0x4
#define AIMSG_ALIVE      0x8
#define AIMSG_DEAD       0x10
#define AIMSG_SAME_TEAM  0x20
#define AIMSG_OTHER_TEAM 0x40

typedef void (*AIActionFn)(HumanAI *ai, AIAction *a);

/* AIAction.flags */
#define AIACTION_PUSHABLE 0x1

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

/* HumanAI.ctrl, the input the AI gives its human */
#define CTRL_GAMMA 0
#define CTRL_ALPHA 1
#define CTRL_FORWARD 4
#define CTRL_CHEAT 19

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
extern AITarget gAITargetPool[AI_TARGETS];
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
