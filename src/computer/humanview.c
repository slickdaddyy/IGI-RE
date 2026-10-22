#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "computer/computer.h"

/* "HumanView" (the eye of a human), the fields set here */
typedef struct HumanView {
    char unk0[0x13A];
    char camOverride;
    char unk13B[0x180 - 0x13B];
    int target[6];
    char hasTarget;
    char unk199[0x1B4 - 0x199];
    float shake;
    float shakeAmount;
    float shakeAngle;
    float shakeStep;
    float dip;
    int unk1C8;
    char unk1CC[0x1E4 - 0x1CC];
    float fovX;
    float fovY;
    int zoomed;
} HumanView;

_Static_assert(offsetof(HumanView, hasTarget) == 0x198, "HumanView");
_Static_assert(offsetof(HumanView, zoomed) == 0x1EC, "HumanView");

static const double kOne = 1.0;
static const double kQuarterPi = 0x1.921fb6p-1;
static const double kSixthPi = 0x1.0c1524p-1;

static inline long double x87_tan(double a)
{
    long double r;

    __asm__("fptan\n\tfstp %%st(0)" : "=t"(r) : "0"((long double)a));
    return r;
}

void HumanView_ClearCamOverride(HumanView *v)
{
    v->camOverride = 0;
}
HOOK(0x465910, HumanView_ClearCamOverride);

void HumanView_SetTarget(HumanView *v, const int *target, char flag)
{
    int i;

    v->hasTarget = flag;
    if (target) {
        for (i = 0; i < 6; i++)
            v->target[i] = target[i];
        return;
    }
    for (i = 0; i < 6; i++)
        v->target[i] = 0;
}
HOOK(0x465920, HumanView_SetTarget);

void HumanView_Shake(HumanView *v, int amount, int angle, int frames)
{
    v->shake = 1.0f;
    *(int *)&v->shakeAmount = amount;
    *(int *)&v->shakeAngle = angle;
    v->shakeStep = kOne / frames;
}
HOOK(0x465970, HumanView_Shake);

void HumanView_SetDip(HumanView *v, float dip)
{
    v->dip = dip;
    v->unk1C8 = 0;
}
HOOK(0x4659B0, HumanView_SetDip);

void HumanView_SetZoomed(HumanView *v)
{
    v->zoomed = 1;
}
HOOK(0x4659D0, HumanView_SetZoomed);

void HumanTaskViewReset(HumanView *v)
{
    v->zoomed = 0;
    v->fovX = x87_tan(kQuarterPi);
    v->fovY = x87_tan(kSixthPi);
}
HOOK(0x4659E0, HumanTaskViewReset);
