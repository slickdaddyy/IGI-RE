#ifndef VIEWCAM_VIEWCAMTYPES_H
#define VIEWCAM_VIEWCAMTYPES_H

#include "types.h"
#include "task/tasktypes.h"
#include "humanbase/humanbasetypes.h"

/* "HumanView" (0x328 bytes), the eye of a human */
typedef struct HumanView {
    Task task;
    double pos[3];
    char unk38[0x30];
    char anim[8];
    struct HumanMotion *motion;
    int unk74;
    Matrix rot;
    float radius;
    char unkA4[0x30];
    int unkD4;
    char unkD8;
    char inWorld;
    char rotDirty;
    char unkDB[5];
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
    char unk13B[5];
    double camPos[3];
    Matrix camRot;
    union {
        double target[3];
        int targetWords[6];     /* HumanView_SetTarget copies dwords */
    };
    char hasTarget;
    char unk199[3];
    float lean[3];
    float leanSpeed[3];
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
    HumanAnimEvent animEvent;
    char unk224[0xFC];
    float duck;
    float duckNow;
} HumanView;
_Static_assert(offsetof(HumanView, eye) == 0xE0, "HumanView");
_Static_assert(offsetof(HumanView, cam) == 0xF8, "HumanView");
_Static_assert(offsetof(HumanView, pitch) == 0x120, "HumanView");
_Static_assert(offsetof(HumanView, camPos) == 0x140, "HumanView");
_Static_assert(offsetof(HumanView, lean) == 0x19C, "HumanView");
_Static_assert(offsetof(HumanView, fovX) == 0x1E4, "HumanView");
_Static_assert(offsetof(HumanView, target) == 0x180, "HumanView");
_Static_assert(offsetof(HumanView, shake) == 0x1B4, "HumanView");
_Static_assert(offsetof(HumanView, weaponPitch) == 0x1CC, "HumanView");
_Static_assert(offsetof(HumanView, weaponPos) == 0x1F0, "HumanView");
_Static_assert(offsetof(HumanView, animEvent) == 0x208, "HumanView");
_Static_assert(offsetof(HumanView, duck) == 0x320, "HumanView");
_Static_assert(sizeof(HumanView) == 0x328, "HumanView");

#endif
