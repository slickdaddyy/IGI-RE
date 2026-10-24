/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "humanplayer/humanplayer.h"

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

typedef struct QTaskList {
    int count;
    int max;
    Task *items[1];
} QTaskList;

typedef struct ReloadMsg {
    HumanPlayer *h;
    int anim[7];
    char done;
    char unlimited;
} ReloadMsg;

typedef struct PositionMsg {
    HumanPlayer *h;
    int unk4;
    double pos[3];
} PositionMsg;

typedef struct WeaponCategory {
    int count;
    int ids[8];
} WeaponCategory;

typedef struct AmmoLimit {
    int id;
    int limit;
} AmmoLimit;

typedef struct Camera {
    char unk0[0x38];
    float m[10];
} Camera;

typedef struct Game {
    char unk0[0x5C];
    Camera *camera;
} Game;

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
ORIG(0x413C80, WeaponInfo *, Weapon_GetType, (Task *weapon));
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
ORIG(0x489E10, HitAnims *, HumanDamage_GetZone, (float *damage, int zone));
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
ORIG(0x4C6940, void, DynCube_Move, (void *world, HumanPlayer *h, double *pos, int unk));
ORIG(0x4C7140, void, DynCube_Add, (void *world, HumanPlayer *h, double *pos, int unk, int n));
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
ORIG(0x4EC190, void, StateMachine_Init, (StateMachine *sm, StateFn update, StateFn *states, int state,
                                         int combo_count, void *combos));
ORIG(0x4EC1E0, void, StateMachine_Update, (StateMachine *sm, HumanPlayer *h));
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
#define sub_4123F0 (FN(0x4123F0, HumanPlayer_InputChannelHandler))
#define sub_412420 (FN(0x412420, HumanPlayer_ReadInputHandler))
#define sub_412440 (FN(0x412440, HumanPlayer_HitHandler))
#define sub_412990 (FN(0x412990, HumanPlayer_BlastHandler))
#define sub_412CE0 (FN(0x412CE0, HumanPlayer_PunchHandler))
#define sub_412F80 (FN(0x412F80, HumanPlayer_FenceHandler))
#define sub_413090 (FN(0x413090, HumanPlayer_FlashbangHandler))
#define sub_413120 (FN(0x413120, HumanPlayer_RenderHandler))
#define HumanPlayer_MapMarkerHandler (FN(0x413730, HumanPlayer_MapMarkerHandler))
#define HumanPlayer_DeadRun ((StateFn)FN(0x45C740, Human_DeadRun))

#define gTaskHandlers ((void **(*)[0x180])0xA96AE0)
#define TASK_HANDLER(slot, type) (((void **)0xA96AE0)[(slot) * 0x180 + (type)])

#define gHumanPlayerType VAR(uint16_t, 0x5385B0)
#define gBodyStates ((StateFn *)0x5385B4)
#define gWeaponStates ((StateFn *)0x538678)
#define gBodyCombos ((void *)0x538710)
#define gAnimSlots ((void *)0x538870)
#define gMoveSpeeds ((float *)0x5389F8)
#define gTurnSpeeds ((float *)0x538B08)
#define gWeaponCycleCount VAR(int, 0x56DF18)
#define gWeaponCycle ((int *)0x56DF1C)
#define gShowPosition VAR(char, 0x56DF94)
#define gAmmoLimitCount VAR(int, 0x56DF98)
#define gAmmoLimits ((AmmoLimit *)0x56DF9C)
#define gUnk56E09C VAR(void *, 0x56E09C)
#define gWeaponCategories ((WeaponCategory *)0x56E0A0)
#define gMoveScale VAR(float, 0x56E1E8)
#define gJumpSpeed VAR(float, 0x56E1EC)
#define gJumpUp VAR(float, 0x56E1F0)
#define gHumanAirSpeed VAR(float, 0x56E1F4)
#define gLeanSide VAR(float, 0x56E1F8)
#define gLeanUp VAR(float, 0x56E1FC)
#define gLeanFrames VAR(int, 0x56E200)
#define gMaxDamage VAR(float, 0x56E204)
#define gUnk56E208 VAR(float, 0x56E208)
#define gHumanPlayerList VAR(QTaskList *, 0x56E210)
#define gUnlimitedAmmo VAR(char, 0x56E214)
#define gScriptNumberType ((void *)0x567C74)

#define kKmhToUnits VAR(float, 0x5334A4)
#define kPerFrameHour VAR(float, 0x5334A0)
#define kFlySpeed VAR(double, 0x533478)
#define kTwoPi VAR(float, 0x5334A8)
#define kRadToDeg VAR(float, 0x5334AC)
#define kUnitsToMeters VAR(double, 0x5334B0)
#define kUnitsToMetersF VAR(float, 0x5333E8)

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
    return h->input[i] != 0 && h->prev_input[i] == 0;
}

static inline void sm_set(StateMachine *sm, int state, HumanPlayer *h)
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

void HumanPlayer_BodyLeanRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_BodyComputerRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_BodyHitRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_BodyDeathRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_BodyFlyRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponDisabledRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponIdleRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponFireRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponDryFireRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponReloadRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponLowerRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponRaiseRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponLeanRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponThrowRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponReleaseRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_CycleWeapon(HumanPlayer *h, int force);
void HumanPlayer_WeaponPlaceRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponRecoverRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponMeleeRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponMedipackStartRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponMedipackRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponWaitRun(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_WeaponComputerRun(StateMachine *sm, HumanPlayer *h);
ScriptValue *DefineHumanPlayerGeneral(ScriptValue *ret, void *unused, void *args);
ScriptValue *DefineHumanPlayerWeaponCycle(ScriptValue *ret, void *unused, int *args);
ScriptValue *DefineHumanPlayerWeaponCategory(ScriptValue *ret, void *unused, void *args);
ScriptValue *DefineHumanPlayerAmmoLimit(ScriptValue *ret, void *unused, int *args);
void HumanPlayer_SetUnlimitedAmmo(void);
void HumanPlayer_CreateHandler(HumanPlayer *h, void *arg);
void HumanPlayer_UpdateBodyInput(StateMachine *sm, HumanPlayer *h);
void HumanPlayer_UpdateWeaponInput(StateMachine *sm, HumanPlayer *h);
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

void HumanPlayer_BodyLeanEnter(StateMachine *sm, HumanPlayer *h)
{
    if (h->crouch)
        Human_PlayAnimSlot(h, 0x13, BLEND);
    else
        Human_PlayAnimSlot(h, sm->state, BLEND);
    sm->run = HumanPlayer_BodyLeanRun;
}
HOOK(0x40DF70, HumanPlayer_BodyLeanEnter);

void HumanPlayer_BodyLeanRun(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;

    (void)sm;
    if (h->input[INPUT_LEFT] != 0) {
        if (h->prev_input[INPUT_LEFT] == 0)
            v->lean_speed[0] = gLeanSide / (long double)gLeanFrames;
        lean_towards(&v->lean[0], v->lean_speed[0], -gLeanSide);
    }
    if (h->input[INPUT_RIGHT] != 0) {
        if (h->prev_input[INPUT_RIGHT] == 0)
            v->lean_speed[0] = gLeanSide / (long double)gLeanFrames;
        lean_towards(&v->lean[0], v->lean_speed[0], gLeanSide);
    }
    if (h->input[INPUT_LEFT] == 0 && h->input[INPUT_RIGHT] == 0) {
        if (h->prev_input[INPUT_LEFT] != 0 || h->prev_input[INPUT_RIGHT] != 0)
            v->lean_speed[0] = gLeanSide / (long double)gLeanFrames;
        lean_towards(&v->lean[0], v->lean_speed[0], 0);
    }
    if (h->crouch) {
        if (h->input[INPUT_FORWARD] != 0) {
            if (h->prev_input[INPUT_FORWARD] == 0)
                v->lean_speed[2] = gLeanUp / (long double)gLeanFrames;
            lean_towards(&v->lean[2], v->lean_speed[2], gLeanUp);
        } else {
            if (h->prev_input[INPUT_FORWARD] != 0)
                v->lean_speed[2] = gLeanUp / (long double)gLeanFrames;
            lean_towards(&v->lean[2], v->lean_speed[2], 0);
        }
    }
}
HOOK(0x40DFC0, HumanPlayer_BodyLeanRun);

void HumanPlayer_BodyComputerEnter(StateMachine *sm, HumanPlayer *h)
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

void HumanPlayer_BodyComputerRun(StateMachine *sm, HumanPlayer *h)
{
    uint32_t in = sm->input;

    if (!pressed(h, INPUT_MAP) && !(in & SM_DEAD))
        return;
    Computer_Delete();
    HumanView_UnlockPitch(h->view);
    Human_UnfreezeView(h);
    h->unk670[0] = 0;
    h->unk670[1] = 0;
    h->unk670[2] = 0;
    if (in & SM_DEAD)
        sm_set(sm, BODY_DEATH, h);
    else
        sm_set(sm, 0, h);
}
HOOK(0x40E3C0, HumanPlayer_BodyComputerRun);

void HumanPlayer_BodyHitEnter(StateMachine *sm, HumanPlayer *h)
{
    HitAnims *a;

    h->crouch = 0;
    a = HumanDamage_GetZone(&h->damage, h->hit_zone);
    Anim_Play(h->anim, 0, a->hit, 0, BLEND);
    HumanPlayer_SetWeaponAnim(h, 0);
    HumanPlayer_SetFlagD44(h);
    sm->run = HumanPlayer_BodyHitRun;
}
HOOK(0x40E470, HumanPlayer_BodyHitEnter);

void HumanPlayer_BodyHitRun(StateMachine *sm, HumanPlayer *h)
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

void HumanPlayer_BodyDeathEnter(StateMachine *sm, HumanPlayer *h)
{
    HitAnims *a;
    unsigned int r;

    h->crouch = 0;
    a = HumanDamage_GetZone(&h->damage, h->hit_zone);
    r = Random_Int(Game_GetRandom(), 2);
    Anim_Play(h->anim, 0, a->death[r], 0, BLEND);
    HumanPlayer_SetWeaponAnim(h, 0);
    Human_DisableLook(h);
    HumanPlayer_SetFlagD44(h);
    HumanViewCam(h, 5);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_Say(h, "player_death_", 3);
    memcpy(h->unk664, h->unk688, sizeof(h->unk664));
    sm->run = HumanPlayer_BodyDeathRun;
}
HOOK(0x40E560, HumanPlayer_BodyDeathEnter);

void HumanPlayer_BodyDeathRun(StateMachine *sm, HumanPlayer *h)
{
    uint32_t in;
    Task *w;

    HumanPlayer_UpdateFall(h);
    in = sm->input;
    if (!(in & 4))
        return;
    memset(h->unk664, 0, sizeof(h->unk664));
    memset(h->unk670, 0, sizeof(h->unk670));
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

void HumanPlayer_BodyFlyEnter(StateMachine *sm, HumanPlayer *h)
{
    h->crouch = 0;
    Human_PlayAnimSlot(h, sm->state, BLEND);
    memset(h->unk664, 0, sizeof(h->unk664));
    memset(h->unk670, 0, sizeof(h->unk670));
    Human_LeaveGround(h);
    sm->run = HumanPlayer_BodyFlyRun;
}
HOOK(0x40E6E0, HumanPlayer_BodyFlyEnter);

void HumanPlayer_BodyFlyRun(StateMachine *sm, HumanPlayer *h)
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
    speed = speed * kFlySpeed;
    fspeed = speed;
    pos[0] = x * speed + pos[0];
    y = fspeed * y;
    z = fspeed * z;
    pos[1] = y + pos[1];
    pos[2] = z + pos[2];
    DynCube_Move(Game_GetWorld(), h, pos, h->unkA0);
}
HOOK(0x40E740, HumanPlayer_BodyFlyRun);

void HumanPlayer_WeaponDisabledEnter(StateMachine *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 0);
    sm->run = HumanPlayer_WeaponDisabledRun;
}
HOOK(0x40E960, HumanPlayer_WeaponDisabledEnter);

void HumanPlayer_WeaponDisabledRun(StateMachine *sm, HumanPlayer *h)
{
    if (!h->weapon_lock)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40E980, HumanPlayer_WeaponDisabledRun);

void HumanPlayer_WeaponIdleEnter(StateMachine *sm)
{
    sm->run = HumanPlayer_WeaponIdleRun;
}
HOOK(0x40E9B0, HumanPlayer_WeaponIdleEnter);

void HumanPlayer_WeaponIdleRun(StateMachine *sm, HumanPlayer *h)
{
    Task *w, *cur, *b, *att;
    WeaponInfo *info;
    WeaponAmmo ammo;
    AmmoEntry *a;
    TaskHandler fn;
    uint32_t in;
    uint16_t type;
    int arg;

    if (h->weapon_anim != h->weapon_anim_req)
        HumanPlayer_SetWeaponAnim(h, h->weapon_anim_req);
    w = WeaponSlots_Get(h->weapons, 0);
    in = sm->input;
    if (h->weapon_lock) {
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
                h->next_weapon = Weapon_GetType(h->saved_weapon)->id;
                h->saved_weapon = NULL;
            } else {
                WeaponSlots_Set(h->weapons, 0, b);
                h->saved_weapon = cur;
            }
            h->input[INPUT_FIRE] = 1.0f;
        }
    }
    if (h->saved_weapon)
        return;
    if (w) {
        info = Weapon_GetType(w);
        Weapon_GetAmmo(w, &ammo);
        if (in & 0x200) {
            switch (info->kind) {
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
                if (a && a->count && h->damage != 0) {
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
                h->unk4D2 = 1;
                sm_set(sm, WEAPON_PLACE, h);
                return;
            }
        }
no_action:
        if (((in & 0x800) || !ammo.clip) && (ammo.reserve || gUnlimitedAmmo) && ammo.clip < info->clip_size) {
            HumanTaskViewReset(h->view);
            sm_set(sm, WEAPON_RELOAD, h);
            return;
        }
        if (h->input[INPUT_LEAN] != 0 && (h->body.input & 4)) {
            HumanTaskViewReset(h->view);
            sm_set(sm, WEAPON_LEAN, h);
            return;
        }
    }
    if (h->next_weapon != -1) {
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
    if (pressed(h, INPUT_MAP) && (h->body.input & 4)) {
        HumanTaskViewReset(h->view);
        sm_set(sm, WEAPON_COMPUTER, h);
    }
}
HOOK(0x40E9C0, HumanPlayer_WeaponIdleRun);

void HumanPlayer_WeaponFireEnter(StateMachine *sm)
{
    sm->run = HumanPlayer_WeaponFireRun;
}
HOOK(0x40EF60, HumanPlayer_WeaponFireEnter);

void HumanPlayer_WeaponFireRun(StateMachine *sm, HumanPlayer *h)
{
    Task *w = WeaponSlots_Get(h->weapons, 0);
    WeaponInfo *info = Weapon_GetType(w);
    WeaponAmmo ammo;
    uint32_t in;

    Weapon_GetAmmo(w, &ammo);
    in = sm->input;
    if (h->weapon_lock) {
        sm_set(sm, WEAPON_DISABLED, h);
        return;
    }
    if (h->input[INPUT_FIRE] != 0) {
        if (!ammo.clip && (in & SM_ANIM_DONE)) {
            sm_set(sm, WEAPON_DRY_FIRE, h);
            return;
        }
        h->fire = 1;
    } else if ((h->input[INPUT_RELOAD] != 0 || !ammo.clip) && ammo.reserve && ammo.clip < info->clip_size) {
        HumanTaskViewReset(h->view);
        sm_set(sm, WEAPON_RELOAD, h);
        return;
    }
    if (h->unk4D3)
        HumanPlayer_SetWeaponAnim(h, 6);
    if ((in & SM_ANIM_DONE) && !(in & 0x200))
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40EF70, HumanPlayer_WeaponFireRun);

void HumanPlayer_WeaponDryFireEnter(StateMachine *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 1);
    Sound_Play(h->view, "guns_dry_1");
    sm->run = HumanPlayer_WeaponDryFireRun;
}
HOOK(0x40F0D0, HumanPlayer_WeaponDryFireEnter);

void HumanPlayer_WeaponDryFireRun(StateMachine *sm, HumanPlayer *h)
{
    if (h->weapon_lock)
        sm_set(sm, WEAPON_DISABLED, h);
    else if (h->input[INPUT_FIRE] == 0)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40F100, HumanPlayer_WeaponDryFireRun);

void HumanPlayer_WeaponReloadEnter(StateMachine *sm)
{
    sm->run = HumanPlayer_WeaponReloadRun;
}
HOOK(0x40F170, HumanPlayer_WeaponReloadEnter);

void HumanPlayer_WeaponReloadRun(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    Task *w = WeaponSlots_Get(h->weapons, 0);
    ReloadMsg msg;
    uint8_t slot;

    h->unk4D0 = 0;
    msg.h = h;
    memcpy(msg.anim, v->unk208, sizeof(msg.anim));
    msg.unlimited = gUnlimitedAmmo;
    slot = Human_GetEvent1();
    ((TaskHandler)TASK_HANDLER(slot, w->type))(w, &msg);
    if (msg.done) {
        h->unk4D0 = 1;
        sm_set(sm, WEAPON_IDLE, h);
    }
}
HOOK(0x40F180, HumanPlayer_WeaponReloadRun);

void HumanPlayer_WeaponSwitchEnter(StateMachine *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 9);
    HumanView_SetWeaponTarget(h->view, WEAPON_DOWN_X, WEAPON_DOWN_Y, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    h->unk4D0 = 0;
    sm->run = HumanPlayer_WeaponLowerRun;
}
HOOK(0x40F230, HumanPlayer_WeaponSwitchEnter);

void HumanPlayer_WeaponLowerRun(StateMachine *sm, HumanPlayer *h)
{
    Task *w;

    if (!HumanView_WeaponAtTarget(h->view))
        return;
    w = Task_FindWeapon(h, h->next_weapon);
    if (w)
        WeaponSlots_Set(h->weapons, 0, w);
    h->next_weapon = -1;
    HumanPlayer_SetWeaponAnim(h, 10);
    HumanView_SetWeaponTarget(h->view, 0, 0, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    sm->run = HumanPlayer_WeaponRaiseRun;
}
HOOK(0x40F280, HumanPlayer_WeaponLowerRun);

void HumanPlayer_WeaponRaiseRun(StateMachine *sm, HumanPlayer *h)
{
    if (h->next_weapon != -1) {
        sm_set(sm, WEAPON_SWITCH, h);
    } else if (HumanView_WeaponAtTarget(h->view)) {
        h->unk4D0 = 1;
        sm_set(sm, WEAPON_IDLE, h);
    }
}
HOOK(0x40F300, HumanPlayer_WeaponRaiseRun);

void HumanPlayer_WeaponLeanEnter(StateMachine *sm, HumanPlayer *h)
{
    HumanPlayer_SetWeaponAnim(h, 1);
    Human_BeginUse(h);
    sm_set(&h->body, BODY_LEAN, h);
    HumanView_SetWeaponTarget(h->view, WEAPON_DOWN_X, WEAPON_DOWN_Y, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    sm->run = HumanPlayer_WeaponLeanRun;
}
HOOK(0x40F370, HumanPlayer_WeaponLeanEnter);

void HumanPlayer_WeaponLeanRun(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;

    if (h->input[INPUT_LEAN] != 0 && !(sm->input & SM_DEAD))
        return;
    Human_EndUse(h);
    v->lean[0] = 0;
    v->lean[1] = 0;
    v->lean[2] = 0;
    sm_set(&h->body, 0, h);
    HumanView_SetWeaponTarget(h->view, 0, 0, WEAPON_SPEED_X, WEAPON_SPEED_Y);
    sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40F3F0, HumanPlayer_WeaponLeanRun);

void HumanPlayer_WeaponAltFireEnter(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponInfo *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));

    h->unk4D2 = 1;
    Anim_Play(v->anim, 0, info->anim688, 0, 0);
    sm->run = HumanPlayer_WeaponWaitRun;
}
HOOK(0x40F4A0, HumanPlayer_WeaponAltFireEnter);

void HumanPlayer_WeaponThrowEnter(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponInfo *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));
    char *p;
    long double fuse;

    Anim_Play(v->anim, 0, info->anim684, 0, 0);
    p = PhysicsObjType_Find(info->unk61C);
    fuse = **(float **)(p + 0x2D4) * VAR(float, 0x5333BC);
    h->grenade_ticking = 0;
    h->grenade_ticks = 0;
    h->grenade_sound = 0;
    h->grenade_fuse = (int)llrintl(fuse);
    sm->run = HumanPlayer_WeaponThrowRun;
}
HOOK(0x40F4F0, HumanPlayer_WeaponThrowEnter);

void HumanPlayer_WeaponThrowRun(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponInfo *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));

    if (v->unk208[1] == 4) {
        h->grenade_ticking = 1;
        h->grenade_sound = Sound_Play(h->view, "grenade_tick_1");
    }
    if (h->grenade_ticking)
        h->grenade_ticks++;
    if ((sm->input & SM_ANIM_DONE) && h->input[INPUT_FIRE] == 0) {
        Anim_Play(v->anim, 0, info->anim688, 0, 0);
        sm->run = HumanPlayer_WeaponReleaseRun;
    }
}
HOOK(0x40F590, HumanPlayer_WeaponThrowRun);

void HumanPlayer_WeaponReleaseRun(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    Task *w = WeaponSlots_Get(h->weapons, 0);
    WeaponInfo *info = Weapon_GetType(w);
    WeaponAmmo ammo;

    h->grenade_ticks++;
    if (v->unk208[1] == 3) {
        h->fire = 1;
        if (h->grenade_sound) {
            Sound_Stop(h->grenade_sound);
            h->grenade_sound = 0;
        }
    }
    if (!(sm->input & SM_ANIM_DONE))
        return;
    Anim_Play(v->anim, 0, info->anim67C, 0, 0);
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

    if (h->cycle_index == -1 || force || pressed(h, INPUT_NEXT_WEAPON))
        dir = 1;
    if (pressed(h, INPUT_PREV_WEAPON))
        dir = -1;
    else if (!dir)
        goto select;
    if (h->saved_weapon) {
        id = Weapon_GetType(h->saved_weapon)->id;
        h->saved_weapon = NULL;
        if (id != -1)
            goto done;
        goto select;
    }
    for (n = gWeaponCycleCount; n; ) {
        n--;
        h->cycle_index += dir;
        if (h->cycle_index >= gWeaponCycleCount)
            h->cycle_index = 0;
        else if (h->cycle_index < 0)
            h->cycle_index = gWeaponCycleCount - 1;
        id = gWeaponCycle[h->cycle_index];
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
                    h->cycle_index = i;
            }
        }
    }
done:
    if (id != cur && Task_FindWeapon(h, id))
        h->next_weapon = id;
}
HOOK(0x40F760, HumanPlayer_CycleWeapon);

void HumanPlayer_WeaponPlaceEnter(StateMachine *sm, HumanPlayer *h)
{
    h->fire = 1;
    sm->run = HumanPlayer_WeaponPlaceRun;
}
HOOK(0x40F930, HumanPlayer_WeaponPlaceEnter);

void HumanPlayer_WeaponPlaceRun(StateMachine *sm, HumanPlayer *h)
{
    Task *w;
    WeaponAmmo ammo;

    if (!h->unk4D3) {
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

void HumanPlayer_WeaponRecoverRun(StateMachine *sm, HumanPlayer *h)
{
    if (HumanView_WeaponAtTarget(h->view))
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40FA20, HumanPlayer_WeaponRecoverRun);

void HumanPlayer_WeaponMeleeEnter(StateMachine *sm, HumanPlayer *h)
{
    h->melee_count = 0;
    sm->run = HumanPlayer_WeaponMeleeRun;
}
HOOK(0x40FA60, HumanPlayer_WeaponMeleeEnter);

void HumanPlayer_WeaponMeleeRun(StateMachine *sm, HumanPlayer *h)
{
    Task *w = WeaponSlots_Get(h->weapons, 0);
    WeaponAmmo ammo;
    uint32_t in;

    Weapon_GetType(w);
    Weapon_GetAmmo(w, &ammo);
    if (h->input[INPUT_FIRE] != 0 && h->melee_count < 3)
        h->fire = 1;
    in = sm->input;
    if (h->unk4D3) {
        HumanPlayer_SetWeaponAnim(h, 8);
        h->melee_count++;
    }
    if (in & SM_ANIM_DONE)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40FA80, HumanPlayer_WeaponMeleeRun);

void HumanPlayer_WeaponMedipackEnter(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponInfo *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));

    Anim_Play(v->anim, 0, info->anim684, 0, 0);
    h->medipack_delay = 0;
    sm->run = HumanPlayer_WeaponMedipackStartRun;
}
HOOK(0x40FB30, HumanPlayer_WeaponMedipackEnter);

void HumanPlayer_WeaponMedipackStartRun(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponInfo *info;

    if (!(sm->input & SM_ANIM_DONE))
        return;
    info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));
    Anim_Play(v->anim, 0, info->anim688, 1, 0);
    sm->run = HumanPlayer_WeaponMedipackRun;
}
HOOK(0x40FB90, HumanPlayer_WeaponMedipackStartRun);

void HumanPlayer_WeaponMedipackRun(StateMachine *sm, HumanPlayer *h)
{
    HumanView *v = h->view;
    WeaponInfo *info = Weapon_GetType(WeaponSlots_Get(h->weapons, 0));
    AmmoEntry *a;
    long double t;

    if (h->input[INPUT_FIRE] == 0) {
        Anim_Play(v->anim, 0, Weapon_GetType(WeaponSlots_Get(h->weapons, 0))->anim68C, 0, 0);
        sm->run = HumanPlayer_WeaponWaitRun;
    }
    if (h->medipack_delay) {
        h->medipack_delay--;
        return;
    }
    a = AmmoList_Find(h->ammo, Script_EvalConst("AMMO_ID_MEDIPACK"));
    if (!a)
        return;
    if (!a->count || h->damage == 0) {
        Anim_Play(v->anim, 0, Weapon_GetType(WeaponSlots_Get(h->weapons, 0))->anim68C, 0, 0);
        sm->run = HumanPlayer_WeaponWaitRun;
        return;
    }
    a->count--;
    t = h->damage - h->max_damage * *info->heal;
    h->damage = t;
    if (t < 0)
        h->damage = 0;
    else if (t > h->max_damage)
        h->damage = h->max_damage;
    h->medipack_delay = (int)llrintl(60.0 / (long double)info->unk644 * 30.0);
}
HOOK(0x40FBE0, HumanPlayer_WeaponMedipackRun);

void HumanPlayer_WeaponWaitRun(StateMachine *sm, HumanPlayer *h)
{
    if (sm->input & SM_ANIM_DONE)
        sm_set(sm, WEAPON_IDLE, h);
}
HOOK(0x40FD70, HumanPlayer_WeaponWaitRun);

void HumanPlayer_WeaponComputerEnter(StateMachine *sm, HumanPlayer *h)
{
    sm_set(&h->body, BODY_COMPUTER, h);
    h->unk4D0 = 0;
    sm->run = HumanPlayer_WeaponComputerRun;
}
HOOK(0x40FDA0, HumanPlayer_WeaponComputerEnter);

void HumanPlayer_WeaponComputerRun(StateMachine *sm, HumanPlayer *h)
{
    if (h->body.state != BODY_COMPUTER) {
        h->unk4D0 = 1;
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
    TaskType_SetHandler(gHumanPlayerType, Vehicle_GetInputInfoEvent(), sub_4123F0);
    TaskType_SetHandler(gHumanPlayerType, Vehicle_GetKeysEvent(), sub_412420);
    TaskType_SetHandler(gHumanPlayerType, Game_GetEvent9(), sub_412440);
    TaskType_SetHandler(gHumanPlayerType, Game_GetEvent11(), sub_412990);
    TaskType_SetHandler(gHumanPlayerType, Human_GetPunchEvent(), sub_412CE0);
    TaskType_SetHandler(gHumanPlayerType, Game_GetEvent12(), sub_412F80);
    TaskType_SetHandler(gHumanPlayerType, FlashBang_GetEvent(), sub_413090);
    TaskType_SetHandler(gHumanPlayerType, StatusScreen_GetEvent(), sub_413120);
    TaskType_SetHandler(gHumanPlayerType, ComputerObject_GetEvent0(), HumanPlayer_MapMarkerHandler);
    gUnk56E09C = ParamTable_Extend(Human_GetParamTable(), 1);
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
    ret->type = gScriptNumberType;
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
    gUnk56E208 = Script_GetArgReal(args, 8);
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
    BoneObj_Update(h->matrix, h->anim);
    memcpy(h->unk650, h->motion->unk4C, sizeof(h->unk650));
    DynCube_Add(Game_GetWorld(), h, pos, h->unkA0, 2);
    HumanView_AddToWorld(h->view);
    for (i = 0; i < gAmmoLimitCount; i++)
        AmmoList_SetLimit(h->ammo, gAmmoLimits[i].id, gAmmoLimits[i].limit);
    StateMachine_Init(&h->body, HumanPlayer_UpdateBodyInput, gBodyStates, 0, 2, gBodyCombos);
    h->anim_slots = gAnimSlots;
    Human_InitDamage(h, gMoveSpeeds);
    h->max_damage = gMaxDamage;
    StateMachine_Init(&h->weapon, HumanPlayer_UpdateWeaponInput, gWeaponStates, WEAPON_IDLE, 0, NULL);
    h->weapon_lock = 0;
    h->unkD45 = 0;
    h->cycle_index = -1;
    h->next_weapon = -1;
    h->unkD50 = -1;
    h->unkD54 = 0;
    h->weapon_anim = 0;
    h->weapon_anim_req = 0;
    h->unkD68 = 0;
    h->unkD78 = 0;
    h->unkD79 = 0;
    h->unkD7A = 0;
    h->unkD8C = 0;
    h->unkD90 = 0;
    h->unkD91 = 0;
    h->unkD92 = 0;
    h->unkD93 = 0;
    h->unkD94 = 0;
    h->unkD9C = 0;
    h->unkDA0 = 0;
    h->unkDA1 = 0;
    h->unkDBC = NULL;
    memset(h->unkDC0, 0, sizeof(h->unkDC0));
    h->saved_weapon = NULL;
    h->unkDE8 = NULL;
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

void HumanPlayer_UpdateBodyInput(StateMachine *sm, HumanPlayer *h)
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

void HumanPlayer_UpdateWeaponInput(StateMachine *sm, HumanPlayer *h)
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
    if (h->unkDE8) {
        TaskRef_Remove(h->unkDE8);
        h->unkDE8 = NULL;
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
    *out = gUnk56E09C;
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
    if (h->unkCF6) {
        h->unkCF6 = 0;
        slot = World_GetEvent1();
        ((TaskHandler)TASK_HANDLER(slot, BoneDynCubeObj_GetType()))(h, arg);
    }
}
HOOK(0x410900, HumanPlayer_DeactivateHandler);

void HumanPlayer_InputHandler(HumanPlayer *h, float **msg)
{
    if (!h->input_locked) {
        memcpy(h->prev_input, h->input, sizeof(h->input));
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
    Camera *cam;

    HumanPlayer_CheckDeath(h);
    Human_UpdatePlatformMove(h);
    Human_UpdateUse(h);
    if (!Computer_Get()) {
        HumanPlayer_SelectWeaponCategory(h);
        HumanPlayer_CycleWeapon(h, 0);
    }
    StateMachine_Update(&h->body, h);
    StateMachine_Update(&h->weapon, h);
    if (pressed(h, INPUT_TOGGLE_D45))
        h->unkD45 ^= 1;
    if (h->body.state == BODY_FLY) {
        if (pressed(h, INPUT_FLY)) {
            memset(h->unk664, 0, sizeof(h->unk664));
            memset(h->unk670, 0, sizeof(h->unk670));
            sm_set(&h->body, 0, h);
        }
    } else if (pressed(h, INPUT_FLY)) {
        sm_set(&h->body, BODY_FLY, h);
    }
    if (h->unkD78 || h->unkD79)
        HumanPlayer_UpdateLadder(h);
    if (h->unkD90 || h->unkD91)
        HumanPlayer_UpdateFenceClimb(h);
    if (h->unkD93 || h->unkD94)
        HumanPlayer_UpdateWire(h);
    if (h->unkDA0 || h->unkDA1)
        HumanPlayer_UpdateZipline(h);
    h->body.input &= ~SM_HIT;
    Human_UpdateBody(h);
    HumanView_UpdateView(h->view, h);
    h->unkD8 = 1;
    BoneObj_GetRootMotion(h->unk514, d, h->anim);
    if (!h->delta_scaled) {
        d[0] = gMoveScale * d[0];
        d[1] = gMoveScale * d[1];
        d[2] = gMoveScale * d[2];
    }
    x = h->matrix[2] * d[2] + h->matrix[1] * d[1] + h->matrix[0] * d[0];
    y = h->matrix[5] * d[2] + h->matrix[4] * d[1] + h->matrix[3] * d[0];
    z = h->matrix[8] * d[2] + h->matrix[7] * d[1] + h->matrix[6] * d[0];
    d[0] = x;
    d[1] = y;
    d[2] = z;
    Human_FireAnimTrigger(h);
    if (h->body.state != BODY_FLY) {
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
    speed = h->unk664[2];
    if (h->unk664[2] < 0)
        speed = -speed;
    Human_printf(h, "Speed Z (m/s): %.2f\n", (double)(speed * kUnitsToMetersF * VAR(float, 0x5333BC)));
    Human_printf(h, "Ground task: %x\n", Human_GetGround(h));
    Human_printf(h, "Attach task: %x\n", h->attach);
    Human_printf(h, "Delta move pos X: %.2f\n", (double)h->move_pos[0]);
    Human_printf(h, "Delta move pos Y: %.2f\n", (double)h->move_pos[1]);
    Human_printf(h, "Delta move pos Z: %.2f\n", (double)h->move_pos[2]);
    Human_printf(h, "Delta move Alpha: %.2f\n", (double)h->move_angle[0]);
    Human_printf(h, "Delta move Beta: %.2f\n", (double)h->move_angle[1]);
    Human_printf(h, "Delta move Gamma: %.2f\n", (double)h->move_angle[2]);
    Human_printf(h, "Distance to ground from feet: %.2fm\n", (double)(h->ground_distance * kUnitsToMeters));
    Human_printf(h, "Ground angle: %.3f\n", (double)(h->ground_angle * kRadToDeg));
    Human_printf(h, "Delta translation X: %.2f\n", (double)d[0]);
    Human_printf(h, "Delta translation Y: %.2f\n", (double)h->delta[1]);
    Human_printf(h, "Delta translation Z: %.2f\n", (double)h->delta[2]);
    if (h->body.input & 4)
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
