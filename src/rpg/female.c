/* The end of HumanSoldierFemale: update, damage handlers, close. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "rpg/soldier.h"

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x413A80, long double, HumanPlayer_GetMoveScale, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (SoldierBlast *e, Soldier *h, double *pos, float radius, int unk));
ORIG(0x450D50, void, HumanAI_Send, (SoldierAI *ai, SoldierAIMsg *msg));
ORIG(0x4538F0, void, SoldierFemale_Footstep, (Soldier *h));
ORIG(0x45D360, void, Soldier_EnsureWeapon, (Soldier *h));
ORIG(0x460C80, void, Human_printf, (Soldier *h, const char *fmt, ...));
ORIG(0x460CF0, void, Human_UpdatePlatformMove, (Soldier *h));
ORIG(0x4610C0, void, Human_UpdateBody, (Soldier *h));
ORIG(0x4613D0, void, Human_FireAnimTrigger, (Soldier *h));
ORIG(0x4614B0, void, Human_UpdateMove, (Soldier *h));
ORIG(0x462CF0, void, Human_MoveOnPlatform, (Soldier *h));
ORIG(0x462E50, void, Human_UpdateWeapon, (Soldier *h));
ORIG(0x463080, void, Human_SendPosition, (Soldier *h));
ORIG(0x463680, int, Human_GetGround, (Soldier *h));
ORIG(0x4637C0, void, Human_SayTimed, (Soldier *h, const char *sound, int count));
ORIG(0x463AD0, void, Human_Bleed, (Soldier *h, float *dir, int unk));
ORIG(0x464800, void, HumanView_UpdateView, (void *view, Soldier *h));
ORIG(0x464BD0, void, HumanView_UpdateBody, (void *view, Soldier *h));
ORIG(0x465800, void, HumanView_GetEye, (double *pos, void *view));
ORIG(0x465970, void, HumanView_Shake, (void *view, float amount, float angle, int frames));
ORIG(0x489C60, int, Human_GetHitZone, (Soldier *h, SoldierHit *hit));
ORIG(0x489E10, HitAnims *, HumanDamage_GetZone, (HumanDamage *d, int part));
ORIG(0x4B3100, void, Vec3d_Normalize, (double *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, double *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4D2FB0, void, BoneObj_GetRootMotion, (void *a, float *delta, void *anim));
ORIG(0x4EC1E0, void, StateMachine_Update, (SoldierSM *sm, Soldier *h));

extern void *gSoldierFemaleList;
extern uint16_t gSoldierFemaleType;

static const float kShakeAmount = 0x1.99999ap+7f;
static const float kShakeAngle = 0x1.aceea0p-6f;
static const float kBlastRadius = 0x1.555556p-2f;
static const double kForward = 4096.0;
static const float kUnitsToMetersF = 0x1p-12f;
static const double kUnitsToMeters = 0x1p-12;
static const float kFps = 30.0f;
static const float kRadToDeg = 0x1.ca5dc0p+5f;

/* fcomp/fnstsw/test ah,0x40: equal, or unordered */
static int is_zero(long double x)
{
    return !(x < 0 || x > 0);
}

/* hit report every damage handler sends to the AI */
static void Soldier_TellAI(Soldier *h, void *data, void *attacker)
{
    SoldierAIMsg msg;

    msg.type = 0xF;
    msg.unk4 = 1;
    msg.unk8 = 0;
    msg.data = data;
    msg.attacker = attacker;
    msg.attacker2 = attacker;
    HumanAI_Send(h->ai, &msg);
}

void SoldierFemale_InputHandler(Soldier *h, SoldierInputMsg *msg)
{
    if (!h->inputLocked) {
        memcpy(h->prevInput, h->input, sizeof(h->input));
        memcpy(h->input, msg->input, sizeof(h->input));
    }
    SoldierFemale_Update(h);
}
HOOK(0x455070, SoldierFemale_InputHandler);

static void Soldier_PrintDebug(Soldier *h)
{
    long double speed = h->vel[2];

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
    Human_printf(h, "Ground angle: %.3f\n", (double)(h->groundAngle * kRadToDeg));
    Human_printf(h, "Delta translation X: %.2f\n", (double)h->delta[0]);
    Human_printf(h, "Delta translation Y: %.2f\n", (double)h->delta[1]);
    Human_printf(h, "Delta translation Z: %.2f\n", (double)h->delta[2]);
    if (h->sm.bits & SB_ON_GROUND)
        Human_printf(h, "Ground!\n");
}

void SoldierFemale_Update(Soldier *h)
{
    float *d = h->delta, x, y, z;

    Soldier_EnsureWeapon(h);
    Human_UpdatePlatformMove(h);
    StateMachine_Update(&h->sm, h);
    if (!is_zero(h->input[20]))
        h->run ^= 1;
    h->sm.bits &= ~SB_HIT;
    Human_UpdateBody(h);
    HumanView_UpdateView(h->view, h);
    h->unkD8 = 1;
    BoneObj_GetRootMotion(h->unk514, d, h->anim);
    if (!h->deltaScaled) {
        long double k = 1.0f;

        if (h->ai && h->ai->unk3D38 == 4)
            k = HumanPlayer_GetMoveScale();
        d[0] = k * d[0];
        d[1] = k * d[1];
        d[2] = k * d[2];
    }
    x = h->matrix[2] * d[2] + h->matrix[1] * d[1] + h->matrix[0] * d[0];
    y = h->matrix[5] * d[2] + h->matrix[4] * d[1] + h->matrix[3] * d[0];
    z = h->matrix[8] * d[2] + h->matrix[7] * d[1] + h->matrix[6] * d[0];
    d[0] = x;
    d[1] = y;
    d[2] = z;
    Human_FireAnimTrigger(h);
    if (!h->attach)
        Human_UpdateMove(h);
    else
        Human_MoveOnPlatform(h);
    HumanView_UpdateBody(h->view, h);
    Human_UpdateWeapon(h);
    Human_SendPosition(h);
    SoldierFemale_Footstep(h);
    Soldier_PrintDebug(h);
}
HOOK(0x4550B0, SoldierFemale_Update);

static void Female_AddDamage(SoldierF *f, int part, long double dmg)
{
    HumanDamage *d = &f->s.damage;

    d->part[part].dmg = dmg + d->part[part].dmg;
    d->total = dmg + d->total;
}

void SoldierFemale_HitHandler(Soldier *h, SoldierHit *hit)
{
    SoldierF *f = (SoldierF *)h;
    HumanDamage *d = &h->damage;
    double scratch[3];
    char headshot = 0;
    int part;
    float dmg;
    long double sum;
    uint32_t bits;

    if (!h->ai || (headshot = h->ai->unk3ABD, !h->ai->unk3ABC)) {
        part = Human_GetHitZone(h, hit);
        d->hitZone = part;
        HumanDamage_GetZone(d, part);
        if ((part == 15 || part == 16) && headshot)
            dmg = hit->damage;
        else
            dmg = d->part[part].mult * hit->damage;
        Female_AddDamage(f, part, dmg);
        Human_Bleed(h, hit->dir, hit->unk00);
        sum = dmg + f->hitSum;
        bits = h->sm.bits;
        f->hitSum = sum;
        if (d->total < d->max) {
            if (sum > f->hitLimit)
                f->hitSum = 0;
            Human_SayTimed(h, "ai_hit_", 3);
        } else {
            f->deathDir[0] = hit->dir[0];
            f->deathDir[1] = hit->dir[1];
            f->deathDir[2] = hit->dir[2];
            f->deathKind = *(int *)((char *)hit->weapon + 0x610) == 2 ? 1 : 2;
            bits |= SB_DEAD;
            h->dying = 1;
        }
        h->sm.bits = bits;
    }
    Soldier_TellAI(h, scratch, hit->attacker);
}
HOOK(0x4553E0, SoldierFemale_HitHandler);

void SoldierFemale_BlastHandler(Soldier *h, SoldierBlast *e)
{
    SoldierF *f = (SoldierF *)h;
    HumanDamage *d = &h->damage;
    double cam[3], v[3];
    long double k, r, sum;
    uint32_t bits;

    if (!h->vulnerable)
        return;
    if (h->ai && h->ai->unk3ABC)
        return;
    HumanView_GetEye(cam, h->view);
    v[0] = e->pos[0] - cam[0];
    v[1] = e->pos[1] - cam[1];
    v[2] = e->pos[2] - cam[2];
    k = Vec3d_Length(v) - e->radius * 3.0f;
    if (!(k >= 0))
        k = 1.0f;
    else
        k = 1.0f - k / (e->radius * 6.0f);
    if (k > 0)
        HumanView_Shake(h->view, k * kShakeAmount, k * kShakeAngle, e->shakeFrames);
    d->hitZone = 13;
    HumanDamage_GetZone(d, 13);
    r = Game_BlastFalloff(e, h, h->pos, h->radius * kBlastRadius, 1);
    if (is_zero(r))
        return;
    r = r * d->part[13].mult;
    Female_AddDamage(f, 13, r);
    bits = h->sm.bits;
    sum = r + f->hitSum;
    f->hitSum = sum;
    if (d->total < d->max) {
        if (sum > f->hitLimit) {
            Human_SayTimed(h, "ai_xplhit_", 3);
            f->hitSum = 0;
        }
    } else {
        f->deathKind = 0;
        bits |= SB_DEAD;
        f->deathDir[0] = h->pos[0] - e->pos[0];
        f->deathDir[1] = h->pos[1] - e->pos[1];
        h->dying = 1;
        f->deathDir[2] = h->pos[2] - e->pos[2];
    }
    h->sm.bits = bits;
}
HOOK(0x455570, SoldierFemale_BlastHandler);

void SoldierFemale_PunchHandler(Soldier *h, SoldierPunch *p)
{
    SoldierF *f = (SoldierF *)h;
    HumanDamage *d = &h->damage;
    double fwd[3], v[3], *a, *b;
    long double dmg, sum;
    int part;
    uint32_t bits;

    if (!h->ai || !h->ai->unk3ABC) {
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
        part = v[2] * fwd[2] + v[1] * fwd[1] + v[0] * fwd[0] >= 0.0 ? 14 : 13;
        d->hitZone = part;
        HumanDamage_GetZone(d, part);
        dmg = d->part[part].mult * p->damage;
        Female_AddDamage(f, part, dmg);
        bits = h->sm.bits;
        sum = dmg + f->hitSum;
        f->hitSum = sum;
        if (d->total < d->max) {
            if (sum > f->hitLimit)
                f->hitSum = 0;
            Human_SayTimed(h, "ai_hit_", 3);
        } else {
            f->deathKind = 2;
            memcpy(f->deathDir, v, sizeof(v));
            bits |= SB_DEAD;
            h->dying = 1;
        }
        h->sm.bits = bits;
    }
    Soldier_TellAI(h, fwd, p->attacker);
}
HOOK(0x455780, SoldierFemale_PunchHandler);

void SoldierFemale_Close(void)
{
    QTaskList_Free(gSoldierFemaleList);
    TaskType_Free(gSoldierFemaleType);
}
HOOK(0x455960, SoldierFemale_Close);

uint16_t SoldierFemale_GetType(void)
{
    return gSoldierFemaleType;
}
HOOK(0x455980, SoldierFemale_GetType);
