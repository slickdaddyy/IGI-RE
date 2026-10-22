#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "humanstate/humanstate.h"

ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x408460, void, sub_408460, (Human *h));
ORIG(0x416850, char, sub_416850, (void));
ORIG(0x416920, void *, sub_416920, (void));
ORIG(0x4388A0, int, sub_4388A0, (void *obj));
ORIG(0x4137E0, void, sub_4137E0, (Human *h, int a));
ORIG(0x413A60, void, sub_413A60, (Human *h, int a));
ORIG(0x42FDD0, void *, sub_42FDD0, (void *obj));
ORIG(0x42FDF0, void, sub_42FDF0, (void *obj, double *pos, float *mat));
ORIG(0x43E4E0, uint16_t, sub_43E4E0, (void));
ORIG(0x43F8A0, char, sub_43F8A0, (void *obj));
ORIG(0x440270, long double, sub_440270, (void));
ORIG(0x443EA0, void, sub_443EA0, (void *obj));
ORIG(0x444B30, void, sub_444B30, (void *obj, double *pos, float *mat));
ORIG(0x444B80, long double, sub_444B80, (void *obj));
ORIG(0x444B90, void, sub_444B90, (void *obj));
ORIG(0x444BA0, void, sub_444BA0, (void *obj));
ORIG(0x444BC0, void, sub_444BC0, (void *obj));
ORIG(0x447D30, long double, sub_447D30, (void *obj));
ORIG(0x447D40, char, sub_447D40, (void *obj));
ORIG(0x447D50, char, sub_447D50, (void *obj));
ORIG(0x447D60, void, sub_447D60, (void *obj, int a));
ORIG(0x447DA0, void, sub_447DA0, (void *obj));
ORIG(0x448570, void, sub_448570, (void *obj, double *pos, float *mat));
ORIG(0x4485C0, long double, sub_4485C0, (void *obj));
ORIG(0x4485D0, short, sub_4485D0, (void *obj, int a, int b));
ORIG(0x450CE0, void, sub_450CE0, (void *noise));
ORIG(0x460CE0, void, sub_460CE0, (void *obj));
ORIG(0x4631A0, void, sub_4631A0, (Human *h, double *pos));
ORIG(0x463220, void, sub_463220, (Human *h, float yaw));
ORIG(0x463260, void, sub_463260, (Human *h));
ORIG(0x4632D0, void, sub_4632D0, (Human *h));
ORIG(0x463310, void, sub_463310, (Human *h, void *obj));
ORIG(0x463550, void, sub_463550, (Human *h));
ORIG(0x463570, void, sub_463570, (Human *h));
ORIG(0x463580, void, sub_463580, (Human *h));
ORIG(0x463590, void, sub_463590, (Human *h));
ORIG(0x4635A0, void, sub_4635A0, (Human *h));
ORIG(0x4635F0, void, sub_4635F0, (Human *h));
ORIG(0x463600, void, sub_463600, (Human *h));
ORIG(0x463610, void, sub_463610, (Human *h));
ORIG(0x463620, void, sub_463620, (Human *h));
ORIG(0x4636A0, void, sub_4636A0, (Human *h, int a));
ORIG(0x4636E0, void *, sub_4636E0, (Human *h));
ORIG(0x463700, void, sub_463700, (Human *h, void *a));
ORIG(0x463740, int, sub_463740, (Human *h));
ORIG(0x463760, void, HumanViewCam, (Human *h, int mode));
ORIG(0x464700, void, sub_464700, (void *view));
ORIG(0x464710, void, sub_464710, (void *view));
ORIG(0x464720, void, sub_464720, (void *view));
ORIG(0x465800, void, sub_465800, (double *pos, void *view));
ORIG(0x475340, void, sub_475340, (void *obj));
ORIG(0x489E70, void, sub_489E70, (Human *h, int anim, float blend));
ORIG(0x489EA0, void, sub_489EA0, (Human *h, int anim, int loop, float blend));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B2D00, void, Vec3_ToAngles, (const float *v, float *pitch, float *yaw));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B38E0, void, Matrix_SetEuler, (float *m, float ax, float ay, float az));
ORIG(0x4B3790, void, Matrix_RotateZ, (float *out, const float *m, float a));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4C3440, void *, sub_4C3440, (Human *h));
ORIG(0x4CC2A0, void, sub_4CC2A0, (void *world, void *result, const double *from, const double *to, int a, char b,
                                  void *filter));
ORIG(0x4D2FB0, void, sub_4D2FB0, (void *a, float *move, void *anim));
ORIG(0x4D64E0, long double, sub_4D64E0, (void *anim, int track));
ORIG(0x4D6520, long double, sub_4D6520, (void *anim, int track));
ORIG(0x4D6580, void, sub_4D6580, (void *anim, int track, float speed));
ORIG(0x4D65B0, void, sub_4D65B0, (void *anim, int track, float time));
ORIG(0x4D7040, int, sub_4D7040, (int a, int b));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (void *sound));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, const double *pos));
ORIG(0x4F0EE0, void *, sub_4F0EE0, (const char *name));
ORIG(0x4F1030, void *, sub_4F1030, (int id));
ORIG(0x4F3380, uint16_t, sub_4F3380, (void));

#define gJumpSpeed VAR(float, 0x56E1EC)
#define gJumpUp VAR(float, 0x56E1F0)
#define gLadderSounds ((const int *)0x538C18)
#define gWireDragSounds ((const int *)0x538C48)

#define PI_F 0x1.921fb6p+1f
#define HALF_PI_F 0x1.921fb6p+0f
#define FPS 30.0f

/* the 0x1C byte message passed to 0x450CE0, probably a noise AI can hear */
typedef struct HumanNoise {
    int type;
    int unk4;
    int unk8;
    float radius;
    void *unk10;
    void *unk14;
    void *unk18;
} HumanNoise;

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
    HumanNoise n;

    n.type = type;
    n.unk4 = h->unkFC;
    n.unk8 = 0x46;
    n.radius = 20480.0f;
    n.unk10 = sub_4C3440(h);
    n.unk14 = a;
    n.unk18 = b;
    sub_450CE0(&n);
}

/* --- running ------------------------------------------------------------ */

static float Run_Start(HumanState *sm, Human *h)
{
    float phase;

    h->unk33D = 1;
    phase = x87_float(sub_4D6520(h->anim, 0));
    sub_489E70(h, sm->state, 0.2f);
    return phase;
}

static void Run_Finish(HumanState *sm, Human *h, int keepPhase, float phase, HumanStateFn update)
{
    if (keepPhase)
        sub_4D65B0(h->anim, 0, (float)(sub_4D64E0(h->anim, 0) * phase));
    sub_413A60(h, 5);
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
        sound = Sound_Play(sub_4F0EE0(name), name);
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

    if (h->unk768 && (obj = *h->unk768) != NULL && obj[0x1C / 2] == sub_43E4E0()) {
        State_Change(sm, h, HSTATE_STAND);
        return;
    }
    h->unk33D = 0;
    sub_489E70(h, sm->state, 0.0f);
    sub_4D6580(h->anim, 0, 0.4f);
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
    sub_4632D0(h);
    sm->update = HumanPlayer_JumpUpdate;
}
HOOK(0x40BA70, HumanPlayer_JumpEnter);

void HumanPlayer_JumpUpdate(HumanState *sm, Human *h)
{
    unsigned in;

    sub_408460(h);
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
    sub_489E70(h, sm->state, 0.0f);
    h->vel[0] = h->move[0] + h->unk728[0];
    h->vel[1] = h->move[1] + h->unk728[1];
    h->vel[2] = h->unk728[2];
    sm->update = HumanPlayer_FallUpdate;
}
HOOK(0x40BDB0, HumanPlayer_FallEnter);

void HumanPlayer_FallUpdate(HumanState *sm, Human *h)
{
    sub_408460(h);
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

    sub_4D2FB0(h->unk514, v, h->anim);
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
    sub_463220(h, (float)((long double)az + offset));
}

static void Action_Begin(Human *h)
{
    HumanViewCam(h, 3);
    HumanPlayer_SetFlagD68(h);
    sub_463590(h);
    sub_464720(h->view);
    sub_464700(h->view);
    sub_4635F0(h);
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
    sub_464710(h->view);
    sub_463600(h);
    sub_463620(h);
    sub_4636A0(h, 0);
    HumanViewCam(h, 1);
    HumanPlayer_ClearFlagD68(h);
    sub_4635A0(h);
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
    void *obj = sub_4636E0(h);
    double pos[3];
    float mat[10];
    long double k, z;
    double x, y;

    sub_4137E0(h, 0);
    sub_489EA0(h, 0x59, 1, 0.0f);
    Human_RotateMove(h);
    sub_444B30(obj, pos, mat);
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
    sub_4631A0(h, pos);
    sub_463260(h);
    Human_FaceObject(h, mat, PI_F);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(sub_447D30(obj));
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
    void *obj = sub_4636E0(h);
    void *sound;
    int r = Action_Tick(sm, h);

    if (r == 0)
        return;
    if (r == 1) {
        sub_447D60(obj, 1);
        sub_447DA0(obj);
        HumanPlayer_PicklockNoise(obj, h);
        sound = Sound_Play(h, "pickloop_end");
        if (sound)
            Sound_SetPos(sound, sub_4C3440(h));
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
    void *obj = sub_4636E0(h);
    double pos[3];
    float mat[10];
    long double x;
    double y, z;

    sub_4137E0(h, 0);
    sub_489EA0(h, 0x54, 0, 0.0f);
    h->useStage = 0;
    Human_RotateMove(h);
    sub_444B30(obj, pos, mat);
    x = mat[0] * (long double)kUse31Side + mat[1] * (long double)kUse31Back;
    y = x87_double(mat[3] * (long double)kUse31Side + mat[4] * (long double)kUse31Back);
    z = x87_double(mat[6] * (long double)kUse31Side + mat[7] * (long double)kUse31Back);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    sub_4631A0(h, pos);
    sub_463260(h);
    Human_FaceObject(h, mat, PI_F);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(sub_444B80(obj));
    Action_Begin(h);
    sm->update = HumanPlayer_Use31Update;
}
HOOK(0x40C2F0, HumanPlayer_Use31Enter);

void HumanPlayer_Use31Update(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    int r;

    if (h->useStage == 0) {
        if (sm->input & HUMAN_ANIM_DONE) {
            sub_489EA0(h, 0x55, 1, 0.0f);
            h->useStage = 1;
        }
    } else if (h->useStage == 1) {
        int n = sub_4D7040(0, 0x53);
        int end = x87_fistp((long double)n * 0x1.b4e81cp-13f * FPS);

        if (h->actionFrame > h->actionFrames - end) {
            sub_489EA0(h, 0x53, 0, 0.2f);
            h->useStage = 2;
        }
    }
    r = Action_Tick(sm, h);
    if (r == 0)
        return;
    if (r == 1)
        sub_444B90(obj);
    else
        sub_444BA0(obj);
    Action_End(h);
    State_Change(sm, h, HSTATE_STAND);
}
HOOK(0x40C510, HumanPlayer_Use31Update);

void HumanPlayer_TypeComputerUpdate(HumanState *sm, Human *h);

void HumanPlayer_TypeComputerEnter(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    double pos[3];
    float mat[10];
    long double x;
    double y, z;

    h->unk33D = 0;
    sub_4137E0(h, 0);
    sub_489EA0(h, 0x5A, 1, 0.0f);
    Human_RotateMove(h);
    sub_448570(obj, pos, mat);
    x = mat[1] * (long double)kTypeBack;
    y = x87_double(mat[4] * (long double)kTypeBack);
    z = x87_double(mat[7] * (long double)kTypeBack);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    sub_4631A0(h, pos);
    sub_463260(h);
    Human_FaceObject(h, mat, PI_F);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(sub_4485C0(obj));
    Action_Begin(h);
    h->actionSound = Sound_Play(h, "typecomp_loop");
    sm->update = HumanPlayer_TypeComputerUpdate;
}
HOOK(0x40C670, HumanPlayer_TypeComputerEnter);

void HumanPlayer_TypeComputerUpdate(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    void *sound;
    int r = Action_Tick(sm, h);

    if (r == 0)
        return;
    if (r == 1) {
        sub_443EA0(obj);
        sound = Sound_Play(h, "typecomp_end");
        if (sound)
            Sound_SetPos(sound, sub_4C3440(h));
    }
    Action_End(h);
    Action_Done(sm, h);
}
HOOK(0x40C870, HumanPlayer_TypeComputerUpdate);

void HumanPlayer_Use33Update(HumanState *sm, Human *h);

void HumanPlayer_Use33Enter(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    double pos[3];
    float mat[10];
    long double x;
    double y;

    sub_444B30(obj, pos, mat);
    if (Human_IsBehind(h, pos, mat))
        Matrix_RotateZ(mat, mat, PI_F);
    x = mat[1] * (long double)kUse33Back;
    y = x87_double(mat[4] * (long double)kUse33Back);
    pos[2] = h->pos[2];
    pos[0] = (double)(x + pos[0]);
    pos[1] = (double)((long double)y + pos[1]);
    sub_4631A0(h, pos);
    Human_FaceObject(h, mat, PI_F);
    sub_4137E0(h, 0);
    HumanPlayer_SetFlagD44(h);
    sub_489EA0(h, 0xC6, 0, 0.0f);
    sub_464720(h->view);
    sub_464700(h->view);
    sub_4635F0(h);
    sub_463310(h, obj);
    sub_463590(h);
    h->unk4D0 = 0;
    sm->update = HumanPlayer_Use33Update;
}
HOOK(0x40C970, HumanPlayer_Use33Enter);

void HumanPlayer_Use33Update(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    int leave = !x87_eq(h->unk1D4, 0) && x87_eq(h->unk134, 0);

    if (!(sm->input & HUMAN_DEAD) && !leave)
        return;
    h->unk4D0 = 1;
    HumanPlayer_ClearFlagD44(h);
    sub_464710(h->view);
    sub_463600(h);
    sub_475340(obj);
    sub_463620(h);
    sub_4636A0(h, 0);
    sub_463550(h);
    sub_4635A0(h);
    State_Change(sm, h, HSTATE_STAND);
}
HOOK(0x40CAE0, HumanPlayer_Use33Update);

void HumanPlayer_Use34Update(HumanState *sm, Human *h);

void HumanPlayer_Use34Enter(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    double pos[3];
    float mat[10];
    float ax, ay, az;
    long double x;
    double y, z;

    sub_42FDF0(obj, pos, mat);
    x = mat[1] * (long double)kUse34Side - mat[2] * (long double)kUse34Up;
    y = x87_double(mat[4] * (long double)kUse34Side - mat[5] * (long double)kUse34Up);
    z = x87_double(mat[7] * (long double)kUse34Side - mat[8] * (long double)kUse34Up);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    sub_4631A0(h, pos);
    Matrix_GetEuler(mat, &ax, &ay, &az);
    sub_463220(h, az);
    sub_4137E0(h, 0);
    sub_489EA0(h, 0xBD, 0, 0.0f);
    HumanViewCam(h, 3);
    sub_464720(h->view);
    sub_464700(h->view);
    sub_4635F0(h);
    sub_463570(h);
    sub_4632D0(h);
    sub_463590(h);
    sm->update = HumanPlayer_Use34Update;
}
HOOK(0x40CB90, HumanPlayer_Use34Enter);

void HumanPlayer_Use34Update(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);

    if (!(sm->input & HUMAN_ANIM_DONE))
        return;
    sub_463580(h);
    sub_4635A0(h);
    sub_464710(h->view);
    sub_463600(h);
    sub_463620(h);
    sub_4636A0(h, 0);
    HumanViewCam(h, 1);
    sub_463260(h);
    sub_463700(h, sub_42FDD0(obj));
    HumanPlayer_StartAction(h, 6);
}
HOOK(0x40CCC0, HumanPlayer_Use34Update);

void HumanPlayer_StartAction(Human *h, int action)
{
    HumanState *sm = &h->sm;
    void *obj;

    sub_4636A0(h, sub_463740(h));
    sub_463610(h);
    h->unk7BC = 0;
    switch (action) {
    case 1:
        sub_460CE0(sub_4636E0(h));
        break;
    case 2:
        sub_444BC0(sub_4636E0(h));
        break;
    case 3:
        obj = sub_4636E0(h);
        if (!sub_447D40(obj)) {
            sub_447DA0(obj);
            HumanPlayer_PicklockNoise(obj, h);
            break;
        }
        if (sub_447D40(obj) && sub_447D50(obj))
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
        if (sub_43F8A0(sub_4636E0(h)))
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
    sub_463620(h);
    sub_4636A0(h, 0);
}
HOOK(0x40CD30, HumanPlayer_StartAction);

void HumanPlayer_Use35Update(HumanState *sm, Human *h);

void HumanPlayer_Use35Enter(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    double pos[3];
    float mat[10];
    long double x;
    double y, z;

    h->unk33D = 0;
    sub_4137E0(h, 0);
    sub_489EA0(h, sub_4485D0(obj, 1, 0), 1, 0.0f);
    Human_RotateMove(h);
    sub_448570(obj, pos, mat);
    x = mat[1] * (long double)kUse35Back;
    y = x87_double(mat[4] * (long double)kUse35Back);
    z = x87_double(mat[7] * (long double)kUse35Back);
    pos[0] = (double)(pos[0] + x);
    pos[1] = (double)((long double)pos[1] + y);
    pos[2] = (double)((long double)pos[2] + z);
    sub_4631A0(h, pos);
    sub_463260(h);
    Human_FaceObject(h, mat, PI_F);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(sub_4485C0(obj));
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
    void *obj = sub_4636E0(h);
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
    sub_4137E0(h, 0);
    sub_489EA0(h, sub_4485D0(obj, 1, 0), 1, 0.0f);
    Human_RotateMove(h);
    sub_448570(obj, pos, mat);
    filter.ignore = h;
    sub_465800(cam, h->view);
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
    b = sub_416850();
    world = sub_416920();
    sub_4CC2A0(world, &res, cam, start, 1, b, &filter);
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
    if (sub_4388A0(obj) >= 0) {
        o = sub_4F1030(sub_4388A0(obj));
        if (o && TaskType_IsA(o[0x1C / 2], sub_4F3380())) {
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
    sub_4631A0(h, pos);
    sub_463260(h);
    sub_463220(h, yaw);
    HumanPlayer_SetFlagD44(h);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(sub_4485C0(obj));
    Action_Begin(h);
    h->actionSound = NULL;
    sm->update = HumanPlayer_Use35Update;
}
HOOK(0x40D180, HumanPlayer_Use36Enter);

void HumanPlayer_Use35Update(HumanState *sm, Human *h)
{
    void *obj = sub_4636E0(h);
    int r = Action_Tick(sm, h);

    if (r == 0)
        return;
    if (r == 1)
        sub_443EA0(obj);
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
    sound = Sound_Play(sub_4F0EE0(buf), buf);
    if (sound)
        Sound_SetPos(sound, h->pos);
    Human_Noise(h, 0xE, h, h);
}

void HumanPlayer_LadderUpdate(HumanState *sm, Human *h);

void HumanPlayer_LadderEnter(HumanState *sm, Human *h)
{
    h->unk33D = 0;
    sub_489E70(h, sm->state, 0.0f);
    sub_4137E0(h, 0);
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
        sub_4D6580(h->anim, 0, 0.0f);
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
            sub_4D6580(h->anim, 0, 1.0f);
        }
        if (in & HUMAN_BACK) {
            if (h->ladderPos == 0) {
                h->unkD7A = 1;
                State_Change(sm, h, HSTATE_STAND);
                return;
            }
            h->ladderDir = -1;
            sub_4D6580(h->anim, 0, -1.0f);
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
    sub_489E70(h, sm->state, 0.0f);
    sub_4137E0(h, 0);
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
    sub_489E70(h, sm->state, 0.0f);
    sub_4137E0(h, 0);
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
    sub_489E70(h, sm->state, 0.0f);
    sub_4137E0(h, 0);
    sub_463580(h);
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
    sub_489E70(h, sm->state, 0.0f);
    sub_4137E0(h, 0);
    h->actionFrame = 0;
    h->actionFrames = FRAMES(sub_440270());
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
    sub_489E70(h, sm->state, 0.0f);
    sub_4137E0(h, 0);
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
        sub_4D6580(h->anim, 0, 0.0f);
        h->wireStep = h->unk540;
        h->ladderDir = 0;
    }
    if (h->ladderDir != 0)
        return;
    if (in & HUMAN_FORWARD) {
        h->ladderDir = 1;
        sub_4D6580(h->anim, 0, 1.0f);
    }
    if (in & HUMAN_BACK) {
        h->ladderDir = -1;
        sub_4D6580(h->anim, 0, -1.0f);
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
    sub_489E70(h, sm->state, 0.0f);
    sub_4137E0(h, 0);
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

    sub_4636E0(h);
    speed = -(long double)h->wireDir[2] * 0x1.613d32p+6f + (long double)h->wireSpeed * 0.99f;
    h->wireSpeed = (float)speed;
    h->vel[0] = (float)(speed * h->wireDir[0]);
    h->vel[1] = (float)(speed * h->wireDir[1]);
    h->vel[2] = (float)(speed * h->wireDir[2]);
}
HOOK(0x40DEF0, HumanPlayer_ZiplineUpdate);
