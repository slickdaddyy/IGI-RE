#ifndef VIEWCAM_H
#define VIEWCAM_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

#define VPAD(from, to) char pad##from[(to) - (from)]

typedef void (*TaskHandler)(void *task, void *arg);

/* gTaskHandlers[slot][type] */
#define gTaskHandlers ((TaskHandler (*)[0x180])0xA96AE0)

typedef struct Task {
    struct Task *next;
    struct Task *prev;
    void *children[3];
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

/* count, then the tasks from +8 */
typedef struct TaskArray {
    int count;
    int unk4;
    Task *items[1];
} TaskArray;

/* rotation plus a serial number that changes with it */
typedef struct Rot {
    float m[9];
    int serial;
} Rot;

typedef struct WeaponInfo {
    VPAD(0x0, 0x610);
    int kind;
    VPAD(0x614, 0x660);
    float throwSpeed;
} WeaponInfo;

enum { WEAPON_KIND_GRENADE = 4 };

typedef struct HumanView HumanView;

typedef struct Human {
    Task task;
    double pos[3];
    VPAD(0x38, 0x68);
    char anim[0x10];
    Rot rot;
    float radius;
    VPAD(0xA4, 0xFC);
    int team;
    VPAD(0x100, 0x110);
    float prevInput[40];
    float input[40];
    char inputLocked;
    VPAD(0x251, 0x2F4);
    unsigned stateInput;
    int prevState;
    int state;
    VPAD(0x300, 0x33D);
    char crouch;
    VPAD(0x33E, 0x4C4);
    Task *weapons[3];
    char unk4D0;
    char fire;
    char unk4D2;
    char shot;
    int unk4D4;
    VPAD(0x4D8, 0x4E0);
    int cookTime;
    VPAD(0x4E4, 0x4EC);
    HumanView *view;
    int viewCam;
    VPAD(0x4F4, 0x4F8);
    float pitch;
    VPAD(0x4FC, 0x508);
    float yaw;
    float yaw2;
    char unk510;
    VPAD(0x511, 0x520);
    float move[3];
    char unk52C;
    VPAD(0x52D, 0x538);
    int animId;
    int animFrame;
    VPAD(0x540, 0x664);
    float vel[3];
    float unk670[3];
    float unk67C[3];
    float throwDir[3];
    char unk694;
    VPAD(0x695, 0x6A8);
    double soundPos[3];
    VPAD(0x6C0, 0x6EC);
    char *ground;
    VPAD(0x6F0, 0x6F8);
    int unk6F8;
    VPAD(0x6FC, 0x728);
    float unk728[3];
    VPAD(0x734, 0x768);
    Task **groundRef;
    Task *platform;
    double platformPos[3];
    Rot platformRot;
    Task **attachedRef;
    Task **climbRef;
    char unk7B8;
    char unk7B9;
    VPAD(0x7BA, 0x7C0);
    int unk7C0;
    VPAD(0x7C4, 0x7D0);
    double lastPos[2];
    VPAD(0x7E0, 0x7E8);
    int unk7E8;
    VPAD(0x7EC, 0x838);
    Task **bloodRef;
    Task **dropRef;
    Task **voiceRef;
    int voiceTime;
    VPAD(0x848, 0x84C);
    unsigned footstep;
    VPAD(0x850, 0xCF5);
    char dead;
    char unkCF6;
} Human;

struct HumanView {
    Task task;
    double pos[3];
    VPAD(0x38, 0x68);
    char anim[0x10];
    Rot rot;
    float radius;
    VPAD(0xA4, 0xD4);
    int unkD4;
    char unkD8;
    char inWorld;
    char rotDirty;
    VPAD(0xDB, 0xE0);
    double eye[3];
    Rot cam;
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
    Rot camRot;
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
VCHECK(Human, rot, 0x78);
VCHECK(Human, team, 0xFC);
VCHECK(Human, inputLocked, 0x250);
VCHECK(Human, state, 0x2FC);
VCHECK(Human, crouch, 0x33D);
VCHECK(Human, unk4D4, 0x4D4);
VCHECK(Human, view, 0x4EC);
VCHECK(Human, move, 0x520);
VCHECK(Human, vel, 0x664);
VCHECK(Human, unk694, 0x694);
VCHECK(Human, soundPos, 0x6A8);
VCHECK(Human, groundRef, 0x768);
VCHECK(Human, platformRot, 0x788);
VCHECK(Human, attachedRef, 0x7B0);
VCHECK(Human, yaw, 0x508);
VCHECK(Human, unk7C0, 0x7C0);
VCHECK(Human, lastPos, 0x7D0);
VCHECK(Human, bloodRef, 0x838);
VCHECK(Human, footstep, 0x84C);
VCHECK(Human, unkCF6, 0xCF6);
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
void HumanView_GetMatrix(Rot *out, HumanView *v);
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
