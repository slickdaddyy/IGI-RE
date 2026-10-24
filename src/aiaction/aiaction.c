#pragma GCC optimize("excess-precision=standard")

#include "aiaction/aiaction.h"
#include "hook.h"

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
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
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

/* an entry of the event stack */
typedef struct AIEvent {
    Node node;
    int type;           /* AIEVENT_* */
    int unkC;
    Vec3d pos;          /* 0x10 */
    Task **source;      /* 0x28 */
    Task **target;      /* 0x2C */
} AIEvent;

/* a queued action */
typedef struct AIAction {
    Node node;
    int unk8;
    char unkC[0x1C];
    void *data;         /* 0x28 */
    char active;        /* 0x2C */
} AIAction;

typedef struct AnimEntry {
    int anim;
    int unk4;
    double weight;
} AnimEntry;

/* "HumanAI" (0x3ED0), the fields used here */
typedef struct HumanAI {
    Task task;
    Vec3d pos;
    Vec3d eye;          /* 0x38 */
    char unk50[0x2C];
    float dirX;         /* 0x7C */
    char unk80[8];
    float dirY;         /* 0x88 */
    char unk8C[8];
    float dirZ;         /* 0x94 */
    char unk98[8];
    double viewAlpha;   /* 0xA0, radians */
    double viewGamma;
    double viewLength;
    double alarmViewLength;
    double secondaryViewAlpha; /* 0xC0 */
    double secondaryViewGamma;
    double secondaryViewLength;
    double secondaryAlarmViewLength;
    char unkE0[0x81C - 0xE0];
    union {
        int eventOrder[48]; /* 0x81C, by SetEventPriority */
        struct {
            int unk81C[8];
            int unk83C;
        };
    };
    int eventCount;     /* 0x8DC */
    int unk8E0;
    char eventUsed[0x18]; /* 0x8E4 */
    List actions;       /* 0x8FC */
    char unk908[0x1508 - 0x908];
    List freeData;      /* 0x1508 */
    char unk1514[0x3ABC - 0x1514];
    char invulnerable;  /* 0x3ABC */
    char noInstantDeath;
    char unk3ABE[2];
    int alarmTriggerId; /* 0x3AC0 */
    int alarmControlId;
    int gunnerId;
    int alarmAccess;
    int deathAnimation; /* 0x3AD0 */
    int scriptInts[5];  /* 0x3AD4 */
    double scriptReals[2]; /* 0x3AE8 */
    AnimEntry anims[6]; /* 0x3AF8, entries start at anims[1] */
    int animMin;        /* 0x3B58, frames */
    int animRange;
    int animTimer;
    int animCount;      /* 0x3B64 */
    int graphId;        /* 0x3B68 */
    void *graph;
    char unk3B70[0x3D15 - 0x3B70];
    char name[0x23];    /* 0x3D15 */
    int aiType;         /* 0x3D38 */
    char unk3D3C[0x3DB8 - 0x3D3C];
    double fallFlatRange; /* 0x3DB8 */
    char unk3DC0[0x3E2C - 0x3DC0];
    int nodeId;         /* 0x3E2C */
    char unk3E30[0x3ED0 - 0x3E30];
} HumanAI;

_Static_assert(offsetof(HumanAI, eventCount) == 0x8DC, "HumanAI");
_Static_assert(offsetof(HumanAI, unk83C) == 0x83C, "HumanAI");
_Static_assert(offsetof(HumanAI, anims) == 0x3AF8, "HumanAI");
_Static_assert(offsetof(HumanAI, aiType) == 0x3D38, "HumanAI");
_Static_assert(sizeof(HumanAI) == 0x3ED0, "HumanAI");

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

typedef struct CombatData {
    Node node;
    List *events;
    char unkC[0x18];
    int unk24;
    char unk28;
    char unk29;
} CombatData;

_Static_assert(offsetof(EventData, ref) == 0x28, "EventData");
_Static_assert(offsetof(CombatData, unk28) == 0x28, "CombatData");

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
#define kDegToRad K(float, 0x53367C)

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
    int i;

    gHumanAIType = TaskType_New("HumanAI", sizeof(HumanAI), Task_GetRootType(), 0);
    TaskType_SetHandler(gHumanAIType, 0, HumanAI_Create);
    TaskType_SetHandler2(gHumanAIType, 1, HumanAI_Run);
    TaskType_SetHandler2(gHumanAIType, 2, HumanAI_Delete);
    TaskType_SetHandler(gHumanAIType, HumanAI_GetEventSlot(), HumanAI_OnEvent);
    TaskType_SetHandler(gHumanAIType, Param_GetTableEvent(), HumanAI_GetParams);
    TaskType_SetHandler(gHumanAIType, Param_GetChangedEvent(), HumanAI_Changed);
    TaskType_SetHandler2(gHumanAIType, Level_GetEventDA8(), HumanAI_OnLevelDA8);
    TaskType_SetHandler2(gHumanAIType, Level_GetEventDAC(), HumanAI_OnLevelDAC);
    TaskType_SetHandler(gHumanAIType, 4, NULL);
    gHumanAIParams = ParamTable_New(3);
    ParamTable_Add(gHumanAIParams, &t, t.name, Param_String32(), 7, NULL);
    ParamTable_Add(gHumanAIParams, &t, &t.graphId, Param_Int32(), 7, NULL);
    ParamTable_Add(gHumanAIParams, &t, &t.unk3B70, Param_ScriptFile(), 2, NULL);
    HumanAI_RegisterFunctions();
    HumanAI_RegisterConstants();

#define LIST_INIT(addr) (l = (List *)(addr), l->head = (Node *)&l->tail, l->tailpred = (Node *)&l->head, l->tail = NULL)
#define NODES(addr, end, size, list)                         \
    for (p = (char *)(addr); p < (char *)(end); p += (size)) \
        ((Node *)p)->prev = NULL, ((Node *)p)->next = NULL, List_AddTail((List *)(list), p)

    LIST_INIT(0x58A9D8);
    for (p = (char *)0x57D840; p < (char *)0x57E1C0; p += 0x130)
        *p = 0;
    LIST_INIT(0x58A9C8);
    NODES(0x57E1C8, 0x58A9C8, 0x28, 0x58A9C8);
    for (i = 0; i < 9; i++)
        gAIScripts[i] = Script_Load(gAIScriptNames[i]);
    LIST_INIT(0x57D768);
    LIST_INIT(0x58A9E8);
    for (p = (char *)0x58AA28; p < (char *)0x5BDA28; p += 0x660) {
        int *q = (int *)p;

        q[1] = 0;
        q[0] = 0;
        List_AddTail((List *)0x58A9E8, p);
        for (i = 8; i < 0x198; i++)
            q[i] = 0;
        q[2] = -1;
        q[3] = -1;
        q[5] = 0;
        q[6] = 0;
        q[4] = 0;
    }
    LIST_INIT(0x5BDA28);
    LIST_INIT(0x57D758);
    NODES(0x57D778, 0x57D838, 0xC, 0x57D758);
    LIST_INIT(0x58AA18);
    LIST_INIT(0x57C748);
    LIST_INIT(0x57E1B8);
    for (p = (char *)0x57C758; p < (char *)0x57CD58; p += 0x18) {
        int *q = (int *)p;

        q[1] = 0;
        q[0] = 0;
        List_AddTail((List *)0x57C748, p);
        LIST_INIT(p + 0xC);
        q[2] = 0;
    }
    for (p = (char *)0x57CD58; p < (char *)0x57D758; p += 0x14) {
        int *q = (int *)p;

        q[1] = 0;
        q[0] = 0;
        List_AddTail((List *)0x57E1B8, p);
        q[3] = -1;
        q[2] = -1;
        q[4] = 0;
    }
    for (i = 0; i < 8; i++)
        ((int *)0x5BDA38)[i] = 0;
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
    CombatData *d = HumanAI_AllocParam(gAICurrent);
    int flags = Script_GetArgInt(args, 0);
    void *fn;

    d->unk28 = 1;
    d->events = gAIEventList;
    d->unk29 = 0;
    d->unk24 = 0;
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
    void *node = Graph_GetNode(gAICurrent->graph, gAICurrent->nodeId);
    AIEvent *e;
    NodeData *d;

    if (GraphNode_HasCriteria(node, AIGraph_GetStairCriteria()))
        return ret_int(out, 2);
    e = AIEvent_GetCurrent();
    if (e->target && *e->target) {
        Task *t = *e->target;
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

    if (*e->source) {
        EventData *d = HumanAI_AllocParam(gAICurrent);

        d->pos = e->pos;
        d->ref = TaskRef_Add(*e->source);
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

    if (e && e->target && *e->target) {
        d->pos = e->pos;
        d->unk24.target = NULL;
        d->unk20.source = NULL;
        if (e->source && *e->source)
            d->unk20.source = TaskRef_Add(*e->source);
        if (e->target && *e->target)
            d->unk24.target = TaskRef_Add(*e->target);
        HumanAI_PushAction(AI_SET_COMBAT, d, flags);
    }
    return ret_int(out, 1);
}
HOOK(0x44DAE0, AIAction_SetCombat);

ScriptValue *AIFunction_RemoveAlarmActions(ScriptValue *out, void *data, void *args)
{
    HumanAI *ai = gAICurrent;
    int tag = ai->unk83C;
    AIAction *a = (AIAction *)ai->actions.head;
    AIAction *next;

    for (next = (AIAction *)a->node.next; next; a = next, next = (AIAction *)next->node.next) {
        if (a->unk8 == tag) {
            HumanAI_EndAction(ai, a);
            ai = gAICurrent;
        }
    }
    return ret_int(out, 1);
}
HOOK(0x44DBC0, AIFunction_RemoveAlarmActions);

ScriptValue *AIFunction_SetViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->viewLength = Script_GetArgReal(args, 0) * kViewScale;
    if (gAICurrent->viewLength > gAICurrent->alarmViewLength)
        gAICurrent->alarmViewLength = gAICurrent->viewLength;
    return ret_int(out, 1);
}
HOOK(0x44DC40, AIFunction_SetViewLength);

ScriptValue *AIFunction_SetAlarmViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->alarmViewLength = Script_GetArgReal(args, 0) * kViewScale;
    return ret_int(out, 1);
}
HOOK(0x44DCD0, AIFunction_SetAlarmViewLength);

ScriptValue *AIFunction_SetViewAlpha(ScriptValue *out, void *data, void *args)
{
    gAICurrent->viewAlpha = Script_GetArgReal(args, 0) * kDegToRad;
    return ret_int(out, 1);
}
HOOK(0x44DD30, AIFunction_SetViewAlpha);

ScriptValue *AIFunction_SetViewGamma(ScriptValue *out, void *data, void *args)
{
    gAICurrent->viewGamma = Script_GetArgReal(args, 0) * kDegToRad;
    return ret_int(out, 1);
}
HOOK(0x44DD90, AIFunction_SetViewGamma);

ScriptValue *AIFunction_SetSecondaryViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->secondaryViewLength = Script_GetArgReal(args, 0) * kViewScale;
    if (gAICurrent->secondaryViewLength > gAICurrent->secondaryAlarmViewLength)
        gAICurrent->secondaryAlarmViewLength = gAICurrent->secondaryViewLength;
    return ret_int(out, 1);
}
HOOK(0x44DDF0, AIFunction_SetSecondaryViewLength);

ScriptValue *AIFunction_SetSecondaryAlarmViewLength(ScriptValue *out, void *data, void *args)
{
    gAICurrent->secondaryAlarmViewLength = Script_GetArgReal(args, 0) * kViewScale;
    return ret_int(out, 1);
}
HOOK(0x44DE80, AIFunction_SetSecondaryAlarmViewLength);

ScriptValue *AIFunction_SetSecondaryViewAlpha(ScriptValue *out, void *data, void *args)
{
    gAICurrent->secondaryViewAlpha = Script_GetArgReal(args, 0) * kDegToRad;
    return ret_int(out, 1);
}
HOOK(0x44DEE0, AIFunction_SetSecondaryViewAlpha);

ScriptValue *AIFunction_SetSecondaryViewGamma(ScriptValue *out, void *data, void *args)
{
    gAICurrent->secondaryViewGamma = Script_GetArgReal(args, 0) * kDegToRad;
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

    if (!gAICurrent->eventUsed[e]) {
        gAICurrent->eventUsed[e] = 1;
        gAICurrent->eventOrder[gAICurrent->eventCount] = e;
        gAICurrent->eventCount++;
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
    gAICurrent->deathAnimation = Script_GetArgInt(args, 0);
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
    gAICurrent->gunnerId = Script_GetArgInt(args, 0);
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
    return ret_int(out, gAICurrent->gunnerId);
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

    if (gAICurrent->gunnerId == -1)
        return ret_int(out, 0);
    t = Task_FindById(gAICurrent->gunnerId);
    if (!t) {
        Warning_Show("StationaryGun ID #%d not defined", gAICurrent->gunnerId);
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
        long double dx = e->pos.x - ai->eye.x;
        long double dy = e->pos.y - ai->eye.y;
        long double dz = e->pos.z - ai->eye.z;

        if (!((ai->dirZ * dz + ai->dirX * dx) + ai->dirY * dy >= 0.0))
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
    out->i = round_int(Vec3d_Distance(&e->pos, &gAICurrent->eye));
    out->d = Vec3d_Distance(&e->pos, &gAICurrent->eye);
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
    out->i = round_int(Vec3d_Distance(&gAICurrent->pos, (Vec3d *)(t + 1)));
    out->d = Vec3d_Distance(&gAICurrent->pos, (Vec3d *)(t + 1));
    out->s = gEmptyString;
    return out;
}
HOOK(0x44E9B0, AIFunction_GetAlarmTriggerDistance);

/* seconds; animTimer gets min + random(range) frames */
ScriptValue *AIFunction_SetAnimationInterval(ScriptValue *out, void *data, void *args)
{
    gAICurrent->animMin = round_int(Script_GetArgReal(args, 0) * kFps);
    gAICurrent->animRange = round_int(Script_GetArgReal(args, 1) * kFps);
    gAICurrent->animTimer = Random_Int(Game_GetRandom(), gAICurrent->animRange) + gAICurrent->animMin;
    return ret_int(out, 2);
}
HOOK(0x44EAF0, AIFunction_SetAnimationInterval);

ScriptValue *AIFunction_AddAnimationEntry(ScriptValue *out, void *data, void *args)
{
    if (gAICurrent->animCount >= 5) {
        Warning_Show("Maximum number of animation entries (#%d) reached", 5);
        Script_GetArgInt(args, 0);
        Script_GetArgReal(args, 1);
        return ret_int(out, 2);
    }
    gAICurrent->anims[gAICurrent->animCount + 1].anim = Script_GetArgInt(args, 0);
    gAICurrent->anims[gAICurrent->animCount + 1].weight = Script_GetArgReal(args, 1);
    gAICurrent->animCount++;
    return ret_int(out, 2);
}
HOOK(0x44EBB0, AIFunction_AddAnimationEntry);

/* picks an entry by weight, or any entry when the weights add up to
   almost nothing */
ScriptValue *AIFunction_GetAnimationToPlay(ScriptValue *out, void *data, void *args)
{
    int n = gAICurrent->animCount;
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
    for (i = 0; i < gAICurrent->animCount; i++) {
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
    HumanAI_SendSimple(*e->source, 6, 0x24);
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
