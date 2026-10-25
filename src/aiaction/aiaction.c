#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "aiaction/aiaction.h"
#include "hook.h"
#include "humanai/humanaitypes.h"

FN_EXTERN(AIActivate_Start);
FN_EXTERN(AICombatAnya_Start);
FN_EXTERN(AICombatGuard_Start);
FN_EXTERN(AICombatGunner_Start);
FN_EXTERN(AICombatPatrol_Start);
FN_EXTERN(AICombatRPG_Start);
FN_EXTERN(AIDead_Start);
FN_EXTERN(AIFallFlat_Start);
FN_EXTERN(AIFireAtNode_Start);
FN_EXTERN(AIFireAtTask_Start);
FN_EXTERN(AIIdle_Start);
FN_EXTERN(AIKickGrenade_Start);
FN_EXTERN(AILookAtEvent_Start);
FN_EXTERN(AIMoveToEvent_Start);
FN_EXTERN(AIPatrol_Start);
FN_EXTERN(AIPlayAnim_Start);
FN_EXTERN(AIPlaySound_Start);
FN_EXTERN(AIRunPanicking_Start);
FN_EXTERN(AIRunToNode_Start);
FN_EXTERN(AISetCombat_Start);
FN_EXTERN(AIStunned_Start);
FN_EXTERN(AIWalkToNode_Start);
FN_EXTERN(HumanAI_AddSymbols);
FN_EXTERN(HumanAI_Create);
FN_EXTERN(HumanAI_Destroy);
FN_EXTERN(HumanAI_GetParams);
FN_EXTERN(HumanAI_OnEvent);
FN_EXTERN(HumanAI_ParamsChanged);
FN_EXTERN(HumanAI_RemoveSymbols);
FN_EXTERN(HumanAI_Update);

ORIG(0x440C00, char, AlarmControl_IsAlarm, (Task *t));
ORIG(0x450C80, uint8_t, HumanAI_GetEventSlot, (void));
ORIG(0x450D80, void, HumanAI_SendSimple, (Task *to, int event, int unk));
ORIG(0x450E10, void, HumanAI_PushAction, (void *fn, void *data, int flags));
ORIG(0x450FF0, void, HumanAI_EndAction, (void *ai, void *action));
ORIG(0x451030, void *, HumanAI_AllocParam, (void *ai));
ORIG(0x475370, char, StationaryGun_IsUsed, (Task *t));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B88C0, void, Script_RegisterIntFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B88F0, void, Script_RegisterFunctionEx, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, void, Script_GetArgString, (void *args, int i, char *out, int size));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4F0F10, void *, Param_ScriptFile, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4F9720, void *, Graph_GetNode, (void *graph, int id));
ORIG(0x509880, unsigned, AIGraph_GetStairCriteria, (void));
ORIG(0x509890, char, GraphNode_HasCriteria, (void *node, unsigned mask));

/* handlers of HumanAI and of the queued actions */
#define HumanAI_Create (FN(0x44F060, HumanAI_Create))
#define HumanAI_Run (FN(0x44F410, HumanAI_Update))
#define HumanAI_Delete (FN(0x44FFC0, HumanAI_Destroy))
#define HumanAI_OnEvent (FN(0x450110, HumanAI_OnEvent))
#define HumanAI_GetParams (FN(0x4505C0, HumanAI_GetParams))
#define HumanAI_Changed (FN(0x4505D0, HumanAI_ParamsChanged))
#define HumanAI_OnLevelDA8 (FN(0x450770, HumanAI_AddSymbols))
#define HumanAI_OnLevelDAC (FN(0x4507D0, HumanAI_RemoveSymbols))
#define AI_DEAD (FN(0x48AB70, AIDead_Start))
#define AI_COMBAT_PATROL (FN(0x48ABA0, AICombatPatrol_Start))
#define AI_COMBAT_ANYA (FN(0x48AC60, AICombatAnya_Start))
#define AI_COMBAT_RPG (FN(0x48CB40, AICombatRPG_Start))
#define AI_COMBAT_GUARD (FN(0x48D000, AICombatGuard_Start))
#define AI_COMBAT_GUNNER (FN(0x48D2C0, AICombatGunner_Start))
#define AI_PLAY_ANIMATION (FN(0x48DEA0, AIPlayAnim_Start))
#define AI_PLAY_SOUND (FN(0x48DF70, AIPlaySound_Start))
#define AI_MOVE_TO_EVENT (FN(0x48DFE0, AIMoveToEvent_Start))
#define AI_LOOK_AT_EVENT (FN(0x48E080, AILookAtEvent_Start))
#define AI_STUNNED (FN(0x48E130, AIStunned_Start))
#define AI_KICK_GRENADE (FN(0x48E1E0, AIKickGrenade_Start))
#define AI_RUN_PANICKING (FN(0x48E270, AIRunPanicking_Start))
#define AI_IDLE (FN(0x48E510, AIIdle_Start))
#define AI_SET_COMBAT (FN(0x48E540, AISetCombat_Start))
#define AI_ACTIVATE (FN(0x48E620, AIActivate_Start))
#define AI_PATROL (FN(0x48E8C0, AIPatrol_Start))
#define AI_FALL_FLAT (FN(0x48EB40, AIFallFlat_Start))
#define AI_WALK_TO_NODE (FN(0x48EBE0, AIWalkToNode_Start))
#define AI_RUN_TO_NODE (FN(0x48EC40, AIRunToNode_Start))
#define AI_FIRE_AT_NODE (FN(0x48ECA0, AIFireAtNode_Start))
#define AI_FIRE_AT_TASK (FN(0x48EDE0, AIFireAtTask_Start))

/* the data blocks of the actions, all taken from HumanAI.freeData */
typedef struct NodeData {
    Node node;
    int id;
    int unkC;
    int unk10;
    char unk14;
    char unk15[3];
    char unk18;
} NodeData;

typedef struct SoundData {
    Node node;
    char name[0x20];
    char unk28;
} SoundData;

typedef struct EventData {
    Node node;
    Vec3d pos;
    union {
        double time;    /* RunPanicking */
        struct {
            union {
                float f;
                Task **source;
            } unk20;
            union {
                char c;
                Task **target;
            } unk24;
        };
    };
    Task **ref;         /* 0x28 */
} EventData;

_Static_assert(offsetof(EventData, ref) == 0x28, "EventData");

#define AI_MONITOR_GRAPHS 64
#define AI_MONITOR_TARGETS 128
#define AI_CLAIMS 16
#define AI_TARGETS 8
#define AI_MOVE_ITEMS 1280
#define AI_GRIDS 128

extern MonitorGraph gMonitorGraphs[AI_MONITOR_GRAPHS];
extern MonitorTarget gMonitorTargets[AI_MONITOR_TARGETS];
extern Claim gClaims[AI_CLAIMS];
extern AITarget gAITargetPool[AI_TARGETS];
extern MoveItem gAIMoveItems[AI_MOVE_ITEMS];
extern NodeGrid gNodeGrids[AI_GRIDS];
extern List gMonitorGraphsFree;
extern List gMonitorGraphsUsed;
extern List gMonitorTargetsFree;
extern List gClaimsFree;
extern List gClaimsUsed;
extern List gGridsFree;
extern List gGridsUsed;
extern List gAIMoveFree;
extern List gAITargets;
extern int gHumanAIUnusedInts[8];

extern uint16_t gHumanAIType;
extern const char *gAIScriptNames[];
extern const int gAIScriptOfType[];
extern const char *gAITypeNames[];
extern void *gHumanAIParams;
extern void *gAIScripts[];
extern HumanAI *gAICurrent;
extern List *gAIEventList;

static const double kFps = 30.0;
static const double kViewScale = 4096.0;
static const double kMinWeight = 0.01;
extern const float kDegToRad;

static ScriptValue *ret_int(ScriptValue *out, int v)
{
    out->i = v;
    out->d = v;
    out->s = gEmptyString;
    return out;
}

/* the event on top of the stack, NULL if there is none */
AIEvent *AIEvent_GetCurrent(void)
{
    AIEvent *e = (AIEvent *)gAIEventList->head;

    return e->node.next ? e : NULL;
}
HOOK(0x44C8C0, AIEvent_GetCurrent);

void HumanAI_RegisterFunctions(void);
void HumanAI_RegisterConstants(void);
void AIEvent_GetPos(Vec3d *out);

void HumanAI_Init(void)
{
    HumanAI t;
    List *l;
    char *p;
    NodeGrid *g;
    MonitorGraph *mg;
    MonitorTarget *mt;
    int i;

    gHumanAIType = TaskType_New("HumanAI", sizeof(HumanAI), Task_GetRootType(), 0);
    TaskType_SetHandler(gHumanAIType, 0, HumanAI_Create);
    TaskType_SetHandler2(gHumanAIType, 1, HumanAI_Run);
    TaskType_SetHandler2(gHumanAIType, 2, HumanAI_Delete);
    TaskType_SetHandler(gHumanAIType, HumanAI_GetEventSlot(), HumanAI_OnEvent);
    TaskType_SetHandler(gHumanAIType, Param_GetTableEvent(), HumanAI_GetParams);
    TaskType_SetHandler(gHumanAIType, Param_GetChangedEvent(), HumanAI_Changed);
    TaskType_SetHandler2(gHumanAIType, Level_GetActivateEvent(), HumanAI_OnLevelDA8);
    TaskType_SetHandler2(gHumanAIType, Level_GetDeactivateEvent(), HumanAI_OnLevelDAC);
    TaskType_SetHandler(gHumanAIType, 4, NULL);
    gHumanAIParams = ParamTable_New(3);
    ParamTable_Add(gHumanAIParams, &t, t.aiTypeExpr, Param_String32(), 7, NULL);
    ParamTable_Add(gHumanAIParams, &t, &t.graphId, Param_Int32(), 7, NULL);
    ParamTable_Add(gHumanAIParams, &t, &t.scriptPath, Param_ScriptFile(), 2, NULL);
    HumanAI_RegisterFunctions();
    HumanAI_RegisterConstants();

#define LIST_INIT(addr) (l = (List *)(addr), l->head = (Node *)&l->tail, l->tailpred = (Node *)&l->head, l->tail = NULL)
#define NODES(addr, end, size, list)                         \
    for (p = (char *)(addr); p < (char *)(end); p += (size)) \
        ((Node *)p)->prev = NULL, ((Node *)p)->next = NULL, List_AddTail((List *)(list), p)

    LIST_INIT(&gAITargets);
    for (i = 0; i < AI_TARGETS; i++)
        gAITargetPool[i].used = 0;
    LIST_INIT(&gAIMoveFree);
    NODES(gAIMoveItems, gAIMoveItems + AI_MOVE_ITEMS, sizeof(MoveItem), &gAIMoveFree);
    for (i = 0; i < 9; i++)
        gAIScripts[i] = Script_Load(gAIScriptNames[i]);
    LIST_INIT(&gGridsUsed);
    LIST_INIT(&gGridsFree);
    for (g = gNodeGrids; g < gNodeGrids + AI_GRIDS; g++) {
        g->node.prev = NULL;
        g->node.next = NULL;
        List_AddTail(&gGridsFree, g);
        memset(g->cells, 0, sizeof(g->cells));
        g->graphId = -1;
        g->nodeId = -1;
        g->w = 0;
        g->h = 0;
        g->refs = 0;
    }
    LIST_INIT(&gClaimsUsed);
    LIST_INIT(&gClaimsFree);
    NODES(gClaims, gClaims + AI_CLAIMS, sizeof(Claim), &gClaimsFree);
    LIST_INIT(&gMonitorGraphsUsed);
    LIST_INIT(&gMonitorGraphsFree);
    LIST_INIT(&gMonitorTargetsFree);
    for (mg = gMonitorGraphs; mg < gMonitorGraphs + AI_MONITOR_GRAPHS; mg++) {
        mg->node.prev = NULL;
        mg->node.next = NULL;
        List_AddTail(&gMonitorGraphsFree, mg);
        LIST_INIT(&mg->targets);
        mg->graph = NULL;
    }
    for (mt = gMonitorTargets; mt < gMonitorTargets + AI_MONITOR_TARGETS; mt++) {
        mt->node.prev = NULL;
        mt->node.next = NULL;
        List_AddTail(&gMonitorTargetsFree, mt);
        mt->tick = -1;
        mt->nodeId = -1;
        mt->target = NULL;
    }
    for (i = 0; i < 8; i++)
        gHumanAIUnusedInts[i] = 0;
#undef LIST_INIT
#undef NODES
}
HOOK(0x44C8D0, HumanAI_Init);

ScriptValue *AIAction_Patrol(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_Combat(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_Dead(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_FallFlat(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_Activate(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_WalkToNode(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_RunToNode(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_FireAtNode(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_FireAtTask(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_PlayAnimation(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_PlaySound(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_MoveToEvent(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_LookAtEvent(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_Stunned(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_KickGrenade(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_RunPanicking(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_Idle(ScriptValue *out, void *data, void *args);
ScriptValue *AIAction_SetCombat(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_DefaultHandler(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_RemoveAlarmActions(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetViewLength(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetAlarmViewLength(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetViewAlpha(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetViewGamma(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetSecondaryViewLength(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetSecondaryAlarmViewLength(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetSecondaryViewAlpha(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetSecondaryViewGamma(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetEventPriority(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetInvulnerability(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetInstantDeath(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetDeathAnimation(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetAlarmTriggerID(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetAlarmControlID(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetAlarmAccess(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetGunnerID(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetScriptIntegerValue(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetScriptRealValue(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetAlarmTriggerID(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetAlarmControlID(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetAlarmAccess(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetGunnerID(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetAlarmControlStatus(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetGunnerStatus(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetScriptIntegerValue(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetCurrentEventType(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_IsEventBehind(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetScriptRealValue(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetRandomValue(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetEventDistance(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetAlarmTriggerDistance(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SetAnimationInterval(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_AddAnimationEntry(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_GetAnimationToPlay(ScriptValue *out, void *data, void *args);
ScriptValue *AIFunction_SendResponse(ScriptValue *out, void *data, void *args);

void HumanAI_RegisterFunctions(void)
{
    Script_RegisterFunction("AIAction_Patrol", AIAction_Patrol, NULL, 3);
    Script_RegisterFunction("AIAction_Combat", AIAction_Combat, NULL, 1);
    Script_RegisterFunction("AIAction_Dead", AIAction_Dead, NULL, 1);
    Script_RegisterFunction("AIAction_FallFlat", AIAction_FallFlat, NULL, 2);
    Script_RegisterFunction("AIAction_Activate", AIAction_Activate, NULL, 3);
    Script_RegisterFunction("AIAction_WalkToNode", AIAction_WalkToNode, NULL, 2);
    Script_RegisterFunction("AIAction_RunToNode", AIAction_RunToNode, NULL, 2);
    Script_RegisterFunction("AIAction_FireAtNode", AIAction_FireAtNode, NULL, 5);
    Script_RegisterFunction("AIAction_FireAtTask", AIAction_FireAtTask, NULL, 5);
    Script_RegisterFunction("AIAction_PlayAnimation", AIAction_PlayAnimation, NULL, 2);
    Script_RegisterFunction("AIAction_PlaySound", AIAction_PlaySound, NULL, 3);
    Script_RegisterFunction("AIAction_MoveToEvent", AIAction_MoveToEvent, NULL, 2);
    Script_RegisterFunction("AIAction_LookAtEvent", AIAction_LookAtEvent, NULL, 2);
    Script_RegisterFunction("AIAction_Stunned", AIAction_Stunned, NULL, 2);
    Script_RegisterFunction("AIAction_KickGrenade", AIAction_KickGrenade, NULL, 1);
    Script_RegisterFunction("AIAction_RunPanicking", AIAction_RunPanicking, NULL, 2);
    Script_RegisterFunction("AIAction_Idle", AIAction_Idle, NULL, 1);
    Script_RegisterFunction("AIAction_SetCombat", AIAction_SetCombat, NULL, 1);
    Script_RegisterFunction("AIFunction_DefaultHandler", AIFunction_DefaultHandler, NULL, 0);
    Script_RegisterFunction("AIFunction_RemoveAlarmActions", AIFunction_RemoveAlarmActions, NULL, 0);
    Script_RegisterFunction("AIFunction_SetViewLength", AIFunction_SetViewLength, NULL, 1);
    Script_RegisterFunction("AIFunction_SetAlarmViewLength", AIFunction_SetAlarmViewLength, NULL, 1);
    Script_RegisterFunction("AIFunction_SetViewAlpha", AIFunction_SetViewAlpha, NULL, 1);
    Script_RegisterFunction("AIFunction_SetViewGamma", AIFunction_SetViewGamma, NULL, 1);
    Script_RegisterFunction("AIFunction_SetSecondaryViewLength", AIFunction_SetSecondaryViewLength, NULL, 1);
    Script_RegisterFunction("AIFunction_SetSecondaryAlarmViewLength", AIFunction_SetSecondaryAlarmViewLength, NULL,
                            1);
    Script_RegisterFunction("AIFunction_SetSecondaryViewAlpha", AIFunction_SetSecondaryViewAlpha, NULL, 1);
    Script_RegisterFunction("AIFunction_SetSecondaryViewGamma", AIFunction_SetSecondaryViewGamma, NULL, 1);
    Script_RegisterFunction("AIFunction_SetEventPriority", AIFunction_SetEventPriority, NULL, 1);
    Script_RegisterFunction("AIFunction_SetInvulnerability", AIFunction_SetInvulnerability, NULL, 1);
    Script_RegisterFunction("AIFunction_SetInstantDeath", AIFunction_SetInstantDeath, NULL, 1);
    Script_RegisterFunction("AIFunction_SetDeathAnimation", AIFunction_SetDeathAnimation, NULL, 1);
    Script_RegisterFunction("AIFunction_SetAlarmTriggerID", AIFunction_SetAlarmTriggerID, NULL, 1);
    Script_RegisterFunction("AIFunction_SetAlarmControlID", AIFunction_SetAlarmControlID, NULL, 1);
    Script_RegisterFunction("AIFunction_SetAlarmAccess", AIFunction_SetAlarmAccess, NULL, 1);
    Script_RegisterFunction("AIFunction_SetGunnerID", AIFunction_SetGunnerID, NULL, 1);
    Script_RegisterFunction("AIFunction_SetScriptIntegerValue", AIFunction_SetScriptIntegerValue, NULL, 2);
    Script_RegisterFunction("AIFunction_SetScriptRealValue", AIFunction_SetScriptRealValue, NULL, 2);
    Script_RegisterIntFunction("AIFunction_GetAlarmTriggerID", AIFunction_GetAlarmTriggerID, NULL, 0);
    Script_RegisterIntFunction("AIFunction_GetAlarmControlID", AIFunction_GetAlarmControlID, NULL, 0);
    Script_RegisterIntFunction("AIFunction_GetAlarmAccess", AIFunction_GetAlarmAccess, NULL, 0);
    Script_RegisterIntFunction("AIFunction_GetGunnerID", AIFunction_GetGunnerID, NULL, 0);
    Script_RegisterIntFunction("AIFunction_GetAlarmControlStatus", AIFunction_GetAlarmControlStatus, NULL, 0);
    Script_RegisterIntFunction("AIFunction_GetGunnerStatus", AIFunction_GetGunnerStatus, NULL, 0);
    Script_RegisterIntFunction("AIFunction_GetScriptIntegerValue", AIFunction_GetScriptIntegerValue, NULL, 1);
    Script_RegisterIntFunction("AIFunction_GetCurrentEventType", AIFunction_GetCurrentEventType, NULL, 0);
    Script_RegisterIntFunction("AIFunction_IsEventBehind", AIFunction_IsEventBehind, NULL, 0);
    Script_RegisterFunctionEx("AIFunction_GetScriptRealValue", AIFunction_GetScriptRealValue, NULL, 1);
    Script_RegisterFunctionEx("AIFunction_GetRandomValue", AIFunction_GetRandomValue, NULL, 1);
    Script_RegisterFunctionEx("AIFunction_GetEventDistance", AIFunction_GetEventDistance, NULL, 0);
    Script_RegisterFunctionEx("AIFunction_GetAlarmTriggerDistance", AIFunction_GetAlarmTriggerDistance, NULL, 0);
    Script_RegisterFunction("AIFunction_SetAnimationInterval", AIFunction_SetAnimationInterval, NULL, 2);
    Script_RegisterFunction("AIFunction_AddAnimationEntry", AIFunction_AddAnimationEntry, NULL, 2);
    Script_RegisterIntFunction("AIFunction_GetAnimationToPlay", AIFunction_GetAnimationToPlay, NULL, 0);
    Script_RegisterFunction("AIFunction_SendResponse", AIFunction_SendResponse, NULL, 0);
}
HOOK(0x44CCA0, HumanAI_RegisterFunctions);

ScriptValue *AIAction_Patrol(ScriptValue *out, void *data, void *args)
{
    NodeData *d = HumanAI_AllocParam(gAICurrent);

    d->id = Script_GetArgInt(args, 0);
    d->unkC = Script_GetArgInt(args, 1);
    HumanAI_PushAction(AI_PATROL, d, Script_GetArgInt(args, 2));
    return ret_int(out, 3);
}
HOOK(0x44D0E0, AIAction_Patrol);

/* each AI type fights its own way, civilians not at all */
ScriptValue *AIAction_Combat(ScriptValue *out, void *data, void *args)
{
    AIParam *d = HumanAI_AllocParam(gAICurrent);
    int flags = Script_GetArgInt(args, 0);
    void *fn;

    d->d.first = 1;
    d->d.events = gAIEventList;
    d->d.spoke = 0;
    d->d.frame = 0;
    switch (gAICurrent->aiType) {
    case 0:
        fn = AI_COMBAT_RPG;
        break;
    case 1:
        fn = AI_COMBAT_GUNNER;
        break;
    case 2: case 5: case 11: case 12: case 13: case 14: case 20: case 21: case 22: case 26: case 27: case 28:
        fn = AI_COMBAT_GUARD;
        break;
    case 3: case 4:
        fn = AI_COMBAT_ANYA;
        break;
    case 7: case 8: case 9: case 10: case 15: case 16: case 17: case 18: case 19: case 23: case 24: case 25:
        fn = AI_COMBAT_PATROL;
        break;
    default:
        return ret_int(out, 1);
    }
    HumanAI_PushAction(fn, d, flags);
    return ret_int(out, 1);
}
HOOK(0x44D160, AIAction_Combat);

ScriptValue *AIAction_Dead(ScriptValue *out, void *data, void *args)
{
    HumanAI_PushAction(AI_DEAD, NULL, Script_GetArgInt(args, 0));
    return ret_int(out, 1);
}
HOOK(0x44D260, AIAction_Dead);

/* does nothing when the AI stands on a node with the AIGraph_GetStairCriteria flag or
   the target of the current event is nearer than fallFlatRange */
ScriptValue *AIAction_FallFlat(ScriptValue *out, void *data, void *args)
{
    void *node = Graph_GetNode(gAICurrent->graph, gAICurrent->node);
    AIEvent *e;
    NodeData *d;

    if (GraphNode_HasCriteria(node, AIGraph_GetStairCriteria()))
        return ret_int(out, 2);
    e = AIEvent_GetCurrent();
    if (e->about && *e->about) {
        Task *t = *e->about;
        Vec3d *a = DynCubeObj_GetPosition(gAICurrent->task.parent);

        if (!(Vec3d_Distance(DynCubeObj_GetPosition(t), a) >= gAICurrent->fallFlatRange))
            return ret_int(out, 2);
    }
    d = HumanAI_AllocParam(gAICurrent);
    *(float *)&d->id = Script_GetArgReal(args, 0);
    HumanAI_PushAction(AI_FALL_FLAT, d, Script_GetArgInt(args, 1));
    return ret_int(out, 2);
}
HOOK(0x44D2C0, AIAction_FallFlat);

ScriptValue *AIAction_Activate(ScriptValue *out, void *data, void *args)
{
    NodeData *d = HumanAI_AllocParam(gAICurrent);

    d->id = Script_GetArgInt(args, 0);
    d->unk14 = Script_GetArgInt(args, 1);
    HumanAI_PushAction(AI_ACTIVATE, d, Script_GetArgInt(args, 2));
    return ret_int(out, 3);
}
HOOK(0x44D420, AIAction_Activate);

ScriptValue *AIAction_WalkToNode(ScriptValue *out, void *data, void *args)
{
    NodeData *d = HumanAI_AllocParam(gAICurrent);

    d->id = Script_GetArgInt(args, 0);
    HumanAI_PushAction(AI_WALK_TO_NODE, d, Script_GetArgInt(args, 1));
    return ret_int(out, 2);
}
HOOK(0x44D4A0, AIAction_WalkToNode);

ScriptValue *AIAction_RunToNode(ScriptValue *out, void *data, void *args)
{
    NodeData *d = HumanAI_AllocParam(gAICurrent);

    d->id = Script_GetArgInt(args, 0);
    HumanAI_PushAction(AI_RUN_TO_NODE, d, Script_GetArgInt(args, 1));
    return ret_int(out, 2);
}
HOOK(0x44D510, AIAction_RunToNode);

/* target, min and max time in seconds, a flag, action flags */
static void fire_at(void *fn, void *args)
{
    NodeData *d = HumanAI_AllocParam(gAICurrent);

    d->id = Script_GetArgInt(args, 0);
    d->unkC = round_int(Script_GetArgReal(args, 1) * kFps);
    d->unk10 = round_int(Script_GetArgReal(args, 2) * kFps);
    d->unk18 = Script_GetArgInt(args, 3);
    HumanAI_PushAction(fn, d, Script_GetArgInt(args, 4));
}

ScriptValue *AIAction_FireAtNode(ScriptValue *out, void *data, void *args)
{
    fire_at(AI_FIRE_AT_NODE, args);
    return ret_int(out, 5);
}
HOOK(0x44D580, AIAction_FireAtNode);

ScriptValue *AIAction_FireAtTask(ScriptValue *out, void *data, void *args)
{
    fire_at(AI_FIRE_AT_TASK, args);
    return ret_int(out, 5);
}
HOOK(0x44D630, AIAction_FireAtTask);

ScriptValue *AIAction_PlayAnimation(ScriptValue *out, void *data, void *args)
{
    NodeData *d = HumanAI_AllocParam(gAICurrent);

    d->id = Script_GetArgInt(args, 0);
    HumanAI_PushAction(AI_PLAY_ANIMATION, d, Script_GetArgInt(args, 1));
    return ret_int(out, 2);
}
HOOK(0x44D6E0, AIAction_PlayAnimation);

ScriptValue *AIAction_PlaySound(ScriptValue *out, void *data, void *args)
{
    SoundData *d = HumanAI_AllocParam(gAICurrent);

    Script_GetArgString(args, 0, d->name, sizeof(d->name));
    d->unk28 = Script_GetArgInt(args, 1);
    HumanAI_PushAction(AI_PLAY_SOUND, d, Script_GetArgInt(args, 2));
    return ret_int(out, 3);
}
HOOK(0x44D750, AIAction_PlaySound);

ScriptValue *AIAction_MoveToEvent(ScriptValue *out, void *data, void *args)
{
    EventData *d = HumanAI_AllocParam(gAICurrent);

    AIEvent_GetPos(&d->pos);
    d->unk24.c = Script_GetArgInt(args, 0);
    HumanAI_PushAction(AI_MOVE_TO_EVENT, d, Script_GetArgInt(args, 1));
    return ret_int(out, 2);
}
HOOK(0x44D7D0, AIAction_MoveToEvent);

/* out is left alone when there is no event */
void AIEvent_GetPos(Vec3d *out)
{
    AIEvent *e = (AIEvent *)gAIEventList->head;

    if (e->node.next)
        *out = e->pos;
}
HOOK(0x44D850, AIEvent_GetPos);

ScriptValue *AIAction_LookAtEvent(ScriptValue *out, void *data, void *args)
{
    EventData *d = HumanAI_AllocParam(gAICurrent);

    AIEvent_GetPos(&d->pos);
    d->unk20.f = Script_GetArgReal(args, 0);
    HumanAI_PushAction(AI_LOOK_AT_EVENT, d, Script_GetArgInt(args, 1));
    return ret_int(out, 2);
}
HOOK(0x44D870, AIAction_LookAtEvent);

ScriptValue *AIAction_Stunned(ScriptValue *out, void *data, void *args)
{
    NodeData *d = HumanAI_AllocParam(gAICurrent);

    *(float *)&d->id = Script_GetArgReal(args, 0);
    HumanAI_PushAction(AI_STUNNED, d, Script_GetArgInt(args, 1));
    return ret_int(out, 2);
}
HOOK(0x44D8F0, AIAction_Stunned);

ScriptValue *AIAction_KickGrenade(ScriptValue *out, void *data, void *args)
{
    AIEvent *e = AIEvent_GetCurrent();
    int flags = Script_GetArgInt(args, 0);

    if (*e->from) {
        EventData *d = HumanAI_AllocParam(gAICurrent);

        d->pos = e->pos;
        d->ref = TaskRef_Add(*e->from);
        HumanAI_PushAction(AI_KICK_GRENADE, d, flags);
    }
    return ret_int(out, 1);
}
HOOK(0x44D960, AIAction_KickGrenade);

ScriptValue *AIAction_RunPanicking(ScriptValue *out, void *data, void *args)
{
    EventData *d = HumanAI_AllocParam(gAICurrent);
    int flags;

    d->time = Script_GetArgReal(args, 0);
    flags = Script_GetArgInt(args, 1);
    AIEvent_GetPos(&d->pos);
    HumanAI_PushAction(AI_RUN_PANICKING, d, flags);
    return ret_int(out, 2);
}
HOOK(0x44DA00, AIAction_RunPanicking);

ScriptValue *AIAction_Idle(ScriptValue *out, void *data, void *args)
{
    HumanAI_PushAction(AI_IDLE, NULL, Script_GetArgInt(args, 0));
    return ret_int(out, 1);
}
HOOK(0x44DA80, AIAction_Idle);

ScriptValue *AIAction_SetCombat(ScriptValue *out, void *data, void *args)
{
    EventData *d = HumanAI_AllocParam(gAICurrent);
    AIEvent *e = AIEvent_GetCurrent();
    int flags = Script_GetArgInt(args, 0);

    if (e && e->about && *e->about) {
        d->pos = e->pos;
        d->unk24.target = NULL;
        d->unk20.source = NULL;
        if (e->from && *e->from)
            d->unk20.source = TaskRef_Add(*e->from);
        if (e->about && *e->about)
            d->unk24.target = TaskRef_Add(*e->about);
        HumanAI_PushAction(AI_SET_COMBAT, d, flags);
    }
    return ret_int(out, 1);
}
HOOK(0x44DAE0, AIAction_SetCombat);

ScriptValue *AIFunction_RemoveAlarmActions(ScriptValue *out, void *data, void *args)
{
    HumanAI *ai = gAICurrent;
    int tag = ai->eventOrder[8];
    AIAction *a = (AIAction *)ai->actions.head;
    AIAction *next;

    for (next = (AIAction *)a->node.next; next; a = next, next = (AIAction *)next->node.next) {
        if (a->event == tag) {
            HumanAI_EndAction(ai, a);
            ai = gAICurrent;
        }
    }
    return ret_int(out, 1);
}
HOOK(0x44DBC0, AIFunction_RemoveAlarmActions);

ScriptValue *AIFunction_SetViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.length = Script_GetArgReal(args, 0) * kViewScale;
    if (gAICurrent->view.length > gAICurrent->view.alarmLength)
        gAICurrent->view.alarmLength = gAICurrent->view.length;
    return ret_int(out, 1);
}
HOOK(0x44DC40, AIFunction_SetViewLength);

ScriptValue *AIFunction_SetAlarmViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.alarmLength = Script_GetArgReal(args, 0) * kViewScale;
    return ret_int(out, 1);
}
HOOK(0x44DCD0, AIFunction_SetAlarmViewLength);

ScriptValue *AIFunction_SetViewAlpha(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.alpha = Script_GetArgReal(args, 0) * kDegToRad;
    return ret_int(out, 1);
}
HOOK(0x44DD30, AIFunction_SetViewAlpha);

ScriptValue *AIFunction_SetViewGamma(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.gamma = Script_GetArgReal(args, 0) * kDegToRad;
    return ret_int(out, 1);
}
HOOK(0x44DD90, AIFunction_SetViewGamma);

ScriptValue *AIFunction_SetSecondaryViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.secLength = Script_GetArgReal(args, 0) * kViewScale;
    if (gAICurrent->view.secLength > gAICurrent->view.secAlarmLength)
        gAICurrent->view.secAlarmLength = gAICurrent->view.secLength;
    return ret_int(out, 1);
}
HOOK(0x44DDF0, AIFunction_SetSecondaryViewLength);

ScriptValue *AIFunction_SetSecondaryAlarmViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.secAlarmLength = Script_GetArgReal(args, 0) * kViewScale;
    return ret_int(out, 1);
}
HOOK(0x44DE80, AIFunction_SetSecondaryAlarmViewLength);

ScriptValue *AIFunction_SetSecondaryViewAlpha(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.secAlpha = Script_GetArgReal(args, 0) * kDegToRad;
    return ret_int(out, 1);
}
HOOK(0x44DEE0, AIFunction_SetSecondaryViewAlpha);

ScriptValue *AIFunction_SetSecondaryViewGamma(ScriptValue *out, void *data, void *args)
{
    gAICurrent->view.secGamma = Script_GetArgReal(args, 0) * kDegToRad;
    return ret_int(out, 1);
}
HOOK(0x44DF40, AIFunction_SetSecondaryViewGamma);

ScriptValue *AIFunction_GetCurrentEventType(ScriptValue *out, void *data, void *args)
{
    AIEvent *e = (AIEvent *)gAIEventList->head;

    if (!e->node.next) {
        Error_Show("EventStack empty");
        for (;;)
            ;
    }
    return ret_int(out, e->type);
}
HOOK(0x44DFA0, AIFunction_GetCurrentEventType);

/* a random real in [0, max), its integer part rounded */
ScriptValue *AIFunction_GetRandomValue(ScriptValue *out, void *data, void *args)
{
    float max = Script_GetArgReal(args, 0);
    long double r = Random_Float(Game_GetRandom()) * max;

    out->i = round_int(r);
    out->d = r;
    out->s = gEmptyString;
    return out;
}
HOOK(0x44E000, AIFunction_GetRandomValue);

/* runs the default script of the AI's type */
ScriptValue *AIFunction_DefaultHandler(ScriptValue *out, void *data, void *args)
{
    Script_Run(gAIScripts[gAIScriptOfType[gAICurrent->aiType]]);
    return ret_int(out, 1);
}
HOOK(0x44E060, AIFunction_DefaultHandler);

ScriptValue *AIFunction_SetEventPriority(ScriptValue *out, void *data, void *args)
{
    int e = Script_GetArgInt(args, 0);

    if (!gAICurrent->eventEnabled[e]) {
        gAICurrent->eventEnabled[e] = 1;
        gAICurrent->eventOrder[gAICurrent->eventOrderCount] = e;
        gAICurrent->eventOrderCount++;
    }
    return ret_int(out, 1);
}
HOOK(0x44E0C0, AIFunction_SetEventPriority);

ScriptValue *AIFunction_SetInvulnerability(ScriptValue *out, void *data, void *args)
{
    gAICurrent->invulnerable = Script_GetArgInt(args, 0);
    return ret_int(out, 1);
}
HOOK(0x44E150, AIFunction_SetInvulnerability);

ScriptValue *AIFunction_SetInstantDeath(ScriptValue *out, void *data, void *args)
{
    gAICurrent->noInstantDeath = (char)Script_GetArgInt(args, 0) == 0;
    return ret_int(out, 1);
}
HOOK(0x44E1B0, AIFunction_SetInstantDeath);

ScriptValue *AIFunction_SetDeathAnimation(ScriptValue *out, void *data, void *args)
{
    gAICurrent->deathAnim = Script_GetArgInt(args, 0);
    return ret_int(out, 1);
}
HOOK(0x44E210, AIFunction_SetDeathAnimation);

ScriptValue *AIFunction_SetAlarmTriggerID(ScriptValue *out, void *data, void *args)
{
    gAICurrent->alarmTriggerId = Script_GetArgInt(args, 0);
    return ret_int(out, 1);
}
HOOK(0x44E270, AIFunction_SetAlarmTriggerID);

ScriptValue *AIFunction_SetAlarmControlID(ScriptValue *out, void *data, void *args)
{
    gAICurrent->alarmControlId = Script_GetArgInt(args, 0);
    return ret_int(out, 1);
}
HOOK(0x44E2D0, AIFunction_SetAlarmControlID);

ScriptValue *AIFunction_SetAlarmAccess(ScriptValue *out, void *data, void *args)
{
    gAICurrent->alarmAccess = Script_GetArgInt(args, 0);
    return ret_int(out, 1);
}
HOOK(0x44E330, AIFunction_SetAlarmAccess);

ScriptValue *AIFunction_SetGunnerID(ScriptValue *out, void *data, void *args)
{
    gAICurrent->statGunId = Script_GetArgInt(args, 0);
    return ret_int(out, 1);
}
HOOK(0x44E390, AIFunction_SetGunnerID);

ScriptValue *AIFunction_GetAlarmTriggerID(ScriptValue *out, void *data, void *args)
{
    return ret_int(out, gAICurrent->alarmTriggerId);
}
HOOK(0x44E3F0, AIFunction_GetAlarmTriggerID);

ScriptValue *AIFunction_GetAlarmControlID(ScriptValue *out, void *data, void *args)
{
    return ret_int(out, gAICurrent->alarmControlId);
}
HOOK(0x44E430, AIFunction_GetAlarmControlID);

ScriptValue *AIFunction_GetAlarmAccess(ScriptValue *out, void *data, void *args)
{
    return ret_int(out, gAICurrent->alarmAccess);
}
HOOK(0x44E470, AIFunction_GetAlarmAccess);

ScriptValue *AIFunction_GetGunnerID(ScriptValue *out, void *data, void *args)
{
    return ret_int(out, gAICurrent->statGunId);
}
HOOK(0x44E4B0, AIFunction_GetGunnerID);

ScriptValue *AIFunction_GetAlarmControlStatus(ScriptValue *out, void *data, void *args)
{
    Task *t;
    int i;

    if (gAICurrent->alarmControlId == -1)
        return ret_int(out, 0);
    t = Task_FindById(gAICurrent->alarmControlId);
    if (!t) {
        Warning_Show("AlarmControl ID #%d not defined", gAICurrent->alarmControlId);
        return ret_int(out, 0);
    }
    i = AlarmControl_IsAlarm(t);
    out->i = i;
    out->d = AlarmControl_IsAlarm(t);
    out->s = gEmptyString;
    return out;
}
HOOK(0x44E4F0, AIFunction_GetAlarmControlStatus);

ScriptValue *AIFunction_GetGunnerStatus(ScriptValue *out, void *data, void *args)
{
    Task *t;
    int i;

    if (gAICurrent->statGunId == -1)
        return ret_int(out, 0);
    t = Task_FindById(gAICurrent->statGunId);
    if (!t) {
        Warning_Show("StationaryGun ID #%d not defined", gAICurrent->statGunId);
        return ret_int(out, 0);
    }
    i = StationaryGun_IsUsed(t);
    out->i = i;
    out->d = StationaryGun_IsUsed(t);
    out->s = gEmptyString;
    return out;
}
HOOK(0x44E5E0, AIFunction_GetGunnerStatus);

ScriptValue *AIFunction_SetScriptIntegerValue(ScriptValue *out, void *data, void *args)
{
    int i = Script_GetArgInt(args, 0);

    gAICurrent->scriptInts[i] = Script_GetArgInt(args, 1);
    return ret_int(out, 1);
}
HOOK(0x44E6D0, AIFunction_SetScriptIntegerValue);

ScriptValue *AIFunction_GetScriptIntegerValue(ScriptValue *out, void *data, void *args)
{
    return ret_int(out, gAICurrent->scriptInts[Script_GetArgInt(args, 0)]);
}
HOOK(0x44E740, AIFunction_GetScriptIntegerValue);

ScriptValue *AIFunction_SetScriptRealValue(ScriptValue *out, void *data, void *args)
{
    int i = Script_GetArgInt(args, 0);

    gAICurrent->scriptReals[i] = Script_GetArgReal(args, 1);
    return ret_int(out, 1);
}
HOOK(0x44E790, AIFunction_SetScriptRealValue);

ScriptValue *AIFunction_GetScriptRealValue(ScriptValue *out, void *data, void *args)
{
    double *v = &gAICurrent->scriptReals[Script_GetArgInt(args, 0)];

    out->i = round_int(*v);
    out->d = *v;
    out->s = gEmptyString;
    return out;
}
HOOK(0x44E800, AIFunction_GetScriptRealValue);

/* true when an event on the stack lies behind the AI */
ScriptValue *AIFunction_IsEventBehind(ScriptValue *out, void *data, void *args)
{
    AIEvent *e = (AIEvent *)gAIEventList->head;
    AIEvent *next;

    for (next = (AIEvent *)e->node.next; next; e = next, next = (AIEvent *)next->node.next) {
        HumanAI *ai = gAICurrent;
        long double dx = e->pos.x - ai->view.eye.x;
        long double dy = e->pos.y - ai->view.eye.y;
        long double dz = e->pos.z - ai->view.eye.z;

        if (!((ai->view.view[7] * dz + ai->view.view[1] * dx) + ai->view.view[4] * dy >= 0.0))
            return ret_int(out, 1);
    }
    return ret_int(out, 0);
}
HOOK(0x44E860, AIFunction_IsEventBehind);

ScriptValue *AIFunction_GetEventDistance(ScriptValue *out, void *data, void *args)
{
    AIEvent *e = AIEvent_GetCurrent();

    if (!e) {
        Error_Show("No EVENT to distance");
        for (;;)
            ;
    }
    out->i = round_int(Vec3d_Distance(&e->pos, &gAICurrent->view.eye));
    out->d = Vec3d_Distance(&e->pos, &gAICurrent->view.eye);
    out->s = gEmptyString;
    return out;
}
HOOK(0x44E930, AIFunction_GetEventDistance);

/* -1 when there is no alarm trigger or it is not a RigidDynCubeObj */
ScriptValue *AIFunction_GetAlarmTriggerDistance(ScriptValue *out, void *data, void *args)
{
    Task *t;

    if (gAICurrent->alarmTriggerId == -1)
        return ret_int(out, -1);
    t = Task_FindById(gAICurrent->alarmTriggerId);
    if (!t) {
        Warning_Show("AlarmTrigger ID #%d not defined", gAICurrent->alarmTriggerId);
        return ret_int(out, -1);
    }
    if (!TaskType_IsA(t->type, RigidDynCubeObj_GetType()))
        return ret_int(out, -1);
    out->i = round_int(Vec3d_Distance(&gAICurrent->view.pos, (Vec3d *)(t + 1)));
    out->d = Vec3d_Distance(&gAICurrent->view.pos, (Vec3d *)(t + 1));
    out->s = gEmptyString;
    return out;
}
HOOK(0x44E9B0, AIFunction_GetAlarmTriggerDistance);

/* seconds; animTimer gets min + random(range) frames */
ScriptValue *AIFunction_SetAnimationInterval(ScriptValue *out, void *data, void *args)
{
    gAICurrent->animBase = round_int(Script_GetArgReal(args, 0) * kFps);
    gAICurrent->animRandom = round_int(Script_GetArgReal(args, 1) * kFps);
    gAICurrent->animTimer = Random_Int(Game_GetRandom(), gAICurrent->animRandom) + gAICurrent->animBase;
    return ret_int(out, 2);
}
HOOK(0x44EAF0, AIFunction_SetAnimationInterval);

ScriptValue *AIFunction_AddAnimationEntry(ScriptValue *out, void *data, void *args)
{
    if (gAICurrent->animEnabled >= 5) {
        Warning_Show("Maximum number of animation entries (#%d) reached", 5);
        Script_GetArgInt(args, 0);
        Script_GetArgReal(args, 1);
        return ret_int(out, 2);
    }
    gAICurrent->anims[gAICurrent->animEnabled + 1].anim = Script_GetArgInt(args, 0);
    gAICurrent->anims[gAICurrent->animEnabled + 1].weight = Script_GetArgReal(args, 1);
    gAICurrent->animEnabled++;
    return ret_int(out, 2);
}
HOOK(0x44EBB0, AIFunction_AddAnimationEntry);

/* picks an entry by weight, or any entry when the weights add up to
   almost nothing */
ScriptValue *AIFunction_GetAnimationToPlay(ScriptValue *out, void *data, void *args)
{
    int n = gAICurrent->animEnabled;
    long double sum, r;
    double total;
    int i;

    if (n <= 0) {
        Warning_Show("No animation entries defined");
        return ret_int(out, -1);
    }
    sum = 0.0;
    for (i = 0; i < n; i++)
        sum += gAICurrent->anims[i + 1].weight;
    total = sum;
    if (!(sum >= kMinWeight))
        return ret_int(out, gAICurrent->anims[Random_Int(Game_GetRandom(), n) + 1].anim);
    r = Random_Float(Game_GetRandom()) * total;
    for (i = 0; i < gAICurrent->animEnabled; i++) {
        r -= gAICurrent->anims[i + 1].weight;
        if (!(r > 0.0))
            return ret_int(out, gAICurrent->anims[i + 1].anim);
    }
    return ret_int(out, gAICurrent->anims[i].anim);
}
HOOK(0x44ECB0, AIFunction_GetAnimationToPlay);

/* answers the source of the current event with AIEVENT_ALERT_RESPONSE */
ScriptValue *AIFunction_SendResponse(ScriptValue *out, void *data, void *args)
{
    AIEvent *e = (AIEvent *)gAIEventList->head;

    if (!e->node.next) {
        Error_Show("No EVENT to respond");
        for (;;)
            ;
    }
    HumanAI_SendSimple(*e->from, 6, 0x24);
    return ret_int(out, 0);
}
HOOK(0x44EE40, AIFunction_SendResponse);

void HumanAI_RegisterConstants(void)
{
    int i;

    Script_RegisterConstant("AIEVENT_CREATE", 0);
    Script_RegisterConstant("AIEVENT_DELETE", 1);
    Script_RegisterConstant("AIEVENT_DEAD", 2);
    Script_RegisterConstant("AIEVENT_ANIMATION", 3);
    Script_RegisterConstant("AIEVENT_IDLE", 4);
    Script_RegisterConstant("AIEVENT_ALERT", 5);
    Script_RegisterConstant("AIEVENT_ALERT_RESPONSE", 6);
    Script_RegisterConstant("AIEVENT_COMBAT", 7);
    Script_RegisterConstant("AIEVENT_ALARMON", 8);
    Script_RegisterConstant("AIEVENT_ALARMOFF", 9);
    Script_RegisterConstant("AIEVENT_WALK", 10);
    Script_RegisterConstant("AIEVENT_GROUNDIMPACT", 11);
    Script_RegisterConstant("AIEVENT_DOOR", 12);
    Script_RegisterConstant("AIEVENT_FENCE", 13);
    Script_RegisterConstant("AIEVENT_LADDER", 14);
    Script_RegisterConstant("AIEVENT_TAKINGDAMAGE", 15);
    Script_RegisterConstant("AIEVENT_GUNSHOT", 16);
    Script_RegisterConstant("AIEVENT_GRENADETHROWN", 17);
    Script_RegisterConstant("AIEVENT_GRENADELAND", 18);
    Script_RegisterConstant("AIEVENT_FLASHBANG", 19);
    Script_RegisterConstant("AIEVENT_GUNSHOTMISS", 20);
    Script_RegisterConstant("AIEVENT_EXPLOSION", 21);
    Script_RegisterConstant("AIEVENT_ENEMYDETECTION", 22);
    Script_RegisterConstant("AIEVENT_FRIENDLYDETECTION", 23);
    Script_RegisterConstant("AIACTIONFLAG_NONE", 0);
    Script_RegisterConstant("AIACTIONFLAG_PUSHABLE", 1);
    Script_RegisterConstant("HUMANAI_DETECTIONEVENT_GUNSHOT_RANGE", 0x1E000);
    Script_RegisterConstant("HUMANAI_DETECTIONEVENT_GUNSHOT_SILENCED_RANGE", 0x1000);
    for (i = 0; i < 29; i++)
        Script_RegisterConstant(gAITypeNames[i], i);
    Script_RegisterConstant("AIALARMACCESS_BEFORECOMBAT", 0);
    Script_RegisterConstant("AIALARMACCESS_AFTERCOMBAT", 1);
}
HOOK(0x44EEB0, HumanAI_RegisterConstants);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(str_53D30C);
REF_EXTERN(str_53D328);
REF_EXTERN(str_53D344);
REF_EXTERN(str_53D364);
REF_EXTERN(str_53D380);
REF_EXTERN(str_53D398);
REF_EXTERN(str_53D3B4);
REF_EXTERN(str_53D3D0);
REF_EXTERN(str_53D3EC);
STORE(igd, 53C928, 4) const char *store_53C928[9] INIT({(__typeof__(store_53C928[0]))ref_str_53D3EC, (__typeof__(store_53C928[1]))ref_str_53D3D0, (__typeof__(store_53C928[2]))ref_str_53D3B4, (__typeof__(store_53C928[3]))ref_str_53D398, (__typeof__(store_53C928[4]))ref_str_53D380, (__typeof__(store_53C928[5]))ref_str_53D364, (__typeof__(store_53C928[6]))ref_str_53D344, (__typeof__(store_53C928[7]))ref_str_53D328, (__typeof__(store_53C928[8]))ref_str_53D30C});
ALIAS(gAIScriptNames, store_53C928, 0);
STORE(igd, 53C94C, 4) const int store_53C94C[29] INIT({0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 8, 8, 8, 8, 7, 7, 7, 7, 7, 8, 8, 8, 7, 7, 7, 8, 8, 8});
ALIAS(gAIScriptOfType, store_53C94C, 0);
DATA(53D30C) char str_53D30C[26] INIT("LOCAL:common/ai/guard.qsc");
PAD(igd, 53D30C, 53D326, 0x2);
DATA(53D30C) char str_53D328[27] INIT("LOCAL:common/ai/patrol.qsc");
PAD(igd, 53D30C, 53D343, 0x1);
DATA(53D30C) char str_53D344[29] INIT("LOCAL:common/ai/civilian.qsc");
PAD(igd, 53D30C, 53D361, 0x3);
DATA(53D30C) char str_53D364[27] INIT("LOCAL:common/ai/priboi.qsc");
PAD(igd, 53D30C, 53D37F, 0x1);
DATA(53D30C) char str_53D380[24] INIT("LOCAL:common/ai/ekk.qsc");
DATA(53D30C) char str_53D398[25] INIT("LOCAL:common/ai/anya.qsc");
PAD(igd, 53D30C, 53D3B1, 0x3);
DATA(53D30C) char str_53D3B4[27] INIT("LOCAL:common/ai/sniper.qsc");
PAD(igd, 53D30C, 53D3CF, 0x1);
DATA(53D30C) char str_53D3D0[27] INIT("LOCAL:common/ai/gunner.qsc");
PAD(igd, 53D30C, 53D3EB, 0x1);
DATA(53D30C) char str_53D3EC[24] INIT("LOCAL:common/ai/rpg.qsc");
BSS(57C758) MonitorGraph gMonitorGraphs[AI_MONITOR_GRAPHS];
BSS(57C758) MonitorTarget gMonitorTargets[AI_MONITOR_TARGETS];
BSS(57D778) Claim gClaims[AI_CLAIMS];
BSS(57E1C8) MoveItem gAIMoveItems[AI_MOVE_ITEMS];
BSS(57E1C8) List gAIMoveFree;
BSS(57E1C8) void *gHumanAIParams;
BSS(58AA28) NodeGrid gNodeGrids[AI_GRIDS];
BSS(5BDA38) int gHumanAIUnusedInts[8];
BSS(5BDA38) HumanAI *gAICurrent;
BSS(5BDA60) List *gAIEventList;
