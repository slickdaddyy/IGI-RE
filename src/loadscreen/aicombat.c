#pragma GCC optimize("excess-precision=standard")

/* The combat actions of the enemy AI: per AI kind a start callback picks a
   pose state, each pose state runs every frame, decides what to do next and
   installs one of the combat moves (0x48D7D0..) as the action's step. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "types.h"
#include "task/tasktypes.h"
#include "sysmisc/sysmisctypes.h"
#include "humanai/humanaitypes.h"
#include "statgun/statguntypes.h"
#include "graphnode/graphnodetypes.h"
#include "weapon/weapontypes.h"
#include "patrol/patroltypes.h"

FN_EXTERN(AICombat_End);
FN_EXTERN(AICombat_KneelFire);
FN_EXTERN(AICombat_KneelReload);
FN_EXTERN(AICombat_KneelRollLeft);
FN_EXTERN(AICombat_KneelRollRight);
FN_EXTERN(AICombat_LieDownFire);
FN_EXTERN(AICombat_LieDownReload);
FN_EXTERN(AICombat_StandCloseCombat);
FN_EXTERN(AICombat_StandFire);
FN_EXTERN(AICombat_StandReload);
FN_EXTERN(AICombat_StandRollLeft);
FN_EXTERN(AICombat_StandRollRight);
FN_EXTERN(AICombat_StandThrowGrenade);

typedef struct Human {
    Task task;
    char unk20[0x4C4 - 0x20];
    Task *weapons[4];
    float aimError;
    char unk4D8[0x7B4 - 0x4D8];
    Task **statGun;
} Human;

typedef struct HumanAI HumanAI;
typedef struct AIAction AIAction;
typedef void (*AIActionFn)(HumanAI *ai, AIAction *a);
typedef char (*AIStepFn)(HumanAI *ai, AIAction *a);

struct HumanAI {
    Task task;
    Vec3d pos;
    Vec3d eye;
    float body[10];
    float view[10];
    char unkA0[0x10];
    double viewLength;
    char unkB8[0x87C - 0xB8];
    int eventCounts[24];
    char unk8DC[0x3AA8 - 0x8DC];
    AITarget *target;
    char unk3AAC[0x3AC8 - 0x3AAC];
    int statGunId;
    char unk3ACC[0x3B68 - 0x3ACC];
    int graphId;
    Graph *graph;
    char unk3B70[0x3C74 - 0x3B70];
    float ctrl[0x28];
    char unk3D14[0x3D44 - 0x3D14];
    int unk3D44;
    int unk3D48;
    int maxBurst;
    int maxTurns;
    float evadeChance;
    float lieChance;
    float unk3D5C;
    float rollChance;
    char unk3D64[4];
    float errorNear;
    float errorFar;
    float burstMin;
    float burstRandom;
    float guardMin;
    float guardRandom;
    char unk3D80[8];
    float grenadeChance;
    float grenadeChance2;
    float grenadeMin;
    float grenadeMax;
    char unk3D98[0x3DB0 - 0x3D98];
    double closeRange;
    double lieRange;
    double coverRange;
    char unk3DC8[8];
    int unk3DD0;
    int unk3DD4;
    int unk3DD8;
    int unk3DDC;
    char unk3DE0[4];
    AISeen *seen;
    int destNode;
    int targetNode;
    char targetMoved;
    char unk3DF1[3];
    int rollDir;
    int nextEvade;
    char unk3DFC;
    char hasGrenade;
    char unk3DFE[2];
    int gunId;
    char unk3E04[0x3E2C - 0x3E04];
    int node;
    char unk3E30[0x3E50 - 0x3E30];
    int unk3E50;
    int shotsLeft;
    int lastAmmo;
    int unk3E5C;
    int shotDelay;
    int unk3E64;
    char unk3E68;
    char unk3E69;
    char unk3E6A;
    char unk3E6B[5];
    Vec3d aim;
    Vec3d aimTarget;
    NodeGrid *grid;
    int gridX;
    int gridY;
    char unk3EAC[4];
    Vec3d cover;
};

_Static_assert(offsetof(Human, weapons) == 0x4C4, "weapons");
_Static_assert(offsetof(Human, aimError) == 0x4D4, "aimError");
_Static_assert(offsetof(Human, statGun) == 0x7B4, "statGun");
_Static_assert(offsetof(HumanAI, viewLength) == 0xB0, "viewLength");
_Static_assert(offsetof(HumanAI, eventCounts) == 0x87C, "eventCounts");
_Static_assert(offsetof(HumanAI, target) == 0x3AA8, "target");
_Static_assert(offsetof(HumanAI, statGunId) == 0x3AC8, "statGunId");
_Static_assert(offsetof(HumanAI, graphId) == 0x3B68, "graphId");
_Static_assert(offsetof(HumanAI, ctrl) == 0x3C74, "ctrl");
_Static_assert(offsetof(HumanAI, unk3D44) == 0x3D44, "unk3D44");
_Static_assert(offsetof(HumanAI, errorNear) == 0x3D68, "errorNear");
_Static_assert(offsetof(HumanAI, grenadeChance) == 0x3D88, "grenadeChance");
_Static_assert(offsetof(HumanAI, closeRange) == 0x3DB0, "closeRange");
_Static_assert(offsetof(HumanAI, unk3DD0) == 0x3DD0, "unk3DD0");
_Static_assert(offsetof(HumanAI, seen) == 0x3DE4, "seen");
_Static_assert(offsetof(HumanAI, rollDir) == 0x3DF4, "rollDir");
_Static_assert(offsetof(HumanAI, gunId) == 0x3E00, "gunId");
_Static_assert(offsetof(HumanAI, node) == 0x3E2C, "node");
_Static_assert(offsetof(HumanAI, unk3E50) == 0x3E50, "unk3E50");
_Static_assert(offsetof(HumanAI, aim) == 0x3E70, "aim");
_Static_assert(offsetof(HumanAI, grid) == 0x3EA0, "grid");
_Static_assert(offsetof(HumanAI, cover) == 0x3EB0, "cover");

/* event counts the combat code looks at (AIEVENT_*) */
#define EV_ALERT 5
#define EV_WALK 10
#define EV_GROUNDIMPACT 11
#define EV_DOOR 12
#define EV_FENCE 13
#define EV_LADDER 14
#define EV_TAKINGDAMAGE 15
#define EV_GUNSHOT 16
#define EV_GRENADELAND 18
#define EV_GUNSHOTMISS 20

/* HumanAI_InPose */
#define POSE_STAND 0
#define POSE_KNEEL 1
#define POSE_LIEDOWN 2
#define POSE_ANIM 3
#define POSE_GUN 4

/* AITarget states */
#define TARGET_NONE -1
#define TARGET_FIRING 0
#define TARGET_RELOADING 1
#define TARGET_WAITING 2

#define CTRL_ROLL_LEFT 2
#define CTRL_ROLL_RIGHT 3
#define CTRL_TRIGGER 6
#define CTRL_COMBAT 26

/* the combat moves and the end callback (src/aitask) */
#define AICombat_End ((AIActionFn)FN(0x48D7D0, AICombat_End))
#define AICombat_StandRollLeft ((AIStepFn)FN(0x48D830, AICombat_StandRollLeft))
#define AICombat_StandRollRight ((AIStepFn)FN(0x48D870, AICombat_StandRollRight))
#define AICombat_StandFire ((AIStepFn)FN(0x48D8B0, AICombat_StandFire))
#define AICombat_StandReload ((AIStepFn)FN(0x48DB10, AICombat_StandReload))
#define AICombat_StandCloseCombat ((AIStepFn)FN(0x48DBB0, AICombat_StandCloseCombat))
#define AICombat_StandThrowGrenade ((AIStepFn)FN(0x48DBF0, AICombat_StandThrowGrenade))
#define AICombat_KneelRollLeft ((AIStepFn)FN(0x48DC30, AICombat_KneelRollLeft))
#define AICombat_KneelRollRight ((AIStepFn)FN(0x48DC80, AICombat_KneelRollRight))
#define AICombat_KneelFire ((AIStepFn)FN(0x48DCD0, AICombat_KneelFire))
#define AICombat_KneelReload ((AIStepFn)FN(0x48DD10, AICombat_KneelReload))
#define AICombat_LieDownFire ((AIStepFn)FN(0x48DDC0, AICombat_LieDownFire))
#define AICombat_LieDownReload ((AIStepFn)FN(0x48DE00, AICombat_LieDownReload))

extern const char *gDetectedSounds[];
extern int gLastDetectedTick;
extern int gCoverFromNode;
extern int gCoverTargetNode;
extern int gCoverCellOrder;

static const float kZero = 0.0f;
static const double kZeroD = 0.0;
static const double kNodeTol = 0.3;
static const float kCell = 8192.0f;
static const float kCellMin = -4096.0f;
static const float kCellMax = 4096.0f;
static const double kEyeScale = 4096.0;
static const float kVisible = 0.79f;
static const double kMinRange = 0.01;
static const float kHalf = 0.5f;
static const float kTurnTol = 0x1.1df46ap-5f;
static const double kFacing = 0.96;
static const float kRpgKneelChance = 0.4f;
static const float kDegToRad = 0x1.1df46ap-6f;

ORIG(0x4C1780, void, Debug_Log, (void *task, int level, const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x451350, AISeen *, HumanAI_CanSeeTarget, (HumanAI *ai));
ORIG(0x463170, char, Human_IsDead, (Task *human));
ORIG(0x463180, char, Human_IsCrouching, (Task *human));
ORIG(0x463130, int, Human_GetTeam, (Task *human));
ORIG(0x451380, void, HumanAI_GetTargetPos, (Vec3d *out, HumanAI *ai));
ORIG(0x413A70, uint16_t, HumanPlayer_GetType, (void));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x4B47C0, int, Random_Int, (void *r, int n));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x453200, GraphNode *, HumanMonitor_FindTargetNode, (HumanAI *ai));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4148F0, long double, Vec3d_PitchAngle, (Vec3d v));
ORIG(0x414950, long double, Vec3d_YawAngle, (Vec3d v));
ORIG(0x450CE0, void, HumanAI_Broadcast, (AIMessage *m));
ORIG(0x4513F0, void, HumanAI_LevelAim, (HumanAI *ai));
ORIG(0x48EFE0, char, HumanAI_InPose, (HumanAI *ai, int pose));
ORIG(0x48EF70, void, HumanAI_SetChannel, (HumanAI *ai, int channel, float value));
ORIG(0x451080, AITarget *, AITarget_Add, (HumanAI *ai, Task *t));
ORIG(0x4511F0, void, AITarget_SetState, (AITarget *t, HumanAI *ai, int state));
ORIG(0x451230, int, AITarget_GetState, (AITarget *t, HumanAI *ai));
ORIG(0x451270, int, AITarget_CountState, (AITarget *t, int state));
ORIG(0x450FF0, void, HumanAI_EndAction, (HumanAI *ai, AIAction *a));
ORIG(0x4F9720, GraphNode *, Graph_GetNode, (Graph *g, int id));
ORIG(0x4F9770, void, Graph_GetNodePos, (Vec3d *out, GraphNode *node));
ORIG(0x4F98A0, void, Graph_GetNodeMatrix, (float *out, GraphNode *node, int unk));
ORIG(0x4F9A40, char, Graph_IsInNode, (const Vec3d *pos, GraphNode *node, double tol, int unk));
ORIG(0x5096C0, GraphStep *, AIGraph_GetStep, (Graph *g, int from, int to));
ORIG(0x509880, unsigned, AIGraph_GetStairCriteria, (void));
ORIG(0x509890, char, GraphNode_HasCriteria, (GraphNode *node, unsigned mask));
ORIG(0x452BE0, NodeGrid *, NodeGrid_Find, (int graphId, int nodeId));
ORIG(0x452C10, void, NodeGrid_Release, (NodeGrid *g, int x, int y));
ORIG(0x452C60, NodeGrid *, NodeGrid_Acquire, (Graph *g, int nodeId, int x, int y));
ORIG(0x453040, void, NodeGrid_GetSize, (int *w, int *h, Graph *g, int nodeId));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4CE410, char, Coll_LineOfSight, (void *world, const Vec3d *from, const Vec3d *to, int event, void *filter,
                                        int flags, int trees, float *visibility));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4F23D0, void, Weapon_GetAmmo, (Task *weapon, int *out));
ORIG(0x413C80, WeaponType *, Weapon_GetType, (Task *weapon));
ORIG(0x475310, void, StationaryGun_GetMuzzle, (void *gun, Vec3d *eye, float *matrix));
ORIG(0x4526E0, uint8_t, HumanAI_CheckSides, (HumanAI *ai));
ORIG(0x4522A0, char, HumanAI_RunTo, (HumanAI *ai, int node, const Vec3d *pos, char replan));
ORIG(0x4525B0, char, HumanAI_FaceTo, (HumanAI *ai, const Vec3d *pos, float tol, char flat));
ORIG(0x452660, void, HumanAI_SetRun, (HumanAI *ai));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1030, StatGun *, Task_FindById, (int id));

static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

static CombatData *combat_data(AIAction *a)
{
    return &((AIParam *)a->param)->d;
}

void AICombat_AlertOthers(HumanAI *ai);

/* refreshes ai->seen, the target position and node and cd->dist; seen
   stays NULL when the target is gone or dead */
void AICombat_Update(HumanAI *ai, CombatData *cd)
{
    Task *human = ai->task.parent;
    AISeen *seen;
    GraphNode *node;

    ai->seen = NULL;
    if (!ai->target)
        return;
    seen = HumanAI_CanSeeTarget(ai);
    if (!seen || !*seen->ref || Human_IsDead(*seen->ref))
        return;
    if (seen->visible) {
        HumanAI_GetTargetPos(&ai->target->pos, ai);
        if (!cd->spoke && (*seen->ref)->type == HumanPlayer_GetType()) {
            if (gLastDetectedTick == -1 || Flow_GetFrames() - gLastDetectedTick > 90) {
                Sound_Play(human, gDetectedSounds[Random_Int(Game_GetRandom(), 6)]);
                gLastDetectedTick = Flow_GetFrames();
            }
            cd->spoke = 1;
        }
    } else if (ai->eventCounts[EV_ALERT] || ai->eventCounts[EV_WALK] || ai->eventCounts[EV_GROUNDIMPACT] ||
               ai->eventCounts[EV_DOOR] || ai->eventCounts[EV_FENCE] || ai->eventCounts[EV_LADDER] ||
               ai->eventCounts[EV_GUNSHOT] || ai->eventCounts[EV_GRENADELAND] ||
               ai->eventCounts[EV_GUNSHOTMISS] || ai->eventCounts[EV_TAKINGDAMAGE]) {
        HumanAI_GetTargetPos(&ai->target->pos, ai);
    }
    node = HumanMonitor_FindTargetNode(ai);
    if (node) {
        if (ai->targetNode == -1) {
            ai->targetNode = node->id;
        } else if (ai->targetNode != node->id) {
            ai->targetNode = node->id;
            ai->targetMoved = 1;
        }
    } else {
        ai->targetNode = -1;
    }
    ai->seen = seen;
    cd->dist = Vec3d_Distance(DynCubeObj_GetPosition(human), &ai->target->pos);
    if (cd->frame == 0)
        AICombat_AlertOthers(ai);
    cd->frame = (cd->frame + 1) % 20;
}
HOOK(0x48A920, AICombat_Update);

void AICombat_AlertOthers(HumanAI *ai)
{
    Task *human = ai->task.parent;
    AIMessage m;

    m.type = 5;
    m.flags = 0x26;
    m.radius = 28672.0f;
    m.team = Human_GetTeam(human);
    m.pos = DynCubeObj_GetPosition(human);
    m.from = human;
    m.about = *ai->seen->ref;
    HumanAI_Broadcast(&m);
}
HOOK(0x48AB10, AICombat_AlertOthers);

void AIDead_Update(HumanAI *ai, AIAction *a);

void AIDead_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIDead_Update;
}
HOOK(0x48AB70, AIDead_Start);

void AIDead_Update(HumanAI *ai, AIAction *a)
{
    Debug_Log(ai, -1, "Dead");
    HumanAI_LevelAim(ai);
}
HOOK(0x48AB80, AIDead_Update);

/* the first event that is about a task becomes the AI's target */
static void take_target(HumanAI *ai, CombatData *cd)
{
    Node *n;

    for (n = cd->events->head; n->next; n = n->next) {
        AIEvent *e = (AIEvent *)n;

        if (e->about && *e->about) {
            ai->target = AITarget_Add(ai, *e->about);
            break;
        }
    }
}

void AICombat_EnterKneel(HumanAI *ai, AIAction *a);
void AICombat_EnterLiedown(HumanAI *ai, AIAction *a);
void AICombat_EnterStand(HumanAI *ai, AIAction *a);

void AICombatPatrol_Start(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    if (Human_IsCrouching(ai->task.parent))
        a->update = AICombat_EnterKneel;
    else if (HumanAI_InPose(ai, POSE_LIEDOWN))
        a->update = AICombat_EnterLiedown;
    else
        a->update = AICombat_EnterStand;
    a->end = AICombat_End;
    ai->unk3DD0 = -1;
    ai->unk3DD4 = ai->unk3D44;
    ai->unk3DD8 = -1;
    ai->unk3E64 = 0;
    ai->destNode = -1;
    take_target(ai, cd);
    cd->count = 0;
    cd->timer = 0;
    cd->events = NULL;
}
HOOK(0x48ABA0, AICombatPatrol_Start);

void AICombatAnya_Enter(HumanAI *ai, AIAction *a);

void AICombatAnya_Start(HumanAI *ai, AIAction *a)
{
    AICombatPatrol_Start(ai, a);
    a->update = AICombatAnya_Enter;
    ai->closeRange = 0;
}
HOOK(0x48AC60, AICombatAnya_Start);

void AICombatAnya_Stand(HumanAI *ai, AIAction *a);

void AICombatAnya_Enter(HumanAI *ai, AIAction *a)
{
    a->update = AICombatAnya_Stand;
    a->resume = AICombatAnya_Enter;
    ai->unk3E50 = 0;
}
HOOK(0x48AC90, AICombatAnya_Enter);

void AIAction_SetStep(HumanAI *ai, AIAction *a, AIActionFn enter, AIStepFn step);
int AICombat_DecideFemale(HumanAI *ai, CombatData *cd);
void AICombat_StartBurst(HumanAI *ai, char first);
void AICombat_EnterReposition(HumanAI *ai, AIAction *a);
void AICombat_EnterFollow(HumanAI *ai, AIAction *a);
void AICombat_EnterLookForTarget(HumanAI *ai, AIAction *a);

/* runs the action's current combat move; 1 when there was one */
static int run_step(HumanAI *ai, AIAction *a)
{
    if (!a->step)
        return 0;
    if (a->step(ai, a))
        a->step = NULL;
    return 1;
}

static void start_fire(HumanAI *ai, AIAction *a, CombatData *cd, AIStepFn fire)
{
    HumanAI_GetTargetPos(&ai->aimTarget, ai);
    AICombat_StartBurst(ai, cd->first);
    cd->first = 0;
    AIAction_SetStep(ai, a, NULL, fire);
}

void AICombatAnya_Stand(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    Debug_Log(ai, -1, "Female Combat - Stand");
    AICombat_Update(ai, cd);
    if (!ai->seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (run_step(ai, a))
        return;
    if (!HumanAI_InPose(ai, POSE_STAND)) {
        if (HumanAI_InPose(ai, POSE_ANIM))
            HumanAI_SetChannel(ai, 3, -1.0f);
        Debug_Log(ai, -1, "Waiting for stand state");
        return;
    }
    switch (AICombat_DecideFemale(ai, cd)) {
    case 0: AIAction_SetStep(ai, a, NULL, AICombat_StandReload); return;
    case 1: AIAction_SetStep(ai, a, NULL, AICombat_StandThrowGrenade); return;
    case 2: start_fire(ai, a, cd, AICombat_StandFire); return;
    case 3: AIAction_SetStep(ai, a, NULL, AICombat_StandCloseCombat); return;
    case 4: AIAction_SetStep(ai, a, AICombat_EnterReposition, NULL); return;
    case 5: AIAction_SetStep(ai, a, AICombat_EnterFollow, NULL); return;
    case 6: AIAction_SetStep(ai, a, AICombat_EnterLookForTarget, NULL); return;
    case 7: HumanAI_EndAction(ai, a); return;
    case -1: Debug_Log(ai, -1, "Waiting for patrol command"); return;
    }
}
HOOK(0x48ACC0, AICombatAnya_Stand);

void AIAction_SetStep(HumanAI *ai, AIAction *a, AIActionFn enter, AIStepFn step)
{
    a->step = step;
    if (enter)
        enter(ai, a);
}
HOOK(0x48AE70, AIAction_SetStep);

void AICombat_FindCover(GraphNode **out, HumanAI *ai, char visible, int max);
int HumanAI_GetGunAmmo(HumanAI *ai);

/* the AI is away from its node: 4 */
static int off_node(HumanAI *ai)
{
    GraphNode *node;
    Vec3d *pos;

    if (ai->destNode != -1)
        return 0;
    node = Graph_GetNode(ai->graph, ai->node);
    pos = DynCubeObj_GetPosition(ai->task.parent);
    return !Graph_IsInNode(pos, node, kNodeTol, 0);
}

/* target not visible: look for a node to move to now and then, give up after maxTurns */
static int decide_hidden(HumanAI *ai, CombatData *cd, int count)
{
    GraphNode *node;

    cd->count = count + 1;
    if (cd->count % 30 > 15) {
        node = NULL;
        AICombat_FindCover(&node, ai, 0, 5);
        if (node && node->id != ai->node) {
            ai->targetMoved = 0;
            ai->destNode = node->id;
            cd->count = 0;
            return 5;
        }
    }
    return (cd->count > ai->maxTurns) + 6;
}

/* moves somewhere better when the target is far away: 5 */
static int decide_cover(HumanAI *ai, CombatData *cd)
{
    GraphNode *node;

    if (cd->dist > ai->coverRange) {
        node = NULL;
        AICombat_FindCover(&node, ai, 1, 5);
        if (node && node->id != ai->node) {
            ai->destNode = node->id;
            return 1;
        }
    }
    return 0;
}

static int decide_fire(HumanAI *ai)
{
    if (AITarget_CountState(ai->target, TARGET_FIRING) < 3) {
        AITarget_SetState(ai->target, ai, TARGET_FIRING);
        return 2;
    }
    AITarget_SetState(ai->target, ai, TARGET_WAITING);
    return 6;
}

/* 0 reload, 2 fire, 4 reposition, 5 follow, 6 look for target, 7 give up */
int AICombat_DecideFemale(HumanAI *ai, CombatData *cd)
{
    AISeen *seen = ai->seen;
    int count = cd->count;

    cd->count = 0;
    AITarget_SetState(ai->target, ai, TARGET_NONE);
    if (off_node(ai))
        return 4;
    if (!seen->visible)
        return decide_hidden(ai, cd, count);
    if (!HumanAI_GetGunAmmo(ai)) {
        AITarget_SetState(ai->target, ai, TARGET_RELOADING);
        return 0;
    }
    if (decide_cover(ai, cd))
        return 5;
    return decide_fire(ai);
}
HOOK(0x48AE90, AICombat_DecideFemale);

char AICombat_TryNode(HumanAI *ai, GraphNode *node, char visible, char check);

/* breadth first search from the node nearest the target for a node where
   the AI can stand; at most `max` nodes are tried after the first */
void AICombat_FindCover(GraphNode **out, HumanAI *ai, char visible, int max)
{
    char seen[0x400];
    int queue[0x400];
    Vec3d target;
    int count = 0, len, *cur = queue, *wp, i, id;
    GraphNode *start, *node;

    *out = NULL;
    if (ai->graph->nodeCount >= 0x400) {
        Warning_Show("FindFreePosition stack not large enough");
        return;
    }
    start = HumanMonitor_FindTargetNode(ai);
    if (!start) {
        Warning_Show("No nodes near target");
        return;
    }
    HumanAI_GetTargetPos(&target, ai);
    gCoverTargetNode = start->id;
    gCoverFromNode = ai->node;
    if (AICombat_TryNode(ai, start, visible, 1)) {
        *out = start;
        return;
    }
    id = start->id;
    memset(seen, 0, sizeof(seen));
    queue[0] = id;
    len = 1;
    queue[1] = -1;
    if (id == -1)
        return;
    seen[id] = 1;
    do {
        GraphLink **links;

        if (count >= max)
            return;
        links = Graph_GetNode(ai->graph, *cur)->links;
        wp = &queue[len];
        for (i = 0; i < 64 && count < max; i++) {
            GraphLink *l = links[i];
            int other;

            if (!l)
                continue;
            other = l->a;
            if (*cur == other)
                other = l->b;
            if (seen[other])
                continue;
            node = Graph_GetNode(ai->graph, other);
            if (AICombat_TryNode(ai, node, visible, 1)) {
                *out = node;
                return;
            }
            *wp++ = other;
            len++;
            count++;
            seen[other] = 1;
            *wp = -1;
        }
    } while (*++cur != -1);
}
HOOK(0x48B050, AICombat_FindCover);

char AICombat_CanSeeFrom(GraphNode *node, HumanAI *ai);
char AICombat_TryCell(HumanAI *ai, NodeGrid *grid, GraphNode *node, int x, int y);

/* cover position: cell (ix, iy) from the node centre, clamped to the node
   and turned into the node's frame */
static void set_cover(HumanAI *ai, GraphNode *node, int ix, int iy)
{
    float m[10];
    Vec3d pos, r;
    long double x, y, lim;
    float limf;

    Graph_GetNodeMatrix(m, node, 1);
    Graph_GetNodePos(&pos, node);
    ai->cover.z = 0;
    x = (long double)ix * kCell;
    ai->cover.x = x;
    y = (long double)iy * kCell;
    ai->cover.y = y;
    lim = node->radius * kCellMin;
    limf = lim;
    if (x < lim) {
        ai->cover.x = limf;
    } else {
        lim = node->radius * kCellMax;
        limf = lim;
        if (x > lim)
            ai->cover.x = limf;
    }
    lim = node->radius * kCellMin;
    if (lim > ai->cover.y) {
        ai->cover.y = lim;
    } else {
        lim = node->radius * kCellMax;
        if (lim < ai->cover.y)
            ai->cover.y = lim;
    }
    r.x = m[1] * ai->cover.y + m[0] * ai->cover.x;
    r.y = m[4] * ai->cover.y + m[3] * ai->cover.x;
    r.z = m[7] * ai->cover.y + m[6] * ai->cover.x;
    ai->cover = r;
    ai->cover.x = pos.x + ai->cover.x;
    ai->cover.y = pos.y + ai->cover.y;
    ai->cover.z = pos.z + ai->cover.z;
}

static void take_cell(HumanAI *ai, GraphNode *node, int x, int y)
{
    if (ai->grid) {
        NodeGrid_Release(ai->grid, ai->gridX, ai->gridY);
        ai->grid = NULL;
    }
    ai->grid = NodeGrid_Acquire(ai->graph, node->id, x, y);
    ai->gridX = x;
    ai->gridY = y;
}

/* 1 when the AI can take a place at node (and then takes it). With `visible`
   the node must lie on the way between the AI and the target. */
char AICombat_TryNode(HumanAI *ai, GraphNode *node, char visible, char check)
{
    NodeGrid *grid;
    int x, y;

    if (visible) {
        GraphStep *toTarget = AIGraph_GetStep(node->graph, node->id, gCoverTargetNode);
        GraphStep *toFrom = AIGraph_GetStep(node->graph, node->id, gCoverFromNode);
        GraphStep *between = AIGraph_GetStep(node->graph, gCoverFromNode, gCoverTargetNode);

        if (!toTarget || !between || !toFrom)
            return 0;
        if (toFrom->dist + toTarget->dist > between->dist)
            return 0;
    }
    grid = NodeGrid_Find(ai->graphId, node->id);
    if (!grid) {
        int w, h;

        if (check && !AICombat_CanSeeFrom(node, ai))
            return 0;
        NodeGrid_GetSize(&w, &h, ai->graph, node->id);
        set_cover(ai, node, -(w / 2), -(h / 2));
        take_cell(ai, node, 0, 0);
        return 1;
    }
    if (grid == ai->grid)
        return 1;
    if (grid->refs >= grid->h * grid->w)
        return 0;
    if (check && !AICombat_CanSeeFrom(node, ai))
        return 0;
    switch (++gCoverCellOrder) {
    case 1:
        if (grid->h <= 0)
            return 0;
        for (y = 0; y < grid->h; y++)
            for (x = grid->w - 1; x >= 0; x--)
                if (AICombat_TryCell(ai, grid, node, x, y))
                    return 1;
        return 0;
    case 2:
        if (grid->w <= 0)
            return 0;
        for (x = 0; x < grid->w; x++)
            for (y = grid->h - 1; y >= 0; y--)
                if (AICombat_TryCell(ai, grid, node, x, y))
                    return 1;
        return 0;
    case 3:
        if (grid->h - 1 < 0)
            return 0;
        for (y = grid->h - 1; y >= 0; y--)
            for (x = grid->w - 1; x >= 0; x--)
                if (AICombat_TryCell(ai, grid, node, x, y))
                    return 1;
        return 0;
    default:
        gCoverCellOrder = 0;
        /* fall through */
    case 0:
        for (x = 0; x < grid->w; x++)
            for (y = 0; y < grid->h; y++)
                if (AICombat_TryCell(ai, grid, node, x, y))
                    return 1;
        return 0;
    }
}
HOOK(0x48B260, AICombat_TryNode);

/* the line-of-sight filter: only the first two fields are set, the rest
   holds the node position's x like in the original */
typedef struct NodeSightFilter {
    char flag;
    char pad[3];
    Task *self;
    double unk8;
} NodeSightFilter;

/* 1 when the target can be seen from eye height above node */
char AICombat_CanSeeFrom(GraphNode *node, HumanAI *ai)
{
    Vec3d from, to;
    NodeSightFilter filter;
    float vis;
    uint8_t ev;
    void *world;

    Graph_GetNodePos(&from, node);
    from.z = ((AIGraph *)ai->graph)->eyeHeight * kEyeScale + from.z;
    HumanAI_GetTargetPos(&to, ai);
    filter.self = ai->task.parent;
    filter.flag = 1;
    filter.unk8 = from.x;
    ev = Game_GetEvent8();
    world = Game_GetWorld();
    if (Coll_LineOfSight(world, &from, &to, ev, &filter, -1, -1, &vis))
        return 1;
    if (vis >= kVisible)
        return 1;
    return 0;
}
HOOK(0x48B6E0, AICombat_CanSeeFrom);

char AICombat_TryCell(HumanAI *ai, NodeGrid *grid, GraphNode *node, int x, int y)
{
    if (grid->cells[x][y] >= 25)
        return 0;
    set_cover(ai, node, x - grid->w / 2, y - grid->h / 2);
    take_cell(ai, node, x, y);
    return 1;
}
HOOK(0x48B780, AICombat_TryCell);

int HumanAI_GetGunAmmo(HumanAI *ai)
{
    Human *h = (Human *)ai->task.parent;
    Task *w = WeaponSlots_Get(h->weapons, 0);
    int ammo[2];

    if (w) {
        Weapon_GetAmmo(w, ammo);
        return ammo[0];
    }
    return 0;
}
HOOK(0x48B980, HumanAI_GetGunAmmo);

/* shots for the next burst and the aim error, which grows with the distance */
void AICombat_StartBurst(HumanAI *ai, char half)
{
    Human *h = (Human *)ai->task.parent;
    float r = Random_Float(Game_GetRandom()) * ai->burstRandom + ai->burstMin;
    Task *w;
    WeaponType *type;
    int ammo[2];

    if (HumanAI_InPose(ai, POSE_GUN)) {
        StatGun *gun = (StatGun *)*h->statGun;

        StationaryGun_GetMuzzle(gun, &ai->eye, ai->view);
        w = WeaponSlots_Get(gun->slots, 0);
    } else {
        w = WeaponSlots_Get(h->weapons, 0);
    }
    type = Weapon_GetType(w);
    Weapon_GetAmmo(w, ammo);
    if (type->category >= 0) {
        if (type->category <= 2) {
            long double d;

            if (type->burst != type->clipSize && type->burst > 1) {
                ai->shotsLeft = type->burst;
            } else {
                ai->shotsLeft = round_int((long double)type->clipSize * r);
                if (ai->shotsLeft < 1)
                    ai->shotsLeft = 1;
                else if (ai->shotsLeft > ai->maxBurst)
                    ai->shotsLeft = ai->maxBurst;
            }
            if (ai->shotsLeft == 0)
                ai->shotsLeft = 1;
            d = Vec3d_Distance(&ai->pos, &ai->aimTarget);
            if (d < kZeroD) {
                h->aimError = ai->errorNear;
            } else {
                if (d > ai->viewLength)
                    d = ai->viewLength;
                if (d < kMinRange)
                    h->aimError = ai->errorNear;
                else
                    h->aimError = d / ai->viewLength * (ai->errorFar - ai->errorNear) + ai->errorNear;
            }
            if (half)
                h->aimError = h->aimError * kHalf;
        } else if (type->category == 4) {
            ai->shotsLeft = 1;
            h->aimError = 1.0f;
        }
    }
    ai->lastAmmo = ammo[0];
    ai->unk3E5C = Flow_GetFrames();
    ai->shotDelay = 0;
    ai->unk3E6A = 0;
    ai->unk3E69 = 1;
    ai->aim = ai->aimTarget;
}
HOOK(0x48B9C0, AICombat_StartBurst);

void AICombat_Stand(HumanAI *ai, AIAction *a);

void AICombat_EnterStand(HumanAI *ai, AIAction *a)
{
    a->update = AICombat_Stand;
    a->resume = AICombat_EnterStand;
    ai->unk3E50 = 0;
}
HOOK(0x48BBD0, AICombat_EnterStand);

int AICombat_ChooseEvade(HumanAI *ai, CombatData *cd);
int AICombat_Decide(HumanAI *ai, CombatData *cd);

void AICombat_Stand(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    Debug_Log(ai, -1, "Combat - Stand");
    AICombat_Update(ai, cd);
    if (!ai->seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    switch (AICombat_ChooseEvade(ai, cd)) {
    case 0:
        AIAction_SetStep(ai, a, AICombat_EnterLiedown, NULL);
        return;
    case 1:
        AIAction_SetStep(ai, a, NULL, AICombat_StandRollLeft);
        ai->ctrl[CTRL_ROLL_LEFT] = 1.0f;
        ai->rollDir = 1;
        return;
    case 2:
        AIAction_SetStep(ai, a, NULL, AICombat_StandRollRight);
        ai->ctrl[CTRL_ROLL_RIGHT] = 1.0f;
        ai->rollDir = -1;
        return;
    }
    if (run_step(ai, a))
        return;
    if (!HumanAI_InPose(ai, POSE_STAND)) {
        if (HumanAI_InPose(ai, POSE_ANIM))
            HumanAI_SetChannel(ai, 3, -1.0f);
        Debug_Log(ai, -1, "Waiting for stand state");
        return;
    }
    switch (AICombat_Decide(ai, cd)) {
    case 0: AIAction_SetStep(ai, a, NULL, AICombat_StandReload); return;
    case 1: AIAction_SetStep(ai, a, NULL, AICombat_StandThrowGrenade); return;
    case 2: start_fire(ai, a, cd, AICombat_StandFire); return;
    case 3: AIAction_SetStep(ai, a, NULL, AICombat_StandCloseCombat); return;
    case 4: AIAction_SetStep(ai, a, AICombat_EnterReposition, NULL); return;
    case 5: AIAction_SetStep(ai, a, AICombat_EnterFollow, NULL); return;
    case 6: AIAction_SetStep(ai, a, AICombat_EnterLookForTarget, NULL); return;
    case 7: HumanAI_EndAction(ai, a); return;
    case -1: Debug_Log(ai, -1, "Waiting for patrol command"); return;
    }
}
HOOK(0x48BC00, AICombat_Stand);

/* when under fire: 0 lie down, 1 roll left, 2 roll right, -1 stay */
int AICombat_ChooseEvade(HumanAI *ai, CombatData *cd)
{
    float r = Random_Float(Game_GetRandom());
    uint8_t sides;
    char left, right;

    if (ai->nextEvade >= 0 && ai->nextEvade >= Flow_GetFrames())
        return -1;
    ai->nextEvade = Flow_GetFrames() + 10;
    if (!ai->eventCounts[EV_GUNSHOTMISS] && !ai->eventCounts[EV_GUNSHOT] && !ai->eventCounts[EV_TAKINGDAMAGE])
        return -1;
    {
        GraphNode *node = Graph_GetNode(ai->graph, ai->node);

        if (GraphNode_HasCriteria(node, AIGraph_GetStairCriteria()))
            return -1;
    }
    if (Random_Float(Game_GetRandom()) > ai->evadeChance)
        return -1;
    if (ai->lieChance == kZero || (ai->lieChance < r && cd->dist > ai->lieRange))
        return 0;
    r = Random_Float(Game_GetRandom());
    sides = HumanAI_CheckSides(ai);
    left = (sides & 1) && ai->rollDir != 1;
    right = (sides & 2) && ai->rollDir != -1;
    if (left && (ai->rollChance == kZero || ai->rollChance < r))
        return 1;
    if (right && (ai->rollChance == 1.0f || !(ai->rollChance < r)))
        return 2;
    return -1;
}
HOOK(0x48BE30, AICombat_ChooseEvade);

/* like AICombat_DecideFemale, but 1 throws a grenade and 3 fights close by */
int AICombat_Decide(HumanAI *ai, CombatData *cd)
{
    AISeen *seen = ai->seen;
    int count = cd->count;

    cd->count = 0;
    AITarget_SetState(ai->target, ai, TARGET_NONE);
    if (off_node(ai))
        return 4;
    if (!seen->visible)
        return decide_hidden(ai, cd, count);
    if (cd->dist < ai->closeRange)
        return 3;
    if (!HumanAI_GetGunAmmo(ai)) {
        if (ai->grenadeMin < cd->dist && ai->grenadeMax > cd->dist && ai->hasGrenade &&
            Random_Float(Game_GetRandom()) <= ai->grenadeChance)
            return 1;
        AITarget_SetState(ai->target, ai, TARGET_RELOADING);
        return 0;
    }
    if (decide_cover(ai, cd))
        return 5;
    if (ai->grenadeMin < cd->dist && ai->grenadeMax > cd->dist && ai->hasGrenade &&
        Random_Float(Game_GetRandom()) <= ai->grenadeChance2)
        return 1;
    return decide_fire(ai);
}
HOOK(0x48BFD0, AICombat_Decide);

void AICombat_Kneel(HumanAI *ai, AIAction *a);

void AICombat_EnterKneel(HumanAI *ai, AIAction *a)
{
    ai->unk3E50 = 0;
    a->update = AICombat_Kneel;
    a->resume = AICombat_EnterKneel;
}
HOOK(0x48C250, AICombat_EnterKneel);

/* leaves a crouching or lying pose for the stand state, then runs enter */
static void get_up(HumanAI *ai, AIAction *a, int channel, AIActionFn enter)
{
    HumanAI_SetChannel(ai, channel, 0);
    AIAction_SetStep(ai, a, AICombat_EnterStand, NULL);
    if (enter)
        AIAction_SetStep(ai, a, enter, NULL);
}

void AICombat_Kneel(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    Debug_Log(ai, -1, "Combat - Kneel");
    AICombat_Update(ai, cd);
    if (!ai->seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    HumanAI_SetChannel(ai, POSE_KNEEL, 1.0f);
    switch (AICombat_ChooseEvade(ai, cd)) {
    case 0:
        AIAction_SetStep(ai, a, AICombat_EnterLiedown, NULL);
        return;
    case 1:
        AIAction_SetStep(ai, a, NULL, AICombat_KneelRollLeft);
        ai->ctrl[CTRL_ROLL_LEFT] = 1.0f;
        ai->rollDir = 1;
        return;
    case 2:
        AIAction_SetStep(ai, a, NULL, AICombat_KneelRollRight);
        ai->ctrl[CTRL_ROLL_RIGHT] = 1.0f;
        ai->rollDir = -1;
        return;
    }
    if (run_step(ai, a))
        return;
    if (!HumanAI_InPose(ai, POSE_KNEEL)) {
        HumanAI_InPose(ai, POSE_ANIM);
        Debug_Log(ai, -1, "Waiting for kneel state");
        return;
    }
    if (ai->lieRange < kZeroD) {
        get_up(ai, a, POSE_KNEEL, NULL);
        return;
    }
    switch (AICombat_Decide(ai, cd)) {
    case 0: AIAction_SetStep(ai, a, NULL, AICombat_KneelReload); return;
    case 1: Debug_Log(ai, -1, "Kneel throw grenade"); return;
    case 2: start_fire(ai, a, cd, AICombat_KneelFire); return;
    case 3: get_up(ai, a, POSE_KNEEL, NULL); return;
    case 4: get_up(ai, a, POSE_KNEEL, AICombat_EnterReposition); return;
    case 5: get_up(ai, a, POSE_KNEEL, AICombat_EnterFollow); return;
    case 6: get_up(ai, a, POSE_KNEEL, AICombat_EnterLookForTarget); return;
    case 7: HumanAI_EndAction(ai, a); return;
    case -1: Debug_Log(ai, -1, "Waiting for patrol command"); return;
    }
}
HOOK(0x48C280, AICombat_Kneel);

void AICombat_Liedown(HumanAI *ai, AIAction *a);

void AICombat_EnterLiedown(HumanAI *ai, AIAction *a)
{
    ai->unk3E50 = 0;
    a->update = AICombat_Liedown;
    a->resume = AICombat_EnterLiedown;
}
HOOK(0x48C510, AICombat_EnterLiedown);

void AICombat_Liedown(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    Debug_Log(ai, -1, "Combat - Liedown");
    AICombat_Update(ai, cd);
    if (!ai->seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    HumanAI_SetChannel(ai, POSE_LIEDOWN, 1.0f);
    if (run_step(ai, a))
        return;
    if (!HumanAI_InPose(ai, POSE_LIEDOWN)) {
        HumanAI_InPose(ai, POSE_ANIM);
        Debug_Log(ai, -1, "Waiting for liedown state");
        return;
    }
    if (ai->lieRange < kZeroD) {
        get_up(ai, a, POSE_LIEDOWN, NULL);
        return;
    }
    switch (AICombat_Decide(ai, cd)) {
    case 0: AIAction_SetStep(ai, a, NULL, AICombat_LieDownReload); return;
    case 1: Debug_Log(ai, -1, "liedown throw grenade"); return;
    case 2: start_fire(ai, a, cd, AICombat_LieDownFire); return;
    case 3: get_up(ai, a, POSE_LIEDOWN, NULL); return;
    case 4: get_up(ai, a, POSE_LIEDOWN, AICombat_EnterReposition); return;
    case 5: get_up(ai, a, POSE_LIEDOWN, AICombat_EnterFollow); return;
    case 6: get_up(ai, a, POSE_LIEDOWN, AICombat_EnterLookForTarget); return;
    case 7: HumanAI_EndAction(ai, a); return;
    case -1: Debug_Log(ai, -1, "Waiting for patrol command"); return;
    }
}
HOOK(0x48C540, AICombat_Liedown);

void AICombat_Reposition(HumanAI *ai, AIAction *a);

void AICombat_EnterReposition(HumanAI *ai, AIAction *a)
{
    a->update = AICombat_Reposition;
}
HOOK(0x48C750, AICombat_EnterReposition);

/* runs to the cover position picked by AICombat_TryNode */
void AICombat_Reposition(HumanAI *ai, AIAction *a)
{
    Debug_Log(ai, -1, "Combat - RePosition");
    ai->ctrl[CTRL_COMBAT] = 1.0f;
    AICombat_Update(ai, combat_data(a));
    if (!ai->seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (!ai->grid) {
        Warning_Show("HumanAI #%d has no nodeoccupationiten", Task_GetId(ai));
        return;
    }
    if (!Graph_IsInNode(&ai->cover, Graph_GetNode(ai->graph, ai->grid->nodeId), kNodeTol, 0))
        Debug_Log(ai, -1, "Position not on node #%d", ai->grid->nodeId);
    if (HumanAI_RunTo(ai, ai->grid->nodeId, &ai->cover, 0))
        AIAction_SetStep(ai, a, a->resume, NULL);
}
HOOK(0x48C760, AICombat_Reposition);

void AICombat_Follow(HumanAI *ai, AIAction *a);

void AICombat_EnterFollow(HumanAI *ai, AIAction *a)
{
    a->update = AICombat_Follow;
}
HOOK(0x48C850, AICombat_EnterFollow);

/* runs towards the target's node, firing when it is right in front */
void AICombat_Follow(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    Debug_Log(ai, -1, "Combat - Follow");
    ai->ctrl[CTRL_COMBAT] = 1.0f;
    AICombat_Update(ai, cd);
    if (!ai->seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (ai->destNode == -1) {
        AIAction_SetStep(ai, a, a->resume, NULL);
        return;
    }
    HumanAI_SetRun(ai);
    Debug_Log(ai, -1, "Tracking target at node #%d", ai->destNode);
    if (ai->targetMoved) {
        GraphNode *node = NULL;

        AICombat_FindCover(&node, ai, ai->seen->visible, 5);
        if (node && node->id != ai->node) {
            ai->destNode = node->id;
            cd->count = 0;
        }
        ai->targetMoved = 0;
    }
    if (HumanAI_RunTo(ai, ai->destNode, &ai->cover, 0)) {
        ai->destNode = -1;
        AIAction_SetStep(ai, a, a->resume, NULL);
        AITarget_SetState(ai->target, ai, TARGET_NONE);
        return;
    }
    if (ai->seen->visible) {
        Vec3d d, f;

        d.x = ai->target->pos.x - ai->pos.x;
        d.y = ai->target->pos.y - ai->pos.y;
        d.z = ai->target->pos.z - ai->pos.z;
        f.x = ai->view[1];
        f.y = ai->view[4];
        f.z = ai->view[7];
        Vec3d_Normalize(&d);
        if (f.z * d.z + f.y * d.y + f.x * d.x > kFacing) {
            if (AITarget_CountState(ai->target, TARGET_FIRING) >= 4 && AITarget_GetState(ai->target, ai)) {
                AITarget_SetState(ai->target, ai, TARGET_WAITING);
                return;
            }
            ai->ctrl[CTRL_TRIGGER] = 1.0f;
            AITarget_SetState(ai->target, ai, TARGET_FIRING);
            return;
        }
    }
    AITarget_SetState(ai->target, ai, TARGET_NONE);
}
HOOK(0x48C860, AICombat_Follow);

void AICombat_LookForTarget(HumanAI *ai, AIAction *a);

void AICombat_EnterLookForTarget(HumanAI *ai, AIAction *a)
{
    a->update = AICombat_LookForTarget;
}
HOOK(0x48CAA0, AICombat_EnterLookForTarget);

void AICombat_LookForTarget(HumanAI *ai, AIAction *a)
{
    Vec3d pos;

    Debug_Log(ai, -1, "Combat - LookForTarget");
    ai->ctrl[CTRL_COMBAT] = 1.0f;
    AICombat_Update(ai, combat_data(a));
    if (!ai->seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    HumanAI_GetTargetPos(&pos, ai);
    HumanAI_FaceTo(ai, &pos, kTurnTol, 0);
    AIAction_SetStep(ai, a, a->resume, NULL);
}
HOOK(0x48CAB0, AICombat_LookForTarget);

void AICombatRPG_Stand(HumanAI *ai, AIAction *a);
void AICombatRPG_Kneel(HumanAI *ai, AIAction *a);

void AICombatRPG_Start(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    if (Human_IsCrouching(ai->task.parent))
        a->update = AICombatRPG_Kneel;
    else
        a->update = AICombatRPG_Stand;
    a->end = AICombat_End;
    ai->unk3DD4 = ai->unk3D44;
    ai->unk3DD0 = -1;
    ai->unk3DD8 = -1;
    take_target(ai, cd);
    cd->events = NULL;
}
HOOK(0x48CB40, AICombatRPG_Start);

/* not visible: count the turns, turn to the target; 1 when done for this frame */
static int rpg_hidden(HumanAI *ai, AIAction *a, CombatData *cd, int count)
{
    Vec3d pos;

    cd->count = count + 1;
    if (cd->count > ai->maxTurns) {
        HumanAI_EndAction(ai, a);
        return 1;
    }
    if (!ai->seen->unk28) {
        Debug_Log(ai, -1, "Turning to target");
        HumanAI_GetTargetPos(&pos, ai);
        HumanAI_FaceTo(ai, &pos, kTurnTol, 0);
        return 1;
    }
    return 0;
}

void AICombatRPG_Stand(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);
    int count = cd->count;
    AISeen *seen;
    char visible, ammo;

    cd->count = 0;
    Debug_Log(ai, -1, "RPG Combat - Stand");
    AICombat_Update(ai, cd);
    seen = ai->seen;
    if (!seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (run_step(ai, a))
        return;
    visible = seen->visible;
    ammo = (char)HumanAI_GetGunAmmo(ai);
    if (!HumanAI_InPose(ai, POSE_STAND))
        return;
    if (!seen->visible && rpg_hidden(ai, a, cd, count))
        return;
    ai->unk3DD4 = ai->unk3D48;
    if (ai->unk3DDC == 5) {
        HumanAI_FaceTo(ai, &ai->target->pos, kTurnTol, 1);
        HumanAI_SetChannel(ai, 9, 1.0f);
        return;
    }
    if (!ammo && ai->unk3DD0 != 1) {
        AIAction_SetStep(ai, a, AICombatRPG_Kneel, AICombat_KneelReload);
        ai->unk3DD0 = 1;
        ai->unk3DD4 = 0;
        return;
    }
    if (ai->unk3DD0 == 0 && Random_Float(Game_GetRandom()) < kRpgKneelChance) {
        a->update = AICombatRPG_Kneel;
        ai->unk3DD0 = 2;
        return;
    }
    if (visible) {
        HumanAI_GetTargetPos(&ai->aimTarget, ai);
        AICombat_StartBurst(ai, ((AIParam *)a->param)->d.first);
        ((AIParam *)a->param)->d.first = 0;
        a->step = AICombat_StandFire;
        ai->unk3DD0 = 0;
    }
}
HOOK(0x48CBD0, AICombatRPG_Stand);

void AICombatRPG_Kneel(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);
    int count = cd->count;
    AISeen *seen;
    char visible, ammo;

    cd->count = 0;
    Debug_Log(ai, -1, "RPG Combat - Kneel");
    AICombat_Update(ai, combat_data(a));
    seen = ai->seen;
    if (!seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (run_step(ai, a))
        return;
    visible = seen->visible;
    ammo = (char)HumanAI_GetGunAmmo(ai);
    HumanAI_SetChannel(ai, POSE_KNEEL, 1.0f);
    if (!HumanAI_InPose(ai, POSE_KNEEL))
        return;
    if (!seen->visible && rpg_hidden(ai, a, cd, count))
        return;
    ai->unk3DD4 = ai->unk3D48;
    if (ai->unk3DDC == 5) {
        a->update = AICombatRPG_Stand;
        return;
    }
    if (!ammo && ai->unk3DD0 != 1) {
        AIAction_SetStep(ai, a, NULL, AICombat_KneelReload);
        ai->unk3DD0 = 1;
        ai->unk3DD4 = 0;
        return;
    }
    if (ai->unk3DD0 == 0 && Random_Float(Game_GetRandom()) < kRpgKneelChance) {
        a->update = AICombatRPG_Stand;
        ai->unk3DD0 = 2;
        return;
    }
    if (visible) {
        HumanAI_GetTargetPos(&ai->aimTarget, ai);
        AICombat_StartBurst(ai, ((AIParam *)a->param)->d.first);
        ((AIParam *)a->param)->d.first = 0;
        a->step = AICombat_KneelFire;
        ai->unk3DD0 = 0;
    }
}
HOOK(0x48CDF0, AICombatRPG_Kneel);

void AICombatGuard_Stand(HumanAI *ai, AIAction *a);

void AICombatGuard_Start(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    a->update = AICombatGuard_Stand;
    a->end = AICombat_End;
    ai->unk3DD0 = -1;
    ai->unk3DD4 = ai->unk3D44;
    ai->unk3DD8 = -1;
    take_target(ai, cd);
    cd->events = NULL;
    cd->count = 0;
}
HOOK(0x48D000, AICombatGuard_Start);

/* guards keep their place: they turn to the target, fire or reload */
void AICombatGuard_Stand(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);
    int count = cd->count;
    AISeen *seen;

    cd->count = 0;
    Debug_Log(ai, -1, "Guard Combat - Stand");
    AICombat_Update(ai, combat_data(a));
    seen = ai->seen;
    if (!seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (run_step(ai, a))
        return;
    if (!HumanAI_InPose(ai, POSE_STAND))
        return;
    if (!seen->visible) {
        Vec3d pos;

        AITarget_SetState(ai->target, ai, TARGET_NONE);
        cd->count = count + 1;
        if (cd->count > ai->maxTurns) {
            HumanAI_EndAction(ai, a);
            return;
        }
        if (!seen->unk28) {
            Debug_Log(ai, -1, "Turning to target");
            HumanAI_GetTargetPos(&pos, ai);
            HumanAI_FaceTo(ai, &pos, kTurnTol, 0);
            return;
        }
    }
    if (!HumanAI_GetGunAmmo(ai)) {
        AIAction_SetStep(ai, a, NULL, AICombat_StandReload);
        ai->unk3DD4 = 0;
        AITarget_SetState(ai->target, ai, TARGET_RELOADING);
        return;
    }
    ai->unk3DD4 = round_int(Random_Float(Game_GetRandom()) * ai->guardRandom + ai->guardMin);
    if (ai->unk3DDC == 5) {
        HumanAI_FaceTo(ai, &ai->target->pos, kTurnTol, 1);
        HumanAI_SetChannel(ai, 9, 1.0f);
    } else if (seen->visible) {
        if (AITarget_CountState(ai->target, TARGET_FIRING) < 4) {
            AITarget_SetState(ai->target, ai, TARGET_FIRING);
            HumanAI_GetTargetPos(&ai->aimTarget, ai);
            AICombat_StartBurst(ai, ((AIParam *)a->param)->d.first);
            ((AIParam *)a->param)->d.first = 0;
            a->step = AICombat_StandFire;
            return;
        }
        AITarget_SetState(ai->target, ai, TARGET_WAITING);
        return;
    }
    AITarget_SetState(ai->target, ai, TARGET_NONE);
}
HOOK(0x48D080, AICombatGuard_Stand);

void AICombatGunner_Stand(HumanAI *ai, AIAction *a);

void AICombatGunner_Start(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);

    a->update = AICombatGunner_Stand;
    a->end = AICombat_End;
    ai->unk3DD4 = ai->unk3D44;
    ai->unk3DD0 = -1;
    ai->unk3DD8 = -1;
    take_target(ai, cd);
    cd->timer = 30;
    cd->events = NULL;
    cd->count = 0;
}
HOOK(0x48D2C0, AICombatGunner_Start);

/* gunners use the stationary gun +0x3AC8 while the target is inside its cone */
void AICombatGunner_Stand(HumanAI *ai, AIAction *a)
{
    CombatData *cd = combat_data(a);
    Human *h = (Human *)ai->task.parent;
    char inCone = 0;
    AISeen *seen;
    int count;

    Debug_Log(ai, -1, "Gunner Combat - Stand");
    AICombat_Update(ai, cd);
    seen = ai->seen;
    if (!seen) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (ai->statGunId != -1) {
        StatGun *gun = Task_FindById(ai->statGunId);

        if (!gun)
            Warning_Show("Stationary gun ID #%d not found", ai->statGunId);
        else if (gun->used && (Task *)gun->user != &h->task)
            goto pose;
        if (gun) {
            Vec3d t, l;
            float pitch, yaw;
            long double yawRaw;

            HumanAI_GetTargetPos(&t, ai);
            {
                long double dx = t.x - gun->basePos.x;
                long double dy = t.y - gun->basePos.y;
                long double dz = t.z - gun->basePos.z;

                l.x = gun->baseMatrix.m[6] * dz + gun->baseMatrix.m[3] * dy + gun->baseMatrix.m[0] * dx;
                l.y = gun->baseMatrix.m[7] * dz + gun->baseMatrix.m[4] * dy + gun->baseMatrix.m[1] * dx;
                l.z = gun->baseMatrix.m[8] * dz + gun->baseMatrix.m[5] * dy + gun->baseMatrix.m[2] * dx;
            }
            l.x = -l.x;
            l.y = -l.y;
            l.z = -l.z;
            pitch = Vec3d_PitchAngle(l);
            yawRaw = Vec3d_YawAngle(l);
            yaw = yawRaw;
            Debug_Log(ai, -1, "Gun->Tgt: A:%1.3f, G:%1.3f", (double)pitch, (double)yawRaw);
            Debug_Log(ai, -1, "Gun params:");
            Debug_Log(ai, -1, "MG:%1.3f, A-:%1.3f, A+:%1.3f", (double)(gun->yawRange * kDegToRad),
                      (double)(gun->minPitch * kDegToRad), (double)(gun->maxPitch * kDegToRad));
            if ((yaw < kZero ? -yaw : yaw) < gun->yawRange * kDegToRad && gun->minPitch * kDegToRad < pitch &&
                gun->maxPitch * kDegToRad > pitch) {
                Debug_Log(ai, -1, "Target is in guncone");
                inCone = 1;
            }
        }
    }
pose:
    if (!HumanAI_InPose(ai, POSE_GUN)) {
        if (inCone) {
            if (cd->timer == 0) {
                HumanAI_SetChannel(ai, POSE_GUN, (float)ai->statGunId);
                return;
            }
            cd->timer--;
        } else {
            cd->timer = 30;
        }
        if (ai->gunId == -1) {
            Debug_Log(ai, -1, "No primary weapon");
            return;
        }
        AICombatGuard_Stand(ai, a);
        return;
    }
    count = cd->count;
    cd->count = 0;
    if (!inCone) {
        if (cd->timer == 0) {
            HumanAI_SetChannel(ai, POSE_GUN, 1.0f);
            AITarget_SetState(ai->target, ai, TARGET_NONE);
            return;
        }
        cd->timer--;
    } else {
        cd->timer = 30;
    }
    StationaryGun_GetMuzzle(*h->statGun, &ai->eye, ai->view);
    if (!seen->visible) {
        AITarget_SetState(ai->target, ai, TARGET_NONE);
        cd->count = count + 1;
        if (cd->count > ai->maxTurns) {
            HumanAI_EndAction(ai, a);
            return;
        }
        a->step = NULL;
        HumanAI_FaceTo(ai, &ai->target->pos, kTurnTol, 0);
        return;
    }
    if (a->step) {
        if (!a->step(ai, a))
            return;
        a->step = NULL;
    }
    if (AITarget_CountState(ai->target, TARGET_FIRING) < 4) {
        AITarget_SetState(ai->target, ai, TARGET_FIRING);
        HumanAI_GetTargetPos(&ai->aimTarget, ai);
        AICombat_StartBurst(ai, ((AIParam *)a->param)->d.first);
        ((AIParam *)a->param)->d.first = 0;
        a->step = AICombat_StandFire;
        return;
    }
    AITarget_SetState(ai->target, ai, TARGET_WAITING);
}
HOOK(0x48D340, AICombatGunner_Stand);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5416CC) int gLastDetectedTick INIT(-1);
PAD(igd, 5416CC, 5416D0, 0x698) INIT({0x24, 0x17, 0x54, 0x00, 0x18, 0x17, 0x54, 0x00, 0x0C, 0x17, 0x54, 0x00, 0x00, 0x17, 0x54, 0x00, 0xF4, 0x16, 0x54, 0x00, 0xE8, 0x16, 0x54, 0x00, 0x64, 0x65, 0x74, 0x65, 0x63, 0x74, 0x65, 0x64, 0x5F, 0x30, 0x36, 0x00, 0x64, 0x65, 0x74, 0x65, 0x63, 0x74, 0x65, 0x64, 0x5F, 0x30, 0x35, 0x00, 0x64, 0x65, 0x74, 0x65, 0x63, 0x74, 0x65, 0x64, 0x5F, 0x30, 0x34, 0x00, 0x64, 0x65, 0x74, 0x65, 0x63, 0x74, 0x65, 0x64, 0x5F, 0x30, 0x33, 0x00, 0x64, 0x65, 0x74, 0x65, 0x63, 0x74, 0x65, 0x64, 0x5F, 0x30, 0x32, 0x00, 0x64, 0x65, 0x74, 0x65, 0x63, 0x74, 0x65, 0x64, 0x5F, 0x30, 0x31, 0x00, 0x44, 0x65, 0x61, 0x64, 0x00, 0x00, 0x00, 0x00, 0x57, 0x61, 0x69, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x70, 0x61, 0x74, 0x72, 0x6F, 0x6C, 0x20, 0x63, 0x6F, 0x6D, 0x6D, 0x61, 0x6E, 0x64, 0x00, 0x00, 0x57, 0x61, 0x69, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x73, 0x74, 0x61, 0x6E, 0x64, 0x20, 0x73, 0x74, 0x61, 0x74, 0x65, 0x00, 0x46, 0x65, 0x6D, 0x61, 0x6C, 0x65, 0x20, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x00, 0x00, 0x00, 0x4E, 0x6F, 0x20, 0x6E, 0x6F, 0x64, 0x65, 0x73, 0x20, 0x6E, 0x65, 0x61, 0x72, 0x20, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x00, 0x00, 0x00, 0x46, 0x69, 0x6E, 0x64, 0x46, 0x72, 0x65, 0x65, 0x50, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x73, 0x74, 0x61, 0x63, 0x6B, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x6C, 0x61, 0x72, 0x67, 0x65, 0x20, 0x65, 0x6E, 0x6F, 0x75, 0x67, 0x68, 0x00, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x00, 0x00, 0x4B, 0x6E, 0x65, 0x65, 0x6C, 0x20, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x20, 0x67, 0x72, 0x65, 0x6E, 0x61, 0x64, 0x65, 0x00, 0x57, 0x61, 0x69, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x6B, 0x6E, 0x65, 0x65, 0x6C, 0x20, 0x73, 0x74, 0x61, 0x74, 0x65, 0x00, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x4B, 0x6E, 0x65, 0x65, 0x6C, 0x00, 0x00, 0x6C, 0x69, 0x65, 0x64, 0x6F, 0x77, 0x6E, 0x20, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x20, 0x67, 0x72, 0x65, 0x6E, 0x61, 0x64, 0x65, 0x00, 0x00, 0x00, 0x57, 0x61, 0x69, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x6C, 0x69, 0x65, 0x64, 0x6F, 0x77, 0x6E, 0x20, 0x73, 0x74, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x4C, 0x69, 0x65, 0x64, 0x6F, 0x77, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x41, 0x49, 0x20, 0x23, 0x25, 0x64, 0x20, 0x68, 0x61, 0x73, 0x20, 0x6E, 0x6F, 0x20, 0x6E, 0x6F, 0x64, 0x65, 0x6F, 0x63, 0x63, 0x75, 0x70, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x69, 0x74, 0x65, 0x6E, 0x00, 0x00, 0x00, 0x50, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x6F, 0x6E, 0x20, 0x6E, 0x6F, 0x64, 0x65, 0x20, 0x23, 0x25, 0x64, 0x00, 0x00, 0x00, 0x00, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x52, 0x65, 0x50, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x00, 0x54, 0x72, 0x61, 0x63, 0x6B, 0x69, 0x6E, 0x67, 0x20, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x20, 0x61, 0x74, 0x20, 0x6E, 0x6F, 0x64, 0x65, 0x20, 0x23, 0x25, 0x64, 0x00, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x46, 0x6F, 0x6C, 0x6C, 0x6F, 0x77, 0x00, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x4C, 0x6F, 0x6F, 0x6B, 0x46, 0x6F, 0x72, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x00, 0x54, 0x75, 0x72, 0x6E, 0x69, 0x6E, 0x67, 0x20, 0x74, 0x6F, 0x20, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x00, 0x00, 0x52, 0x50, 0x47, 0x20, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x00, 0x00, 0x52, 0x50, 0x47, 0x20, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x4B, 0x6E, 0x65, 0x65, 0x6C, 0x00, 0x00, 0x47, 0x75, 0x61, 0x72, 0x64, 0x20, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x6F, 0x20, 0x70, 0x72, 0x69, 0x6D, 0x61, 0x72, 0x79, 0x20, 0x77, 0x65, 0x61, 0x70, 0x6F, 0x6E, 0x00, 0x00, 0x00, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x20, 0x69, 0x73, 0x20, 0x69, 0x6E, 0x20, 0x67, 0x75, 0x6E, 0x63, 0x6F, 0x6E, 0x65, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x47, 0x3A, 0x25, 0x31, 0x2E, 0x33, 0x66, 0x2C, 0x20, 0x41, 0x2D, 0x3A, 0x25, 0x31, 0x2E, 0x33, 0x66, 0x2C, 0x20, 0x41, 0x2B, 0x3A, 0x25, 0x31, 0x2E, 0x33, 0x66, 0x00, 0x00, 0x00, 0x00, 0x47, 0x75, 0x6E, 0x20, 0x70, 0x61, 0x72, 0x61, 0x6D, 0x73, 0x3A, 0x00, 0x47, 0x75, 0x6E, 0x2D, 0x3E, 0x54, 0x67, 0x74, 0x3A, 0x20, 0x41, 0x3A, 0x25, 0x31, 0x2E, 0x33, 0x66, 0x2C, 0x20, 0x47, 0x3A, 0x25, 0x31, 0x2E, 0x33, 0x66, 0x00, 0x00, 0x53, 0x74, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x61, 0x72, 0x79, 0x20, 0x67, 0x75, 0x6E, 0x20, 0x49, 0x44, 0x20, 0x23, 0x25, 0x64, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x66, 0x6F, 0x75, 0x6E, 0x64, 0x00, 0x47, 0x75, 0x6E, 0x6E, 0x65, 0x72, 0x20, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x00, 0x00, 0x00, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x20, 0x2D, 0x20, 0x52, 0x6F, 0x6C, 0x6C, 0x20, 0x6C, 0x65, 0x66, 0x74, 0x00, 0x00, 0x00, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x20, 0x2D, 0x20, 0x52, 0x6F, 0x6C, 0x6C, 0x20, 0x72, 0x69, 0x67, 0x68, 0x74, 0x00, 0x00, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x20, 0x2D, 0x20, 0x46, 0x69, 0x72, 0x65, 0x00, 0x00, 0x00, 0x00, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x20, 0x2D, 0x20, 0x52, 0x65, 0x6C, 0x6F, 0x61, 0x64, 0x00, 0x00, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x20, 0x2D, 0x20, 0x43, 0x6C, 0x6F, 0x73, 0x65, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x20, 0x54, 0x72, 0x69, 0x67, 0x67, 0x65, 0x72, 0x00, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x20, 0x2D, 0x20, 0x54, 0x68, 0x72, 0x6F, 0x77, 0x69, 0x6E, 0x67, 0x20, 0x67, 0x72, 0x65, 0x6E, 0x61, 0x64, 0x65, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x6E, 0x65, 0x65, 0x6C, 0x20, 0x2D, 0x20, 0x52, 0x6F, 0x6C, 0x6C, 0x20, 0x6C, 0x65, 0x66, 0x74, 0x00, 0x00, 0x00, 0x4B, 0x6E, 0x65, 0x65, 0x6C, 0x20, 0x2D, 0x20, 0x52, 0x6F, 0x6C, 0x6C, 0x20, 0x72, 0x69, 0x67, 0x68, 0x74, 0x00, 0x00, 0x4B, 0x6E, 0x65, 0x65, 0x6C, 0x20, 0x2D, 0x20, 0x46, 0x69, 0x72, 0x65, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x6E, 0x65, 0x65, 0x6C, 0x20, 0x2D, 0x20, 0x52, 0x65, 0x6C, 0x6F, 0x61, 0x64, 0x00, 0x00, 0x4C, 0x69, 0x65, 0x44, 0x6F, 0x77, 0x6E, 0x20, 0x2D, 0x20, 0x46, 0x69, 0x72, 0x65, 0x00, 0x00, 0x4C, 0x69, 0x65, 0x44, 0x6F, 0x77, 0x6E, 0x20, 0x2D, 0x20, 0x52, 0x65, 0x6C, 0x6F, 0x61, 0x64, 0x00, 0x00, 0x00, 0x00, 0x52, 0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x61, 0x63, 0x74, 0x69, 0x6F, 0x6E, 0x00, 0x00, 0x00, 0x57, 0x61, 0x69, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x73, 0x74, 0x61, 0x6E, 0x64, 0x73, 0x74, 0x61, 0x74, 0x65, 0x00, 0x00, 0x49, 0x6E, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72, 0x65, 0x73, 0x73, 0x00, 0x50, 0x6C, 0x61, 0x79, 0x69, 0x6E, 0x67, 0x20, 0x61, 0x6E, 0x69, 0x6D, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x2D, 0x20, 0x25, 0x64, 0x00, 0x00, 0x4D, 0x6F, 0x76, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x65, 0x76, 0x65, 0x6E, 0x74, 0x20, 0x25, 0x64, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x6F, 0x6F, 0x6B, 0x20, 0x61, 0x74, 0x20, 0x65, 0x76, 0x65, 0x6E, 0x74, 0x2C, 0x20, 0x25, 0x64, 0x00, 0x00, 0x00, 0x53, 0x74, 0x75, 0x6E, 0x6E, 0x65, 0x64, 0x00, 0x4B, 0x69, 0x63, 0x6B, 0x20, 0x67, 0x72, 0x65, 0x6E, 0x61, 0x64, 0x65, 0x00, 0x00, 0x00, 0x00, 0x52, 0x75, 0x6E, 0x50, 0x61, 0x6E, 0x69, 0x63, 0x6B, 0x69, 0x6E, 0x67, 0x20, 0x73, 0x74, 0x61, 0x63, 0x6B, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x6C, 0x61, 0x72, 0x67, 0x65, 0x20, 0x65, 0x6E, 0x6F, 0x75, 0x67, 0x68, 0x00, 0x52, 0x75, 0x6E, 0x20, 0x70, 0x61, 0x6E, 0x69, 0x63, 0x6B, 0x69, 0x6E, 0x67, 0x00, 0x00, 0x00, 0x49, 0x64, 0x6C, 0x65, 0x00, 0x00, 0x00, 0x00, 0x53, 0x65, 0x74, 0x20, 0x43, 0x6F, 0x6D, 0x62, 0x61, 0x74, 0x00, 0x00, 0x4C, 0x65, 0x76, 0x65, 0x6C, 0x20, 0x51, 0x54, 0x61, 0x73, 0x6B, 0x20, 0x49, 0x44, 0x20, 0x23, 0x25, 0x64, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x64, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x64, 0x20, 0x28, 0x41, 0x49, 0x20, 0x41, 0x63, 0x74, 0x69, 0x76, 0x61, 0x74, 0x65, 0x29, 0x00, 0x00, 0x00, 0x00, 0x50, 0x61, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x69, 0x6E, 0x67, 0x2C, 0x20, 0x57, 0x61, 0x6C, 0x6B, 0x74, 0x6F, 0x3A, 0x25, 0x64, 0x2C, 0x20, 0x43, 0x75, 0x72, 0x3A, 0x25, 0x64, 0x00, 0x00, 0x00, 0x50, 0x61, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x69, 0x6E, 0x67, 0x2C, 0x20, 0x52, 0x75, 0x6E, 0x74, 0x6F, 0x3A, 0x25, 0x64, 0x2C, 0x20, 0x43, 0x75, 0x72, 0x3A, 0x25, 0x64, 0x00, 0x00, 0x00, 0x00, 0x50, 0x61, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x69, 0x6E, 0x67, 0x2C, 0x20, 0x4C, 0x6F, 0x6F, 0x6B, 0x41, 0x74, 0x4E, 0x6F, 0x64, 0x65, 0x20, 0x3A, 0x20, 0x25, 0x64, 0x00, 0x50, 0x61, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x69, 0x6E, 0x67, 0x2C, 0x20, 0x53, 0x74, 0x61, 0x6E, 0x64, 0x61, 0x63, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x3A, 0x20, 0x25, 0x64, 0x00, 0x00, 0x00, 0x00, 0x50, 0x61, 0x74, 0x72, 0x6F, 0x6C, 0x6C, 0x69, 0x6E, 0x67, 0x2C, 0x20, 0x44, 0x65, 0x6C, 0x61, 0x79, 0x20, 0x3A, 0x20, 0x25, 0x64, 0x00, 0x00, 0x46, 0x61, 0x6C, 0x6C, 0x20, 0x66, 0x6C, 0x61, 0x74, 0x20, 0x2D, 0x20, 0x25, 0x64, 0x00, 0x00, 0x57, 0x61, 0x6C, 0x6B, 0x20, 0x54, 0x6F, 0x20, 0x4E, 0x6F, 0x64, 0x65, 0x20, 0x2D, 0x20, 0x25, 0x64, 0x00, 0x00, 0x00, 0x52, 0x75, 0x6E, 0x20, 0x54, 0x6F, 0x20, 0x4E, 0x6F, 0x64, 0x65, 0x20, 0x2D, 0x20, 0x25, 0x64, 0x00, 0x00, 0x00, 0x00, 0x46, 0x69, 0x72, 0x65, 0x20, 0x41, 0x74, 0x20, 0x4E, 0x6F, 0x64, 0x65, 0x20, 0x2D, 0x20, 0x25, 0x64, 0x2C, 0x20, 0x25, 0x64, 0x00, 0x00, 0x00, 0x46, 0x69, 0x72, 0x65, 0x41, 0x74, 0x54, 0x61, 0x73, 0x6B, 0x20, 0x74, 0x61, 0x72, 0x67, 0x65, 0x74, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x62, 0x61, 0x73, 0x65, 0x64, 0x20, 0x6F, 0x6E, 0x20, 0x71, 0x6F, 0x62, 0x6A, 0x20, 0x74, 0x61, 0x73, 0x6B, 0x74, 0x79, 0x70, 0x65, 0x00, 0x00, 0x00, 0x00, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x41, 0x49, 0x20, 0x2D, 0x20, 0x46, 0x69, 0x72, 0x65, 0x41, 0x74, 0x54, 0x61, 0x73, 0x6B, 0x2C, 0x20, 0x74, 0x61, 0x73, 0x6B, 0x20, 0x69, 0x64, 0x20, 0x23, 0x25, 0x64, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x64, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x64, 0x00, 0x00, 0x00, 0x46, 0x69, 0x72, 0x65, 0x20, 0x41, 0x74, 0x20, 0x54, 0x61, 0x73, 0x6B, 0x20, 0x2D, 0x20, 0x25, 0x64, 0x2C, 0x20, 0x25, 0x64, 0x00, 0x00, 0x00, 0x41, 0x49, 0x28, 0x25, 0x64, 0x29, 0x20, 0x74, 0x79, 0x70, 0x65, 0x28, 0x25, 0x73, 0x29, 0x20, 0x63, 0x68, 0x61, 0x6E, 0x6E, 0x65, 0x6C, 0x28, 0x25, 0x64, 0x29, 0x00, 0x47, 0x65, 0x72, 0x6D, 0x61, 0x6E, 0x79});
ALIAS(gDetectedSounds, pad_5416D0, 0);
BSS(5C8BAC) int gCoverFromNode;
PAD(igb, 5C8BAC, 5C8BB0, 0x4);
BSS(5C8BAC) int gCoverTargetNode;
BSS(5C8BAC) int gCoverCellOrder;
