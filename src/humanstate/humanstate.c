#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "humanstate/humanstate.h"
#include "humanai/humanaitypes.h"

ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x408460, void, HumanPlayer_UpdateFall, (Human *h));
ORIG(0x416850, char, Game_GetEvent8, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4388A0, int, PlaceExplosiveTBA_GetTargetId, (void *obj));
ORIG(0x4137E0, void, HumanPlayer_SetWeaponAnim, (Human *h, int a));
ORIG(0x413A60, void, HumanPlayer_RequestWeaponAnim, (Human *h, int a));
ORIG(0x42FDD0, void *, Car_GetMountedGun, (void *obj));
ORIG(0x42FDF0, void, Car_GetGunnerPos, (void *obj, double *pos, float *mat));
ORIG(0x43E4E0, uint16_t, Elevator_GetType, (void));
ORIG(0x43F8A0, char, Wire_IsSloped, (void *obj));
ORIG(0x440270, long double, Fence_GetClimbTime, (void));
ORIG(0x443EA0, void, Terminal_SetHacked, (void *obj));
ORIG(0x444B30, void, Obj_GetUsePoint, (void *obj, double *pos, float *mat));
ORIG(0x444B80, long double, Cabinet_GetSearchTime, (void *obj));
ORIG(0x444B90, void, Cabinet_SetSearched, (void *obj));
ORIG(0x444BA0, void, Cabinet_Reset, (void *obj));
ORIG(0x444BC0, void, Switch_Press, (void *obj));
ORIG(0x447D30, long double, Door_GetPickTime, (void *obj));
ORIG(0x447D40, char, Door_IsLocked, (void *obj));
ORIG(0x447D50, char, Door_IsPickable, (void *obj));
ORIG(0x447D60, void, Door_SetPicked, (void *obj, int a));
ORIG(0x447DA0, void, Door_Toggle, (void *obj));
ORIG(0x448570, void, GenericTBA_GetUsePos, (void *obj, double *pos, float *mat));
ORIG(0x4485C0, long double, GenericTBA_GetUseTime, (void *obj));
ORIG(0x4485D0, short, GenericTBA_GetAnim, (void *obj, int a, int b));
ORIG(0x450CE0, void, HumanAI_Broadcast, (void *noise));
ORIG(0x460CE0, void, Human_MarkDelete, (void *obj));
ORIG(0x4631A0, void, Human_SetPosition, (Human *h, double *pos));
ORIG(0x463220, void, Human_SetYaw, (Human *h, float yaw));
ORIG(0x463260, void, Human_SnapToGround, (Human *h));
ORIG(0x4632D0, void, Human_LeaveGround, (Human *h));
ORIG(0x463310, void, Human_SetPlatform, (Human *h, void *obj));
ORIG(0x463550, void, Human_ClearPlatform, (Human *h));
ORIG(0x463570, void, Human_DisableCollision, (Human *h));
ORIG(0x463580, void, Human_EnableCollision, (Human *h));
ORIG(0x463590, void, Human_SetExactMotion, (Human *h));
ORIG(0x4635A0, void, Human_ClearExactMotion, (Human *h));
ORIG(0x4635F0, void, Human_FreezeView, (Human *h));
ORIG(0x463600, void, Human_UnfreezeView, (Human *h));
ORIG(0x463610, void, Human_BeginUse, (Human *h));
ORIG(0x463620, void, Human_EndUse, (Human *h));
ORIG(0x4636A0, void, Human_SetClimbObject, (Human *h, int a));
ORIG(0x4636E0, void *, Human_GetClimbObject, (Human *h));
ORIG(0x463700, void, Human_SetAttached, (Human *h, void *a));
ORIG(0x463740, int, Human_GetAttached, (Human *h));
ORIG(0x463760, void, HumanViewCam, (Human *h, int mode));
ORIG(0x464700, void, HumanView_LockPitch, (void *view));
ORIG(0x464710, void, HumanView_UnlockPitch, (void *view));
ORIG(0x464720, void, HumanView_ClearAngles, (void *view));
ORIG(0x465800, void, HumanView_GetEye, (double *pos, void *view));
ORIG(0x475340, void, StationaryGun_Release, (void *obj));
ORIG(0x489E70, void, Human_PlayAnimSlot, (Human *h, int anim, float blend));
ORIG(0x489EA0, void, Human_PlayAnim, (Human *h, int anim, int loop, float blend));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B2D00, void, Vec3_ToAngles, (const float *v, float *pitch, float *yaw));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (float *m, float ax, float ay, float az));
ORIG(0x4B3790, void, Matrix_RotateZ, (float *out, const float *m, float a));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4C3440, void *, DynCubeObj_GetPosition, (Human *h));
ORIG(0x4CC2A0, void, Coll_Lines, (void *world, void *result, const double *from, const double *to, int a, char b,
                                  void *filter, unsigned flags));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (void *a, float *move, void *anim));
ORIG(0x4D64E0, long double, Anim_GetLength, (void *anim, int track));
ORIG(0x4D6520, long double, Anim_GetPhase, (void *anim, int track));
ORIG(0x4D6580, void, Anim_SetSpeed, (void *anim, int track, float speed));
ORIG(0x4D65B0, void, Anim_SetTime, (void *anim, int track, float time));
ORIG(0x4D7040, int, Anim_GetLengthTicks, (int a, int b));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (void *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, const double *pos));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (const char *name));
ORIG(0x4F1030, void *, Task_FindById, (int id));
ORIG(0x4F3380, uint16_t, EditRigidObj_GetType, (void));

extern float gJumpSpeed;
extern float gJumpUp;
extern const int gLadderSounds[];
extern const int gWireDragSounds[];

#define PI_F 0x1.921fb6p+1f
#define HALF_PI_F 0x1.921fb6p+0f
#define FPS 30.0f

/* fcomp: equal also when unordered */
static int x87_eq(long double a, long double b)
{
    return !(a < b || a > b);
}

static long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

/* round to float or double like a store would, keeping the value usable */
static float x87_float(long double x)
{
    float f;

    __asm__("fsts %0" : "=m"(f) : "t"(x));
    return f;
}

static double x87_double(long double x)
{
    double d;

    __asm__("fstl %0" : "=m"(d) : "t"(x));
    return d;
}

/* fistp with the current rounding mode, low 32 bits */
static int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

static void State_Change(HumanState *sm, Human *h, int state)
{
    sm->prev = sm->state;
    sm->state = state;
    sm->update = sm->enter[state];
    sm->update(sm, h);
}

static void Human_Noise(Human *h, int type, void *a, void *b)
{
    AIMessage n;

    n.type = type;
    n.team = h->unkFC;
    n.flags = 0x46;
    n.radius = 20480.0f;
    n.pos = DynCubeObj_GetPosition(h);
    n.from = a;
    n.about = b;
    HumanAI_Broadcast(&n);
}

/* --- running ------------------------------------------------------------ */

static float Run_Start(HumanState *sm, Human *h)
{
    float phase;

    h->unk33D = 1;
    phase = x87_float(Anim_GetPhase(h->anim, 0));
    Human_PlayAnimSlot(h, sm->state, 0.2f);
    return phase;
}

static void Run_Finish(HumanState *sm, Human *h, int keepPhase, float phase, HumanStateFn update)
{
    if (keepPhase)
        Anim_SetTime(h->anim, 0, (float)(Anim_GetLength(h->anim, 0) * phase));
    HumanPlayer_RequestWeaponAnim(h, 5);
    sm->update = update;
}

/* falling, unk10000 and dying win over everything else */
static int Run_Common(HumanState *sm, Human *h, unsigned in)
{
    if (!(in & HUMAN_ON_GROUND)) {
        State_Change(sm, h, HSTATE_FALL);
        return 1;
    }
    if (in & HUMAN_UNK10000) {
        State_Change(sm, h, HSTATE_UNK46);
        return 1;
    }
    if (in & HUMAN_DEAD) {
        State_Change(sm, h, HSTATE_DIE);
        return 1;
    }
    return 0;
}

static void Run_Stop(HumanState *sm, Human *h, unsigned in, int walk, int crouch)
{
    if (in & HUMAN_RUN)
        return;
    State_Change(sm, h, (in & HUMAN_CROUCH) ? crouch : walk);
}

void HumanPlayer_RunForwardLeftUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (Run_Common(sm, h, in))
        return;
    if (!(in & HUMAN_LEFT)) {
        State_Change(sm, h, HSTATE_RUN_FORWARD);
        return;
    }
    if (!(in & HUMAN_FORWARD)) {
        State_Change(sm, h, HSTATE_RUN_LEFT);
        return;
    }
    Run_Stop(sm, h, in, HSTATE_WALK_FORWARD_LEFT, HSTATE_CROUCH_FORWARD_LEFT);
}
HOOK(0x40AEF0, HumanPlayer_RunForwardLeftUpdate);

void HumanPlayer_RunForwardRightUpdate(HumanState *sm, Human *h);

void HumanPlayer_RunForwardRightEnter(HumanState *sm, Human *h)
{
    float phase = Run_Start(sm, h);
    int prev = sm->prev;

    Run_Finish(sm, h, prev == HSTATE_WALK_FORWARD_RIGHT || prev == HSTATE_RUN_FORWARD || prev == HSTATE_RUN_RIGHT,
               phase, HumanPlayer_RunForwardRightUpdate);
}
HOOK(0x40B020, HumanPlayer_RunForwardRightEnter);

void HumanPlayer_RunForwardRightUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (Run_Common(sm, h, in))
        return;
    if (!(in & HUMAN_RIGHT)) {
        State_Change(sm, h, HSTATE_RUN_FORWARD);
        return;
    }
    if (!(in & HUMAN_FORWARD)) {
        State_Change(sm, h, HSTATE_RUN_RIGHT);
        return;
    }
    Run_Stop(sm, h, in, HSTATE_WALK_FORWARD_RIGHT, HSTATE_CROUCH_FORWARD_RIGHT);
}
HOOK(0x40B0A0, HumanPlayer_RunForwardRightUpdate);

void HumanPlayer_RunBackUpdate(HumanState *sm, Human *h);

void HumanPlayer_RunBackEnter(HumanState *sm, Human *h)
{
    float phase = Run_Start(sm, h);

    Run_Finish(sm, h, sm->prev == HSTATE_CROUCH_BACK, phase, HumanPlayer_RunBackUpdate);
}
HOOK(0x40B1D0, HumanPlayer_RunBackEnter);

void HumanPlayer_RunBackUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (Run_Common(sm, h, in))
        return;
    if (in & HUMAN_LEFT) {
        State_Change(sm, h, HSTATE_RUN_BACK_LEFT);
        return;
    }
    if (in & HUMAN_RIGHT) {
        State_Change(sm, h, HSTATE_RUN_BACK_RIGHT);
        return;
    }
    if (!(in & HUMAN_BACK)) {
        State_Change(sm, h, HSTATE_RUN_STAND);
        return;
    }
    Run_Stop(sm, h, in, HSTATE_WALK_BACK, HSTATE_CROUCH_BACK);
}
HOOK(0x40B240, HumanPlayer_RunBackUpdate);

void HumanPlayer_RunBackLeftUpdate(HumanState *sm, Human *h);

void HumanPlayer_RunBackLeftEnter(HumanState *sm, Human *h)
{
    float phase = Run_Start(sm, h);
    int prev = sm->prev;

    Run_Finish(sm, h, prev == HSTATE_WALK_BACK_LEFT || prev == HSTATE_CROUCH_BACK_LEFT || prev == HSTATE_RUN_BACK,
               phase, HumanPlayer_RunBackLeftUpdate);
}
HOOK(0x40B390, HumanPlayer_RunBackLeftEnter);

void HumanPlayer_RunBackLeftUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (Run_Common(sm, h, in))
        return;
    if (!(in & HUMAN_BACK)) {
        State_Change(sm, h, HSTATE_RUN_LEFT);
        return;
    }
    if (!(in & HUMAN_LEFT)) {
        State_Change(sm, h, HSTATE_RUN_BACK);
        return;
    }
    Run_Stop(sm, h, in, HSTATE_WALK_BACK_LEFT, HSTATE_CROUCH_BACK_LEFT);
}
HOOK(0x40B410, HumanPlayer_RunBackLeftUpdate);

void HumanPlayer_RunBackRightUpdate(HumanState *sm, Human *h);

void HumanPlayer_RunBackRightEnter(HumanState *sm, Human *h)
{
    float phase = Run_Start(sm, h);
    int prev = sm->prev;

    Run_Finish(sm, h, prev == HSTATE_WALK_BACK_RIGHT || prev == HSTATE_CROUCH_BACK_RIGHT || prev == HSTATE_RUN_BACK,
               phase, HumanPlayer_RunBackRightUpdate);
}
HOOK(0x40B540, HumanPlayer_RunBackRightEnter);

void HumanPlayer_RunBackRightUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (Run_Common(sm, h, in))
        return;
    if (!(in & HUMAN_BACK)) {
        State_Change(sm, h, HSTATE_RUN_RIGHT);
        return;
    }
    if (!(in & HUMAN_RIGHT)) {
        State_Change(sm, h, HSTATE_RUN_BACK);
        return;
    }
    Run_Stop(sm, h, in, HSTATE_WALK_BACK_RIGHT, HSTATE_CROUCH_BACK_RIGHT);
}
HOOK(0x40B5C0, HumanPlayer_RunBackRightUpdate);

void HumanPlayer_RunLeftUpdate(HumanState *sm, Human *h);

void HumanPlayer_RunLeftEnter(HumanState *sm, Human *h)
{
    float phase = Run_Start(sm, h);

    Run_Finish(sm, h, sm->prev == HSTATE_WALK_LEFT, phase, HumanPlayer_RunLeftUpdate);
}
HOOK(0x40B6F0, HumanPlayer_RunLeftEnter);

void HumanPlayer_RunLeftUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (Run_Common(sm, h, in))
        return;
    if (!(in & HUMAN_LEFT)) {
        State_Change(sm, h, HSTATE_RUN_STAND);
        return;
    }
    if (in & HUMAN_FORWARD) {
        State_Change(sm, h, HSTATE_RUN_FORWARD_LEFT);
        return;
    }
    if (in & HUMAN_BACK) {
        State_Change(sm, h, HSTATE_RUN_BACK_LEFT);
        return;
    }
    Run_Stop(sm, h, in, HSTATE_WALK_LEFT, HSTATE_CROUCH_LEFT);
}
HOOK(0x40B760, HumanPlayer_RunLeftUpdate);

void HumanPlayer_RunRightUpdate(HumanState *sm, Human *h);

void HumanPlayer_RunRightEnter(HumanState *sm, Human *h)
{
    float phase = Run_Start(sm, h);

    Run_Finish(sm, h, sm->prev == HSTATE_WALK_RIGHT, phase, HumanPlayer_RunRightUpdate);
}
HOOK(0x40B8B0, HumanPlayer_RunRightEnter);

void HumanPlayer_RunRightUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (Run_Common(sm, h, in))
        return;
    if (!(in & HUMAN_RIGHT)) {
        State_Change(sm, h, HSTATE_RUN_STAND);
        return;
    }
    if (in & HUMAN_FORWARD) {
        State_Change(sm, h, HSTATE_RUN_FORWARD_RIGHT);
        return;
    }
    if (in & HUMAN_BACK) {
        State_Change(sm, h, HSTATE_RUN_BACK_RIGHT);
        return;
    }
    Run_Stop(sm, h, in, HSTATE_WALK_RIGHT, HSTATE_CROUCH_RIGHT);
}
HOOK(0x40B920, HumanPlayer_RunRightUpdate);

/* --- jumping and falling ------------------------------------------------ */

static void Human_Land(Human *h)
{
    void *sound;
    char *name;

    if (h->ground) {
        /* the name is also left on the stack as a second argument */
        name = h->ground + 0x114;
        sound = Sound_Play(Level_GetDynamic(name), name);
        if (sound)
            Sound_SetPos(sound, h->unk6A8);
    }
    h->vel[0] = 0;
    h->vel[1] = 0;
    h->vel[2] = 0;
    h->unk670[0] = 0;
    h->unk670[1] = 0;
    h->unk670[2] = 0;
}

void HumanPlayer_JumpUpdate(HumanState *sm, Human *h);

void HumanPlayer_JumpEnter(HumanState *sm, Human *h)
{
    long double x, y;
    float fx;
    uint16_t *obj;

    if (h->unk768 && (obj = *h->unk768) != NULL && obj[0x1C / 2] == Elevator_GetType()) {
        State_Change(sm, h, HSTATE_STAND);
        return;
    }
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    Anim_SetSpeed(h->anim, 0, 0.4f);
    Sound_Play(h->view, "jump_1");
    x = h->move[0];
    y = h->move[1];
    if (x87_eq(x, 0) && x87_eq(y, 0)) {
        fx = (float)x;
    } else {
        long double inv = 1.0 / x87_sqrt(y * y + x * x);

        fx = x87_float(x * inv);
        y = y * inv;
        fx = x87_float(gJumpSpeed * (long double)fx);
        y = y * gJumpSpeed;
    }
    h->vel[0] = (float)(fx + (long double)h->unk728[0]);
    h->vel[1] = (float)(y + h->unk728[1]);
    h->vel[2] = (float)((long double)gJumpUp + h->unk728[2]);
    Human_LeaveGround(h);
    sm->update = HumanPlayer_JumpUpdate;
}
HOOK(0x40BA70, HumanPlayer_JumpEnter);

void HumanPlayer_JumpUpdate(HumanState *sm, Human *h)
{
    unsigned in;

    HumanPlayer_UpdateFall(h);
    in = sm->input;
    if (!(in & HUMAN_ON_GROUND))
        return;
    Human_Land(h);
    if (sm->input & HUMAN_CROUCH) {
        if (in & HUMAN_FORWARD)
            State_Change(sm, h, HSTATE_CROUCH_FORWARD);
        else if (in & HUMAN_BACK)
            State_Change(sm, h, HSTATE_CROUCH_BACK);
        else if (in & HUMAN_LEFT)
            State_Change(sm, h, HSTATE_CROUCH_LEFT);
        else if (in & HUMAN_RIGHT)
            State_Change(sm, h, HSTATE_CROUCH_RIGHT);
        else
            State_Change(sm, h, HSTATE_STAND);
    } else {
        if (in & HUMAN_FORWARD)
            State_Change(sm, h, HSTATE_WALK_FORWARD);
        else if (in & HUMAN_BACK)
            State_Change(sm, h, HSTATE_WALK_BACK);
        else if (in & HUMAN_LEFT)
            State_Change(sm, h, HSTATE_WALK_LEFT);
        else if (in & HUMAN_RIGHT)
            State_Change(sm, h, HSTATE_WALK_RIGHT);
        else
            State_Change(sm, h, HSTATE_STAND);
    }
}
HOOK(0x40BBD0, HumanPlayer_JumpUpdate);

void HumanPlayer_FallUpdate(HumanState *sm, Human *h);

void HumanPlayer_FallEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    h->vel[0] = h->move[0] + h->unk728[0];
    h->vel[1] = h->move[1] + h->unk728[1];
    h->vel[2] = h->unk728[2];
    sm->update = HumanPlayer_FallUpdate;
}
HOOK(0x40BDB0, HumanPlayer_FallEnter);

void HumanPlayer_FallUpdate(HumanState *sm, Human *h)
{
    HumanPlayer_UpdateFall(h);
    if (!(sm->input & HUMAN_ON_GROUND))
        return;
    Human_Land(h);
    State_Change(sm, h, HSTATE_STAND);
}
HOOK(0x40BE40, HumanPlayer_FallUpdate);

/* --- using objects ------------------------------------------------------ */

#define FRAMES(seconds) x87_fistp((seconds) * FPS)

static const double kPicklockSide = 409.6;
static const double kPicklockBack = 2949.12;
static const double kUse31Side = 778.24;
static const double kUse31Back = 2457.6;
static const double kTypeBack = 2764.8;
static const double kUse33Back = 2867.2;
static const double kUse34Side = -2048.0;
static const double kUse34Up = 1433.6;
static const double kUse35Back = 3481.6;
static const double kUse36Back = 2048.0;

/* move = rot * move, every row from the old vector */
static void Human_RotateMove(Human *h)
{
    float *m = h->rot;
    float *v = h->move;
    float x, y;

    BoneObj_GetRootMotion(h->unk514, v, h->anim);
    x = (float)((long double)m[2] * v[2] + (long double)m[1] * v[1] + (long double)m[0] * v[0]);
    y = (float)((long double)m[5] * v[2] + (long double)m[4] * v[1] + (long double)m[3] * v[0]);
    v[2] = (float)((long double)m[8] * v[2] + (long double)m[7] * v[1] + (long double)m[6] * v[0]);
    v[0] = x;
    v[1] = y;
}

/* true when the human is on the negative side of the object's Y axis */
static int Human_IsBehind(Human *h, const double *pos, const float *mat)
{
    long double dx = (long double)h->pos[0] - pos[0];
    long double dy = (long double)h->pos[1] - pos[1];
    long double dz = (long double)h->pos[2] - pos[2];

    return !((long double)mat[7] * dz + (long double)mat[4] * dy + (long double)mat[1] * dx >= 0.0);
}

/* turn to the object's heading plus offset */
static void Human_FaceObject(Human *h, const float *mat, float offset)
{
    float ax, ay, az;

    Matrix_GetEuler(mat, &ax, &ay, &az);
    Human_SetYaw(h, (float)((long double)az + offset));
}

static void Action_Begin(Human *h)
{
    HumanViewCam(h, 3);
    HumanPlayer_SetFlagD68(h);
    Human_SetExactMotion(h);
    HumanView_ClearAngles(h->view);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
}

/* 0 while the action runs, 1 when it completed, 2 when it was cancelled
   by letting go of the use key or dying */
static int Action_Tick(HumanState *sm, Human *h)
{
    int cancel = x87_eq(h->unk1D4, 0);

    if (sm->input & HUMAN_DEAD)
        cancel = 1;
    if (h->actionFrame < h->actionFrames)
        h->actionFrame++;
    if (cancel)
        return 2;
    return h->actionFrame == h->actionFrames;
}

static void Action_End(Human *h)
{
    HumanPlayer_ClearFlagD44(h);
    HumanView_UnlockPitch(h->view);
    Human_UnfreezeView(h);
    Human_EndUse(h);
    Human_SetClimbObject(h, 0);
    HumanViewCam(h, 1);
    HumanPlayer_ClearFlagD68(h);
    Human_ClearExactMotion(h);
}

static void Action_Done(HumanState *sm, Human *h)
{
    if (h->actionSound)
        Sound_Stop(h->actionSound);
    State_Change(sm, h, HSTATE_STAND);
}

void HumanPlayer_PicklockUpdate(HumanState *sm, Human *h);

void HumanPlayer_PicklockEnter(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3];
    float mat[10];
    long double k, z;
    double x, y;

    HumanPlayer_SetWeaponAnim(h, 0);
    Human_PlayAnim(h, 0x59, 1, 0.0f);
    Human_RotateMove(h);
    Obj_GetUsePoint(obj, pos, mat);
    if (Human_IsBehind(h, pos, mat)) {
        Matrix_RotateZ(mat, mat, PI_F);
        k = kPicklockSide;
    } else {
        k = -kPicklockSide;
    }
    x = x87_double(mat[0] * k + mat[1] * (long double)kPicklockBack);
    y = x87_double(mat[3] * k + mat[4] * (long double)kPicklockBack);
    z = mat[6] * k + mat[7] * (long double)kPicklockBack;
    pos[0] = (double)((long double)x + pos[0]);
    pos[1] = (double)((long double)y + pos[1]);
    pos[2] = (double)(z + pos[2]);
    Human_SetPosition(h, pos);
    Human_SnapToGround(h);
    Human_FaceObject(h, mat, PI_F);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(Door_GetPickTime(obj));
    Action_Begin(h);
    HumanPlayer_SetFlagD44(h);
    h->actionSound = Sound_Play(h, "picklock_loop");
    sm->update = HumanPlayer_PicklockUpdate;
}
HOOK(0x40BED0, HumanPlayer_PicklockEnter);

void HumanPlayer_SetFlagD44(Human *h)
{
    h->unkD44 = 1;
}
HOOK(0x40C140, HumanPlayer_SetFlagD44);

void HumanPlayer_SetFlagD68(Human *h)
{
    h->unkD68 = 1;
}
HOOK(0x40C150, HumanPlayer_SetFlagD68);

void HumanPlayer_PicklockUpdate(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    void *sound;
    int r = Action_Tick(sm, h);

    if (r == 0)
        return;
    if (r == 1) {
        Door_SetPicked(obj, 1);
        Door_Toggle(obj);
        HumanPlayer_PicklockNoise(obj, h);
        sound = Sound_Play(h, "pickloop_end");
        if (sound)
            Sound_SetPos(sound, DynCubeObj_GetPosition(h));
    }
    Action_End(h);
    Action_Done(sm, h);
}
HOOK(0x40C160, HumanPlayer_PicklockUpdate);

void HumanPlayer_PicklockNoise(void *obj, Human *h)
{
    Human_Noise(h, 0xC, obj, h);
}
HOOK(0x40C270, HumanPlayer_PicklockNoise);

void HumanPlayer_ClearFlagD44(Human *h)
{
    h->unkD44 = 0;
}
HOOK(0x40C2D0, HumanPlayer_ClearFlagD44);

void HumanPlayer_ClearFlagD68(Human *h)
{
    h->unkD68 = 0;
}
HOOK(0x40C2E0, HumanPlayer_ClearFlagD68);

void HumanPlayer_Use31Update(HumanState *sm, Human *h);

void HumanPlayer_Use31Enter(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3];
    float mat[10];
    long double x;
    double y, z;

    HumanPlayer_SetWeaponAnim(h, 0);
    Human_PlayAnim(h, 0x54, 0, 0.0f);
    h->useStage = 0;
    Human_RotateMove(h);
    Obj_GetUsePoint(obj, pos, mat);
    x = mat[0] * (long double)kUse31Side + mat[1] * (long double)kUse31Back;
    y = x87_double(mat[3] * (long double)kUse31Side + mat[4] * (long double)kUse31Back);
    z = x87_double(mat[6] * (long double)kUse31Side + mat[7] * (long double)kUse31Back);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    Human_SetPosition(h, pos);
    Human_SnapToGround(h);
    Human_FaceObject(h, mat, PI_F);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(Cabinet_GetSearchTime(obj));
    Action_Begin(h);
    sm->update = HumanPlayer_Use31Update;
}
HOOK(0x40C2F0, HumanPlayer_Use31Enter);

void HumanPlayer_Use31Update(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    int r;

    if (h->useStage == 0) {
        if (sm->input & HUMAN_ANIM_DONE) {
            Human_PlayAnim(h, 0x55, 1, 0.0f);
            h->useStage = 1;
        }
    } else if (h->useStage == 1) {
        int n = Anim_GetLengthTicks(0, 0x53);
        int end = x87_fistp((long double)n * 0x1.b4e81cp-13f * FPS);

        if (h->actionFrame > h->actionFrames - end) {
            Human_PlayAnim(h, 0x53, 0, 0.2f);
            h->useStage = 2;
        }
    }
    r = Action_Tick(sm, h);
    if (r == 0)
        return;
    if (r == 1)
        Cabinet_SetSearched(obj);
    else
        Cabinet_Reset(obj);
    Action_End(h);
    State_Change(sm, h, HSTATE_STAND);
}
HOOK(0x40C510, HumanPlayer_Use31Update);

void HumanPlayer_TypeComputerUpdate(HumanState *sm, Human *h);

void HumanPlayer_TypeComputerEnter(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3];
    float mat[10];
    long double x;
    double y, z;

    h->unk33D = 0;
    HumanPlayer_SetWeaponAnim(h, 0);
    Human_PlayAnim(h, 0x5A, 1, 0.0f);
    Human_RotateMove(h);
    GenericTBA_GetUsePos(obj, pos, mat);
    x = mat[1] * (long double)kTypeBack;
    y = x87_double(mat[4] * (long double)kTypeBack);
    z = x87_double(mat[7] * (long double)kTypeBack);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    Human_SetPosition(h, pos);
    Human_SnapToGround(h);
    Human_FaceObject(h, mat, PI_F);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(GenericTBA_GetUseTime(obj));
    Action_Begin(h);
    h->actionSound = Sound_Play(h, "typecomp_loop");
    sm->update = HumanPlayer_TypeComputerUpdate;
}
HOOK(0x40C670, HumanPlayer_TypeComputerEnter);

void HumanPlayer_TypeComputerUpdate(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    void *sound;
    int r = Action_Tick(sm, h);

    if (r == 0)
        return;
    if (r == 1) {
        Terminal_SetHacked(obj);
        sound = Sound_Play(h, "typecomp_end");
        if (sound)
            Sound_SetPos(sound, DynCubeObj_GetPosition(h));
    }
    Action_End(h);
    Action_Done(sm, h);
}
HOOK(0x40C870, HumanPlayer_TypeComputerUpdate);

void HumanPlayer_Use33Update(HumanState *sm, Human *h);

void HumanPlayer_Use33Enter(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3];
    float mat[10];
    long double x;
    double y;

    Obj_GetUsePoint(obj, pos, mat);
    if (Human_IsBehind(h, pos, mat))
        Matrix_RotateZ(mat, mat, PI_F);
    x = mat[1] * (long double)kUse33Back;
    y = x87_double(mat[4] * (long double)kUse33Back);
    pos[2] = h->pos[2];
    pos[0] = (double)(x + pos[0]);
    pos[1] = (double)((long double)y + pos[1]);
    Human_SetPosition(h, pos);
    Human_FaceObject(h, mat, PI_F);
    HumanPlayer_SetWeaponAnim(h, 0);
    HumanPlayer_SetFlagD44(h);
    Human_PlayAnim(h, 0xC6, 0, 0.0f);
    HumanView_ClearAngles(h->view);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_SetPlatform(h, obj);
    Human_SetExactMotion(h);
    h->unk4D0 = 0;
    sm->update = HumanPlayer_Use33Update;
}
HOOK(0x40C970, HumanPlayer_Use33Enter);

void HumanPlayer_Use33Update(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    int leave = !x87_eq(h->unk1D4, 0) && x87_eq(h->unk134, 0);

    if (!(sm->input & HUMAN_DEAD) && !leave)
        return;
    h->unk4D0 = 1;
    HumanPlayer_ClearFlagD44(h);
    HumanView_UnlockPitch(h->view);
    Human_UnfreezeView(h);
    StationaryGun_Release(obj);
    Human_EndUse(h);
    Human_SetClimbObject(h, 0);
    Human_ClearPlatform(h);
    Human_ClearExactMotion(h);
    State_Change(sm, h, HSTATE_STAND);
}
HOOK(0x40CAE0, HumanPlayer_Use33Update);

void HumanPlayer_Use34Update(HumanState *sm, Human *h);

void HumanPlayer_Use34Enter(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3];
    float mat[10];
    float ax, ay, az;
    long double x;
    double y, z;

    Car_GetGunnerPos(obj, pos, mat);
    x = mat[1] * (long double)kUse34Side - mat[2] * (long double)kUse34Up;
    y = x87_double(mat[4] * (long double)kUse34Side - mat[5] * (long double)kUse34Up);
    z = x87_double(mat[7] * (long double)kUse34Side - mat[8] * (long double)kUse34Up);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    Human_SetPosition(h, pos);
    Matrix_GetEuler(mat, &ax, &ay, &az);
    Human_SetYaw(h, az);
    HumanPlayer_SetWeaponAnim(h, 0);
    Human_PlayAnim(h, 0xBD, 0, 0.0f);
    HumanViewCam(h, 3);
    HumanView_ClearAngles(h->view);
    HumanView_LockPitch(h->view);
    Human_FreezeView(h);
    Human_DisableCollision(h);
    Human_LeaveGround(h);
    Human_SetExactMotion(h);
    sm->update = HumanPlayer_Use34Update;
}
HOOK(0x40CB90, HumanPlayer_Use34Enter);

void HumanPlayer_Use34Update(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);

    if (!(sm->input & HUMAN_ANIM_DONE))
        return;
    Human_EnableCollision(h);
    Human_ClearExactMotion(h);
    HumanView_UnlockPitch(h->view);
    Human_UnfreezeView(h);
    Human_EndUse(h);
    Human_SetClimbObject(h, 0);
    HumanViewCam(h, 1);
    Human_SnapToGround(h);
    Human_SetAttached(h, Car_GetMountedGun(obj));
    HumanPlayer_StartAction(h, 6);
}
HOOK(0x40CCC0, HumanPlayer_Use34Update);

void HumanPlayer_StartAction(Human *h, int action)
{
    HumanState *sm = &h->sm;
    void *obj;

    Human_SetClimbObject(h, Human_GetAttached(h));
    Human_BeginUse(h);
    h->unk7BC = 0;
    switch (action) {
    case 1:
        Human_MarkDelete(Human_GetClimbObject(h));
        break;
    case 2:
        Switch_Press(Human_GetClimbObject(h));
        break;
    case 3:
        obj = Human_GetClimbObject(h);
        if (!Door_IsLocked(obj)) {
            Door_Toggle(obj);
            HumanPlayer_PicklockNoise(obj, h);
            break;
        }
        if (Door_IsLocked(obj) && Door_IsPickable(obj))
            State_Change(sm, h, HSTATE_PICKLOCK);
        return;
    case 4:
        State_Change(sm, h, HSTATE_USE31);
        return;
    case 5:
        State_Change(sm, h, HSTATE_TYPE_COMPUTER);
        return;
    case 6:
        State_Change(sm, h, HSTATE_USE33);
        return;
    case 7:
        State_Change(sm, h, HSTATE_USE34);
        return;
    case 9:
        if (Wire_IsSloped(Human_GetClimbObject(h)))
            h->unkDA0 = 1;
        else
            h->unkD93 = 1;
        return;
    case 10:
        h->unkD78 = 1;
        return;
    case 11:
        h->unkD90 = 1;
        return;
    case 12:
        State_Change(sm, h, HSTATE_USE35);
        return;
    case 13:
        State_Change(sm, h, HSTATE_USE36);
        return;
    default:
        return;
    }
    Human_EndUse(h);
    Human_SetClimbObject(h, 0);
}
HOOK(0x40CD30, HumanPlayer_StartAction);

void HumanPlayer_Use35Update(HumanState *sm, Human *h);

void HumanPlayer_Use35Enter(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3];
    float mat[10];
    long double x;
    double y, z;

    h->unk33D = 0;
    HumanPlayer_SetWeaponAnim(h, 0);
    Human_PlayAnim(h, GenericTBA_GetAnim(obj, 1, 0), 1, 0.0f);
    Human_RotateMove(h);
    GenericTBA_GetUsePos(obj, pos, mat);
    x = mat[1] * (long double)kUse35Back;
    y = x87_double(mat[4] * (long double)kUse35Back);
    z = x87_double(mat[7] * (long double)kUse35Back);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    Human_SetPosition(h, pos);
    Human_SnapToGround(h);
    Human_FaceObject(h, mat, PI_F);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(GenericTBA_GetUseTime(obj));
    Action_Begin(h);
    h->actionSound = NULL;
    sm->update = HumanPlayer_Use35Update;
}
HOOK(0x40CF70, HumanPlayer_Use35Enter);

typedef struct TraceFilter {
    int unk0;
    Human *ignore;
} TraceFilter;

typedef struct TraceResult {
    int unk0[2];
    double point[3];
    char unk20[0x18];
    float normal[3];
    int unk44[2];
    int unk4C;
    int unk50[2];
    int unk58;
    char unk5C[0x14];
} TraceResult;

/* like Use35, but stands where the line from the camera to the object
   first hits something, facing away from the surface */
void HumanPlayer_Use36Enter(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    double pos[3], start[3], cam[3], d[3];
    float mat[10];
    float dir[3], pitch, yaw;
    TraceFilter filter;
    TraceResult res;
    long double dx, x, y, z;
    char b;
    void *world;
    uint16_t *o;

    h->unk33D = 0;
    HumanPlayer_SetWeaponAnim(h, 0);
    Human_PlayAnim(h, GenericTBA_GetAnim(obj, 1, 0), 1, 0.0f);
    Human_RotateMove(h);
    GenericTBA_GetUsePos(obj, pos, mat);
    filter.ignore = h;
    HumanView_GetEye(cam, h->view);
    dx = (long double)pos[0] - cam[0];
    start[0] = pos[0];
    start[1] = pos[1];
    start[2] = pos[2];
    d[0] = x87_double(dx);
    d[1] = (double)((long double)pos[1] - cam[1]);
    d[2] = (double)((long double)pos[2] - cam[2]);
    pos[0] = cam[0];
    pos[1] = cam[1];
    pos[2] = cam[2];
    dir[0] = (float)dx;
    dir[1] = (float)d[1];
    dir[2] = (float)d[2];
    b = Game_GetEvent8();
    world = Game_GetWorld();
    Coll_Lines(world, &res, cam, start, 1, b, &filter, 0xFFFFFFFF);
    if (res.unk58 && res.unk4C) {
        dir[0] = res.normal[0];
        pos[0] = res.point[0];
        pos[1] = res.point[1];
        pos[2] = res.point[2];
        dir[1] = res.normal[1];
        dir[2] = res.normal[2];
    }
    if (!(x87_eq(dir[0], 0) && x87_eq(dir[1], 0) && x87_eq(dir[2], 0))) {
        x = dir[0];
        y = dir[1];
        z = dir[2];
        long double inv = 1.0 / x87_sqrt(x * x + y * y + z * z);

        dir[0] = (float)(x * inv);
        dir[1] = (float)(y * inv);
        dir[2] = (float)(z * inv);
    }
    Vec3_ToAngles(dir, &pitch, &yaw);
    yaw = x87_float((long double)yaw + HALF_PI_F);
    if (PlaceExplosiveTBA_GetTargetId(obj) >= 0) {
        o = Task_FindById(PlaceExplosiveTBA_GetTargetId(obj));
        if (o && TaskType_IsA(o[0x1C / 2], EditRigidObj_GetType())) {
            double *p = (double *)((char *)o + 0xF0);

            p[0] = pos[0];
            p[1] = pos[1];
            p[2] = pos[2];
            Matrix_SetEuler((float *)((char *)o + 0x108), HALF_PI_F, 0.0f, yaw);
            Matrix_SetEuler((float *)((char *)o + 0x70), HALF_PI_F, 0.0f, yaw);
        }
    }
    Vec3d_Normalize(d);
    x = d[0] * (long double)kUse36Back;
    d[0] = x87_double(x);
    d[1] = (double)(d[1] * (long double)kUse36Back);
    d[2] = (double)(d[2] * (long double)kUse36Back);
    pos[0] = (double)(pos[0] - x);
    pos[1] = (double)((long double)pos[1] - d[1]);
    pos[2] = (double)((long double)pos[2] - d[2]);
    Human_SetPosition(h, pos);
    Human_SnapToGround(h);
    Human_SetYaw(h, yaw);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(GenericTBA_GetUseTime(obj));
    Action_Begin(h);
    h->actionSound = NULL;
    sm->update = HumanPlayer_Use35Update;
}
HOOK(0x40D180, HumanPlayer_Use36Enter);

void HumanPlayer_Use35Update(HumanState *sm, Human *h)
{
    void *obj = Human_GetClimbObject(h);
    int r = Action_Tick(sm, h);

    if (r == 0)
        return;
    if (r == 1)
        Terminal_SetHacked(obj);
    Action_End(h);
    Action_Done(sm, h);
}
HOOK(0x40D580, HumanPlayer_Use35Update);

/* --- ladders and wires -------------------------------------------------- */

static void Human_StepSound(Human *h, int *counter, const int *sounds)
{
    char buf[32];
    void *sound;
    int n = sounds[*counter];

    *counter = (unsigned)(*counter + 1) % 12;
    crt_sprintf(buf, "walk_ladder_%d", n + 1);
    sound = Sound_Play(Level_GetDynamic(buf), buf);
    if (sound)
        Sound_SetPos(sound, h->pos);
    Human_Noise(h, 0xE, h, h);
}

void HumanPlayer_LadderUpdate(HumanState *sm, Human *h);

void HumanPlayer_LadderEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    HumanPlayer_SetWeaponAnim(h, 0);
    h->ladderDir = 0;
    h->ladderStep = -1.0f;
    sm->update = HumanPlayer_LadderUpdate;
}
HOOK(0x40D660, HumanPlayer_LadderEnter);

void HumanPlayer_LadderUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;
    int dir;

    if (h->unk538 == 8 && !x87_eq(h->unk540, h->ladderStep)) {
        Anim_SetSpeed(h->anim, 0, 0.0f);
        h->ladderStep = h->unk540;
        dir = h->ladderDir;
        h->ladderDir = 0;
        h->ladderPos += dir;
        Human_StepSound(h, &h->ladderSound, gLadderSounds);
    }
    if (h->ladderDir == 0) {
        if (in & HUMAN_FORWARD) {
            if (h->ladderPos == h->ladderTop - 6) {
                State_Change(sm, h, HSTATE_LADDER_OFF_TOP);
                return;
            }
            h->ladderDir = 1;
            Anim_SetSpeed(h->anim, 0, 1.0f);
        }
        if (in & HUMAN_BACK) {
            if (h->ladderPos == 0) {
                h->unkD7A = 1;
                State_Change(sm, h, HSTATE_STAND);
                return;
            }
            h->ladderDir = -1;
            Anim_SetSpeed(h->anim, 0, -1.0f);
        }
    }
    if (!x87_eq(h->unk1D4, 0) && x87_eq(h->unk134, 0)) {
        State_Change(sm, h, HSTATE_LADDER_SLIDE);
        return;
    }
    if (in & HUMAN_DEAD)
        h->unkD7A = 1;
}
HOOK(0x40D6B0, HumanPlayer_LadderUpdate);

void HumanPlayer_LadderOnUpdate(HumanState *sm, Human *h);

void HumanPlayer_LadderOnEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    HumanPlayer_SetWeaponAnim(h, 0);
    sm->update = HumanPlayer_LadderOnUpdate;
}
HOOK(0x40D8E0, HumanPlayer_LadderOnEnter);

void HumanPlayer_LadderOnUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (h->unk538 == 8 && !x87_eq(h->unk540, h->ladderStep)) {
        h->ladderStep = h->unk540;
        Human_StepSound(h, &h->ladderSound, gLadderSounds);
    }
    if (in & HUMAN_ANIM_DONE) {
        State_Change(sm, h, HSTATE_LADDER);
        return;
    }
    if (in & HUMAN_DEAD)
        h->unkD7A = 1;
}
HOOK(0x40D920, HumanPlayer_LadderOnUpdate);

void HumanPlayer_LadderOffTopUpdate(HumanState *sm, Human *h);

void HumanPlayer_LadderOffTopEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    HumanPlayer_SetWeaponAnim(h, 0);
    sm->update = HumanPlayer_LadderOffTopUpdate;
}
HOOK(0x40DA40, HumanPlayer_LadderOffTopEnter);

void HumanPlayer_LadderOffTopUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;

    if (h->unk538 == 8 && !x87_eq(h->unk540, h->ladderStep)) {
        h->ladderStep = h->unk540;
        Human_StepSound(h, &h->ladderSound, gLadderSounds);
    }
    if (in & HUMAN_ANIM_DONE) {
        h->unkD7A = 1;
        State_Change(sm, h, HSTATE_STAND);
        return;
    }
    if (in & HUMAN_DEAD)
        h->unkD7A = 1;
}
HOOK(0x40DA80, HumanPlayer_LadderOffTopUpdate);

void HumanPlayer_LadderSlideUpdate(HumanState *sm, Human *h);

void HumanPlayer_LadderSlideEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    HumanPlayer_SetWeaponAnim(h, 0);
    Human_EnableCollision(h);
    h->wireSlideSound = Sound_Play(h, "wire_slide_1");
    sm->update = HumanPlayer_LadderSlideUpdate;
}
HOOK(0x40DBA0, HumanPlayer_LadderSlideEnter);

void HumanPlayer_LadderSlideUpdate(HumanState *sm, Human *h)
{
    h->vel[0] = (float)((long double)h->vel[0] * 0.99f);
    h->vel[1] = (float)((long double)h->vel[1] * 0.99f);
    h->vel[2] = (float)(((long double)h->vel[2] - 0x1.64ce9ep+5f) * 0.99f);
    if (!(sm->input & HUMAN_ON_GROUND))
        return;
    h->unkD7A = 1;
    h->vel[0] = 0;
    h->vel[1] = 0;
    h->vel[2] = 0;
    h->unk670[0] = 0;
    h->unk670[1] = 0;
    h->unk670[2] = 0;
    if (h->wireSlideSound) {
        Sound_Stop(h->wireSlideSound);
        h->wireSlideSound = NULL;
    }
    State_Change(sm, h, HSTATE_STAND);
}
HOOK(0x40DBF0, HumanPlayer_LadderSlideUpdate);

void HumanPlayer_State41Update(HumanState *sm, Human *h);

void HumanPlayer_State41Enter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    HumanPlayer_SetWeaponAnim(h, 0);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(Fence_GetClimbTime());
    sm->update = HumanPlayer_State41Update;
}
HOOK(0x40DCA0, HumanPlayer_State41Enter);

void HumanPlayer_State41Update(HumanState *sm, Human *h)
{
    unsigned in;

    if (h->actionFrame < h->actionFrames)
        h->actionFrame++;
    in = sm->input;
    if (in & HUMAN_ANIM_DONE) {
        h->unkD92 = 1;
        State_Change(sm, h, HSTATE_FALL);
        return;
    }
    if (in & HUMAN_DEAD)
        h->unkD92 = 1;
}
HOOK(0x40DD00, HumanPlayer_State41Update);

void HumanPlayer_WireDragUpdate(HumanState *sm, Human *h);

void HumanPlayer_WireDragEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    HumanPlayer_SetWeaponAnim(h, 0);
    h->wireStep = -1.0f;
    sm->update = HumanPlayer_WireDragUpdate;
}
HOOK(0x40DD60, HumanPlayer_WireDragEnter);

void HumanPlayer_WireDragUpdate(HumanState *sm, Human *h)
{
    unsigned in = sm->input;
    char buf[32];
    int n;

    if (h->unk538 == 8 && !x87_eq(h->unk540, h->wireStep)) {
        Anim_SetSpeed(h->anim, 0, 0.0f);
        h->wireStep = h->unk540;
        h->ladderDir = 0;
    }
    if (h->ladderDir != 0)
        return;
    if (in & HUMAN_FORWARD) {
        h->ladderDir = 1;
        Anim_SetSpeed(h->anim, 0, 1.0f);
    }
    if (in & HUMAN_BACK) {
        h->ladderDir = -1;
        Anim_SetSpeed(h->anim, 0, -1.0f);
    }
    if (h->ladderDir != 0) {
        n = gWireDragSounds[h->wireSound];
        h->wireSound = (h->wireSound - 1) & 1;
        crt_sprintf(buf, "wire_drag_%d", n + 1);
        Sound_Play(h, buf);
    }
}
HOOK(0x40DDA0, HumanPlayer_WireDragUpdate);

void HumanPlayer_ZiplineUpdate(HumanState *sm, Human *h);

void HumanPlayer_ZiplineEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    Human_PlayAnimSlot(h, sm->state, 0.0f);
    HumanPlayer_SetWeaponAnim(h, 0);
    h->vel[0] = 0;
    h->vel[1] = 0;
    h->vel[2] = 0;
    h->unk670[0] = 0;
    h->unk670[1] = 0;
    h->unk670[2] = 0;
    sm->update = HumanPlayer_ZiplineUpdate;
}
HOOK(0x40DE90, HumanPlayer_ZiplineEnter);

void HumanPlayer_ZiplineUpdate(HumanState *sm, Human *h)
{
    long double speed;

    Human_GetClimbObject(h);
    speed = -(long double)h->wireDir[2] * 0x1.613d32p+6f + (long double)h->wireSpeed * 0.99f;
    h->wireSpeed = (float)speed;
    h->vel[0] = (float)(speed * h->wireDir[0]);
    h->vel[1] = (float)(speed * h->wireDir[1]);
    h->vel[2] = (float)(speed * h->wireDir[2]);
}
HOOK(0x40DEF0, HumanPlayer_ZiplineUpdate);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD_CONST(igd, 538C18, 538C18, 0x30) INIT({0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01});
ALIAS(gLadderSounds, pad_538C18, 0);
PAD_CONST(igd, 538C18, 538C48, 0x564) INIT({0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x54, 0x6F, 0x67, 0x67, 0x6C, 0x65, 0x20, 0x66, 0x6C, 0x79, 0x00, 0x00, 0x54, 0x6F, 0x67, 0x67, 0x6C, 0x65, 0x20, 0x74, 0x68, 0x69, 0x72, 0x64, 0x20, 0x70, 0x65, 0x72, 0x73, 0x6F, 0x6E, 0x00, 0x5A, 0x6F, 0x6F, 0x6D, 0x20, 0x72, 0x65, 0x73, 0x65, 0x74, 0x00, 0x00, 0x5A, 0x6F, 0x6F, 0x6D, 0x20, 0x6F, 0x75, 0x74, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x6F, 0x6F, 0x6D, 0x20, 0x69, 0x6E, 0x00, 0x57, 0x65, 0x61, 0x70, 0x6F, 0x6E, 0x20, 0x73, 0x65, 0x6C, 0x65, 0x63, 0x74, 0x00, 0x00, 0x00, 0x41, 0x63, 0x74, 0x69, 0x76, 0x61, 0x74, 0x65, 0x20, 0x6D, 0x61, 0x70, 0x20, 0x63, 0x6F, 0x6D, 0x70, 0x75, 0x74, 0x65, 0x72, 0x00, 0x00, 0x00, 0x50, 0x65, 0x65, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x65, 0x78, 0x74, 0x20, 0x77, 0x65, 0x61, 0x70, 0x6F, 0x6E, 0x00, 0x4A, 0x75, 0x6D, 0x70, 0x00, 0x00, 0x00, 0x00, 0x43, 0x72, 0x6F, 0x75, 0x63, 0x68, 0x00, 0x00, 0x41, 0x63, 0x74, 0x69, 0x76, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00, 0x00, 0x52, 0x65, 0x6C, 0x6F, 0x61, 0x64, 0x00, 0x00, 0x41, 0x6C, 0x74, 0x65, 0x72, 0x6E, 0x61, 0x74, 0x65, 0x20, 0x66, 0x69, 0x72, 0x65, 0x00, 0x00, 0x46, 0x69, 0x72, 0x65, 0x00, 0x00, 0x00, 0x00, 0x44, 0x6F, 0x77, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x55, 0x70, 0x00, 0x00, 0x52, 0x69, 0x67, 0x68, 0x74, 0x00, 0x00, 0x00, 0x4C, 0x65, 0x66, 0x74, 0x00, 0x00, 0x00, 0x00, 0x41, 0x6E, 0x61, 0x6C, 0x6F, 0x67, 0x20, 0x59, 0x00, 0x00, 0x00, 0x00, 0x41, 0x6E, 0x61, 0x6C, 0x6F, 0x67, 0x20, 0x58, 0x00, 0x00, 0x00, 0x00, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x5F, 0x66, 0x61, 0x6C, 0x6C, 0x5F, 0x33, 0x00, 0x00, 0x00, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x5F, 0x66, 0x61, 0x6C, 0x6C, 0x5F, 0x32, 0x00, 0x00, 0x00, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x5F, 0x66, 0x61, 0x6C, 0x6C, 0x5F, 0x31, 0x00, 0x00, 0x00, 0x6A, 0x75, 0x6D, 0x70, 0x5F, 0x31, 0x00, 0x00, 0x70, 0x69, 0x63, 0x6B, 0x6C, 0x6F, 0x63, 0x6B, 0x5F, 0x6C, 0x6F, 0x6F, 0x70, 0x00, 0x00, 0x00, 0x70, 0x69, 0x63, 0x6B, 0x6C, 0x6F, 0x6F, 0x70, 0x5F, 0x65, 0x6E, 0x64, 0x00, 0x00, 0x00, 0x00, 0x74, 0x79, 0x70, 0x65, 0x63, 0x6F, 0x6D, 0x70, 0x5F, 0x6C, 0x6F, 0x6F, 0x70, 0x00, 0x00, 0x00, 0x74, 0x79, 0x70, 0x65, 0x63, 0x6F, 0x6D, 0x70, 0x5F, 0x65, 0x6E, 0x64, 0x00, 0x00, 0x00, 0x00, 0x77, 0x61, 0x6C, 0x6B, 0x5F, 0x6C, 0x61, 0x64, 0x64, 0x65, 0x72, 0x5F, 0x25, 0x64, 0x00, 0x00, 0x77, 0x69, 0x72, 0x65, 0x5F, 0x73, 0x6C, 0x69, 0x64, 0x65, 0x5F, 0x31, 0x00, 0x00, 0x00, 0x00, 0x77, 0x69, 0x72, 0x65, 0x5F, 0x64, 0x72, 0x61, 0x67, 0x5F, 0x25, 0x64, 0x00, 0x00, 0x00, 0x00, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x5F, 0x64, 0x65, 0x61, 0x74, 0x68, 0x5F, 0x00, 0x00, 0x00, 0x57, 0x45, 0x41, 0x50, 0x4F, 0x4E, 0x5F, 0x49, 0x44, 0x5F, 0x50, 0x52, 0x4F, 0x58, 0x49, 0x4D, 0x49, 0x54, 0x59, 0x4D, 0x49, 0x4E, 0x45, 0x00, 0x41, 0x4D, 0x4D, 0x4F, 0x5F, 0x49, 0x44, 0x5F, 0x4D, 0x32, 0x30, 0x33, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x41, 0x50, 0x4F, 0x4E, 0x5F, 0x49, 0x44, 0x5F, 0x4D, 0x31, 0x36, 0x41, 0x32, 0x00, 0x41, 0x4D, 0x4D, 0x4F, 0x5F, 0x49, 0x44, 0x5F, 0x4D, 0x45, 0x44, 0x49, 0x50, 0x41, 0x43, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x41, 0x50, 0x4F, 0x4E, 0x5F, 0x49, 0x44, 0x5F, 0x42, 0x49, 0x4E, 0x4F, 0x43, 0x55, 0x4C, 0x41, 0x52, 0x53, 0x00, 0x00, 0x00, 0x00, 0x67, 0x75, 0x6E, 0x73, 0x5F, 0x64, 0x72, 0x79, 0x5F, 0x31, 0x00, 0x00, 0x67, 0x72, 0x65, 0x6E, 0x61, 0x64, 0x65, 0x5F, 0x74, 0x69, 0x63, 0x6B, 0x5F, 0x31, 0x00, 0x00, 0x55, 0x6E, 0x6C, 0x69, 0x6D, 0x69, 0x74, 0x65, 0x64, 0x41, 0x6D, 0x6D, 0x6F, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x41, 0x6D, 0x6D, 0x6F, 0x4C, 0x69, 0x6D, 0x69, 0x74, 0x00, 0x00, 0x44, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x57, 0x65, 0x61, 0x70, 0x6F, 0x6E, 0x43, 0x61, 0x74, 0x65, 0x67, 0x6F, 0x72, 0x79, 0x00, 0x44, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x57, 0x65, 0x61, 0x70, 0x6F, 0x6E, 0x43, 0x79, 0x63, 0x6C, 0x65, 0x00, 0x00, 0x00, 0x00, 0x44, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x47, 0x65, 0x6E, 0x65, 0x72, 0x61, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x00, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x20, 0x61, 0x6E, 0x67, 0x6C, 0x65, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x2C, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x00, 0x00, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x20, 0x70, 0x6F, 0x73, 0x69, 0x74, 0x69, 0x6F, 0x6E, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x2C, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x2C, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x00, 0x47, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x21, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x74, 0x72, 0x61, 0x6E, 0x73, 0x6C, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x5A, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x74, 0x72, 0x61, 0x6E, 0x73, 0x6C, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x59, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x74, 0x72, 0x61, 0x6E, 0x73, 0x6C, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x58, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x00, 0x47, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x20, 0x61, 0x6E, 0x67, 0x6C, 0x65, 0x3A, 0x20, 0x25, 0x2E, 0x33, 0x66, 0x0A, 0x00, 0x44, 0x69, 0x73, 0x74, 0x61, 0x6E, 0x63, 0x65, 0x20, 0x74, 0x6F, 0x20, 0x67, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x20, 0x66, 0x72, 0x6F, 0x6D, 0x20, 0x66, 0x65, 0x65, 0x74, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x6D, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x6D, 0x6F, 0x76, 0x65, 0x20, 0x47, 0x61, 0x6D, 0x6D, 0x61, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x6D, 0x6F, 0x76, 0x65, 0x20, 0x42, 0x65, 0x74, 0x61, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x6D, 0x6F, 0x76, 0x65, 0x20, 0x41, 0x6C, 0x70, 0x68, 0x61, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x6D, 0x6F, 0x76, 0x65, 0x20, 0x70, 0x6F, 0x73, 0x20, 0x5A, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x6D, 0x6F, 0x76, 0x65, 0x20, 0x70, 0x6F, 0x73, 0x20, 0x59, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x44, 0x65, 0x6C, 0x74, 0x61, 0x20, 0x6D, 0x6F, 0x76, 0x65, 0x20, 0x70, 0x6F, 0x73, 0x20, 0x58, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x41, 0x74, 0x74, 0x61, 0x63, 0x68, 0x20, 0x74, 0x61, 0x73, 0x6B, 0x3A, 0x20, 0x25, 0x78, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x47, 0x72, 0x6F, 0x75, 0x6E, 0x64, 0x20, 0x74, 0x61, 0x73, 0x6B, 0x3A, 0x20, 0x25, 0x78, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x53, 0x70, 0x65, 0x65, 0x64, 0x20, 0x5A, 0x20, 0x28, 0x6D, 0x2F, 0x73, 0x29, 0x3A, 0x20, 0x25, 0x2E, 0x32, 0x66, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x65, 0x61, 0x72, 0x63, 0x61, 0x6E, 0x64, 0x79, 0x00, 0x00, 0x00, 0x00, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x5F, 0x68, 0x69, 0x74, 0x5F, 0x00, 0x44, 0x69, 0x66, 0x66, 0x69, 0x63, 0x75, 0x6C, 0x74, 0x79, 0x20, 0x6C, 0x65, 0x76, 0x65, 0x6C, 0x20, 0x6E, 0x6F, 0x74, 0x20, 0x64, 0x65, 0x66, 0x69, 0x6E, 0x65, 0x64, 0x20, 0x25, 0x64, 0x00, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x5F, 0x78, 0x70, 0x6C, 0x68, 0x69, 0x74, 0x5F, 0x00, 0x00, 0x70, 0x75, 0x6E, 0x63, 0x68, 0x65, 0x64, 0x5F, 0x30, 0x32, 0x00, 0x00, 0x70, 0x75, 0x6E, 0x63, 0x68, 0x65, 0x64, 0x5F, 0x30, 0x31, 0x00, 0x00, 0x65, 0x6C, 0x66, 0x65, 0x6E, 0x63, 0x65, 0x5F, 0x73, 0x70, 0x61, 0x72, 0x6B, 0x31, 0x00, 0x00, 0x52, 0x41, 0x4E, 0x47, 0x45, 0x3A, 0x20, 0x31, 0x30, 0x30, 0x30, 0x2B, 0x4D, 0x0A, 0x00, 0x00, 0x4D, 0x6F, 0x64, 0x65, 0x6C, 0x3A, 0x20, 0x25, 0x73, 0x0A, 0x00, 0x00, 0x52, 0x41, 0x4E, 0x47, 0x45, 0x3A, 0x20, 0x25, 0x30, 0x2E, 0x32, 0x66, 0x4D, 0x0A, 0x00, 0x00, 0x48, 0x75, 0x6D, 0x61, 0x6E, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x5F, 0x4C, 0x6F, 0x61, 0x64, 0x50, 0x61, 0x72, 0x61, 0x6D, 0x65, 0x74, 0x65, 0x72, 0x73, 0x3A, 0x20, 0x43, 0x6F, 0x75, 0x6C, 0x64, 0x6E, 0x27, 0x74, 0x20, 0x6C, 0x6F, 0x61, 0x64, 0x20, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x20, 0x22, 0x25, 0x73, 0x22, 0x00, 0x00, 0x00, 0x4C, 0x4F, 0x43, 0x41, 0x4C, 0x3A, 0x68, 0x75, 0x6D, 0x61, 0x6E, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x2F, 0x68, 0x75, 0x6D, 0x61, 0x6E, 0x70, 0x6C, 0x61, 0x79, 0x65, 0x72, 0x2E, 0x71, 0x73, 0x63});
ALIAS(gWireDragSounds, pad_538C48, 0);
BSS(56E1EC) float gJumpSpeed;
BSS(56E1EC) float gJumpUp;
