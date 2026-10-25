/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "humanplayer/humanplayer.h"
#include "qasm/qasmtypes.h"
#include "game/gametypes.h"
#include "task/taskvars.h"
#include "config/configvars.h"

FN_EXTERN(HumanPlayer_BlastHandler);
FN_EXTERN(HumanPlayer_FenceHandler);
FN_EXTERN(HumanPlayer_FlashbangHandler);
FN_EXTERN(HumanPlayer_HitHandler);
FN_EXTERN(HumanPlayer_InputChannelHandler);
FN_EXTERN(HumanPlayer_PunchHandler);
FN_EXTERN(HumanPlayer_ReadInputHandler);
FN_EXTERN(HumanPlayer_RenderHandler);
FN_EXTERN(Human_DeadRun);
FN_EXTERN(Human_RunBase);
FN_EXTERN(HumanPlayer_MapMarkerHandler);

typedef void (*TaskHandler)(void *task, void *arg);
typedef void (*TaskHandler1)(void *task);

typedef struct PositionMsg {
    HumanPlayer *h;
    int unk4;
    double pos[3];
} PositionMsg;

typedef struct AmmoLimit {
    int id;
    int limit;
} AmmoLimit;

typedef struct CameraTask {
    char unk0[0x38];
    float m[10];
} CameraTask;

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401FA0, void, TaskType_SetFlags, (uint16_t type, uint32_t flags));
ORIG(0x408420, void, HumanPlayer_CheckDeath, (HumanPlayer *h));
ORIG(0x408460, void, HumanPlayer_UpdateFall, (HumanPlayer *h));
ORIG(0x408720, int, Weapon_HasAmmo, (Task *weapon));
ORIG(0x40C140, void, HumanPlayer_SetFlagD44, (HumanPlayer *h));
ORIG(0x40C2D0, void, HumanPlayer_ClearFlagD44, (HumanPlayer *h));
ORIG(0x40CD30, void, HumanPlayer_StartAction, (HumanPlayer *h, int arg));
ORIG(0x411000, void, HumanPlayer_SelectWeaponCategory, (HumanPlayer *h));
ORIG(0x411240, void, HumanPlayer_UpdateEarRinging, (HumanPlayer *h));
ORIG(0x411300, void, HumanPlayer_FadeHitFlash, (HumanPlayer *h));
ORIG(0x411350, void, HumanPlayer_FadeFlash, (HumanPlayer *h));
ORIG(0x4113C0, void, HumanPlayer_UpdateFenceTimer, (HumanPlayer *h));
ORIG(0x4113E0, void, HumanPlayer_UpdateLadder, (HumanPlayer *h));
ORIG(0x411770, void, HumanPlayer_UpdateFenceClimb, (HumanPlayer *h));
ORIG(0x411AE0, void, HumanPlayer_UpdateWire, (HumanPlayer *h));
ORIG(0x411F20, void, HumanPlayer_UpdateZipline, (HumanPlayer *h));
ORIG(0x4137E0, void, HumanPlayer_SetWeaponAnim, (HumanPlayer *h, int anim));
ORIG(0x413A60, void, HumanPlayer_RequestWeaponAnim, (HumanPlayer *h, int anim));
ORIG(0x413C80, WeaponType *, Weapon_GetType, (Task *weapon));
ORIG(0x413CC0, Task *, Task_FindWeapon, (HumanPlayer *h, int id));
ORIG(0x415020, int, Game_CheatKeyPressed, (int key));
ORIG(0x4167E0, uint8_t, Game_GetEvent1, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x4168C0, QTaskList *, Game_GetTaskList2, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416D40, Game *, Game_Get, (void));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460C10, uint8_t, Human_GetEvent1, (void));
ORIG(0x460C80, void, Human_printf, (HumanPlayer *h, const char *fmt, ...));
ORIG(0x460CF0, void, Human_UpdatePlatformMove, (HumanPlayer *h));
ORIG(0x460EE0, void, Human_UpdateUse, (HumanPlayer *h));
ORIG(0x4610C0, void, Human_UpdateBody, (HumanPlayer *h));
ORIG(0x4613D0, void, Human_FireAnimTrigger, (HumanPlayer *h));
ORIG(0x4614B0, void, Human_UpdateMove, (HumanPlayer *h));
ORIG(0x462CF0, void, Human_MoveOnPlatform, (HumanPlayer *h));
ORIG(0x462E50, void, Human_UpdateWeapon, (HumanPlayer *h));
ORIG(0x463080, void, Human_SendPosition, (HumanPlayer *h));
ORIG(0x463170, char, Human_IsDead, (HumanPlayer *h));
ORIG(0x4632D0, void, Human_LeaveGround, (HumanPlayer *h));
ORIG(0x4635F0, void, Human_FreezeView, (HumanPlayer *h));
ORIG(0x463600, void, Human_UnfreezeView, (HumanPlayer *h));
ORIG(0x463610, void, Human_BeginUse, (HumanPlayer *h));
ORIG(0x463620, void, Human_EndUse, (HumanPlayer *h));
ORIG(0x463680, int, Human_GetGround, (HumanPlayer *h));
ORIG(0x463740, Task *, Human_GetAttached, (HumanPlayer *h));
ORIG(0x463760, void, HumanViewCam, (HumanPlayer *h, int mode));
ORIG(0x4638A0, void, Human_Say, (HumanPlayer *h, const char *name, int n));
ORIG(0x463920, void, Human_Footstep, (HumanPlayer *h));
ORIG(0x464660, void, HumanView_AddToWorld, (HumanView *v));
ORIG(0x464700, void, HumanView_LockPitch, (HumanView *v));
ORIG(0x464710, void, HumanView_UnlockPitch, (HumanView *v));
ORIG(0x464770, void, HumanView_SetWeaponTarget, (HumanView *v, float a, float b, float c, float d));
ORIG(0x4647A0, void, HumanView_SetWeaponAngles, (HumanView *v, float a, float b));
ORIG(0x4647D0, char, HumanView_WeaponAtTarget, (HumanView *v));
ORIG(0x464800, void, HumanView_UpdateView, (HumanView *v, HumanPlayer *h));
ORIG(0x464BD0, void, HumanView_UpdateBody, (HumanView *v, HumanPlayer *h));
ORIG(0x465880, void, HumanView_GetMatrix, (float *m, HumanView *v));
ORIG(0x4659E0, void, HumanTaskViewReset, (HumanView *v));
ORIG(0x46C020, Task *, Computer_New, (HumanPlayer *h));
ORIG(0x46C050, void, Computer_Delete, (void));
ORIG(0x46C060, Task *, Computer_Get, (void));
ORIG(0x47CDA0, int, Script_EvalConst, (const char *expr));
ORIG(0x47CE70, void, AmmoList_SetLimit, (int *list, int id, int limit));
ORIG(0x47CEE0, AmmoEntry *, AmmoList_Find, (int *list, int id));
ORIG(0x489C30, void, Human_InitDamage, (HumanPlayer *h, void *speeds));
ORIG(0x489E10, HitAnims *, HumanDamage_GetZone, (HumanDamage *damage, int zone));
ORIG(0x489E70, void, Human_PlayAnimSlot, (HumanPlayer *h, int slot, float blend));
ORIG(0x489EA0, void, Human_PlayAnim, (HumanPlayer *h, int anim, int flag, float blend));
ORIG(0x48A330, void, Human_DisableLook, (HumanPlayer *h));
ORIG(0x48F360, void, AppMain_ParseCmdLineArgs, (const char *name, void (*fn)(void)));
ORIG(0x4B2D00, void, Vec3_ToAngles, (float *v, float *pitch, float *yaw));
ORIG(0x4B47C0, unsigned int, Random_Int, (void *r, unsigned int n));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C1790, void, QTaskList_Add, (QTaskList *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (QTaskList *list, void *item));
ORIG(0x4C1800, QTaskList *, QTaskList_New, (int max));
ORIG(0x4C6940, void, DynCube_Move, (void *world, HumanPlayer *h, double *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, HumanPlayer *h, double *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, HumanPlayer *h));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (void *a, float *delta, void *anim));
ORIG(0x4D38C0, void, BoneObj_Update, (float *matrix, void *anim));
ORIG(0x4D61D0, void, Anim_Play, (void *anim, int track, int id, int flag, float blend));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4D9620, void, BoneDynCubeObj_Init, (void *world, HumanPlayer *h, int a, int b));
ORIG(0x4D96F0, void, BoneDynCubeObj_Deinit, (HumanPlayer *h));
ORIG(0x4E6B00, int, Sound_Play, (HumanView *v, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (int sound));
ORIG(0x4E7BD0, void, Debug_printf, (const char *fmt, ...));
ORIG(0x4EC190, void, StateMachine_Init, (HumanPlayerSM *sm, StateFn update, StateFn *states, int state,
                                         int combo_count, void *combos));
ORIG(0x4EC1E0, void, StateMachine_Update, (HumanPlayerSM *sm, HumanPlayer *h));
ORIG(0x4EE140, char *, PhysicsObjType_Find, (void *p));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *p, int n));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F2030, int, Weapon_GetDeselectEvent, (void));
ORIG(0x4F2060, Task *, WeaponSlots_Get, (Task **slots, int i));
ORIG(0x4F2070, void, WeaponSlots_Set, (Task **slots, int i, Task *weapon));
ORIG(0x4F2290, void, Weapon_DeleteAll, (HumanPlayer *h));
ORIG(0x4F2350, void, Weapon_UseAmmo, (Task *weapon, char unlimited));
ORIG(0x4F2390, void, Weapon_Drop, (Task *weapon));
ORIG(0x4F23D0, void, Weapon_GetAmmo, (Task *weapon, WeaponAmmo *out));
ORIG(0x4F40D0, uint8_t, Vehicle_GetControlEvent, (void));
ORIG(0x4F40E0, uint8_t, Vehicle_GetInputInfoEvent, (void));
ORIG(0x4F4140, uint8_t, Vehicle_GetKeysEvent, (void));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416870, uint8_t, Game_GetEvent12, (void));
ORIG(0x460C40, uint8_t, Human_GetPunchEvent, (void));
ORIG(0x460C50, void *, Human_GetParamTable, (void));
ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));
ORIG(0x47E990, uint8_t, FlashBang_GetEvent, (void));
ORIG(0x487190, uint8_t, StatusScreen_GetEvent, (void));

/* handlers registered by HumanPlayer_Open that live outside this file */
#define Human_RunBase (FN(0x454EE0, Human_RunBase))
#define HumanPlayer_MapMarkerHandler (FN(0x413730, HumanPlayer_MapMarkerHandler))
#define HumanPlayer_DeadRun ((StateFn)FN(0x45C740, Human_DeadRun))

#define TASK_HANDLER(slot, type) (((void **)gTaskHandlers)[(slot) * 0x180 + (type)])

extern uint16_t gHumanPlayerType;
extern StateFn gBodyStates[];
extern StateFn gWeaponStates[];
extern char gBodyCombos[];
extern char gAnimSlots[];
extern float gMoveSpeeds[];
extern float gTurnSpeeds[];
extern int gWeaponCycleCount;
extern int gWeaponCycle[];
extern char gShowPosition;
extern int gAmmoLimitCount;
extern AmmoLimit gAmmoLimits[];
extern void *gHumanPlayerParams;
extern WeaponCategory gWeaponCategories[];
extern float gMoveScale;
extern float gJumpSpeed;
extern float gJumpUp;
extern float gHumanAirSpeed;
extern float gLeanSide;
extern float gLeanUp;
extern int gLeanFrames;
extern float gMaxDamage;
extern float gFenceDamage;
extern QTaskList *gHumanPlayerList;
extern char gUnlimitedAmmo;

extern float kKmhToUnits;
extern float kPerFrameHour;
extern double kFlySpeedD;
extern const float kTwoPi;
extern float kRadToDeg;
extern double kUnitsToMeters;
extern float kUnitsToMetersF;
extern float kFps;

#define BLEND 0x1.99999ap-3f
#define WEAPON_DOWN_X -0x1.38c354p-1f
#define WEAPON_DOWN_Y -0x1.657186p-2f
#define WEAPON_SPEED_X 0x1.dc975cp-5f
#define WEAPON_SPEED_Y 0x1.10567ep-5f

enum {
    BODY_LEAN = 0x2C,
    BODY_COMPUTER,
    BODY_HIT,
    BODY_DEATH,
    BODY_FLY,
};

enum {
    WEAPON_DISABLED,
    WEAPON_IDLE,
    WEAPON_FIRE,
    WEAPON_RELOAD,
    WEAPON_SWITCH,
    WEAPON_LEAN,
    WEAPON_ALT_FIRE,
    WEAPON_THROW,
    WEAPON_PLACE,
    WEAPON_MELEE,
    WEAPON_MEDIPACK,
    WEAPON_COMPUTER,
    WEAPON_DRY_FIRE,
};

enum {
    WEAPON_KIND_MELEE = 3,
    WEAPON_KIND_GRENADE,
    WEAPON_KIND_GUN,
    WEAPON_KIND_MEDIPACK,
    WEAPON_KIND_MINE,
};

static inline int pressed(HumanPlayer *h, int i)
{
    return h->input[i] != 0 && h->prevInput[i] == 0;
}

static inline void sm_set(HumanPlayerSM *sm, int state, HumanPlayer *h)
{
    sm->prev = sm->state;
    sm->state = state;
    sm->run = sm->states[state];
    sm->run(sm, h);
}

static inline Task *current_weapon(HumanPlayer *h)
{
    return WeaponSlots_Get(h->weapons, 0);
}

void HumanPlayer_BodyLeanRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_BodyComputerRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_BodyHitRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_BodyDeathRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_BodyFlyRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponDisabledRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponIdleRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponFireRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponDryFireRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponReloadRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponLowerRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponRaiseRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponLeanRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponThrowRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponReleaseRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_CycleWeapon(HumanPlayer *h, int force);
void HumanPlayer_WeaponPlaceRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponRecoverRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponMeleeRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponMedipackStartRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponMedipackRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponWaitRun(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_WeaponComputerRun(HumanPlayerSM *sm, HumanPlayer *h);
ScriptValue *DefineHumanPlayerGeneral(ScriptValue *ret, void *unused, void *args);
ScriptValue *DefineHumanPlayerWeaponCycle(ScriptValue *ret, void *unused, int *args);
ScriptValue *DefineHumanPlayerWeaponCategory(ScriptValue *ret, void *unused, void *args);
ScriptValue *DefineHumanPlayerAmmoLimit(ScriptValue *ret, void *unused, int *args);
void HumanPlayer_SetUnlimitedAmmo(void);
void HumanPlayer_CreateHandler(HumanPlayer *h, void *arg);
void HumanPlayer_UpdateBodyInput(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_UpdateWeaponInput(HumanPlayerSM *sm, HumanPlayer *h);
void HumanPlayer_DeleteHandler(HumanPlayer *h);
void HumanPlayer_GetParamTableHandler(HumanPlayer *h, void **out);
void HumanPlayer_ParamChangedHandler(HumanPlayer *h, void *arg);
void HumanPlayer_DeactivateHandler(HumanPlayer *h, void *arg);
void HumanPlayer_InputHandler(HumanPlayer *h, float **msg);
void HumanPlayer_Update(HumanPlayer *h);
void HumanPlayer_SendPosition(HumanPlayer *h);

/* moves the view offset in lean[i] towards target at lean_speed[i] per frame */
static void lean_towards(float *pos, float speed, float target)
{
    long double t;

    if (*pos < target) {
        t = (long double)speed + *pos;
        *pos = t;
        if (t > target)
            *pos = target;
    } else if (*pos > target) {
        t = (long double)*pos - speed;
        *pos = t;
        if (t < target)
            *pos = target;
    }
}

void HumanPlayer_BodyLeanEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    if (h->crouch)
        Human_PlayAnimSlot(h, 0x13, BLEND);
    else
        Human_PlayAnimSlot(h, sm->state, BLEND);
    sm->run = HumanPlayer_BodyLeanRun;
}
HOOK(0x40DF70, HumanPlayer_BodyLeanEnter);

void HumanPlayer_BodyLeanRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;

    (void)sm;
    if (h->input[INPUT_LEFT] != 0) {
        if (h->prevInput[INPUT_LEFT] == 0)
            v->leanSpeed[0] = gLeanSide / (long double)gLeanFrames;
        lean_towards(&v->lean[0], v->leanSpeed[0], -gLeanSide);
    }
    if (h->input[INPUT_RIGHT] != 0) {
        if (h->prevInput[INPUT_RIGHT] == 0)
            v->leanSpeed[0] = gLeanSide / (long double)gLeanFrames;
        lean_towards(&v->lean[0], v->leanSpeed[0], gLeanSide);
    }
    if (h->input[INPUT_LEFT] == 0 && h->input[INPUT_RIGHT] == 0) {
        if (h->prevInput[INPUT_LEFT] != 0 || h->prevInput[INPUT_RIGHT] != 0)
            v->leanSpeed[0] = gLeanSide / (long double)gLeanFrames;
        lean_towards(&v->lean[0], v->leanSpeed[0], 0);
    }
    if (h->crouch) {
        if (h->input[INPUT_FORWARD] != 0) {
            if (h->prevInput[INPUT_FORWARD] == 0)
                v->leanSpeed[2] = gLeanUp / (long double)gLeanFrames;
            lean_towards(&v->lean[2], v->leanSpeed[2], gLeanUp);
        } else {
            if (h->prevInput[INPUT_FORWARD] != 0)
                v->leanSpeed[2] = gLeanUp / (long double)gLeanFrames;
            lean_towards(&v->lean[2], v->leanSpeed[2], 0);
        }
    }
}
HOOK(0x40DFC0, HumanPlayer_BodyLeanRun);

void HumanPlayer_BodyComputerEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    h->crouch = 1;
    Human_PlayAnimSlot(h, sm->state, BLEND);
    HumanPlayer_RequestWeaponAnim(h, 4);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Computer_New(h);
    sm->run = HumanPlayer_BodyComputerRun;
}
HOOK(0x40E370, HumanPlayer_BodyComputerEnter);

void HumanPlayer_BodyComputerRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    uint32_t in = sm->input;

    if (!pressed(h, INPUT_MAP) && !(in & SM_DEAD))
        return;
    Computer_Delete();
    HumanView_UnlockPitch(h->view);
    Human_UnfreezeView(h);
    h->push[0] = 0;
    h->push[1] = 0;
    h->push[2] = 0;
    if (in & SM_DEAD)
        sm_set(sm, BODY_DEATH, h);
    else
        sm_set(sm, 0, h);
}
HOOK(0x40E3C0, HumanPlayer_BodyComputerRun);

void HumanPlayer_BodyHitEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HitAnims *a;

    h->crouch = 0;
    a = HumanDamage_GetZone(&h->damage, h->damage.hitZone);
    Anim_Play(h->anim, 0, a->hit, 0, BLEND);
    HumanPlayer_SetWeaponAnim(h, 0);
    HumanPlayer_SetFlagD44(h);
    sm->run = HumanPlayer_BodyHitRun;
}
HOOK(0x40E470, HumanPlayer_BodyHitEnter);

void HumanPlayer_BodyHitRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    uint32_t in = sm->input;

    if (in & SM_DEAD) {
        sm_set(sm, BODY_DEATH, h);
    } else if (in & SM_HIT) {
        sm_set(sm, BODY_HIT, h);
    } else if (in & SM_ANIM_DONE) {
        HumanPlayer_ClearFlagD44(h);
        sm_set(sm, 0, h);
    }
}
HOOK(0x40E4D0, HumanPlayer_BodyHitRun);

void HumanPlayer_BodyDeathEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HitAnims *a;
    unsigned int r;

    h->crouch = 0;
    a = HumanDamage_GetZone(&h->damage, h->damage.hitZone);
    r = Random_Int(Game_GetRandom(), 2);
    Anim_Play(h->anim, 0, a->death[r], 0, BLEND);
    HumanPlayer_SetWeaponAnim(h, 0);
    Human_DisableLook(h);
    HumanPlayer_SetFlagD44(h);
    HumanViewCam(h, 5);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_Say(h, "player_death_", 3);
    memcpy(h->vel, h->outVel, sizeof(h->vel));
    sm->run = HumanPlayer_BodyDeathRun;
}
HOOK(0x40E560, HumanPlayer_BodyDeathEnter);

void HumanPlayer_BodyDeathRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    uint32_t in;
    Task *w;

    HumanPlayer_UpdateFall(h);
    in = sm->input;
    if (!(in & 4))
        return;
    memset(h->vel, 0, sizeof(h->vel));
    memset(h->push, 0, sizeof(h->push));
    if (!(in & SM_ANIM_DONE))
        return;
    w = WeaponSlots_Get(h->weapons, 0);
    if (w) {
        Weapon_Drop(w);
        WeaponSlots_Set(h->weapons, 0, NULL);
        sm_set(&h->weapon, WEAPON_IDLE, h);
    }
    Weapon_DeleteAll(h);
    sm->run = HumanPlayer_DeadRun;
}
HOOK(0x40E620, HumanPlayer_BodyDeathRun);

void HumanPlayer_BodyFlyEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    h->crouch = 0;
    Human_PlayAnimSlot(h, sm->state, BLEND);
    memset(h->vel, 0, sizeof(h->vel));
    memset(h->push, 0, sizeof(h->push));
    Human_LeaveGround(h);
    sm->run = HumanPlayer_BodyFlyRun;
}
HOOK(0x40E6E0, HumanPlayer_BodyFlyEnter);

void HumanPlayer_BodyFlyRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    float m[9];
    double pos[3];
    long double x = 0, k, speed;
    float y = 0, z = 0, fspeed;
    uint32_t in;

    HumanView_GetMatrix(m, h->view);
    in = sm->input;
    if (in & 0x80) {
        x = m[1];
        y = m[4];
        z = m[7];
    }
    if (in & 0x100) {
        x = x + -m[1];
        y = -m[4] + y;
        z = -m[7] + z;
    }
    if (in & 0x40) {
        x = x + m[0];
        y = m[3] + y;
        z = m[6] + z;
    }
    if (in & 0x20) {
        x = x + -m[0];
        y = -m[3] + y;
        z = -m[6] + z;
    }
    speed = (in & 0x7000) ? 10.0f : 1.0f;
    if (x == 0 && y == 0 && z == 0)
        return;
    memcpy(pos, h->pos, sizeof(pos));
    if (x != 0 || y != 0 || z != 0) {
        k = 1.0 / sqrtl(x * x + y * y + z * z);
        x = x * k;
        y = y * k;
        z = z * k;
    }
    speed = speed * kFlySpeedD;
    fspeed = speed;
    pos[0] = x * speed + pos[0];
    y = fspeed * y;
    z = fspeed * z;
    pos[1] = y + pos[1];
    pos[2] = z + pos[2];
    DynCube_Move(Game_GetWorld(), h, pos, h->radius);
}
HOOK(0x40E740, HumanPlayer_BodyFlyRun);

void HumanPlayer_WeaponDisabledEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 0);
    sm->run = HumanPlayer_WeaponDisabledRun;
}
HOOK(0x40E960, HumanPlayer_WeaponDisabledEnter);

void HumanPlayer_WeaponDisabledRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    if (!h->weaponLock)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40E980, HumanPlayer_WeaponDisabledRun);

void HumanPlayer_WeaponIdleEnter(HumanPlayerSM *sm)
{
    sm->run = HumanPlayer_WeaponIdleRun;
}
HOOK(0x40E9B0, HumanPlayer_WeaponIdleEnter);

void HumanPlayer_WeaponIdleRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    Task *w, *cur, *b, *att;
    WeaponType *info;
    WeaponAmmo ammo;
    AmmoEntry *a;
    TaskHandler fn;
    uint32_t in;
    uint16_t type;
    int arg;

    if (h->weaponAnim != h->weaponAnimReq)
        HumanPlayer_SetWeaponAnim(h, h->weaponAnimReq);
    w = WeaponSlots_Get(h->weapons, 0);
    in = sm->input;
    if (h->weaponLock) {
        HumanTaskViewReset(h->view);
        sm_set(sm, WEAPON_DISABLED, h);
        return;
    }
    if (pressed(h, INPUT_BINOCULARS)) {
        cur = WeaponSlots_Get(h->weapons, 0);
        b = Task_FindWeapon(h, Script_EvalConst("WEAPON_ID_BINOCULARS"));
        if (b) {
            if (b == cur) {
                ((TaskHandler1)TASK_HANDLER(Weapon_GetDeselectEvent(), b->type))(b);
                h->nextWeapon = Weapon_GetType(h->savedWeapon)->id;
                h->savedWeapon = NULL;
            } else {
                WeaponSlots_Set(h->weapons, 0, b);
                h->savedWeapon = cur;
            }
            h->input[INPUT_FIRE] = 1.0f;
        }
    }
    if (h->savedWeapon)
        return;
    if (w) {
        info = Weapon_GetType(w);
        Weapon_GetAmmo(w, &ammo);
        if (in & 0x200) {
            switch (info->category) {
            case WEAPON_KIND_MELEE:
                sm_set(sm, WEAPON_MELEE, h);
                return;
            case WEAPON_KIND_GRENADE:
                if (ammo.clip) {
                    sm_set(sm, WEAPON_THROW, h);
                    return;
                }
                break;
            case WEAPON_KIND_MEDIPACK:
                a = AmmoList_Find(h->ammo, Script_EvalConst("AMMO_ID_MEDIPACK"));
                if (a && a->count && h->damage.total != 0) {
                    sm_set(sm, WEAPON_MEDIPACK, h);
                    return;
                }
                break;
            case WEAPON_KIND_MINE:
                if (ammo.clip) {
                    sm_set(sm, WEAPON_PLACE, h);
                    return;
                }
                break;
            default:
                sm_set(sm, ammo.clip ? WEAPON_FIRE : WEAPON_DRY_FIRE, h);
                return;
            }
        } else if (in & 0x400) {
            if (info->id == Script_EvalConst("WEAPON_ID_M16A2")) {
                if (!gUnlimitedAmmo) {
                    a = AmmoList_Find(h->ammo, Script_EvalConst("AMMO_ID_M203"));
                    if (!a || !a->count)
                        goto no_action;
                    a->count--;
                }
                sm_set(sm, WEAPON_ALT_FIRE, h);
                return;
            }
            if (info->id == Script_EvalConst("WEAPON_ID_PROXIMITYMINE")) {
                h->altFire = 1;
                sm_set(sm, WEAPON_PLACE, h);
                return;
            }
        }
no_action:
        if (((in & 0x800) || !ammo.clip) && (ammo.reserve || gUnlimitedAmmo) && ammo.clip < info->clipSize) {
            HumanTaskViewReset(h->view);
            sm_set(sm, WEAPON_RELOAD, h);
            return;
        }
        if (h->input[INPUT_LEAN] != 0 && (h->sm.input & 4)) {
            HumanTaskViewReset(h->view);
            sm_set(sm, WEAPON_LEAN, h);
            return;
        }
    }
    if (h->nextWeapon != -1) {
        HumanTaskViewReset(h->view);
        sm_set(sm, WEAPON_SWITCH, h);
        return;
    }
    if (h->unk7B9 && h->input[INPUT_USE] == 0)
        h->unk7B9 = 0;
    if (Human_GetAttached(h) && !h->unk7B9 && h->input[INPUT_USE] != 0) {
        arg = 0;
        type = Human_GetAttached(h)->type;
        fn = (TaskHandler)TASK_HANDLER(Game_GetEvent3(), type);
        att = Human_GetAttached(h);
        fn(att, &arg);
        HumanTaskViewReset(h->view);
        HumanPlayer_StartAction(h, arg);
        return;
    }
    if (pressed(h, INPUT_MAP) && (h->sm.input & 4)) {
        HumanTaskViewReset(h->view);
        sm_set(sm, WEAPON_COMPUTER, h);
    }
}
HOOK(0x40E9C0, HumanPlayer_WeaponIdleRun);

void HumanPlayer_WeaponFireEnter(HumanPlayerSM *sm)
{
    sm->run = HumanPlayer_WeaponFireRun;
}
HOOK(0x40EF60, HumanPlayer_WeaponFireEnter);

void HumanPlayer_WeaponFireRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    Task *w = WeaponSlots_Get(h->weapons, 0);
    WeaponType *info = Weapon_GetType(w);
    WeaponAmmo ammo;
    uint32_t in;

    Weapon_GetAmmo(w, &ammo);
    in = sm->input;
    if (h->weaponLock) {
        sm_set(sm, WEAPON_DISABLED, h);
        return;
    }
    if (h->input[INPUT_FIRE] != 0) {
        if (!ammo.clip && (in & SM_ANIM_DONE)) {
            sm_set(sm, WEAPON_DRY_FIRE, h);
            return;
        }
        h->fire = 1;
    } else if ((h->input[INPUT_RELOAD] != 0 || !ammo.clip) && ammo.reserve && ammo.clip < info->clipSize) {
        HumanTaskViewReset(h->view);
        sm_set(sm, WEAPON_RELOAD, h);
        return;
    }
    if (h->fired)
        HumanPlayer_SetWeaponAnim(h, 6);
    if ((in & SM_ANIM_DONE) && !(in & 0x200))
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40EF70, HumanPlayer_WeaponFireRun);

void HumanPlayer_WeaponDryFireEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 1);
    Sound_Play(h->view, "guns_dry_1");
    sm->run = HumanPlayer_WeaponDryFireRun;
}
HOOK(0x40F0D0, HumanPlayer_WeaponDryFireEnter);

void HumanPlayer_WeaponDryFireRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    if (h->weaponLock)
        sm_set(sm, WEAPON_DISABLED, h);
    else if (h->input[INPUT_FIRE] == 0)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40F100, HumanPlayer_WeaponDryFireRun);

void HumanPlayer_WeaponReloadEnter(HumanPlayerSM *sm)
{
    sm->run = HumanPlayer_WeaponReloadRun;
}
HOOK(0x40F170, HumanPlayer_WeaponReloadEnter);

void HumanPlayer_WeaponReloadRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    Task *w = WeaponSlots_Get(h->weapons, 0);
    ReloadMsg msg;
    uint8_t slot;

    h->weaponReady = 0;
    msg.h = (Task *)h;
    memcpy(msg.anim, &v->animEvent, sizeof(msg.anim));
    msg.unlimited = gUnlimitedAmmo;
    slot = Human_GetEvent1();
    ((TaskHandler)TASK_HANDLER(slot, w->type))(w, &msg);
    if (msg.done) {
        h->weaponReady = 1;
        sm_set(sm, WEAPON_IDLE, h);
    }
}
HOOK(0x40F180, HumanPlayer_WeaponReloadRun);

void HumanPlayer_WeaponSwitchEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 9);
    HumanView_SetWeaponTarget(h->view, WEAPON_DOWN_X, WEAPON_DOWN_Y, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    h->weaponReady = 0;
    sm->run = HumanPlayer_WeaponLowerRun;
}
HOOK(0x40F230, HumanPlayer_WeaponSwitchEnter);

void HumanPlayer_WeaponLowerRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    Task *w;

    if (!HumanView_WeaponAtTarget(h->view))
        return;
    w = Task_FindWeapon(h, h->nextWeapon);
    if (w)
        WeaponSlots_Set(h->weapons, 0, w);
    h->nextWeapon = -1;
    HumanPlayer_SetWeaponAnim(h, 10);
    HumanView_SetWeaponTarget(h->view, 0, 0, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    sm->run = HumanPlayer_WeaponRaiseRun;
}
HOOK(0x40F280, HumanPlayer_WeaponLowerRun);

void HumanPlayer_WeaponRaiseRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    if (h->nextWeapon != -1) {
        sm_set(sm, WEAPON_SWITCH, h);
    } else if (HumanView_WeaponAtTarget(h->view)) {
        h->weaponReady = 1;
        sm_set(sm, WEAPON_IDLE, h);
    }
}
HOOK(0x40F300, HumanPlayer_WeaponRaiseRun);

void HumanPlayer_WeaponLeanEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 1);
    Human_BeginUse(h);
    sm_set(&h->sm, BODY_LEAN, h);
    HumanView_SetWeaponTarget(h->view, WEAPON_DOWN_X, WEAPON_DOWN_Y, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    sm->run = HumanPlayer_WeaponLeanRun;
}
HOOK(0x40F370, HumanPlayer_WeaponLeanEnter);

void HumanPlayer_WeaponLeanRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;

    if (h->input[INPUT_LEAN] != 0 && !(sm->input & SM_DEAD))
        return;
    Human_EndUse(h);
    v->lean[0] = 0;
    v->lean[1] = 0;
    v->lean[2] = 0;
    sm_set(&h->sm, 0, h);
    HumanView_SetWeaponTarget(h->view, 0, 0, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40F3F0, HumanPlayer_WeaponLeanRun);

void HumanPlayer_WeaponAltFireEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponType *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));

    h->altFire = 1;
    Anim_Play(v->anim, 0, info->anim[3], 0, 0);
    sm->run = HumanPlayer_WeaponWaitRun;
}
HOOK(0x40F4A0, HumanPlayer_WeaponAltFireEnter);

void HumanPlayer_WeaponThrowEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponType *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));
    char *p;
    long double fuse;

    Anim_Play(v->anim, 0, info->anim[2], 0, 0);
    p = PhysicsObjType_Find(info->key);
    fuse = **(float **)(p + 0x2D4) * kFps;
    h->grenadeTicking = 0;
    h->cookTime = 0;
    h->grenadeSound = 0;
    h->grenadeFuse = (int)llrintl(fuse);
    sm->run = HumanPlayer_WeaponThrowRun;
}
HOOK(0x40F4F0, HumanPlayer_WeaponThrowEnter);

void HumanPlayer_WeaponThrowRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponType *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));

    if (v->animEvent.sound == 4) {
        h->grenadeTicking = 1;
        h->grenadeSound = Sound_Play(h->view, "grenade_tick_1");
    }
    if (h->grenadeTicking)
        h->cookTime++;
    if ((sm->input & SM_ANIM_DONE) && h->input[INPUT_FIRE] == 0) {
        Anim_Play(v->anim, 0, info->anim[3], 0, 0);
        sm->run = HumanPlayer_WeaponReleaseRun;
    }
}
HOOK(0x40F590, HumanPlayer_WeaponThrowRun);

void HumanPlayer_WeaponReleaseRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    Task *w = WeaponSlots_Get(h->weapons, 0);
    WeaponType *info = Weapon_GetType(w);
    WeaponAmmo ammo;

    h->cookTime++;
    if (v->animEvent.sound == 3) {
        h->fire = 1;
        if (h->grenadeSound) {
            Sound_Stop(h->grenadeSound);
            h->grenadeSound = 0;
        }
    }
    if (!(sm->input & SM_ANIM_DONE))
        return;
    Anim_Play(v->anim, 0, info->anim[0], 0, 0);
    HumanView_SetWeaponAngles(h->view, WEAPON_DOWN_X, WEAPON_DOWN_Y);
    HumanView_SetWeaponTarget(h->view, 0, 0, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    Weapon_GetAmmo(w, &ammo);
    if (!ammo.reserve && !gUnlimitedAmmo) {
        HumanPlayer_CycleWeapon(h, 1);
        sm->run = HumanPlayer_WeaponLowerRun;
        return;
    }
    Weapon_UseAmmo(w, gUnlimitedAmmo);
    sm->run = HumanPlayer_WeaponRecoverRun;
}
HOOK(0x40F640, HumanPlayer_WeaponReleaseRun);

/* picks the weapon to switch to from the next/previous keys, the weapon key
   or the weapon that was out before the binoculars */
void HumanPlayer_CycleWeapon(HumanPlayer *h, int force)
{
    Task *w = WeaponSlots_Get(h->weapons, 0);
    int cur = w ? Weapon_GetType(w)->id : -1;
    int dir = 0, id = -1, i, n;

    if (h->cycleIndex == -1 || force || pressed(h, INPUT_NEXT_WEAPON))
        dir = 1;
    if (pressed(h, INPUT_PREV_WEAPON))
        dir = -1;
    else if (!dir)
        goto select;
    if (h->savedWeapon) {
        id = Weapon_GetType(h->savedWeapon)->id;
        h->savedWeapon = NULL;
        if (id != -1)
            goto done;
        goto select;
    }
    for (n = gWeaponCycleCount; n; ) {
        n--;
        h->cycleIndex += dir;
        if (h->cycleIndex >= gWeaponCycleCount)
            h->cycleIndex = 0;
        else if (h->cycleIndex < 0)
            h->cycleIndex = gWeaponCycleCount - 1;
        id = gWeaponCycle[h->cycleIndex];
        w = Task_FindWeapon(h, id);
        if (w && Weapon_HasAmmo(w)) {
            if (id != -1)
                goto done;
            break;
        }
    }
select:
    id = (int)llrintl(h->input[INPUT_WEAPON]);
    w = Task_FindWeapon(h, id);
    if (w) {
        if (!Weapon_HasAmmo(w)) {
            id = -1;
        } else {
            for (i = 0; i < gWeaponCycleCount; i++) {
                if (gWeaponCycle[i] == id)
                    h->cycleIndex = i;
            }
        }
    }
done:
    if (id != cur && Task_FindWeapon(h, id))
        h->nextWeapon = id;
}
HOOK(0x40F760, HumanPlayer_CycleWeapon);

void HumanPlayer_WeaponPlaceEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    h->fire = 1;
    sm->run = HumanPlayer_WeaponPlaceRun;
}
HOOK(0x40F930, HumanPlayer_WeaponPlaceEnter);

void HumanPlayer_WeaponPlaceRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    Task *w;
    WeaponAmmo ammo;

    if (!h->fired) {
        sm_set(sm, WEAPON_IDLE, h);
        return;
    }
    HumanView_SetWeaponAngles(h->view, WEAPON_DOWN_X, WEAPON_DOWN_Y);
    w = WeaponSlots_Get(h->weapons, 0);
    Weapon_GetAmmo(w, &ammo);
    if (!ammo.reserve && !gUnlimitedAmmo) {
        HumanPlayer_CycleWeapon(h, 1);
        sm->run = HumanPlayer_WeaponLowerRun;
        return;
    }
    Weapon_UseAmmo(w, gUnlimitedAmmo);
    HumanView_SetWeaponTarget(h->view, 0, 0, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    sm->run = HumanPlayer_WeaponRecoverRun;
}
HOOK(0x40F950, HumanPlayer_WeaponPlaceRun);

void HumanPlayer_WeaponRecoverRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    if (HumanView_WeaponAtTarget(h->view))
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40FA20, HumanPlayer_WeaponRecoverRun);

void HumanPlayer_WeaponMeleeEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    h->meleeCount = 0;
    sm->run = HumanPlayer_WeaponMeleeRun;
}
HOOK(0x40FA60, HumanPlayer_WeaponMeleeEnter);

void HumanPlayer_WeaponMeleeRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    Task *w = WeaponSlots_Get(h->weapons, 0);
    WeaponAmmo ammo;
    uint32_t in;

    Weapon_GetType(w);
    Weapon_GetAmmo(w, &ammo);
    if (h->input[INPUT_FIRE] != 0 && h->meleeCount < 3)
        h->fire = 1;
    in = sm->input;
    if (h->fired) {
        HumanPlayer_SetWeaponAnim(h, 8);
        h->meleeCount++;
    }
    if (in & SM_ANIM_DONE)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40FA80, HumanPlayer_WeaponMeleeRun);

void HumanPlayer_WeaponMedipackEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponType *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));

    Anim_Play(v->anim, 0, info->anim[2], 0, 0);
    h->medipackDelay = 0;
    sm->run = HumanPlayer_WeaponMedipackStartRun;
}
HOOK(0x40FB30, HumanPlayer_WeaponMedipackEnter);

void HumanPlayer_WeaponMedipackStartRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponType *info;

    if (!(sm->input & SM_ANIM_DONE))
        return;
    info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));
    Anim_Play(v->anim, 0, info->anim[3], 1, 0);
    sm->run = HumanPlayer_WeaponMedipackRun;
}
HOOK(0x40FB90, HumanPlayer_WeaponMedipackStartRun);

void HumanPlayer_WeaponMedipackRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponType *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));
    AmmoEntry *a;
    long double t;

    if (h->input[INPUT_FIRE] == 0) {
        Anim_Play(v->anim, 0, Weapon_GetType(WeaponSlots_Get(h->weapons, 0))->anim[4], 0, 0);
        sm->run = HumanPlayer_WeaponWaitRun;
    }
    if (h->medipackDelay) {
        h->medipackDelay--;
        return;
    }
    a = AmmoList_Find(h->ammo, Script_EvalConst("AMMO_ID_MEDIPACK"));
    if (!a)
        return;
    if (!a->count || h->damage.total == 0) {
        Anim_Play(v->anim, 0, Weapon_GetType(WeaponSlots_Get(h->weapons, 0))->anim[4], 0, 0);
        sm->run = HumanPlayer_WeaponWaitRun;
        return;
    }
    a->count--;
    t = h->damage.total - h->damage.max * *(float *)info->data2;
    h->damage.total = t;
    if (t < 0)
        h->damage.total = 0;
    else if (t > h->damage.max)
        h->damage.total = h->damage.max;
    h->medipackDelay = (int)llrintl(60.0 / (long double)info->rate * 30.0);
}
HOOK(0x40FBE0, HumanPlayer_WeaponMedipackRun);

void HumanPlayer_WeaponWaitRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    if (sm->input & SM_ANIM_DONE)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40FD70, HumanPlayer_WeaponWaitRun);

void HumanPlayer_WeaponComputerEnter(HumanPlayerSM *sm, HumanPlayer *h)
{
    sm_set(&h->sm, BODY_COMPUTER, h);
    h->weaponReady = 0;
    sm->run = HumanPlayer_WeaponComputerRun;
}
HOOK(0x40FDA0, HumanPlayer_WeaponComputerEnter);

void HumanPlayer_WeaponComputerRun(HumanPlayerSM *sm, HumanPlayer *h)
{
    if (h->sm.state != BODY_COMPUTER) {
        h->weaponReady = 1;
        sm_set(sm, WEAPON_IDLE, h);
    }
}
HOOK(0x40FDF0, HumanPlayer_WeaponComputerRun);

void HumanPlayer_Open(void)
{
    gHumanPlayerType = TaskType_New("HumanPlayer", sizeof(HumanPlayer), Human_GetType(), 0);
    TaskType_SetFlags(gHumanPlayerType, 1);
    gHumanPlayerList = QTaskList_New(8);
    TaskType_SetHandler(gHumanPlayerType, 0, HumanPlayer_CreateHandler);
    TaskType_SetHandler2(gHumanPlayerType, 1, Human_RunBase);
    TaskType_SetHandler2(gHumanPlayerType, 2, HumanPlayer_DeleteHandler);
    TaskType_SetHandler(gHumanPlayerType, Param_GetTableEvent(), HumanPlayer_GetParamTableHandler);
    TaskType_SetHandler(gHumanPlayerType, Param_GetChangedEvent(), HumanPlayer_ParamChangedHandler);
    TaskType_SetHandler(gHumanPlayerType, World_GetEvent1(), HumanPlayer_DeactivateHandler);
    TaskType_SetHandler(gHumanPlayerType, Vehicle_GetControlEvent(), HumanPlayer_InputHandler);
    TaskType_SetHandler(gHumanPlayerType, Vehicle_GetInputInfoEvent(), FN(0x4123F0, HumanPlayer_InputChannelHandler));
    TaskType_SetHandler(gHumanPlayerType, Vehicle_GetKeysEvent(), FN(0x412420, HumanPlayer_ReadInputHandler));
    TaskType_SetHandler(gHumanPlayerType, Game_GetEvent9(), FN(0x412440, HumanPlayer_HitHandler));
    TaskType_SetHandler(gHumanPlayerType, Game_GetEvent11(), FN(0x412990, HumanPlayer_BlastHandler));
    TaskType_SetHandler(gHumanPlayerType, Human_GetPunchEvent(), FN(0x412CE0, HumanPlayer_PunchHandler));
    TaskType_SetHandler(gHumanPlayerType, Game_GetEvent12(), FN(0x412F80, HumanPlayer_FenceHandler));
    TaskType_SetHandler(gHumanPlayerType, FlashBang_GetEvent(), FN(0x413090, HumanPlayer_FlashbangHandler));
    TaskType_SetHandler(gHumanPlayerType, StatusScreen_GetEvent(), FN(0x413120, HumanPlayer_RenderHandler));
    TaskType_SetHandler(gHumanPlayerType, ComputerObject_GetEvent0(), HumanPlayer_MapMarkerHandler);
    gHumanPlayerParams = ParamTable_Extend(Human_GetParamTable(), 1);
    Script_RegisterFunction("DefineHumanPlayerGeneral", DefineHumanPlayerGeneral, NULL, -1);
    Script_RegisterFunction("DefineHumanPlayerWeaponCycle", DefineHumanPlayerWeaponCycle, NULL, -1);
    Script_RegisterFunction("DefineHumanPlayerWeaponCategory", DefineHumanPlayerWeaponCategory, NULL, -1);
    Script_RegisterFunction("DefineHumanPlayerAmmoLimit", DefineHumanPlayerAmmoLimit, NULL, -1);
    AppMain_ParseCmdLineArgs("UnlimitedAmmo", HumanPlayer_SetUnlimitedAmmo);
}
HOOK(0x40FE30, HumanPlayer_Open);

static void return_one(ScriptValue *ret)
{
    ret->i = 1;
    ret->d = 1.0;
    ret->s = gEmptyString;
}

ScriptValue *DefineHumanPlayerGeneral(ScriptValue *ret, void *unused, void *args)
{
    int i = 9, k;

    (void)unused;
    gMoveScale = Script_GetArgReal(args, 0);
    gJumpSpeed = Script_GetArgReal(args, 1) * kKmhToUnits * kPerFrameHour;
    gJumpUp = Script_GetArgReal(args, 2) * kKmhToUnits * kPerFrameHour;
    gHumanAirSpeed = Script_GetArgReal(args, 3) * kKmhToUnits * kPerFrameHour;
    gLeanSide = Script_GetArgReal(args, 4) * 4096.0;
    gLeanUp = Script_GetArgReal(args, 5) * 4096.0;
    gLeanFrames = (int)llrintl(Script_GetArgReal(args, 6) * 30.0);
    gMaxDamage = Script_GetArgReal(args, 7);
    gFenceDamage = Script_GetArgReal(args, 8);
    for (k = 0; k < 17; k++)
        gMoveSpeeds[k * 4] = Script_GetArgReal(args, i++);
    for (k = 0; k < 17; k++)
        gTurnSpeeds[k * 4] = Script_GetArgReal(args, i++);
    return_one(ret);
    return ret;
}
HOOK(0x410070, DefineHumanPlayerGeneral);

ScriptValue *DefineHumanPlayerWeaponCycle(ScriptValue *ret, void *unused, int *args)
{
    int i;

    (void)unused;
    gWeaponCycleCount = 0;
    for (i = 0; i < *args; i++)
        gWeaponCycle[gWeaponCycleCount++] = Script_GetArgInt(args, i);
    return_one(ret);
    return ret;
}
HOOK(0x4101C0, DefineHumanPlayerWeaponCycle);

ScriptValue *DefineHumanPlayerWeaponCategory(ScriptValue *ret, void *unused, void *args)
{
    WeaponCategory *c;
    int i = 0, k;

    (void)unused;
    for (c = gWeaponCategories; c < gWeaponCategories + 9; c++) {
        c->count = Script_GetArgInt(args, i++);
        for (k = 0; k < c->count; k++)
            c->ids[k] = Script_GetArgInt(args, i++);
    }
    return_one(ret);
    return ret;
}
HOOK(0x410240, DefineHumanPlayerWeaponCategory);

ScriptValue *DefineHumanPlayerAmmoLimit(ScriptValue *ret, void *unused, int *args)
{
    AmmoLimit *l;
    int i;

    (void)unused;
    gAmmoLimitCount = 0;
    for (i = 0; i < *args; gAmmoLimitCount++) {
        l = &gAmmoLimits[gAmmoLimitCount];
        l->id = Script_GetArgInt(args, i++);
        l->limit = Script_GetArgInt(args, i++);
    }
    return_one(ret);
    return ret;
}
HOOK(0x4102D0, DefineHumanPlayerAmmoLimit);

void HumanPlayer_SetUnlimitedAmmo(void)
{
    gUnlimitedAmmo = 1;
}
HOOK(0x410360, HumanPlayer_SetUnlimitedAmmo);

void HumanPlayer_CreateHandler(HumanPlayer *h, void *arg)
{
    double pos[3] = {0, 0, 0};
    int i;

    ((TaskHandler)TASK_HANDLER(0, Human_GetType()))(h, arg);
    QTaskList_Add(gHumanPlayerList, h);
    h->unk695 = 1;
    h->unk696 = 1;
    h->unk697 = 1;
    h->unk698 = 1;
    BoneDynCubeObj_Init(Game_GetWorld(), h, 0, 0);
    h->unkCC = 2;
    Human_PlayAnim(h, 2, 0, 0);
    BoneObj_Update(h->matrix.m, h->anim);
    memcpy(h->unk650, h->motion->unk4C, sizeof(h->unk650));
    DynCube_Add(Game_GetWorld(), h, pos, h->radius, 2);
    HumanView_AddToWorld(h->view);
    for (i = 0; i < gAmmoLimitCount; i++)
        AmmoList_SetLimit(h->ammo, gAmmoLimits[i].id, gAmmoLimits[i].limit);
    StateMachine_Init(&h->sm, HumanPlayer_UpdateBodyInput, gBodyStates, 0, 2, gBodyCombos);
    h->animSlots = (struct AnimSlot *)gAnimSlots;
    Human_InitDamage(h, gMoveSpeeds);
    h->damage.max = gMaxDamage;
    StateMachine_Init(&h->weapon, HumanPlayer_UpdateWeaponInput, gWeaponStates, WEAPON_IDLE, 0, NULL);
    h->weaponLock = 0;
    h->unkD45 = 0;
    h->cycleIndex = -1;
    h->nextWeapon = -1;
    h->weaponCategory = -1;
    h->weaponCycle = 0;
    h->weaponAnim = 0;
    h->weaponAnimReq = 0;
    h->showBar = 0;
    h->ladderEnter = 0;
    h->onLadder = 0;
    h->ladderLeave = 0;
    h->ladderSound = 0;
    h->unkD90 = 0;
    h->unkD91 = 0;
    h->unkD92 = 0;
    h->wireEnter = 0;
    h->onWire = 0;
    h->wireSound = 0;
    h->ziplineEnter = 0;
    h->onZipline = 0;
    h->unkDBC = NULL;
    memset(&h->unkDC0, 0, 0x24);
    h->savedWeapon = NULL;
    h->attacker = NULL;
}
HOOK(0x410370, HumanPlayer_CreateHandler);

static uint32_t common_input(HumanPlayer *h, uint32_t in)
{
    static const uint32_t bits[10] = {0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000};
    int i;

    for (i = 0; i < 10; i++) {
        if (h->input[INPUT_LEFT + i] != 0)
            in |= bits[i];
    }
    return in;
}

void HumanPlayer_UpdateBodyInput(HumanPlayerSM *sm, HumanPlayer *h)
{
    uint32_t flags = h->motion->flags;
    uint32_t in = sm->input & 0xFFF7001C;

    if (flags & 0x100)
        in |= SM_ANIM_DONE;
    if (flags & 0x10000)
        in |= SM_ANIM_EVENT;
    if (h->unkD45)
        in |= 0x40000;
    else
        in &= ~0x40000u;
    in = common_input(h, in);
    if (h->input[INPUT_NEXT_WEAPON] != 0)
        in |= 0x80000;
    sm->input = in;
}
HOOK(0x4105C0, HumanPlayer_UpdateBodyInput);

void HumanPlayer_UpdateWeaponInput(HumanPlayerSM *sm, HumanPlayer *h)
{
    uint32_t flags = h->view->motion->flags;
    uint32_t in = sm->input & 0xFFF7001C;

    if (flags & 0x100)
        in |= SM_ANIM_DONE;
    if (flags & 0x10000)
        in |= SM_ANIM_EVENT;
    in = common_input(h, in);
    if (h->input[INPUT_LEAN] != 0)
        in |= 0x8000;
    if (h->input[INPUT_NEXT_WEAPON] != 0)
        in |= 0x80000;
    sm->input = in;
}
HOOK(0x410700, HumanPlayer_UpdateWeaponInput);

void HumanPlayer_DeleteHandler(HumanPlayer *h)
{
    if (h->attacker) {
        TaskRef_Remove(h->attacker);
        h->attacker = NULL;
    }
    QTaskList_Remove(gHumanPlayerList, h);
    BoneDynCubeObj_Deinit(h);
    DynCube_Remove(Game_GetWorld(), h);
    ((TaskHandler1)TASK_HANDLER(2, Human_GetType()))(h);
    if (h->unkDBC)
        TaskRef_Remove(h->unkDBC);
}
HOOK(0x410840, HumanPlayer_DeleteHandler);

void HumanPlayer_GetParamTableHandler(HumanPlayer *h, void **out)
{
    (void)h;
    *out = gHumanPlayerParams;
}
HOOK(0x4108B0, HumanPlayer_GetParamTableHandler);

void HumanPlayer_ParamChangedHandler(HumanPlayer *h, void *arg)
{
    uint8_t slot = Param_GetChangedEvent();

    ((TaskHandler)TASK_HANDLER(slot, Human_GetType()))(h, arg);
}
HOOK(0x4108C0, HumanPlayer_ParamChangedHandler);

void HumanPlayer_DeactivateHandler(HumanPlayer *h, void *arg)
{
    uint8_t slot;

    if (Computer_Get())
        Computer_Delete();
    if (h->vulnerable) {
        h->vulnerable = 0;
        slot = World_GetEvent1();
        ((TaskHandler)TASK_HANDLER(slot, BoneDynCubeObj_GetType()))(h, arg);
    }
}
HOOK(0x410900, HumanPlayer_DeactivateHandler);

void HumanPlayer_InputHandler(HumanPlayer *h, float **msg)
{
    if (!h->inputLocked) {
        memcpy(h->prevInput, h->input, sizeof(h->input));
        memcpy(h->input, msg[1], sizeof(h->input));
    }
    HumanPlayer_Update(h);
}
HOOK(0x410960, HumanPlayer_InputHandler);

void HumanPlayer_Update(HumanPlayer *h)
{
    float *d = h->delta;
    float x, y, z, pitch, yaw, v[3];
    long double speed, a;
    CameraTask *cam;

    HumanPlayer_CheckDeath(h);
    Human_UpdatePlatformMove(h);
    Human_UpdateUse(h);
    if (!Computer_Get()) {
        HumanPlayer_SelectWeaponCategory(h);
        HumanPlayer_CycleWeapon(h, 0);
    }
    StateMachine_Update(&h->sm, h);
    StateMachine_Update(&h->weapon, h);
    if (pressed(h, INPUT_TOGGLE_D45))
        h->unkD45 ^= 1;
    if (h->sm.state == BODY_FLY) {
        if (pressed(h, INPUT_FLY)) {
            memset(h->vel, 0, sizeof(h->vel));
            memset(h->push, 0, sizeof(h->push));
            sm_set(&h->sm, 0, h);
        }
    } else if (pressed(h, INPUT_FLY)) {
        sm_set(&h->sm, BODY_FLY, h);
    }
    if (h->ladderEnter || h->onLadder)
        HumanPlayer_UpdateLadder(h);
    if (h->unkD90 || h->unkD91)
        HumanPlayer_UpdateFenceClimb(h);
    if (h->wireEnter || h->onWire)
        HumanPlayer_UpdateWire(h);
    if (h->ziplineEnter || h->onZipline)
        HumanPlayer_UpdateZipline(h);
    h->sm.input &= ~SM_HIT;
    Human_UpdateBody(h);
    HumanView_UpdateView(h->view, h);
    h->frozen = 1;
    BoneObj_GetRootMotion(h->boneOffset, d, h->anim);
    if (!h->deltaScaled) {
        d[0] = gMoveScale * d[0];
        d[1] = gMoveScale * d[1];
        d[2] = gMoveScale * d[2];
    }
    x = h->matrix.m[2] * d[2] + h->matrix.m[1] * d[1] + h->matrix.m[0] * d[0];
    y = h->matrix.m[5] * d[2] + h->matrix.m[4] * d[1] + h->matrix.m[3] * d[0];
    z = h->matrix.m[8] * d[2] + h->matrix.m[7] * d[1] + h->matrix.m[6] * d[0];
    d[0] = x;
    d[1] = y;
    d[2] = z;
    Human_FireAnimTrigger(h);
    if (h->sm.state != BODY_FLY) {
        if (!h->attach)
            Human_UpdateMove(h);
        else
            Human_MoveOnPlatform(h);
    }
    HumanView_UpdateBody(h->view, h);
    Human_UpdateWeapon(h);
    Human_SendPosition(h);
    HumanPlayer_SendPosition(h);
    Human_Footstep(h);
    HumanPlayer_UpdateEarRinging(h);
    HumanPlayer_FadeHitFlash(h);
    HumanPlayer_FadeFlash(h);
    HumanPlayer_UpdateFenceTimer(h);
    speed = h->vel[2];
    if (h->vel[2] < 0)
        speed = -speed;
    Human_printf(h, "Speed Z (m/s): %.2f\n", (double)(speed * kUnitsToMetersF * kFps));
    Human_printf(h, "Ground task: %x\n", Human_GetGround(h));
    Human_printf(h, "Attach task: %x\n", h->attach);
    Human_printf(h, "Delta move pos X: %.2f\n", (double)h->movePos[0]);
    Human_printf(h, "Delta move pos Y: %.2f\n", (double)h->movePos[1]);
    Human_printf(h, "Delta move pos Z: %.2f\n", (double)h->movePos[2]);
    Human_printf(h, "Delta move Alpha: %.2f\n", (double)h->moveAngle[0]);
    Human_printf(h, "Delta move Beta: %.2f\n", (double)h->moveAngle[1]);
    Human_printf(h, "Delta move Gamma: %.2f\n", (double)h->moveAngle[2]);
    Human_printf(h, "Distance to ground from feet: %.2fm\n", (double)(h->groundDistance * kUnitsToMeters));
    Human_printf(h, "Ground angle: %.3f\n", (double)(h->slopeAngle * kRadToDeg));
    Human_printf(h, "Delta translation X: %.2f\n", (double)d[0]);
    Human_printf(h, "Delta translation Y: %.2f\n", (double)h->delta[1]);
    Human_printf(h, "Delta translation Z: %.2f\n", (double)h->delta[2]);
    if (h->sm.input & 4)
        Human_printf(h, "Ground!\n");
    if (Game_CheatKeyPressed(0x57))
        gShowPosition ^= 1;
    if (!gShowPosition)
        return;
    Debug_printf("Player position: %.2f, %.2f, %.2f\n", (double)(h->pos[0] * kUnitsToMeters),
                 (double)(h->pos[1] * kUnitsToMeters), (double)(h->pos[2] * kUnitsToMeters));
    if (!Game_Get()->camera)
        return;
    cam = Game_Get()->camera;
    v[0] = cam->m[2];
    v[1] = cam->m[5];
    v[2] = cam->m[8];
    Vec3_ToAngles(v, &pitch, &yaw);
    a = yaw;
    if (a < 0) {
        do
            a += kTwoPi;
        while (a < 0);
    }
    Debug_printf("Player angle: %.2f, %.2f\n", (double)(pitch * kRadToDeg), (double)(a * kRadToDeg));
}
HOOK(0x4109A0, HumanPlayer_Update);

void HumanPlayer_SendPosition(HumanPlayer *h)
{
    PositionMsg msg;
    TaskHandler fn;
    uint16_t type;
    int i;

    if (Human_IsDead(h))
        return;
    msg.h = h;
    memcpy(msg.pos, h->pos, sizeof(msg.pos));
    for (i = 0; i < Game_GetTaskList2()->count; i++) {
        type = Game_GetTaskList2()->items[i]->type;
        fn = (TaskHandler)TASK_HANDLER(Game_GetEvent1(), type);
        fn(Game_GetTaskList2()->items[i], &msg);
    }
}
HOOK(0x410F70, HumanPlayer_SendPosition);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(HumanPlayer_BodyComputerEnter);
REF_EXTERN(HumanPlayer_BodyDeathEnter);
REF_EXTERN(HumanPlayer_BodyFlyEnter);
REF_EXTERN(HumanPlayer_BodyHitEnter);
REF_EXTERN(HumanPlayer_BodyLeanEnter);
REF_EXTERN(HumanPlayer_FallEnter);
REF_EXTERN(HumanPlayer_JumpEnter);
REF_EXTERN(HumanPlayer_LadderEnter);
REF_EXTERN(HumanPlayer_LadderOffTopEnter);
REF_EXTERN(HumanPlayer_LadderOnEnter);
REF_EXTERN(HumanPlayer_LadderSlideEnter);
REF_EXTERN(HumanPlayer_PicklockEnter);
REF_EXTERN(HumanPlayer_RunBackEnter);
REF_EXTERN(HumanPlayer_RunBackLeftEnter);
REF_EXTERN(HumanPlayer_RunBackRightEnter);
REF_EXTERN(HumanPlayer_RunForwardRightEnter);
REF_EXTERN(HumanPlayer_RunLeftEnter);
REF_EXTERN(HumanPlayer_RunRightEnter);
REF_EXTERN(HumanPlayer_State41Enter);
REF_EXTERN(HumanPlayer_TypeComputerEnter);
REF_EXTERN(HumanPlayer_Use31Enter);
REF_EXTERN(HumanPlayer_Use33Enter);
REF_EXTERN(HumanPlayer_Use34Enter);
REF_EXTERN(HumanPlayer_Use35Enter);
REF_EXTERN(HumanPlayer_Use36Enter);
REF_EXTERN(HumanPlayer_WeaponAltFireEnter);
REF_EXTERN(HumanPlayer_WeaponComputerEnter);
REF_EXTERN(HumanPlayer_WeaponDisabledEnter);
REF_EXTERN(HumanPlayer_WeaponDryFireEnter);
REF_EXTERN(HumanPlayer_WeaponFireEnter);
REF_EXTERN(HumanPlayer_WeaponIdleEnter);
REF_EXTERN(HumanPlayer_WeaponLeanEnter);
REF_EXTERN(HumanPlayer_WeaponMedipackEnter);
REF_EXTERN(HumanPlayer_WeaponMeleeEnter);
REF_EXTERN(HumanPlayer_WeaponPlaceEnter);
REF_EXTERN(HumanPlayer_WeaponReloadEnter);
REF_EXTERN(HumanPlayer_WeaponSwitchEnter);
REF_EXTERN(HumanPlayer_WeaponThrowEnter);
REF_EXTERN(HumanPlayer_WireDragEnter);
REF_EXTERN(HumanPlayer_ZiplineEnter);
REF_EXTERN(HumanState_Enter00);
REF_EXTERN(HumanState_Enter01);
REF_EXTERN(HumanState_Enter02);
REF_EXTERN(HumanState_Enter03);
REF_EXTERN(HumanState_Enter04);
REF_EXTERN(HumanState_Enter05);
REF_EXTERN(HumanState_Enter06);
REF_EXTERN(HumanState_Enter07);
REF_EXTERN(HumanState_Enter08);
REF_EXTERN(HumanState_Enter09);
REF_EXTERN(HumanState_Enter10);
REF_EXTERN(HumanState_Enter11);
REF_EXTERN(HumanState_Enter12);
REF_EXTERN(HumanState_Enter13);
REF_EXTERN(HumanState_Enter14);
REF_EXTERN(HumanState_Enter15);
REF_EXTERN(HumanState_Enter16);
REF_EXTERN(HumanState_Enter17);
REF_EXTERN(HumanState_Enter18);
REF_EXTERN(HumanState_Enter19);
REF_EXTERN(HumanState_Enter20);
REF_EXTERN(HumanState_Enter21);
DATA(5333E8) float kUnitsToMetersF INIT(0.00024414062f);
DATA(533478) double kFlySpeedD INIT(1638.4);
DATA(5334AC) float kRadToDeg INIT(57.295776f);
DATA(5385B0) uint16_t gHumanPlayerType INIT(385);
PAD(igd, 5385B0, 5385B2, 0x2);
STORE(igd, 5385B0, 4) __typeof__(gBodyStates[0]) store_5385B4[49] INIT({(__typeof__(store_5385B4[0]))ref_HumanState_Enter00, (__typeof__(store_5385B4[1]))ref_HumanState_Enter01, (__typeof__(store_5385B4[2]))ref_HumanState_Enter02, (__typeof__(store_5385B4[3]))ref_HumanState_Enter03, (__typeof__(store_5385B4[4]))ref_HumanState_Enter04, (__typeof__(store_5385B4[5]))ref_HumanState_Enter05, (__typeof__(store_5385B4[6]))ref_HumanState_Enter06, (__typeof__(store_5385B4[7]))ref_HumanState_Enter07, (__typeof__(store_5385B4[8]))ref_HumanState_Enter08, (__typeof__(store_5385B4[9]))ref_HumanState_Enter09, (__typeof__(store_5385B4[10]))ref_HumanState_Enter10, (__typeof__(store_5385B4[11]))ref_HumanState_Enter11, (__typeof__(store_5385B4[12]))ref_HumanState_Enter12, (__typeof__(store_5385B4[13]))ref_HumanState_Enter13, (__typeof__(store_5385B4[14]))ref_HumanState_Enter14, (__typeof__(store_5385B4[15]))ref_HumanState_Enter15, (__typeof__(store_5385B4[16]))ref_HumanState_Enter16, (__typeof__(store_5385B4[17]))ref_HumanState_Enter17, (__typeof__(store_5385B4[18]))ref_HumanState_Enter18, (__typeof__(store_5385B4[19]))ref_HumanState_Enter19, (__typeof__(store_5385B4[20]))ref_HumanState_Enter20, (__typeof__(store_5385B4[21]))ref_HumanState_Enter21, (__typeof__(store_5385B4[22]))ref_HumanPlayer_RunForwardRightEnter, (__typeof__(store_5385B4[23]))ref_HumanPlayer_RunBackEnter, (__typeof__(store_5385B4[24]))ref_HumanPlayer_RunBackLeftEnter, (__typeof__(store_5385B4[25]))ref_HumanPlayer_RunBackRightEnter, (__typeof__(store_5385B4[26]))ref_HumanPlayer_RunLeftEnter, (__typeof__(store_5385B4[27]))ref_HumanPlayer_RunRightEnter, (__typeof__(store_5385B4[28]))ref_HumanPlayer_JumpEnter, (__typeof__(store_5385B4[29]))ref_HumanPlayer_FallEnter, (__typeof__(store_5385B4[30]))ref_HumanPlayer_PicklockEnter, (__typeof__(store_5385B4[31]))ref_HumanPlayer_Use31Enter, (__typeof__(store_5385B4[32]))ref_HumanPlayer_TypeComputerEnter, (__typeof__(store_5385B4[33]))ref_HumanPlayer_Use33Enter, (__typeof__(store_5385B4[34]))ref_HumanPlayer_Use34Enter, (__typeof__(store_5385B4[35]))ref_HumanPlayer_Use35Enter, (__typeof__(store_5385B4[36]))ref_HumanPlayer_Use36Enter, (__typeof__(store_5385B4[37]))ref_HumanPlayer_LadderEnter, (__typeof__(store_5385B4[38]))ref_HumanPlayer_LadderOnEnter, (__typeof__(store_5385B4[39]))ref_HumanPlayer_LadderOffTopEnter, (__typeof__(store_5385B4[40]))ref_HumanPlayer_LadderSlideEnter, (__typeof__(store_5385B4[41]))ref_HumanPlayer_State41Enter, (__typeof__(store_5385B4[42]))ref_HumanPlayer_WireDragEnter, (__typeof__(store_5385B4[43]))ref_HumanPlayer_ZiplineEnter, (__typeof__(store_5385B4[44]))ref_HumanPlayer_BodyLeanEnter, (__typeof__(store_5385B4[45]))ref_HumanPlayer_BodyComputerEnter, (__typeof__(store_5385B4[46]))ref_HumanPlayer_BodyHitEnter, (__typeof__(store_5385B4[47]))ref_HumanPlayer_BodyDeathEnter, (__typeof__(store_5385B4[48]))ref_HumanPlayer_BodyFlyEnter});
ALIAS(gBodyStates, store_5385B4, 0);
STORE(igd, 5385B0, 4) __typeof__(gWeaponStates[0]) store_538678[14] INIT({(__typeof__(store_538678[0]))ref_HumanPlayer_WeaponDisabledEnter, (__typeof__(store_538678[1]))ref_HumanPlayer_WeaponIdleEnter, (__typeof__(store_538678[2]))ref_HumanPlayer_WeaponFireEnter, (__typeof__(store_538678[3]))ref_HumanPlayer_WeaponReloadEnter, (__typeof__(store_538678[4]))ref_HumanPlayer_WeaponSwitchEnter, (__typeof__(store_538678[5]))ref_HumanPlayer_WeaponLeanEnter, (__typeof__(store_538678[6]))ref_HumanPlayer_WeaponAltFireEnter, (__typeof__(store_538678[7]))ref_HumanPlayer_WeaponThrowEnter, (__typeof__(store_538678[8]))ref_HumanPlayer_WeaponPlaceEnter, (__typeof__(store_538678[9]))ref_HumanPlayer_WeaponMeleeEnter, (__typeof__(store_538678[10]))ref_HumanPlayer_WeaponMedipackEnter, (__typeof__(store_538678[11]))ref_HumanPlayer_WeaponComputerEnter, (__typeof__(store_538678[12]))ref_HumanPlayer_WeaponDryFireEnter});
ALIAS(gWeaponStates, store_538678, 0);
DATA(5385B0) uint8_t dat_5386B0[48] INIT({1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 2, 0, 0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 2});
DATA(5385B0) uint8_t dat_5386E0[48] INIT({1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 8, 0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 8});
STORE(igd, 5385B0, 1) char store_538710[16] INIT({4, 0, 0, 0, -80, -122, 83, 0, 4, 0, 0, 0, -32, -122, 83});
ALIAS(gBodyCombos, store_538710, 0);
STORE(igd, 538870, 1) char store_538870[392] INIT({2, 0, 0, 0, 1, 0, 0, 0, 4, 0, 0, 0, 1, 0, 0, 0, 37, 0, 0, 0, 1, 0, 0, 0, 35, 0, 0, 0, 1, 0, 0, 0, 64, 0, 0, 0, 1, 0, 0, 0, 70, 0, 0, 0, 1, 0, 0, 0, 68, 0, 0, 0, 1, 0, 0, 0, 11, 0, 0, 0, 1, 0, 0, 0, 10, 0, 0, 0, 1, 0, 0, 0, 57, 0, 0, 0, 1, 0, 0, 0, 74, 0, 0, 0, 1, 0, 0, 0, 72, 0, 0, 0, 1, 0, 0, 0, 75, 0, 0, 0, 1, 0, 0, 0, 77, 0, 0, 0, 1, 0, 0, 0, 76, 0, 0, 0, 1, 0, 0, 0, 81, 0, 0, 0, 1, 0, 0, 0, 80, 0, 0, 0, 1, 0, 0, 0, 14, 0, 0, 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 0, 1, 0, 0, 0, 17, 0, 0, 0, 1, 0, 0, 0, 41, 0, 0, 0, 1, 0, 0, 0, 39, 0, 0, 0, 1, 0, 0, 0, 27, 0, 0, 0, 1, 0, 0, 0, 49, 0, 0, 0, 1, 0, 0, 0, 47, 0, 0, 0, 1, 0, 0, 0, 29, 0, 0, 0, 1, 0, 0, 0, 28, 0, 0, 0, 1, 0, 0, 0, 25, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -88, 0, 0, 0, 1, 0, 0, 0, -86, 0, 0, 0, 2, 0, 0, 0, -86, 0, 0, 0, 0, 0, 0, 0, -87, 0, 0, 0, 1, 0, 0, 0, -85, 0, 0, 0, 0, 0, 0, 0, -84, 0, 0, 0, 1, 0, 0, 0, -83, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 19, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1});
ALIAS(gAnimSlots, store_538870, 0);
STORE(igd, 538870, 4) float store_5389F8[68] INIT({0.5f, 1.30321e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.30321e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.30321e-43f, 2.81661e-43f, 2.83062e-43f, 0.5f, 1.35926e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.35926e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.35926e-43f, 2.81661e-43f, 2.83062e-43f, 0.5f, 1.33123e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.33123e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.33123e-43f, 2.81661e-43f, 2.83062e-43f, 0.5f, 1.34525e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.34525e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.34525e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 8.82818e-44f, 2.81661e-43f, 2.83062e-43f, 1.0f, 8.82818e-44f, 2.81661e-43f, 2.83062e-43f, 1.0f, 8.82818e-44f, 3.01279e-43f, 3.01279e-43f, 100.0f, 8.82818e-44f, 1.56945e-43f, 1.56945e-43f, 100.0f, 8.82818e-44f, 2.85865e-43f, 2.85865e-43f});
ALIAS(gMoveSpeeds, store_5389F8, 0);
STORE(igd, 538870, 4) float store_538B08[68] INIT({0.5f, 1.30321e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.30321e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.30321e-43f, 2.81661e-43f, 2.83062e-43f, 0.5f, 1.35926e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.35926e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.35926e-43f, 2.81661e-43f, 2.83062e-43f, 0.5f, 1.33123e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.33123e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.33123e-43f, 2.81661e-43f, 2.83062e-43f, 0.5f, 1.34525e-43f, 2.81661e-43f, 2.83062e-43f, 0.75f, 1.34525e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 1.34525e-43f, 2.81661e-43f, 2.83062e-43f, 1.0f, 8.82818e-44f, 2.81661e-43f, 2.83062e-43f, 1.0f, 8.82818e-44f, 2.81661e-43f, 2.83062e-43f, 1.0f, 8.82818e-44f, 3.01279e-43f, 3.01279e-43f, 100.0f, 8.82818e-44f, 1.56945e-43f, 1.56945e-43f, 100.0f, 8.82818e-44f, 2.85865e-43f, 2.85865e-43f});
ALIAS(gTurnSpeeds, store_538B08, 0);
BSS(56DF18) int gWeaponCycleCount;
STORE(igb, 56DF18, 4) int store_56DF1C[30];
ALIAS(gWeaponCycle, store_56DF1C, 0);
BSS(56DF18) char gShowPosition;
PAD(igb, 56DF18, 56DF95, 0x3);
BSS(56DF18) int gAmmoLimitCount;
STORE(igb, 56DF18, 4) __typeof__(gAmmoLimits[0]) store_56DF9C[32];
ALIAS(gAmmoLimits, store_56DF9C, 0);
BSS(56DF18) void *gHumanPlayerParams;
PAD(igb, 56DF18, 56E0A0, 0x148);
ALIAS(gWeaponCategories, pad_56E0A0, 0);
BSS(56DF18) float gMoveScale;
BSS(56E1F8) float gLeanSide;
BSS(56E1F8) float gLeanUp;
BSS(56E1F8) int gLeanFrames;
BSS(56E1F8) float gMaxDamage;
BSS(56E210) QTaskList *gHumanPlayerList;
BSS(56E210) char gUnlimitedAmmo;
PAD(igb, 56E210, 56E215, 0x3);
