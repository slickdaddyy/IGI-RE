#ifndef VIEWCAM_H
#define VIEWCAM_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "qasm/qasmtypes.h"
#include "weapon/weapontypes.h"
#include "viewcam/viewcamtypes.h"

typedef void (*TaskHandler)(void *task, void *arg);

enum { WEAPON_KIND_GRENADE = 4 };

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
