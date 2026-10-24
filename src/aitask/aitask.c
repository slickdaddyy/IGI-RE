/* The AI action handlers: combat moves, and the start / update / end
   callbacks of the actions queued by the AIAction_* script functions. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "igi.h"
#include "hook.h"
#include "types.h"

typedef struct Task {
    void *next;
    void *prev;
    void *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
    Vec3d pos;
} Task;

typedef struct Human {
    Task task;
    char unk38[0x4C4 - 0x38];
    Task *weapons[10];
    char unk4EC[0x7B4 - 0x4EC];
    Task **statGun;
} Human;

typedef struct WeaponType {
    char unk0[0x610];
    int kind;
    char unk614[0x64C - 0x614];
    int unk64C;
} WeaponType;

typedef struct GraphLink {
    char unk0[8];
    int a;
    int b;
} GraphLink;

typedef struct GraphNode {
    int id;
    char unk4[0x13C - 4];
    GraphLink *links[64];
} GraphNode;

typedef struct Graph {
    char unk0[0x5C];
    int unk5C;
} Graph;

/* {path, cur, end, move, prevMove, ...}, see the patrol notes */
typedef struct PatrolRoute {
    void *path;
    void *cur;
    void *end;
    void *move;
    void *prevMove;
    char crouch;
    char walk;
    char unk16[2];
    int timer;
} PatrolRoute;

typedef struct AIMessage {
    int type;
    int team;
    unsigned flags;
    float radius;
    Vec3d *pos;
    Task *from;
    Task *about;
} AIMessage;

typedef struct HumanAI {
    Task task;
    Vec3d eye;
    float body[10];
    float view[10];
    char unkA0[0x1514 - 0xA0];
    int routeCount;
    PatrolRoute routes[8];
    char unk15F8[0x3AA8 - 0x15F8];
    void *target;
    void *unk3AAC;
    void *claim;
    void *alarmClaim;
    char unk3AB8[2];
    char deadDone;
    char unk3ABB[5];
    int alarmTriggerId;
    int alarmControlId;
    char unk3AC8[0x3B6C - 0x3AC8];
    Graph *graph;
    char unk3B70[0x3C74 - 0x3B70];
    float ctrl[0x28];
    char unk3D14;
    char typeName[0x23];
    int aiType;
    char unk3D3C[0x3D80 - 0x3D3C];
    float shotDelayMin;
    float shotDelayRandom;
    char unk3D88[0x3DC8 - 0x3D88];
    double aimSpeed;
    char unk3DD0[0x3DE8 - 0x3DD0];
    int unk3DE8;
    char unk3DEC[5];
    char reloadToggle;
    char unk3DF2[2];
    int rollDir;
    char unk3DF8[0x3E2C - 0x3DF8];
    int node;
    int destNode;
    char unk3E34[0x3E50 - 0x3E34];
    int unk3E50;
    int shotsLeft;
    int lastAmmo;
    int unk3E5C;
    int shotDelay;
    char unk3E64[0x3E70 - 0x3E64];
    Vec3d aim;
    Vec3d aimTarget;
    char unk3EA0[0x10];
    Vec3d unk3EB0;
    char unk3EC8[8];
} HumanAI;

typedef struct AIAction AIAction;
typedef void (*AIActionFn)(HumanAI *ai, AIAction *a);

struct AIAction {
    void *next;
    void *prev;
    int event;
    int delay;
    unsigned flags;
    AIActionFn start;
    AIActionFn update;
    int unk1C;
    AIActionFn end;
    int unk24;
    void *param;
    char used;
    char unk2D[3];
};

/* the parameter blocks, each after an 8 byte list node */
typedef struct AnimParam {
    char node[8];
    int anim;
    char started;
} AnimParam;

typedef struct SoundParam {
    char node[8];
    char name[0x20];
    char onSelf;
} SoundParam;

/* RunPanicking: the search callback gets this part of its PosParam */
typedef struct PanicData {
    Vec3d pos;
    double radius;
    int node;
} PanicData;

typedef struct PosParam {
    char node[8];
    Vec3d pos;
    union {
        struct {
            int node;
            char run;
            char unk25[3];
            Task **ref;
        } move;
        struct {
            float time;
            int until;
        } look;
        struct {
            double radius;
            int node;
        } panic;
        struct {
            Task **from;
            Task **about;
        } combat;
    };
} PosParam;

typedef struct TimedParam {
    char node[8];
    float time;
    int until;
} TimedParam;

typedef struct NodeParam {
    char node[8];
    int id;
    int node1;
    int node2;
    char run;
} NodeParam;

typedef struct FireParam {
    char node[8];
    int id;
    int time;
    int interval;
    int wait;
    char noReload;
    char aimed;
} FireParam;

typedef char (*AIGraphFn)(HumanAI *ai, GraphNode *node, void *ctx);

_Static_assert(offsetof(Human, weapons) == 0x4C4, "Human");
_Static_assert(offsetof(Human, statGun) == 0x7B4, "Human");
_Static_assert(offsetof(WeaponType, unk64C) == 0x64C, "WeaponType");
_Static_assert(offsetof(GraphNode, links) == 0x13C, "GraphNode");
_Static_assert(sizeof(PatrolRoute) == 0x1C, "PatrolRoute");
_Static_assert(offsetof(HumanAI, routes) == 0x1518, "routes");
_Static_assert(offsetof(HumanAI, target) == 0x3AA8, "target");
_Static_assert(offsetof(HumanAI, alarmTriggerId) == 0x3AC0, "alarmTriggerId");
_Static_assert(offsetof(HumanAI, graph) == 0x3B6C, "graph");
_Static_assert(offsetof(HumanAI, typeName) == 0x3D15, "typeName");
_Static_assert(offsetof(HumanAI, aiType) == 0x3D38, "aiType");
_Static_assert(offsetof(HumanAI, aimSpeed) == 0x3DC8, "aimSpeed");
_Static_assert(offsetof(HumanAI, reloadToggle) == 0x3DF1, "reloadToggle");
_Static_assert(offsetof(HumanAI, node) == 0x3E2C, "node");
_Static_assert(offsetof(HumanAI, shotDelay) == 0x3E60, "shotDelay");
_Static_assert(offsetof(HumanAI, aim) == 0x3E70, "aim");
_Static_assert(offsetof(HumanAI, unk3EB0) == 0x3EB0, "unk3EB0");
_Static_assert(sizeof(HumanAI) == 0x3ED0, "HumanAI");
_Static_assert(sizeof(AIAction) == 0x30, "AIAction");
_Static_assert(offsetof(PosParam, move.ref) == 0x28, "PosParam");
_Static_assert(offsetof(PosParam, panic.node) == 0x28, "PosParam");
_Static_assert(offsetof(PanicData, node) == 0x20, "PanicData");
_Static_assert(offsetof(FireParam, aimed) == 0x19, "FireParam");

/* poses checked by HumanAI_InPose */
enum { POSE_STAND, POSE_KNEEL, POSE_LIE, POSE_ANIM, POSE_GUN };

/* HumanAI.ctrl entries written directly */
#define CTRL_ROLL_LEFT 2
#define CTRL_ROLL_RIGHT 3
#define CTRL_FIRE 6
#define CTRL_RELOAD 8

/* per AI type: channel -> HumanAI.ctrl index, pose -> human state (-1 none) */
extern const int *const gAIChannelMap[];
extern const int *const gAIPoseMap[];
extern uint16_t gQObjType;
extern float kFps;
extern float kMinusFps;
extern double kUnit;

static const float kFaceTol = 0x1.1df46ap-5f;
static const float kLookTol = 0x1.aceea0p-5f;

ORIG(0x4C1780, void, Debug_Log, (void *task, int level, const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4028B0, int, Flow_GetFrames, (void));
ORIG(0x413C80, WeaponType *, Weapon_GetType, (Task *weapon));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B3320, char, Vec3d_IsZero, (const Vec3d *v));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4F23D0, void, Weapon_GetAmmo, (Task *weapon, int *out));
ORIG(0x4F2350, void, Weapon_UseAmmo, (Task *weapon, char unlimited));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, Vec3d *pos));
ORIG(0x4F9720, GraphNode *, Graph_GetNode, (Graph *graph, int id));
ORIG(0x4F9770, void, Graph_GetNodePos, (Vec3d *out, GraphNode *node));
ORIG(0x5096F0, void, AIGraph_FindNearest, (GraphNode **out, Graph *graph, const Vec3d *pos));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x463110, int, Human_GetState, (Task *human));
ORIG(0x463130, int, Human_GetTeam, (Task *human));
ORIG(0x475310, void, StationaryGun_GetMuzzle, (Task *gun, Vec3d *pos, float *mat));
ORIG(0x450D50, void, HumanAI_Send, (void *task, AIMessage *m));
ORIG(0x450FF0, void, HumanAI_EndAction, (HumanAI *ai, AIAction *a));
ORIG(0x4511F0, void, AITarget_SetState, (void *t, HumanAI *ai, int state));
ORIG(0x4512A0, void, AITarget_Remove, (void *t, HumanAI *ai, char count));
ORIG(0x451320, void, HumanAI_PopRoute, (HumanAI *ai));
ORIG(0x451380, void, HumanAI_GetTargetPos, (Vec3d *out, HumanAI *ai));
ORIG(0x4513F0, void, HumanAI_LevelAim, (HumanAI *ai));
ORIG(0x4522A0, char, HumanAI_RunTo, (HumanAI *ai, int dest, const Vec3d *pos, char replan));
ORIG(0x452580, char, HumanAI_AimAt, (HumanAI *ai, const Vec3d *pos, float tol, char flat));
ORIG(0x4525B0, char, HumanAI_FaceTo, (HumanAI *ai, const Vec3d *pos, float tol, char flat));
ORIG(0x4525E0, void, HumanAI_SetWalk, (HumanAI *ai));
ORIG(0x452660, void, HumanAI_SetRun, (HumanAI *ai));
ORIG(0x4532D0, char, AIClaim_IsClaimed, (Task *task));
ORIG(0x453300, void *, AIClaim_Set, (Task *task, char on));
ORIG(0x453390, void, AIClaim_Release, (void *claim));
ORIG(0x453540, int, PatrolPathCommand_GetType, (void *cmd));
ORIG(0x453720, void *, PatrolRoute_Current, (PatrolRoute *r));
ORIG(0x453760, void, PatrolRoute_Advance, (PatrolRoute *r));
ORIG(0x4537E0, void, PatrolRoute_Init, (PatrolRoute *r, int path, int start));
ORIG(0x485320, int, GameScreen_GetView, (void *task));
ORIG(0x48B260, char, AICombat_TryNode, (HumanAI *ai, GraphNode *node, char a, char b));
ORIG(0x48B9C0, void, AICombat_StartBurst, (HumanAI *ai, char a));

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

/* rounds to double like a store to memory */
static inline double rd(long double x)
{
    volatile double d = x;

    return d;
}

void HumanAI_SetChannel(HumanAI *ai, int channel, float value);
char HumanAI_InPose(HumanAI *ai, int pose);
char AICombat_Fire(HumanAI *ai);

/* end callback of the combat actions */
void AICombat_End(HumanAI *ai)
{
    if (ai->target) {
        AITarget_Remove(ai->target, ai, ai->deadDone);
        AITarget_SetState(ai->target, ai, -1);
        ai->target = NULL;
    }
    if (HumanAI_InPose(ai, POSE_GUN))
        HumanAI_SetChannel(ai, 4, 1.0f);
}
HOOK(0x48D7D0, AICombat_End);

char AICombat_StandRollLeft(HumanAI *ai)
{
    Debug_Log(ai, -1, "Stand - Roll left");
    ai->ctrl[CTRL_ROLL_LEFT] = 1.0f;
    ai->rollDir = 1;
    return HumanAI_InPose(ai, POSE_KNEEL) != 0;
}
HOOK(0x48D830, AICombat_StandRollLeft);

char AICombat_StandRollRight(HumanAI *ai)
{
    Debug_Log(ai, -1, "Stand - Roll right");
    ai->ctrl[CTRL_ROLL_RIGHT] = 1.0f;
    ai->rollDir = -1;
    return HumanAI_InPose(ai, POSE_KNEEL) != 0;
}
HOOK(0x48D870, AICombat_StandRollRight);

char AICombat_StandFire(HumanAI *ai)
{
    Debug_Log(ai, -1, "Stand - Fire");
    HumanAI_GetTargetPos(&ai->aimTarget, ai);
    return AICombat_Fire(ai) != 0;
}
HOOK(0x48D8B0, AICombat_StandFire);

/* turns the aim towards aimTarget and pulls the trigger; 1 when out of
   shots or ammo */
char AICombat_Fire(HumanAI *ai)
{
    Human *h = (Human *)ai->task.parent;
    WeaponType *wt;
    Task *gun;
    int ammo[2];
    char single;
    Vec3d d;

    if (HumanAI_InPose(ai, POSE_GUN)) {
        Task *sg = *h->statGun;

        StationaryGun_GetMuzzle(sg, &ai->eye, ai->view);
        gun = WeaponSlots_Get((Task **)((char *)sg + 0x20C), 0);
    } else {
        gun = WeaponSlots_Get(h->weapons, 0);
    }
    d.x = ai->aimTarget.x - ai->aim.x;
    d.y = ai->aimTarget.y - ai->aim.y;
    d.z = ai->aimTarget.z - ai->aim.z;
    if (!Vec3d_IsZero(&d)) {
        d.x = rd(d.x * ai->aimSpeed);
        d.y = rd(d.y * ai->aimSpeed);
        d.z = rd(d.z * ai->aimSpeed);
        ai->aim.x = d.x + ai->aim.x;
        ai->aim.y = d.y + ai->aim.y;
        ai->aim.z = d.z + ai->aim.z;
    }
    wt = Weapon_GetType(gun);
    Weapon_GetAmmo(gun, ammo);
    if (wt->kind < 0 || wt->kind > 2)
        return 0;
    if (ai->shotsLeft == 0 || ammo[0] == 0) {
        ai->unk3E50++;
        return 1;
    }
    HumanAI_SetChannel(ai, 6, 1.0f);
    single = wt->unk64C == 1;
    if (ammo[0] != ai->lastAmmo) {
        ai->lastAmmo = ammo[0];
        ai->shotsLeft--;
        if (single) {
            long double r = Random_Float(Game_GetRandom());

            ai->shotDelay = round_int((r * ai->shotDelayRandom + ai->shotDelayMin) * kFps);
            return 0;
        }
        ai->shotDelay = 0;
    }
    if (HumanAI_FaceTo(ai, &ai->aim, kFaceTol, 0)) {
        if (ai->shotDelay == 0) {
            ai->ctrl[CTRL_FIRE] = 1.0f;
            return 0;
        }
        ai->shotDelay--;
    }
    return 0;
}
HOOK(0x48D8E0, AICombat_Fire);

/* returns 1 while reloading; ammo is only refilled when reloadToggle is clear */
static char reload(HumanAI *ai, const char *msg, int channel, int pose, char toggle)
{
    Task *gun = WeaponSlots_Get(((Human *)ai->task.parent)->weapons, 0);
    int ammo[2];

    Debug_Log(ai, -1, msg);
    if (channel >= 0)
        HumanAI_SetChannel(ai, channel, 1.0f);
    Weapon_GetAmmo(gun, ammo);
    if (ammo[0] != 0 && HumanAI_InPose(ai, pose)) {
        if (toggle)
            ai->reloadToggle ^= 1;
        return 1;
    }
    ai->ctrl[CTRL_FIRE] = 0.0f;
    if (ai->reloadToggle) {
        ai->ctrl[CTRL_RELOAD] = 1.0f;
        return 0;
    }
    Weapon_UseAmmo(gun, 1);
    return 0;
}

char AICombat_StandReload(HumanAI *ai)
{
    return reload(ai, "Stand - Reload", -1, POSE_STAND, 1);
}
HOOK(0x48DB10, AICombat_StandReload);

char AICombat_StandCloseCombat(HumanAI *ai)
{
    Debug_Log(ai, -1, "Stand - CloseCombat Trigger");
    HumanAI_SetChannel(ai, 9, 1.0f);
    return HumanAI_InPose(ai, POSE_STAND) == 0;
}
HOOK(0x48DBB0, AICombat_StandCloseCombat);

char AICombat_StandThrowGrenade(HumanAI *ai)
{
    Debug_Log(ai, -1, "Stand - Throwing grenade");
    if (HumanAI_InPose(ai, POSE_STAND)) {
        HumanAI_SetChannel(ai, 8, 1.0f);
        return 0;
    }
    return 1;
}
HOOK(0x48DBF0, AICombat_StandThrowGrenade);

char AICombat_KneelRollLeft(HumanAI *ai)
{
    Debug_Log(ai, -1, "Kneel - Roll left");
    HumanAI_SetChannel(ai, 1, 1.0f);
    ai->ctrl[CTRL_ROLL_LEFT] = 1.0f;
    ai->rollDir = 1;
    return HumanAI_InPose(ai, POSE_KNEEL) != 0;
}
HOOK(0x48DC30, AICombat_KneelRollLeft);

char AICombat_KneelRollRight(HumanAI *ai)
{
    Debug_Log(ai, -1, "Kneel - Roll right");
    HumanAI_SetChannel(ai, 1, 1.0f);
    ai->ctrl[CTRL_ROLL_RIGHT] = 1.0f;
    ai->rollDir = -1;
    return HumanAI_InPose(ai, POSE_KNEEL) != 0;
}
HOOK(0x48DC80, AICombat_KneelRollRight);

char AICombat_KneelFire(HumanAI *ai)
{
    Debug_Log(ai, -1, "Kneel - Fire");
    HumanAI_SetChannel(ai, 1, 1.0f);
    HumanAI_GetTargetPos(&ai->aimTarget, ai);
    return AICombat_Fire(ai) != 0;
}
HOOK(0x48DCD0, AICombat_KneelFire);

char AICombat_KneelReload(HumanAI *ai)
{
    return reload(ai, "Kneel - Reload", 1, POSE_KNEEL, 1);
}
HOOK(0x48DD10, AICombat_KneelReload);

char AICombat_LieDownFire(HumanAI *ai)
{
    Debug_Log(ai, -1, "LieDown - Fire");
    HumanAI_SetChannel(ai, 2, 1.0f);
    HumanAI_GetTargetPos(&ai->aimTarget, ai);
    return AICombat_Fire(ai) != 0;
}
HOOK(0x48DDC0, AICombat_LieDownFire);

char AICombat_LieDownReload(HumanAI *ai)
{
    return reload(ai, "LieDown - Reload", 2, POSE_LIE, 0);
}
HOOK(0x48DE00, AICombat_LieDownReload);

void AIPlayAnim_Update(HumanAI *ai, AIAction *a);

void AIPlayAnim_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIPlayAnim_Update;
    ((AnimParam *)a->param)->started = 0;
}
HOOK(0x48DEA0, AIPlayAnim_Start);

void AIPlayAnim_Update(HumanAI *ai, AIAction *a)
{
    Debug_Log(ai, -1, "Playing animation - %d", ((AnimParam *)a->param)->anim);
    if (HumanAI_InPose(ai, POSE_ANIM)) {
        Debug_Log(ai, -1, "In progress");
        return;
    }
    if (((AnimParam *)a->param)->started) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (!HumanAI_InPose(ai, POSE_STAND)) {
        Debug_Log(ai, -1, "Waiting for standstate");
        return;
    }
    Debug_Log(ai, -1, "Requesting action");
    HumanAI_SetChannel(ai, 3, ((AnimParam *)a->param)->anim);
    ((AnimParam *)a->param)->started = 1;
}
HOOK(0x48DEC0, AIPlayAnim_Update);

void AIPlaySound_Update(HumanAI *ai, AIAction *a);

void AIPlaySound_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIPlaySound_Update;
}
HOOK(0x48DF70, AIPlaySound_Start);

void AIPlaySound_Update(HumanAI *ai, AIAction *a)
{
    SoundParam *p = a->param;
    Task *s;

    if (p->onSelf) {
        Sound_Play(ai->task.parent, p->name);
        HumanAI_EndAction(ai, a);
        return;
    }
    s = Sound_Play(Level_GetDynamic(), p->name);
    if (s)
        Sound_SetPos(s, &ai->task.pos);
    HumanAI_EndAction(ai, a);
}
HOOK(0x48DF80, AIPlaySound_Update);

void AIMoveToEvent_Update(HumanAI *ai, AIAction *a);

void AIMoveToEvent_Start(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;
    GraphNode *n;

    a->update = AIMoveToEvent_Update;
    AIGraph_FindNearest(&n, ai->graph, &p->pos);
    p->move.node = n->id;
}
HOOK(0x48DFE0, AIMoveToEvent_Start);

void AIMoveToEvent_Update(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;

    if (p->move.run)
        HumanAI_SetRun(ai);
    else
        HumanAI_SetWalk(ai);
    Debug_Log(ai, -1, "Move to event %d", p->move.node);
    if (HumanAI_RunTo(ai, p->move.node, &p->pos, 1))
        HumanAI_EndAction(ai, a);
}
HOOK(0x48E020, AIMoveToEvent_Update);

/* now - time * -30, i.e. time seconds from now */
static int ticks_after(float time)
{
    int now = Flow_GetFrames();

    return now - round_int(time * kMinusFps);
}

void AILookAtEvent_Update(HumanAI *ai, AIAction *a);

void AILookAtEvent_Start(HumanAI *ai, AIAction *a)
{
    a->update = AILookAtEvent_Update;
    ((PosParam *)a->param)->look.until = -1;
}
HOOK(0x48E080, AILookAtEvent_Start);

void AILookAtEvent_Update(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;

    Debug_Log(ai, -1, "Look at event, %d", p->look.until - Flow_GetFrames());
    if (p->look.until < 0)
        p->look.until = ticks_after(p->look.time);
    if (HumanAI_FaceTo(ai, &p->pos, kFaceTol, 0) && Flow_GetFrames() > p->look.until)
        HumanAI_EndAction(ai, a);
}
HOOK(0x48E0A0, AILookAtEvent_Update);

void AIStunned_Update(HumanAI *ai, AIAction *a);

void AIStunned_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIStunned_Update;
    ((TimedParam *)a->param)->until = -1;
}
HOOK(0x48E130, AIStunned_Start);

void AIStunned_Update(HumanAI *ai, AIAction *a)
{
    TimedParam *p = a->param;

    Debug_Log(ai, -1, "Stunned");
    if (p->until < 0)
        p->until = ticks_after(p->time);
    if (Flow_GetFrames() > p->until) {
        HumanAI_EndAction(ai, a);
        return;
    }
    if (!HumanAI_InPose(ai, POSE_ANIM))
        HumanAI_SetChannel(ai, 3, 38.0f);
}
HOOK(0x48E150, AIStunned_Update);

void AIKickGrenade_Update(HumanAI *ai, AIAction *a);
void AIKickGrenade_End(HumanAI *ai, AIAction *a);

void AIKickGrenade_Start(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;
    GraphNode *n;

    a->update = AIKickGrenade_Update;
    a->end = AIKickGrenade_End;
    AIGraph_FindNearest(&n, ai->graph, &p->pos);
    p->move.run = 0;
    p->move.node = n->id;
}
HOOK(0x48E1E0, AIKickGrenade_Start);

void AIKickGrenade_End(HumanAI *ai, AIAction *a)
{
    Task **ref = ((PosParam *)a->param)->move.ref;

    if (ref)
        TaskRef_Remove(ref);
}
HOOK(0x48E230, AIKickGrenade_End);

/* never ends by itself */
void AIKickGrenade_Update(HumanAI *ai, AIAction *a)
{
    Debug_Log(ai, -1, "Kick grenade");
}
HOOK(0x48E250, AIKickGrenade_Update);

void AIRunPanicking_Update(HumanAI *ai, AIAction *a);
char AIGraph_Search(HumanAI *ai, GraphNode *start, int max, AIGraphFn fn, void *ctx);
char AIRunPanicking_Check(HumanAI *ai, GraphNode *node, PanicData *d);

void AIRunPanicking_Start(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;
    GraphNode *n;

    a->update = AIRunPanicking_Update;
    p->panic.node = -1;
    if (!(Vec3d_Distance(&p->pos, &ai->task.pos) <= p->panic.radius * kUnit))
        return;
    if (ai->graph->unk5C >= 0x400) {
        Warning_Show("RunPanicking stack not large enough");
        return;
    }
    AIGraph_FindNearest(&n, ai->graph, &p->pos);
    if (n)
        AIGraph_Search(ai, n, 10, (AIGraphFn)AIRunPanicking_Check, (PanicData *)&p->pos);
}
HOOK(0x48E270, AIRunPanicking_Start);

/* breadth first walk of the AI graph from start, until fn returns 1 or
   max more nodes were queued */
char AIGraph_Search(HumanAI *ai, GraphNode *start, int max, AIGraphFn fn, void *ctx)
{
    char seen[0x400];
    int queue[0x400];
    int *q, *tail;
    int added = 0, i;

    if (!start || !fn)
        return 0;
    memset(seen, 0, sizeof(seen));
    queue[0] = start->id;
    queue[1] = -1;
    if (queue[0] == -1)
        return 0;
    seen[queue[0]] = 1;
    tail = &queue[1];
    for (q = queue; *q != -1; q++) {
        GraphNode *n = Graph_GetNode(ai->graph, *q);

        if (fn(ai, n, ctx))
            return 1;
        for (i = 0; i < 64 && added < max; i++) {
            GraphLink *l = n->links[i];
            int other;

            if (!l)
                continue;
            other = l->a;
            if (*q == other)
                other = l->b;
            if (seen[other])
                continue;
            seen[other] = 1;
            *tail++ = other;
            added++;
            *tail = -1;
        }
    }
    return 0;
}
HOOK(0x48E300, AIGraph_Search);

/* a node outside the panic radius the AI can use */
char AIRunPanicking_Check(HumanAI *ai, GraphNode *node, PanicData *d)
{
    Vec3d pos;

    Graph_GetNodePos(&pos, node);
    if (!(Vec3d_Distance(&d->pos, &pos) <= d->radius * kUnit) && AICombat_TryNode(ai, node, 0, 0)) {
        d->node = node->id;
        return 1;
    }
    return 0;
}
HOOK(0x48E430, AIRunPanicking_Check);

void AIRunPanicking_Update(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;

    Debug_Log(ai, -1, "Run panicking");
    if (p->panic.node != -1) {
        HumanAI_SetChannel(ai, 5, 1.0f);
        if (!HumanAI_RunTo(ai, p->panic.node, &ai->unk3EB0, 0))
            return;
        ai->unk3DE8 = -1;
    }
    HumanAI_EndAction(ai, a);
}
HOOK(0x48E4A0, AIRunPanicking_Update);

void AIIdle_Update(HumanAI *ai, AIAction *a);

void AIIdle_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIIdle_Update;
}
HOOK(0x48E510, AIIdle_Start);

void AIIdle_Update(HumanAI *ai, AIAction *a)
{
    Debug_Log(ai, -1, "Idle");
    HumanAI_LevelAim(ai);
}
HOOK(0x48E520, AIIdle_Update);

void AISetCombat_Update(HumanAI *ai, AIAction *a);
void AISetCombat_End(HumanAI *ai, AIAction *a);

void AISetCombat_Start(HumanAI *ai, AIAction *a)
{
    a->update = AISetCombat_Update;
    a->end = AISetCombat_End;
}
HOOK(0x48E540, AISetCombat_Start);

/* sends message 7 (combat) about the event to the AI itself */
void AISetCombat_Update(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;
    Task *h = ai->task.parent;
    AIMessage m;

    Debug_Log(ai, -1, "Set Combat");
    m.type = 7;
    m.team = Human_GetTeam(h);
    m.flags = 0;
    m.radius = 0;
    m.pos = &p->pos;
    m.about = NULL;
    m.from = NULL;
    if (p->combat.about && *p->combat.about)
        m.about = *p->combat.about;
    if (p->combat.from && *p->combat.from)
        m.from = *p->combat.from;
    HumanAI_Send(ai, &m);
    HumanAI_EndAction(ai, a);
}
HOOK(0x48E560, AISetCombat_Update);

void AISetCombat_End(HumanAI *ai, AIAction *a)
{
    PosParam *p = a->param;

    if (p->combat.about)
        TaskRef_Remove(p->combat.about);
    if (p->combat.from)
        TaskRef_Remove(p->combat.from);
}
HOOK(0x48E5F0, AISetCombat_End);

void AIActivate_Update(HumanAI *ai, AIAction *a);
void AIActivate_End(HumanAI *ai, AIAction *a);
char AIActivate_Check(HumanAI *ai, GraphNode *node, int *out);

/* finds a node next to the task to use (and the alarm control when the
   task is the AI's alarm trigger), and claims them */
void AIActivate_Start(HumanAI *ai, AIAction *a)
{
    NodeParam *p = a->param;
    Task *control = NULL, *t;
    char alarm = 0;
    GraphNode *n;
    int found;

    a->update = AIActivate_Update;
    a->end = AIActivate_End;
    if (p->id == ai->alarmTriggerId) {
        control = Task_FindById(ai->alarmControlId);
        if (!control) {
            Warning_Show("Level QTask ID #%d not defined (AI Activate)", ai->alarmControlId);
            p->node1 = -1;
            return;
        }
        alarm = 1;
    }
    t = Task_FindById(p->id);
    if (!t) {
        Warning_Show("Level QTask ID #%d not defined (AI Activate)", p->id);
        p->node1 = -1;
        return;
    }
    p->node1 = -1;
    p->node2 = -1;
    if (AIClaim_IsClaimed(t))
        return;
    if (alarm && AIClaim_IsClaimed(control))
        return;
    if (!TaskType_IsA(t->type, RigidDynCubeObj_GetType()))
        return;
    AIGraph_FindNearest(&n, ai->graph, &t->pos);
    if (!n)
        return;
    found = -1;
    if (AIGraph_Search(ai, n, 10, (AIGraphFn)AIActivate_Check, &found)) {
        if (n->id == found) {
            p->node1 = found;
        } else {
            p->node1 = n->id;
            p->node2 = found;
        }
    }
    ai->claim = AIClaim_Set(t, 1);
    if (alarm)
        ai->alarmClaim = AIClaim_Set(control, 1);
}
HOOK(0x48E620, AIActivate_Start);

char AIActivate_Check(HumanAI *ai, GraphNode *node, int *out)
{
    if (AICombat_TryNode(ai, node, 0, 0)) {
        *out = node->id;
        return 1;
    }
    return 0;
}
HOOK(0x48E7A0, AIActivate_Check);

void AIActivate_Update(HumanAI *ai, AIAction *a)
{
    NodeParam *p = a->param;

    if (p->run)
        HumanAI_SetRun(ai);
    else
        HumanAI_SetWalk(ai);
    if (p->node1 != -1) {
        if (HumanAI_RunTo(ai, p->node1, NULL, 0)) {
            HumanAI_SetChannel(ai, 4, p->id);
            p->node1 = -1;
        }
        return;
    }
    if (p->node2 != -1) {
        if (!HumanAI_RunTo(ai, p->node2, NULL, 0))
            return;
        p->node2 = -1;
    }
    HumanAI_EndAction(ai, a);
}
HOOK(0x48E7D0, AIActivate_Update);

void AIActivate_End(HumanAI *ai, AIAction *a)
{
    if (ai->claim) {
        AIClaim_Release(ai->claim);
        ai->claim = NULL;
    }
    if (ai->alarmClaim) {
        AIClaim_Release(ai->alarmClaim);
        ai->alarmClaim = NULL;
    }
}
HOOK(0x48E870, AIActivate_End);

void AIPatrol_Update(HumanAI *ai, AIAction *a);
void AIPatrol_End(HumanAI *ai, AIAction *a);

void AIPatrol_Start(HumanAI *ai, AIAction *a)
{
    NodeParam *p = a->param;

    HumanAI_SetWalk(ai);
    a->update = AIPatrol_Update;
    a->end = AIPatrol_End;
    PatrolRoute_Init(&ai->routes[ai->routeCount++], p->id, p->node1);
}
HOOK(0x48E8C0, AIPatrol_Start);

/* runs the current command of the top patrol route */
void AIPatrol_Update(HumanAI *ai, AIAction *a)
{
    PatrolRoute *r = &ai->routes[ai->routeCount - 1];
    void *cmd = PatrolRoute_Current(r);
    int type, arg;
    Vec3d pos;

    if (r->crouch)
        HumanAI_SetChannel(ai, 1, 1.0f);
    if (r->walk)
        HumanAI_SetWalk(ai);
    else
        HumanAI_SetRun(ai);
    if (!cmd)
        return;
    type = PatrolPathCommand_GetType(cmd);
    arg = GameScreen_GetView(cmd);
    switch (type) {
    case 0:
        Debug_Log(ai, -1, "Patrolling, Standaction : %d", arg);
        if (r->timer == -1) {
            r->timer = 0;
            HumanAI_SetChannel(ai, 3, arg);
            return;
        }
        if (!HumanAI_InPose(ai, r->crouch != 0))
            return;
        break;
    case 1:
        if (r->timer == -1)
            r->timer = Flow_GetFrames() + arg;
        Debug_Log(ai, -1, "Patrolling, Delay : %d", r->timer - Flow_GetFrames());
        if (Flow_GetFrames() <= r->timer)
            return;
        break;
    case 2:
        r->crouch = 0;
        r->walk = 1;
        Debug_Log(ai, -1, "Patrolling, Walkto:%d, Cur:%d", arg, ai->node);
        if (!HumanAI_RunTo(ai, arg, NULL, 1))
            return;
        break;
    case 3:
        r->crouch = 0;
        r->walk = 0;
        Debug_Log(ai, -1, "Patrolling, Runto:%d, Cur:%d", arg, ai->node);
        if (!HumanAI_RunTo(ai, arg, NULL, 1))
            return;
        break;
    case 4:
        r->crouch = 1;
        break;
    case 5:
        Debug_Log(ai, -1, "Patrolling, LookAtNode : %d", arg);
        Graph_GetNodePos(&pos, Graph_GetNode(ai->graph, arg));
        if (!HumanAI_AimAt(ai, &pos, kLookTol, 0))
            return;
        break;
    case 7:
        HumanAI_EndAction(ai, a);
        return;
    default:
        return;
    }
    PatrolRoute_Advance(r);
    r->timer = -1;
}
HOOK(0x48E920, AIPatrol_Update);

void AIPatrol_End(HumanAI *ai, AIAction *a)
{
    HumanAI_PopRoute(ai);
}
HOOK(0x48EB30, AIPatrol_End);

void AIFallFlat_Update(HumanAI *ai, AIAction *a);

void AIFallFlat_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIFallFlat_Update;
    ((TimedParam *)a->param)->until = -1;
}
HOOK(0x48EB40, AIFallFlat_Start);

void AIFallFlat_Update(HumanAI *ai, AIAction *a)
{
    TimedParam *p = a->param;

    Debug_Log(ai, -1, "Fall flat - %d", p->until - Flow_GetFrames());
    HumanAI_SetChannel(ai, 2, 1.0f);
    if (p->until < 0)
        p->until = ticks_after(p->time);
    if (Flow_GetFrames() > p->until)
        HumanAI_EndAction(ai, a);
}
HOOK(0x48EB60, AIFallFlat_Update);

void AIWalkToNode_Update(HumanAI *ai, AIAction *a);

void AIWalkToNode_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIWalkToNode_Update;
}
HOOK(0x48EBE0, AIWalkToNode_Start);

void AIWalkToNode_Update(HumanAI *ai, AIAction *a)
{
    NodeParam *p = a->param;

    Debug_Log(ai, -1, "Walk To Node - %d", p->id);
    HumanAI_SetWalk(ai);
    ai->destNode = p->id;
    if (HumanAI_RunTo(ai, p->id, NULL, 1))
        HumanAI_EndAction(ai, a);
}
HOOK(0x48EBF0, AIWalkToNode_Update);

void AIRunToNode_Update(HumanAI *ai, AIAction *a);

void AIRunToNode_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIRunToNode_Update;
}
HOOK(0x48EC40, AIRunToNode_Start);

void AIRunToNode_Update(HumanAI *ai, AIAction *a)
{
    NodeParam *p = a->param;

    Debug_Log(ai, -1, "Run To Node - %d", p->id);
    HumanAI_SetRun(ai);
    ai->destNode = p->id;
    if (HumanAI_RunTo(ai, p->id, NULL, 1))
        HumanAI_EndAction(ai, a);
}
HOOK(0x48EC50, AIRunToNode_Update);

/* the shared part of FireAtNode / FireAtTask once aimTarget is set */
static void fire_at(HumanAI *ai, FireParam *p)
{
    if (!p->aimed) {
        AICombat_StartBurst(ai, 0);
        p->aimed = 1;
    }
    if (p->wait >= 0) {
        p->wait--;
        return;
    }
    if (AICombat_Fire(ai)) {
        p->aimed = 0;
        p->wait = p->interval;
    }
}

/* the gun, or NULL when the action is over; also handles an empty clip */
static Task *fire_gun(HumanAI *ai, AIAction *a, const char *msg, int *stop)
{
    FireParam *p = a->param;
    Task *gun = WeaponSlots_Get(((Human *)ai->task.parent)->weapons, 0);
    int ammo[2];

    *stop = 0;
    Debug_Log(ai, -1, msg, p->id, p->time);
    if (p->time-- < 0 || !gun) {
        HumanAI_EndAction(ai, a);
        *stop = 1;
        return NULL;
    }
    Weapon_GetAmmo(gun, ammo);
    if (ammo[0] == 0) {
        if (p->noReload) {
            ai->ctrl[CTRL_RELOAD] = 1.0f;
            *stop = 1;
            return NULL;
        }
        Weapon_UseAmmo(gun, 1);
        p->aimed = 0;
    }
    return gun;
}

void AIFireAtNode_Update(HumanAI *ai, AIAction *a);

void AIFireAtNode_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIFireAtNode_Update;
    ((FireParam *)a->param)->aimed = 0;
    ((FireParam *)a->param)->wait = -1;
}
HOOK(0x48ECA0, AIFireAtNode_Start);

void AIFireAtNode_Update(HumanAI *ai, AIAction *a)
{
    FireParam *p = a->param;
    int stop;

    fire_gun(ai, a, "Fire At Node - %d, %d", &stop);
    if (stop)
        return;
    Graph_GetNodePos(&ai->aimTarget, Graph_GetNode(ai->graph, p->id));
    fire_at(ai, p);
}
HOOK(0x48ECC0, AIFireAtNode_Update);

void AIFireAtTask_Update(HumanAI *ai, AIAction *a);

void AIFireAtTask_Start(HumanAI *ai, AIAction *a)
{
    a->update = AIFireAtTask_Update;
    ((FireParam *)a->param)->aimed = 0;
    ((FireParam *)a->param)->wait = -1;
}
HOOK(0x48EDE0, AIFireAtTask_Start);

void AIFireAtTask_Update(HumanAI *ai, AIAction *a)
{
    FireParam *p = a->param;
    Task *t;
    int stop;

    fire_gun(ai, a, "Fire At Task - %d, %d", &stop);
    if (stop)
        return;
    t = Task_FindById(p->id);
    if (!t) {
        Warning_Show("HumanAI - FireAtTask, task id #%d not defined", p->id);
        HumanAI_EndAction(ai, a);
        return;
    }
    if (!TaskType_IsA(t->type, gQObjType)) {
        Warning_Show("FireAtTask target not based on qobj tasktype");
        HumanAI_EndAction(ai, a);
        return;
    }
    memcpy(&ai->aimTarget, &t->pos, sizeof(Vec3d));
    fire_at(ai, p);
}
HOOK(0x48EE00, AIFireAtTask_Update);

/* sets an input of the human through the AI type's channel table; channels
   the type lacks only format a message nobody prints */
void HumanAI_SetChannel(HumanAI *ai, int channel, float value)
{
    char buf[256];
    int i = gAIChannelMap[ai->aiType][channel];

    if (i == -1) {
        crt_sprintf(buf, "AI(%d) type(%s) channel(%d)", Task_GetId(ai), ai->typeName, channel);
        return;
    }
    ai->ctrl[i] = value;
}
HOOK(0x48EF70, HumanAI_SetChannel);

/* is the human in the state the AI type uses for this pose */
char HumanAI_InPose(HumanAI *ai, int pose)
{
    int state = Human_GetState(ai->task.parent);
    int want = gAIPoseMap[ai->aiType][pose];

    if (want == -1)
        return 0;
    return state == want;
}
HOOK(0x48EFE0, HumanAI_InPose);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD_CONST(igd, 54151C, 54151C, 0x13C) INIT({0x8C, 0x13, 0x54, 0x00, 0x04, 0x14, 0x54, 0x00, 0x2C, 0x14, 0x54, 0x00, 0x54, 0x14, 0x54, 0x00, 0x54, 0x14, 0x54, 0x00, 0x7C, 0x14, 0x54, 0x00, 0xA4, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xB4, 0x13, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xDC, 0x13, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xCC, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0xF4, 0x14, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x0E, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x23, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x23, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0B, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x23, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF});
ALIAS(gAIChannelMap, pad_54151C, 0);
PAD_CONST(igd, 54151C, 541658, 0x74) INIT({0x90, 0x15, 0x54, 0x00, 0xCC, 0x15, 0x54, 0x00, 0xE0, 0x15, 0x54, 0x00, 0xF4, 0x15, 0x54, 0x00, 0xF4, 0x15, 0x54, 0x00, 0x08, 0x16, 0x54, 0x00, 0x1C, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0xA4, 0x15, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0xB8, 0x15, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x30, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54, 0x00, 0x44, 0x16, 0x54});
ALIAS(gAIPoseMap, pad_541658, 0);
