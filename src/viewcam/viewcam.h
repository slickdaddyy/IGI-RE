#ifndef VIEWCAM_H
#define VIEWCAM_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "qasm/qasmtypes.h"
#include "weapon/weapontypes.h"
#include "humanbase/humanbasetypes.h"

#define VPAD(from, to) char pad##from[(to) - (from)]

typedef void (*TaskHandler)(void *task, void *arg);

/* gTaskHandlers[slot][type] */
extern TaskHandler gTaskHandlers[][0x180];

enum { WEAPON_KIND_GRENADE = 4 };

typedef struct HumanView HumanView;

struct HumanView {
    Task task;
    double pos[3];
    VPAD(0x38, 0x68);
    char anim[0x10];
    Matrix rot;
    float radius;
    VPAD(0xA4, 0xD4);
    int unkD4;
    char unkD8;
    char inWorld;
    char rotDirty;
    VPAD(0xDB, 0xE0);
    double eye[3];
    Matrix cam;
    float pitch;
    float pitchInput;
    float recoilPitch;
    float yaw;
    float unk130;
    float recoilYaw;
    char unk138;
    char unk139;
    char camOverride;
    VPAD(0x13B, 0x140);
    double camPos[3];
    Matrix camRot;
    double target[3];
    char hasTarget;
    VPAD(0x199, 0x19C);
    float offset[3];
    float unk1A8[3];
    float shake;
    float shakeAmount;
    float shakeAngle;
    float shakeStep;
    float dip;
    float dipPhase;
    float weaponPitch;
    float weaponPitchTarget;
    float weaponPitchSpeed;
    float weaponYaw;
    float weaponYawTarget;
    float weaponYawSpeed;
    float fovX;
    float fovY;
    int zoomed;
    float weaponPos[3];
    float unk1FC[3];
    int unk208;
    int bone;
    VPAD(0x210, 0x218);
    float bonePos[3];
    VPAD(0x224, 0x320);
    float duck;
    float duckNow;
};

#define VCHECK(t, f, off) _Static_assert(offsetof(t, f) == (off), #t "." #f)
VCHECK(HumanView, eye, 0xE0);
VCHECK(HumanView, cam, 0xF8);
VCHECK(HumanView, pitch, 0x120);
VCHECK(HumanView, camPos, 0x140);
VCHECK(HumanView, target, 0x180);
VCHECK(HumanView, offset, 0x19C);
VCHECK(HumanView, shake, 0x1B4);
VCHECK(HumanView, weaponPitch, 0x1CC);
VCHECK(HumanView, fovX, 0x1E4);
VCHECK(HumanView, weaponPos, 0x1F0);
VCHECK(HumanView, bone, 0x20C);
VCHECK(HumanView, bonePos, 0x218);
VCHECK(HumanView, duck, 0x320);
_Static_assert(sizeof(HumanView) == 0x328, "HumanView size");
#undef VCHECK

/* fcomp: equal also when unordered */
static inline int x87_eq(long double a, long double b)
{
    return !(a < b || a > b);
}

static inline long double x87_sin(long double x)
{
    __asm__("fsin" : "+t"(x));
    return x;
}

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

void Human_SetPosition(Human *h, double *pos);
void HumanView_UpdatePosition(HumanView *v);
void HumanView_GetEye(double *out, HumanView *v);
void HumanView_GetMatrix(Matrix *out, HumanView *v);
void HumanView_ClearAngles(HumanView *v);
void HumanView_ClearWeaponAngles(HumanView *v);
void HumanView_AddToWorld(HumanView *v);
void HumanView_CreateHandler(HumanView *v);
void HumanView_DeleteHandler(HumanView *v);
void HumanView_EnterHandler(HumanView *v, void *arg);
void HumanView_LeaveWorldHandler(HumanView *v, void **world);
void HumanView_JoinWorldHandler(HumanView *v);
void HumanView_SoundInfoHandler(HumanView *v, void *ear);
void HumanView_ApplyRot(HumanView *v);
char HumanView_WeaponAtTarget(HumanView *v);
char Human_IsCrouching(Human *h);
void Human_HideShadow(Human *h);
void Human_ShowShadow(Human *h);
void Human_SetGround(Human *h, Task *t);
void Human_BloodDrop(void *arg);
void Human_GetEyePos(double *out, Human *h);

#endif
