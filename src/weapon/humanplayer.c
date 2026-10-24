/* Player helpers that live next to the weapon code: weapon keys, ladder,
   wire and zipline attach/detach, damage handlers and the screen effects. */
#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "weapon/humanplayer.h"
#include "weapon/weapon.h"
#include "qvertex/qvertextypes.h"
#include "coll/colltypes.h"
#include "humanai/humanaitypes.h"
#include "loadscreen/loadscreentypes.h"

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, void *, TaskRef_Add, (void *task));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x406220, int *, Profile_GetActive, (void));
ORIG(0x40C140, void, HumanPlayer_SetFlagD44, (HumanPlayer *h));
ORIG(0x40C2D0, void, HumanPlayer_ClearFlagD44, (HumanPlayer *h));
ORIG(0x414E20, void, Model_GetPoint, (double *out, HumanAttach *obj, int i));
ORIG(0x414ED0, void, Model_GetLocalPoint, (float *out, HumanAttach *obj, int i));
ORIG(0x416850, char, Game_GetEvent8, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (void *blast, HumanPlayer *h, Vec3d *pos, float radius, int unk));
ORIG(0x416D20, void *, Game_GetRandom, (void));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x416D80, char, PlayerXPHit, (void));
ORIG(0x43F820, void, Wire_GetMatrix, (float *m, HumanAttach *obj));
ORIG(0x450CE0, void, HumanAI_Broadcast, (void *noise));
ORIG(0x45E060, void, HumanPlayerInput_ReadChannels, (int unk, int unk2));
ORIG(0x460BE0, uint16_t, Human_GetType, (void));
ORIG(0x460C80, void, Human_printf, (HumanPlayer *h, const char *fmt, ...));
ORIG(0x4631A0, void, Human_SetPosition, (HumanPlayer *h, double *pos));
ORIG(0x463220, void, Human_SetYaw, (HumanPlayer *h, float yaw));
ORIG(0x4632D0, void, Human_LeaveGround, (HumanPlayer *h));
ORIG(0x463570, void, Human_DisableCollision, (HumanPlayer *h));
ORIG(0x463580, void, Human_EnableCollision, (HumanPlayer *h));
ORIG(0x463590, void, Human_SetExactMotion, (HumanPlayer *h));
ORIG(0x4635A0, void, Human_ClearExactMotion, (HumanPlayer *h));
ORIG(0x4635F0, void, Human_FreezeView, (HumanPlayer *h));
ORIG(0x463600, void, Human_UnfreezeView, (HumanPlayer *h));
ORIG(0x463610, void, Human_BeginUse, (HumanPlayer *h));
ORIG(0x463620, void, Human_EndUse, (HumanPlayer *h));
ORIG(0x4636A0, void, Human_SetClimbObject, (HumanPlayer *h, int a));
ORIG(0x4636E0, HumanAttach *, Human_GetClimbObject, (HumanPlayer *h));
ORIG(0x463760, void, HumanViewCam, (HumanPlayer *h, int mode));
ORIG(0x4637C0, void, Human_SayTimed, (HumanPlayer *h, const char *sound, int count));
ORIG(0x463AD0, void, Human_Bleed, (HumanPlayer *h, float *dir, int unk));
ORIG(0x464230, void, Human_HideShadow, (HumanPlayer *h));
ORIG(0x464240, void, Human_ShowShadow, (HumanPlayer *h));
ORIG(0x464700, void, HumanView_LockPitch, (HumanView *v));
ORIG(0x464710, void, HumanView_UnlockPitch, (HumanView *v));
ORIG(0x464720, void, HumanView_ClearAngles, (HumanView *v));
ORIG(0x465800, void, HumanView_GetEye, (double *pos, HumanView *v));
ORIG(0x4658A0, void, HumanView_GetForward, (float *dir, HumanView *v));
ORIG(0x465970, void, HumanView_Shake, (HumanView *v, float amount, float angle, int frames));
ORIG(0x467F30, void, ComputerObject_DrawMarker, (void *query));
ORIG(0x487190, uint8_t, StatusScreen_GetEvent, (void));
ORIG(0x489C60, int, Human_GetHitZone, (HumanPlayer *h, void *hit));
ORIG(0x489E10, void, HumanDamage_GetZone, (HumanDamage *d, int part));
ORIG(0x489E70, void, Human_PlayAnimSlot, (HumanPlayer *h, int anim, float blend));
ORIG(0x489ED0, void, Human_PlayWeaponAnim, (HumanPlayer *h, int anim, int loop, float blend));
ORIG(0x489F00, void, Human_StopWeaponAnim, (HumanPlayer *h, int a));
ORIG(0x489F20, void, HumanPlayer_PlayAnim, (HumanPlayer *h, int anim, int loop, float blend));
ORIG(0x491CF0, int *, Display_GetMode, (void));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4B4740, unsigned int, Random_Next, (void *r));
ORIG(0x4B4770, long double, Random_Float, (void *r));
ORIG(0x4B4A00, void, Draw_RectA, (float x, float y, float w, float h, float r, float g, float b, float a, int flags,
                                  int unk));
ORIG(0x4B4BE0, void, Draw_RectGradient, (float x, float y, float w, float h, float r0, float g0, float b0, float r1,
                                         float g1, float b1, float r2, float g2, float b2, float r3, float g3,
                                         float b3, float a0, float a1, float a2, float a3, float z, int flags,
                                         int unk));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, double *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4CC2A0, void, Coll_Lines, (void *t, void *results, const double *starts, const double *ends, int count,
                                  char unk, void *unk2, unsigned int flags));
ORIG(0x4D2D50, long double, BoneObj_GetTicksPerFrame, (void));
ORIG(0x4D65B0, void, Anim_SetTime, (void *anim, int track, float time));
ORIG(0x4E6B00, int, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (int sound));
ORIG(0x4E6C60, void, Sound_SetVolume, (int sound, float unk, float level));
ORIG(0x4E7C30, char, DebugText_IsEnabled, (void));
ORIG(0x4F2060, void *, WeaponSlots_Get, (void **slots, int i));

extern uint16_t gHumanPlayerType;
extern char gInputChannels[][16];
extern uint16_t gTaskType548638;
extern void *gUnk56E09C;
extern float gMoveScale;
extern float gFenceDamage;
extern void *gHumanPlayerList;
extern int gQVertexStrip;

extern WeaponCategory gWeaponCategories[];

extern void (*gRenderFuncs[])(QVertexStrip *);

typedef struct HumanBlast {
    int unk00;
    double *pos;
    int unk08;
    float radius;
    float unk10;
    int shakeFrames;
} HumanBlast;

typedef struct HumanPunch {
    float damage;
    void *attacker;
} HumanPunch;

typedef struct HumanFlash {
    double *pos;
    float radius;
} HumanFlash;

static const float kPi = 0x1.921fb6p+1f;
static const float kEarFade = 0x1.b4e81cp-8f;
static const float kHitFlashFade = 0x1.6c16c2p-6f;
static const float kFlashFadeSlow = 0x1.111112p-6f;
static const float kFlashFadeFast = 0x1.6c16c2p-7f;
static const double kLadderBack = 1515.52;
static const double kLadderUp = 3915.776;
static const double kLadderTopBack = 3481.6;
static const double kLadderTopUp = 1638.4;
static const double kHangSide = 2457.6;
static const double kHangUp = 3891.2;
static const float kWireEnd = 3072.0f;
static const double kWireDown = 2150.4;
static const double kZiplineDown = 3686.4;
static const float kEasy = 0.75f;
static const float kHard = 1.25f;
static const float kShakeMin = 0.25f;
static const float kShakeAmount = 0x1.99999ap+7f;
static const float kShakeAngle = 0x1.aceea0p-7f;
static const double kFlashScale = 0.4;
static const float kPushHit = 0x1.7b425ep+7f;
static const float kPushBlast = 0x1.7b425ep+9f;
static const float kBlastPushRange = 1.5f;
static const float kBlastEarRange = 102400.0f;
static const float kBlastEarScale = 0x1.47ae14p-17f;
static const float kBlastRadius = 0x1.555556p-2f;
static const double kForward = 4096.0;
static const double kFlashbangEarRange = 102400.0;
static const float kAnimTimeRandom = 0x1.666666p-3f;
static const float kBarX = 0x1.433334p-2f;
static const float kBarY = 0x1.accccc0p-1f;
static const float kBarW = 0x1.79999ap-2f;
static const float kBarH = 0x1.555556p-7f;
static const float kFrameX = 0.3125f;
static const float kFrameY = 0x1.aaaaaap-1f;
static const float kFrameW = 0.375f;
static const float kFrameH = 0x1.333334p-6f;
static const float kLineZ = 0x1.0624dep-10f;
static const double kRangeDistance = 4096000.0;
static const double kUnitsToMeters = 1.0 / 4096;
static const float kMaxFlash = 0x1.fae148p-1f;

/* fcomp/fnstsw/test ah,0x40: equal, or unordered */
static int is_zero(long double x)
{
    return !(x < 0 || x > 0);
}

static void State_Set(HumanPlayer *h, int n)
{
    h->state.prev = h->state.state;
    h->state.state = n;
    h->state.update = h->state.enter[n];
    h->state.update(&h->state, (struct Human *)h);
}

/* weapon category keys: pick the next weapon of that category the player has */
void HumanPlayer_SelectWeaponCategory(HumanPlayer *h)
{
    WeaponCategory *c;
    int cat = -1, i, n, id;

    for (i = 0; i < 9; i++)
        if (!is_zero(h->input[INPUT_CATEGORY + i]) && is_zero(h->prevInput[INPUT_CATEGORY + i]))
            cat = i;
    if (cat < 0)
        return;
    if (cat != h->weaponCategory)
        h->weaponCycle = 0;
    c = &gWeaponCategories[cat];
    for (n = 0; n < c->count; n++) {
        id = c->ids[h->weaponCycle];
        if (++h->weaponCycle >= c->count)
            h->weaponCycle = 0;
        if (Task_FindWeapon(h, id)) {
            h->input[INPUT_WEAPON] = (float)id;
            break;
        }
    }
    h->weaponCategory = cat;
}
HOOK(0x411000, HumanPlayer_SelectWeaponCategory);

/* ringing ears after a flashbang or a close explosion */
void HumanPlayer_UpdateEarRinging(HumanPlayer *h)
{
    long double v = h->earLevel - kEarFade;

    h->earLevel = v;
    if (!(v >= 0))
        h->earLevel = 0;
    else if (v > 1.0f)
        h->earLevel = 1.0f;
    if (is_zero(h->earLevel)) {
        if (h->earSound) {
            Sound_Stop(h->earSound);
            h->earSound = 0;
        }
    } else if (!h->earSound) {
        h->earSound = Sound_Play(h->view, "earcandy");
    }
    if (h->earSound)
        Sound_SetVolume(h->earSound, -1.0f, h->earLevel);
}
HOOK(0x411240, HumanPlayer_UpdateEarRinging);

void HumanPlayer_FadeHitFlash(HumanPlayer *h)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (!is_zero(h->hitFlash[i])) {
            long double v = h->hitFlash[i] - kHitFlashFade;

            h->hitFlash[i] = v;
            if (!(v >= 0))
                h->hitFlash[i] = 0;
        }
    }
}
HOOK(0x411300, HumanPlayer_FadeHitFlash);

/* white screen from a flashbang, fades slower while above 1 */
void HumanPlayer_FadeFlash(HumanPlayer *h)
{
    if (is_zero(h->flash))
        return;
    h->flash = h->flash - (h->flash > 1.0f ? kFlashFadeFast : kFlashFadeSlow);
    if (!(h->flash >= 0))
        h->flash = 0;
}
HOOK(0x411350, HumanPlayer_FadeFlash);

void HumanPlayer_UpdateFenceTimer(HumanPlayer *h)
{
    if (h->fenceTimer)
        h->fenceTimer--;
}
HOOK(0x4113C0, HumanPlayer_UpdateFenceTimer);

/* the common part of grabbing a ladder, wire or zipline */
static void Attach_Begin(HumanPlayer *h)
{
    h->vel[0] = h->vel[1] = h->vel[2] = 0;
    h->push[0] = h->push[1] = h->push[2] = 0;
}

/* starts or ends climbing the attached ladder, from the bottom or the top */
void HumanPlayer_UpdateLadder(HumanPlayer *h)
{
    HumanAttach *obj = Human_GetClimbObject(h);
    const float *m = obj->matrix;
    float ax, ay, az;
    double v[3], b, c;
    long double a, x;

    if (!h->ladderEnter) {
        if (!h->ladderLeave)
            return;
        h->ladderLeave = 0;
        h->onLadder = 0;
        Human_EndUse(h);
        Human_UnfreezeView(h);
        HumanView_UnlockPitch(h->view);
        Human_ShowShadow(h);
        Human_EnableCollision(h);
        Human_ClearExactMotion(h);
        HumanViewCam(h, 1);
        HumanPlayer_ClearFlagD44(h);
        Human_PlayAnimSlot(h, 0, 0);
        if (h->state.input & HUMAN_DEAD)
            State_Set(h, HSTATE_DIE);
        return;
    }
    if (h->ladderFromTop) {
        Model_GetPoint(v, obj, 3);
        x = v[0] + obj->pos.x;
        v[1] = v[1] + obj->pos.y;
        v[2] = v[2] + obj->pos.z;
        a = m[2] * kLadderTopBack - m[1] * kLadderTopUp;
        b = m[5] * kLadderTopBack - m[4] * kLadderTopUp;
        c = m[8] * kLadderTopBack - m[7] * kLadderTopUp;
    } else {
        Model_GetPoint(v, obj, 2);
        x = v[0] + obj->pos.x;
        v[1] = v[1] + obj->pos.y;
        v[2] = v[2] + obj->pos.z;
        a = m[1] * kLadderBack + m[2] * kLadderUp;
        b = m[4] * kLadderBack + m[5] * kLadderUp;
        c = m[7] * kLadderBack + m[8] * kLadderUp;
    }
    v[0] = a + x;
    v[1] = b + v[1];
    v[2] = c + v[2];
    Human_SetPosition(h, v);
    Matrix_GetEuler(m, &ax, &ay, &az);
    az = az + kPi;
    Human_SetYaw(h, az);
    h->ladderPos = h->ladderFromTop ? obj->steps - 6 : 0;
    Attach_Begin(h);
    h->ladderEnter = 0;
    h->onLadder = 1;
    Human_BeginUse(h);
    Human_FreezeView(h);
    HumanView_LockPitch(h->view);
    HumanView_ClearAngles(h->view);
    Human_DisableCollision(h);
    Human_HideShadow(h);
    Human_LeaveGround(h);
    Human_SetExactMotion(h);
    HumanViewCam(h, 3);
    HumanPlayer_SetFlagD44(h);
    h->ladderTop = obj->steps;
    State_Set(h, h->ladderFromTop ? HSTATE_LADDER_FROM_TOP : HSTATE_LADDER);
}
HOOK(0x4113E0, HumanPlayer_UpdateLadder);

/* attach for state 41; while attached it keeps sending a noise */
void HumanPlayer_UpdateFenceClimb(HumanPlayer *h)
{
    HumanAttach *obj = Human_GetClimbObject(h);
    const float *m = obj->matrix;
    long double dx, dy, dz;
    double lx, ly, k, v[3];
    float ax, ay, az, turn;

    if (h->unkD90) {
        dx = h->pos.x - obj->pos.x;
        dy = h->pos.y - obj->pos.y;
        dz = h->pos.z - obj->pos.z;
        lx = m[3] * dy + m[6] * dz + m[0] * dx;
        ly = m[4] * dy + m[7] * dz + m[1] * dx;
        if (!(ly >= 0)) {
            k = -kHangSide;
            turn = 0;
        } else {
            k = kHangSide;
            turn = kPi;
        }
        v[0] = m[1] * k + m[0] * lx + m[2] * kHangUp;
        v[1] = m[4] * k + m[3] * lx + m[5] * kHangUp;
        v[2] = m[7] * k + m[6] * lx + m[8] * kHangUp;
        v[0] = v[0] + obj->pos.x;
        v[1] = v[1] + obj->pos.y;
        v[2] = v[2] + obj->pos.z;
        Human_SetPosition(h, v);
        Matrix_GetEuler(m, &ax, &ay, &az);
        az = az + turn;
        Human_SetYaw(h, az);
        Attach_Begin(h);
        h->unkD90 = 0;
        h->unkD91 = 1;
        Human_BeginUse(h);
        Human_FreezeView(h);
        HumanView_ClearAngles(h->view);
        HumanView_LockPitch(h->view);
        Human_DisableCollision(h);
        Human_HideShadow(h);
        Human_LeaveGround(h);
        Human_SetExactMotion(h);
        HumanViewCam(h, 3);
        HumanPlayer_SetFlagD44(h);
        State_Set(h, HSTATE_UNK41);
    } else if (h->unkD92) {
        h->unkD92 = 0;
        h->unkD91 = 0;
        Human_EndUse(h);
        Human_UnfreezeView(h);
        HumanView_UnlockPitch(h->view);
        Human_ShowShadow(h);
        Human_EnableCollision(h);
        Human_ClearExactMotion(h);
        HumanViewCam(h, 1);
        HumanPlayer_ClearFlagD44(h);
        Human_SetClimbObject(h, 0);
        Human_PlayAnimSlot(h, 0, 0);
        if (h->state.input & HUMAN_DEAD)
            State_Set(h, HSTATE_DIE);
    } else {
        AIMessage n;

        n.type = 0xD;
        n.team = h->unk0FC;
        n.flags = 0x46;
        n.radius = 20480.0f;
        n.pos = (Vec3d *)(DynCubeObj_GetPosition(h));
        n.from = (Task *)h;
        n.about = (Task *)h;
        HumanAI_Broadcast(&n);
    }
}
HOOK(0x411770, HumanPlayer_UpdateFenceClimb);

/* lateral position of the player along the wire, in the wire's frame */
static long double Wire_Side(HumanPlayer *h, HumanAttach *obj, const float *m)
{
    long double dx = h->pos.x - obj->pos.x;
    long double dy = h->pos.y - obj->pos.y;
    long double dz = h->pos.z - obj->pos.z;

    return m[7] * dz + m[4] * dy + m[1] * dx;
}

/* true when y is past one of the ends of the wire */
static int Wire_Off(HumanAttach *obj, double y)
{
    float e0[3], e1[3];

    Model_GetLocalPoint(e0, obj, 0);
    Model_GetLocalPoint(e1, obj, 1);
    return e0[1] + kWireEnd > y || !(e1[1] - kWireEnd >= y);
}

/* the leave checks shared by the wire and the zipline */
static int Wire_Leave(HumanPlayer *h, int off)
{
    if (!is_zero(h->input[9]) && is_zero(h->prevInput[9]))
        return 1;
    return off || (h->state.input & HUMAN_DEAD);
}

static void Wire_Detach(HumanPlayer *h)
{
    Human_EndUse(h);
    Human_UnfreezeView(h);
    HumanView_UnlockPitch(h->view);
    Human_EnableCollision(h);
    Human_ClearExactMotion(h);
    HumanViewCam(h, 1);
    HumanPlayer_ClearFlagD44(h);
}

static void Wire_Done(HumanPlayer *h, HumanAttach *obj)
{
    Human_ShowShadow(h);
    obj->unk130 = 0;
    State_Set(h, h->state.input & HUMAN_DEAD ? HSTATE_DIE : HSTATE_UNK29);
}

/* hand over hand along a wire */
void HumanPlayer_UpdateWire(HumanPlayer *h)
{
    HumanAttach *obj = Human_GetClimbObject(h);
    float m[10], e0[3], e1[3], ax, ay, yaw;
    double side, y, v[3];

    if (!h->wireEnter) {
        Wire_GetMatrix(m, obj);
        side = Wire_Side(h, obj, m);
        if (!Wire_Leave(h, Wire_Off(obj, side)))
            return;
        h->onWire = 0;
        Wire_Detach(h);
        Wire_Done(h, obj);
        return;
    }
    Wire_GetMatrix(m, obj);
    side = Wire_Side(h, obj, m);
    y = side;
    Model_GetLocalPoint(e0, obj, 0);
    Model_GetLocalPoint(e1, obj, 1);
    if (e0[1] + kWireEnd > side)
        y = e0[1] + kWireEnd;
    else if (!(e1[1] - kWireEnd >= side))
        y = e1[1] - kWireEnd;
    v[0] = m[1] * y - m[2] * kWireDown;
    v[1] = m[4] * y - m[5] * kWireDown;
    v[2] = m[7] * y - m[8] * kWireDown;
    v[0] = v[0] + obj->pos.x;
    v[1] = v[1] + obj->pos.y;
    v[2] = v[2] + obj->pos.z;
    Human_SetPosition(h, v);
    Attach_Begin(h);
    Matrix_GetEuler(m, &ax, &ay, &yaw);
    if (y > 0)
        yaw = yaw + kPi;
    Human_SetYaw(h, yaw);
    h->wireEnter = 0;
    h->onWire = 1;
    Human_BeginUse(h);
    Human_FreezeView(h);
    HumanView_LockPitch(h->view);
    Human_DisableCollision(h);
    Human_LeaveGround(h);
    HumanView_ClearAngles(h->view);
    Human_SetExactMotion(h);
    HumanViewCam(h, 3);
    HumanPlayer_SetFlagD44(h);
    Human_HideShadow(h);
    State_Set(h, HSTATE_WIRE);
}
HOOK(0x411AE0, HumanPlayer_UpdateWire);

void HumanPlayer_UpdateZipline(HumanPlayer *h)
{
    HumanAttach *obj = Human_GetClimbObject(h);
    float m[10], e0[3], e1[3], ax, ay, yaw;
    double side, a[3], b[3], d[3], v[3];
    long double y;

    if (!h->ziplineEnter) {
        Wire_GetMatrix(m, obj);
        side = Wire_Side(h, obj, m);
        if (!Wire_Leave(h, Wire_Off(obj, side)))
            return;
        h->onZipline = 0;
        Wire_Detach(h);
        if (h->ziplineSound) {
            Sound_Stop(h->ziplineSound);
            h->ziplineSound = 0;
        }
        Wire_Done(h, obj);
        return;
    }
    Wire_GetMatrix(m, obj);
    side = Wire_Side(h, obj, m);
    Model_GetLocalPoint(e0, obj, 0);
    Model_GetLocalPoint(e1, obj, 1);
    y = e0[1] + kWireEnd;
    if (!(y > side)) {
        y = e1[1] - kWireEnd;
        if (y >= side)
            y = side;
    }
    v[0] = m[1] * y - m[2] * kZiplineDown;
    v[1] = m[4] * y - m[5] * kZiplineDown;
    v[2] = m[7] * y - m[8] * kZiplineDown;
    v[0] = v[0] + obj->pos.x;
    v[1] = v[1] + obj->pos.y;
    v[2] = v[2] + obj->pos.z;
    Human_SetPosition(h, v);
    Matrix_GetEuler(m, &ax, &ay, &yaw);
    Model_GetPoint(a, obj, 0);
    Model_GetPoint(b, obj, 1);
    d[0] = b[0] - a[0];
    d[1] = b[1] - a[1];
    d[2] = b[2] - a[2];
    if (d[2] > 0) {
        d[0] = -d[0];
        d[1] = -d[1];
        d[2] = -d[2];
        yaw = yaw + kPi;
    }
    Vec3d_Normalize(d);
    h->ziplineSpeed = 0;
    h->ziplineDir[0] = d[0];
    h->ziplineDir[1] = d[1];
    h->ziplineDir[2] = d[2];
    Human_SetYaw(h, yaw);
    h->ziplineEnter = 0;
    h->onZipline = 1;
    Human_BeginUse(h);
    Human_FreezeView(h);
    HumanView_LockPitch(h->view);
    HumanView_ClearAngles(h->view);
    Human_DisableCollision(h);
    Human_SetExactMotion(h);
    Human_LeaveGround(h);
    HumanViewCam(h, 3);
    HumanPlayer_SetFlagD44(h);
    h->ziplineSound = Sound_Play(h, "wire_slide_1");
    Human_HideShadow(h);
    State_Set(h, HSTATE_ZIPLINE);
}
HOOK(0x411F20, HumanPlayer_UpdateZipline);

typedef struct InputChannelQuery {
    int index;
    char *channel;
} InputChannelQuery;

void HumanPlayer_InputChannelHandler(void *task, InputChannelQuery *q)
{
    (void)task;
    if (q->index >= 0 && q->index < 40)
        q->channel = gInputChannels[q->index];
    else
        q->channel = NULL;
}
HOOK(0x4123F0, HumanPlayer_InputChannelHandler);

void HumanPlayer_ReadInputHandler(void *task, int *arg)
{
    (void)task;
    HumanPlayerInput_ReadChannels(*arg, 0);
}
HOOK(0x412420, HumanPlayer_ReadInputHandler);

static float Damage_Scale(const int *profile, float dmg)
{
    int level = *profile;

    switch (level) {
    case 0:
        return dmg * kEasy;
    case 1:
        return dmg;
    case 2:
        return dmg * kHard;
    default:
        Warning_Show("Difficulty level not defined %d", level);
        return dmg;
    }
}

static void Damage_Add(HumanDamage *d, int part, float dmg)
{
    if (!PlayerXPHit()) {
        d->part[part].dmg = dmg + d->part[part].dmg;
        d->total = dmg + d->total;
    }
}

static float Flash_Random(void)
{
    return (Random_Float(Game_GetRandom()) + 1.0) * kFlashScale;
}

static void Flash_RandomAll(HumanPlayer *h)
{
    h->hitFlash[0] = Flash_Random();
    h->hitFlash[1] = Flash_Random();
    h->hitFlash[2] = Flash_Random();
    h->hitFlash[3] = Flash_Random();
}

static void Shake_Random(HumanPlayer *h)
{
    long double r = Random_Float(Game_GetRandom());

    if (!(r >= kShakeMin))
        r = kShakeMin;
    else if (r > 1.0f)
        r = 1.0f;
    HumanView_Shake(h->view, r * kShakeAmount, r * kShakeAngle, 15);
}

/* pushes the player along the horizontal part of a direction */
static void Push_Add(HumanPlayer *h, long double x, float y, long double z, float k)
{
    float py;

    if (!(is_zero(x) && is_zero(y) && is_zero(z))) {
        long double n = 1.0 / sqrtl(x * x + y * y + z * z);

        x = x * n;
        y = y * n;
    }
    x = x * k;
    py = y * k;
    h->push[0] = x + h->push[0];
    h->push[1] = py + h->push[1];
}

static int *Profile_Check(void)
{
    int *p = Profile_GetActive();

    if (!p)
        Warning_Show("No active playerprofile found");
    return p;
}

void HumanPlayer_HitHandler(HumanPlayer *h, HumanHit *hit)
{
    HumanDamage *d = &h->damage;
    float *dir = hit->dir;
    int *profile, part;
    float a, b;
    long double c, f, g;

    if (!h->vulnerable)
        return;
    profile = Profile_Check();
    if (!profile)
        return;
    part = Human_GetHitZone(h, hit);
    d->hitZone = part;
    HumanDamage_GetZone(d, part);
    Damage_Add(d, part, Damage_Scale(profile, d->part[part].mult * hit->damage));
    if (d->total >= d->max && !h->attacker && hit->attacker &&
        TaskType_IsA(*(uint16_t *)((char *)hit->attacker + 0x1C), gTaskType548638))
        h->attacker = TaskRef_Add(hit->attacker);
    Human_Bleed(h, dir, (int)hit->shot);
    if (!h->quiet)
        Human_SayTimed(h, "player_hit_", 4);
    Shake_Random(h);

    /* flash the screen edges on the side the hit came from */
    if (is_zero(dir[0]) && is_zero(dir[1]) && is_zero(dir[2])) {
        f = Flash_Random();
        h->hitFlash[0] = h->hitFlash[1] = h->hitFlash[2] = h->hitFlash[3] = f;
    } else {
        const float *m = h->view->matrix;

        f = (Random_Float(Game_GetRandom()) + 1.0) * kFlashScale;
        a = m[0] * dir[0] + m[6] * dir[2] + m[3] * dir[1];
        b = m[1] * dir[0] + m[7] * dir[2] + m[4] * dir[1];
        c = m[2] * dir[0] + m[8] * dir[2] + m[5] * dir[1];
        g = f * 0.5f;
        if (fabsf(a) > fabsf(b) && fabsf(a) > fabsl(c)) {
            if (a > 0) {
                h->hitFlash[0] = f;
                h->hitFlash[1] = f;
                h->hitFlash[2] = g;
                h->hitFlash[3] = g;
            } else {
                h->hitFlash[0] = g;
                h->hitFlash[1] = g;
                h->hitFlash[2] = f;
                h->hitFlash[3] = f;
            }
        } else if (fabsf(b) >= fabsf(a) && fabsf(b) >= fabsl(c)) {
            if (b > 0) {
                h->hitFlash[0] = g;
                h->hitFlash[1] = f;
                h->hitFlash[2] = f;
                h->hitFlash[3] = g;
            } else {
                h->hitFlash[0] = h->hitFlash[1] = h->hitFlash[2] = h->hitFlash[3] = f;
            }
        } else if (c > 0) {
            h->hitFlash[0] = g;
            h->hitFlash[1] = f;
            h->hitFlash[2] = f;
            h->hitFlash[3] = g;
        } else {
            h->hitFlash[0] = f;
            h->hitFlash[1] = g;
            h->hitFlash[2] = g;
            h->hitFlash[3] = f;
        }
    }
    if (!h->noclip && !(is_zero(dir[0]) && is_zero(dir[1]) && is_zero(dir[2])))
        Push_Add(h, dir[0], dir[1], dir[2], kPushHit);
}
HOOK(0x412440, HumanPlayer_HitHandler);

void HumanPlayer_BlastHandler(HumanPlayer *h, HumanBlast *e)
{
    HumanDamage *d = &h->damage;
    int *profile;
    double cam[3], v[3];
    long double dist, f, r;

    if (!h->vulnerable)
        return;
    profile = Profile_Check();
    if (!profile)
        return;
    HumanView_GetEye(cam, h->view);
    v[0] = cam[0] - e->pos[0];
    v[1] = cam[1] - e->pos[1];
    v[2] = cam[2] - e->pos[2];
    dist = Vec3d_Length(v);
    if (!(dist >= (e->unk10 + e->radius) * kBlastPushRange) && !h->noclip)
        Push_Add(h, v[0], v[1], v[2], kPushBlast);
    if (!(dist >= kBlastEarRange))
        h->earLevel = (1.0 - dist * kBlastEarScale) + h->earLevel;
    f = dist - e->radius * 3.0f;
    if (!(f >= 0))
        f = 1.0f;
    else
        f = 1.0f - f / (e->radius * 6.0f);
    if (f > 0)
        HumanView_Shake(h->view, f * kShakeAmount, f * kShakeAngle, e->shakeFrames);
    d->hitZone = 13;
    HumanDamage_GetZone(d, 13);
    r = Game_BlastFalloff(e, h, &h->pos, h->radius * kBlastRadius, 1);
    if (is_zero(r))
        return;
    Damage_Add(d, 13, Damage_Scale(profile, r * d->part[13].mult));
    if (!h->quiet)
        Human_SayTimed(h, "player_xplhit_", 1);
    Flash_RandomAll(h);
}
HOOK(0x412990, HumanPlayer_BlastHandler);

void HumanPlayer_PunchHandler(HumanPlayer *h, HumanPunch *p)
{
    HumanDamage *d = &h->damage;
    int *profile, part;
    double fwd[3], v[3], *a, *b;

    if (!h->vulnerable)
        return;
    profile = Profile_Check();
    if (!profile)
        return;
    fwd[0] = h->matrix[1] * kForward;
    fwd[1] = h->matrix[4] * kForward;
    fwd[2] = h->matrix[7] * kForward;
    a = DynCubeObj_GetPosition(h);
    b = DynCubeObj_GetPosition(p->attacker);
    v[0] = a[0] - b[0];
    a = DynCubeObj_GetPosition(h);
    b = DynCubeObj_GetPosition(p->attacker);
    v[1] = a[1] - b[1];
    a = DynCubeObj_GetPosition(h);
    b = DynCubeObj_GetPosition(p->attacker);
    v[2] = a[2] - b[2];
    Vec3d_Normalize(v);
    part = v[2] * fwd[2] + v[1] * fwd[1] + v[0] * fwd[0] >= 0 ? 14 : 13;
    d->hitZone = part;
    HumanDamage_GetZone(d, part);
    Damage_Add(d, part, Damage_Scale(profile, d->part[part].mult * p->damage));
    if (!h->quiet) {
        Human_SayTimed(h, "player_hit_", 4);
        Sound_Play(h->view, Random_Next(Game_GetRandom()) & 1 ? "punched_01" : "punched_02");
    }
    Shake_Random(h);
    Flash_RandomAll(h);
}
HOOK(0x412CE0, HumanPlayer_PunchHandler);

void HumanPlayer_FenceHandler(HumanPlayer *h)
{
    HumanDamage *d = &h->damage;
    long double dmg;

    if (PlayerXPHit() || h->fenceTimer)
        return;
    d->hitZone = 13;
    HumanDamage_GetZone(d, 13);
    h->fenceTimer = 45;
    dmg = gFenceDamage * d->part[13].mult;
    d->part[13].dmg = dmg + d->part[13].dmg;
    d->total = dmg + d->total;
    HumanView_Shake(h->view, kShakeAmount, kShakeAngle, 45);
    Flash_RandomAll(h);
    if (!(d->total >= d->max))
        Sound_Play(h->view, "elfence_spark1");
}
HOOK(0x412F80, HumanPlayer_FenceHandler);

void HumanPlayer_FlashbangHandler(HumanPlayer *h, HumanFlash *e)
{
    double cam[3], v[3];
    long double dist;

    HumanView_GetEye(cam, h->view);
    v[0] = e->pos[0] - cam[0];
    v[1] = e->pos[1] - cam[1];
    v[2] = e->pos[2] - cam[2];
    dist = Vec3d_Length(v);
    if (!(dist >= kFlashbangEarRange))
        h->earLevel = 1.0f;
    if (!(dist - e->radius >= 0.0))
        h->flash = 2.0f;
}
HOOK(0x413090, HumanPlayer_FlashbangHandler);

/* the bar of a timed action (lock picking...) and its frame */
static void Render_ActionBar(HumanPlayer *h, int width, int height, float *top)
{
    QVertexStrip s;
    QVertex v[2];
    float fw = width, fh = height;
    float x, y, w, hh, right, bottom;

    Draw_RectA(fw * kBarX, fh * kBarY, (long double)h->barValue / h->barMax * fw * kBarW, fh * kBarH, 128.0f, 0, 0,
               0.5f, 0x80000, 0);
    hh = fh * kFrameH;
    w = fw * kFrameW;
    y = fh * kFrameY;
    x = fw * kFrameX;
    Draw_RectA(x, y, w, hh, 0, 100.0f, 0, 0.5f, 0x80000, 0);
    *top = hh;
    right = w + x;
    bottom = hh + y;
    v[0].z = v[1].z = kLineZ;
    v[0].u = v[1].u = 0;
    v[0].v = v[1].v = 0;
    v[0].a = v[1].a = 1.0f;
    v[0].r = v[1].r = 0;
    v[0].g = v[1].g = 1.0f;
    v[0].b = v[1].b = 0;
    s.lines = 1;
    s.flags = 0;
    s.queue = 0;
    s.count = 2;
    s.verts = v;
    v[0].x = x;
    v[0].y = y;
    v[1].x = right;
    v[1].y = y;
    gRenderFuncs[gQVertexStrip](&s);
    v[0].x = x;
    v[0].y = bottom;
    v[1].x = right;
    v[1].y = bottom;
    gRenderFuncs[gQVertexStrip](&s);
    v[0].x = x;
    v[0].y = y;
    v[1].x = x;
    v[1].y = bottom;
    gRenderFuncs[gQVertexStrip](&s);
    v[0].x = right;
    v[0].y = y;
    v[1].x = right;
    v[1].y = bottom + 1.0f;
    gRenderFuncs[gQVertexStrip](&s);
}

/* debug range finder: distance to what the view points at */
static void Render_Range(HumanPlayer *h, CollIgnore *ignore)
{
    double cam[3], end[3];
    float dir[3];
    char hit[0x70];
    char text[24];
    void *task, *world;
    char unk;

    ignore->task = (Task *)h;
    HumanView_GetEye(cam, h->view);
    HumanView_GetForward(dir, h->view);
    end[1] = dir[1];
    end[2] = dir[2];
    end[1] = end[1] * kRangeDistance;
    end[2] = end[2] * kRangeDistance;
    end[0] = cam[0] + dir[0] * kRangeDistance;
    end[1] = cam[1] + end[1];
    end[2] = cam[2] + end[2];
    unk = Game_GetEvent8();
    world = Game_GetWorld();
    Coll_Lines(world, hit, cam, end, 1, unk, ignore, -1);
    if (*(int *)(hit + 0x58)) {
        crt_sprintf(text, "RANGE: %0.2fM\n", *(double *)(hit + 0x60) * kUnitsToMeters);
        task = *(void **)hit;
        if (task && TaskType_IsA(*(uint16_t *)((char *)task + 0x1C), RigidDynCubeObj_GetType()))
            Human_printf(h, "Model: %s\n", *(char **)((char *)task + 0x6C) + 0x14);
    } else {
        crt_sprintf(text, "RANGE: 1000+M\n");
    }
    Human_printf(h, text);
}

void HumanPlayer_RenderHandler(HumanPlayer *h, void *arg)
{
    char *settings = *(char **)(Game_Get() + 0x5C);
    int width = Display_GetMode()[1];
    int height = Display_GetMode()[2];
    CollIgnore ignore;
    void *w;
    float a;
    uint8_t ev;
    uint16_t type;

    if (h->savedWeapon) {
        w = WeaponSlots_Get(h->weapons, 0);
        if (w)
            gTaskHandlers[StatusScreen_GetEvent()][*(uint16_t *)((char *)w + 0x1C)](w, arg);
        return;
    }
    ev = StatusScreen_GetEvent();
    type = Human_GetType();
    gTaskHandlers[ev][type](h, arg);
    if (h->showBar)
        Render_ActionBar(h, width, height, (float *)&ignore.unk0);
    if (DebugText_IsEnabled())
        Render_Range(h, &ignore);
    if (*(int *)(settings + 0xE8) != 1)
        return;
    if (!(is_zero(h->hitFlash[0]) && is_zero(h->hitFlash[1]) && is_zero(h->hitFlash[2]) && is_zero(h->hitFlash[3])))
        Draw_RectGradient(0, 0, width, height, 255.0f, 0, 0, 255.0f, 0, 0, 255.0f, 0, 0, 255.0f, 0, 0, h->hitFlash[0],
                          h->hitFlash[3], h->hitFlash[1], h->hitFlash[2], 0, 0x80000, 0);
    if (!is_zero(h->flash)) {
        a = h->flash;
        if (!(a >= 0))
            a = 0;
        else if (a > kMaxFlash)
            a = kMaxFlash;
        Draw_RectA(0, 0, width, height, 255.0f, 255.0f, 255.0f, a, 0x80000, 0);
    }
}
HOOK(0x413120, HumanPlayer_RenderHandler);

typedef struct HumanShapeQuery {
    char unk00[0xC];
    Vec3d *pos;
    float *matrix;
    float radius;
    int unk18;
    int unk1C;
    int unk20;
} HumanShapeQuery;

void HumanPlayer_MapMarkerHandler(HumanPlayer *h, HumanShapeQuery *q)
{
    q->pos = &h->pos;
    q->matrix = h->matrix;
    q->radius = h->radius;
    q->unk1C = 2;
    q->unk20 = 3;
    ComputerObject_DrawMarker(q);
}
HOOK(0x413730, HumanPlayer_MapMarkerHandler);

void HumanPlayer_Close(void)
{
    QTaskList_Free(gHumanPlayerList);
    QTaskList_Free(gUnk56E09C);
    TaskType_Free(gHumanPlayerType);
}
HOOK(0x413770, HumanPlayer_Close);

void HumanPlayer_LoadParameters(void)
{
    void *script = Script_Load("LOCAL:humanplayer/humanplayer.qsc");

    if (!script) {
        Error_Show("HumanPlayer_LoadParameters: Couldn't load script \"%s\"", "LOCAL:humanplayer/humanplayer.qsc");
        for (;;)
            ;
    }
    Script_Run(script);
    Script_Free(script);
}
HOOK(0x4137A0, HumanPlayer_LoadParameters);

/* plays the body and weapon animations of the current weapon */
void HumanPlayer_SetWeaponAnim(HumanPlayer *h, int anim)
{
    HumanView *view = h->view;
    int body = -1, weapon = -1, bodyLoop = 0, weaponLoop = 0;
    float bodyBlend = 0.4f, weaponBlend = 0.4f, time = 0, r;
    void *slot;
    WeaponType *w;

    h->weaponAnim = anim;
    if (!anim) {
        h->weaponAnimReq = 0;
        Human_StopWeaponAnim(h, 0);
        return;
    }
    slot = WeaponSlots_Get(h->weapons, 0);
    if (!slot)
        return;
    w = Weapon_GetType(slot);
    switch (anim) {
    case 1:
    case 9:
        body = w->anim[0];
        weapon = w->anim2[0];
        bodyLoop = weaponLoop = 1;
        break;
    case 2:
        body = w->anim[1];
        weapon = w->anim2[1];
        bodyLoop = weaponLoop = 1;
        break;
    case 3:
        body = w->anim[1];
        weapon = w->anim2[2];
        bodyLoop = weaponLoop = 1;
        break;
    case 4:
        body = w->anim[0];
        weapon = w->anim2[3];
        bodyLoop = weaponLoop = 1;
        break;
    case 5:
        body = w->anim[0];
        weapon = w->anim2[4];
        bodyLoop = weaponLoop = 1;
        break;
    case 6:
        body = w->anim[2];
        weapon = w->anim2[5];
        bodyBlend = weaponBlend = 0;
        r = Random_Float(Game_GetRandom()) * kAnimTimeRandom;
        time = BoneObj_GetTicksPerFrame() * r * 30.0f;
        break;
    case 7:
        body = w->anim[5];
        weapon = w->anim2[6];
        bodyBlend = weaponBlend = 0;
        break;
    case 8:
        if (h->meleeCount >= 0 && h->meleeCount <= 2) {
            body = w->anim[2 + h->meleeCount];
            weapon = w->anim2[5];
        }
        bodyBlend = weaponBlend = 0;
        break;
    case 10:
        body = w->anim[0];
        weapon = w->anim2[0];
        bodyLoop = weaponLoop = 1;
        bodyBlend = weaponBlend = 0;
        break;
    default:
        return;
    }
    if (body != -1) {
        HumanPlayer_PlayAnim(h, body, bodyLoop, bodyBlend);
        if (!is_zero(time))
            Anim_SetTime(view->anim, 0, time);
    }
    if (weapon != -1)
        Human_PlayWeaponAnim(h, weapon, weaponLoop, weaponBlend);
}
HOOK(0x4137E0, HumanPlayer_SetWeaponAnim);

void HumanPlayer_RequestWeaponAnim(HumanPlayer *h, int anim)
{
    h->weaponAnimReq = anim;
}
HOOK(0x413A60, HumanPlayer_RequestWeaponAnim);

uint16_t HumanPlayer_GetType(void)
{
    return gHumanPlayerType;
}
HOOK(0x413A70, HumanPlayer_GetType);

float HumanPlayer_GetMoveScale(void)
{
    return gMoveScale;
}
HOOK(0x413A80, HumanPlayer_GetMoveScale);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 538720, 538720, 0x150) INIT({0x30, 0x8D, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x24, 0x8D, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x1C, 0x8D, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x14, 0x8D, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x10, 0x8D, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x08, 0x8D, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x8D, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xF0, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xE8, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xDC, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xD4, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xCC, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xC0, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xB8, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0xA0, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x90, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8, 0x42, 0x88, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x7C, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x70, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x5C, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x50, 0x8C, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F});
ALIAS(gInputChannels, pad_538720, 0);
BSS(A84A54) int gQVertexStrip;
