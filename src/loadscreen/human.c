#pragma GCC optimize("excess-precision=standard")

#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "humanai/humanaitypes.h"
#include "humanplayer/humanplayertypes.h"
#include "viewcam/viewcamtypes.h"

#define BONE(m, i) ((Vec3f *)((char *)(m)->bones + (i) * 0x84))

/* what hit a human: the shot's kind at +0x44 picks the zone table entry */
typedef struct Shot {
    char unk0[8];
    Vec3d from;
    char unk20[0x24];
    int kind;
} Shot;

extern const int gHitZones[];

static const float kZero = 0.0f;
static const float kVisible = 0.79f;
static const double kHeadRange = 614.4;
static const float kLookMax = 0x1.eb7c18p-1f;
static const double kOne = 1.0;

ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x489610, uint8_t, AIDetection_GetEvent0, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4CE410, char, Coll_LineOfSight, (void *world, const Vec3d *from, const Vec3d *to, int event, void *filter,
                                        int flags, int trees, float *visibility));
ORIG(0x414F40, void, Model_GetBonePoint, (Vec3d *out, Human *h, int i, const char *name));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4D61D0, void, Anim_Play, (void *anim, int track, int id, int loop, float blend));
ORIG(0x4D6130, void, Anim_PlayOriented, (void *anim, int track, int a, void *b, int id, int loop, float blend));
ORIG(0x4D62B0, void, Anim_FadeOut, (void *anim, int track, float time));
ORIG(0x4B4660, void, Quat_SetAxisAngle, (float *q, const Vec3f *axis, float angle));
ORIG(0x4D7070, void, Bone_Rotate, (void *bone, const float *q, int a, int b, int c));

/* 1 when target can be seen from `from` (clear line or visibility of at least 0.79) */
char AIDetection_IsVisible(Task *self, const Vec3d *from, Task *target, const Vec3d *to)
{
    SightFilter filter;
    float vis;
    uint8_t ev;
    void *world;

    filter.self = self;
    filter.self2 = self;
    filter.target = target;
    ev = AIDetection_GetEvent0();
    world = Game_GetWorld();
    if (Coll_LineOfSight(world, from, to, ev, &filter, -1, -1, &vis))
        return 1;
    if (vis >= kVisible)
        return 1;
    return 0;
}
HOOK(0x489B20, AIDetection_IsVisible);

void AIDetection_RemoveRefs(AIDetection *d)
{
    AISeen *n, *next;

    for (n = (AISeen *)d->enemies.head; (next = (AISeen *)n->node.next) != NULL; n = next)
        TaskRef_Remove(n->ref);
    for (n = (AISeen *)d->friends.head; (next = (AISeen *)n->node.next) != NULL; n = next)
        TaskRef_Remove(n->ref);
}
HOOK(0x489B90, AIDetection_RemoveRefs);

AISeen *AIDetection_FindSeen(AIDetection *d, Task *t)
{
    AISeen *n;

    for (n = (AISeen *)d->enemies.head; n->node.next; n = (AISeen *)n->node.next)
        if (n->valid && *n->ref == t)
            return n;
    for (n = (AISeen *)d->friends.head; n->node.next; n = (AISeen *)n->node.next)
        if (n->valid && *n->ref == t)
            return n;
    return NULL;
}
HOOK(0x489BE0, AIDetection_FindSeen);

void Human_ResetDamage(Human *h);

void Human_InitDamage(Human *h, HitAnims *zones)
{
    h->damage.max = 1.0f;
    h->damage.zones = zones;
    Human_ResetDamage(h);
    h->damage.hitZone = -1;
}
HOOK(0x489C30, Human_InitDamage);

/* zone 13 / 14: hit from the front or back, 15: near the head */
int Human_GetHitZone(Human *h, HumanHit *hit)
{
    float x = -hit->dir[0], y = -hit->dir[1], z = -hit->dir[2];
    float side = z * h->matrix.m[6] + y * h->matrix.m[3] + x * h->matrix.m[0];
    long double front = z * h->matrix.m[7] + y * h->matrix.m[4] + x * h->matrix.m[1];
    int kind = hit->shot->kind;
    int zone;
    Vec3d head, d;
    long double hx;

    if (kind < 30) {
        zone = gHitZones[kind];
        if (zone != 12)
            goto head;
    } else {
        zone = 12;
    }
    if (front <= kZero) {
        long double af = front < kZero ? -front : front;
        float as = side < kZero ? -side : side;

        if (af > as)
            zone = 14;
    }
    if (front < kZero)
        return zone;
    if (front > (side < kZero ? -side : side))
        zone = 13;
head:
    if (front < kZero)
        return zone;
    Model_GetBonePoint(&head, h, 4, "000_01_1");
    hx = head.x + h->pos[0];
    head.x = hx;
    head.y = head.y + h->pos[1];
    head.z = head.z + h->pos[2];
    d.x = hit->shot->from.x - hx;
    d.y = hit->shot->from.y - head.y;
    d.z = hit->shot->from.z - head.z;
    if (Vec3d_Length(&d) < kHeadRange)
        return 15;
    return zone;
}
HOOK(0x489C60, Human_GetHitZone);

HitAnims *HumanDamage_GetZone(HumanDamage *d, int zone)
{
    return &d->zones[zone];
}
HOOK(0x489E10, HumanDamage_GetZone);

void Human_ResetDamage(Human *h)
{
    HumanDamage *d = &h->damage;
    int i;

    d->total = 0;
    for (i = 0; i < 17; i++) {
        HitAnims *z = HumanDamage_GetZone(d, i);

        d->part[i].dmg = 0;
        d->part[i].mult = z->mult;
    }
}
HOOK(0x489E30, Human_ResetDamage);

void Human_PlayAnim(Human *h, int anim, int loop, float blend);

void Human_PlayAnimSlot(Human *h, int slot, float blend)
{
    AnimSlot *s = &h->animSlots[slot];

    Human_PlayAnim(h, s->anim, s->loop, blend);
}
HOOK(0x489E70, Human_PlayAnimSlot);

void Human_PlayAnim(Human *h, int anim, int loop, float blend)
{
    Anim_Play(h->anim, 0, anim, loop, blend);
}
HOOK(0x489EA0, Human_PlayAnim);

void Human_PlayWeaponAnim(Human *h, int anim, int loop, float blend)
{
    Anim_PlayOriented(h->anim, 1, 5, h->unk650, anim, loop, blend);
}
HOOK(0x489ED0, Human_PlayWeaponAnim);

void Human_StopWeaponAnim(Human *h, float time)
{
    Anim_FadeOut(h->anim, 1, time);
}
HOOK(0x489F00, Human_StopWeaponAnim);

void HumanPlayer_PlayAnim(Human *h, int anim, int loop, float blend)
{
    Anim_Play(h->view->anim, 0, anim, loop, blend);
}
HOOK(0x489F20, HumanPlayer_PlayAnim);

char HumanPlayer_AnimDone(Human *h)
{
    return (h->view->motion->flags & 0x100) != 0;
}
HOOK(0x489F50, HumanPlayer_AnimDone);

static void normalize(Vec3f *v)
{
    if (v->x != kZero || v->y != kZero || v->z != kZero) {
        long double inv = kOne / __builtin_sqrtl((long double)v->x * v->x + (long double)v->y * v->y +
                                                 (long double)v->z * v->z);

        v->x = v->x * inv;
        v->y = v->y * inv;
        v->z = v->z * inv;
    }
}

/* turns two bones of the body towards the view's look angles, at most 55 degrees each */
void Human_ApplyLook(Human *h)
{
    float pitch, yaw, a;
    Vec3f axis;
    float q[5]; /* a Quat: Quat_SetAxisAngle also clears the int after w */
    HumanMotion *m;

    if (!h->look)
        return;
    pitch = h->view->pitch;
    yaw = h->view->yaw;
    if (pitch != kZero) {
        a = pitch;
        if (pitch < -kLookMax)
            a = -kLookMax;
        else if (pitch > kLookMax)
            a = kLookMax;
        axis.x = h->matrix.m[0];
        axis.y = h->matrix.m[3];
        axis.z = h->matrix.m[6];
        Quat_SetAxisAngle(q, &axis, a);
        Bone_Rotate((char *)h->motion + 0x84, q, 0, 50, 50);
        if (pitch < -kLookMax)
            a = pitch + kLookMax;
        else if (pitch > kLookMax)
            a = pitch - kLookMax;
        else
            goto yaw;
        if (a != kZero) {
            axis.x = h->matrix.m[0];
            axis.y = h->matrix.m[3];
            axis.z = h->matrix.m[6];
            Quat_SetAxisAngle(q, &axis, a);
            Bone_Rotate((char *)h->motion + 0x210, q, 0, 50, 50);
        }
    }
yaw:
    if (yaw == kZero)
        return;
    a = yaw;
    if (yaw < -kLookMax)
        a = -kLookMax;
    else if (yaw > kLookMax)
        a = kLookMax;
    axis.x = h->matrix.m[2];
    axis.y = h->matrix.m[5];
    axis.z = h->matrix.m[8];
    normalize(&axis);
    Quat_SetAxisAngle(q, &axis, a);
    Bone_Rotate((char *)h->motion + 0x84, q, 0, 50, 50);
    if (yaw < -kLookMax)
        a = yaw + kLookMax;
    else if (yaw > kLookMax)
        a = yaw - kLookMax;
    else
        return;
    if (a == kZero)
        return;
    m = h->motion;
    axis.x = *(float *)((char *)m + 0x2AC) - *(float *)((char *)m + 0x228);
    axis.y = *(float *)((char *)m + 0x2B0) - *(float *)((char *)m + 0x22C);
    axis.z = *(float *)((char *)m + 0x2B4) - *(float *)((char *)m + 0x230);
    normalize(&axis);
    Quat_SetAxisAngle(q, &axis, a);
    Bone_Rotate((char *)h->motion + 0x210, q, 0, 50, 50);
}
HOOK(0x489F70, Human_ApplyLook);

void Human_EnableLook(Human *h)
{
    h->look = 1;
}
HOOK(0x48A320, Human_EnableLook);

void Human_DisableLook(Human *h)
{
    h->look = 0;
}
HOOK(0x48A330, Human_DisableLook);

/* world position of a model bone */
Vec3f *Human_GetBonePos(Vec3f *out, Human *h, int bone)
{
    Vec3f p = *BONE(h->motion, bone);
    Vec3f r;

    r.x = p.z * h->matrix.m[6] + p.y * h->matrix.m[3] + p.x * h->matrix.m[0];
    r.y = p.z * h->matrix.m[7] + p.y * h->matrix.m[4] + p.x * h->matrix.m[1];
    r.z = p.z * h->matrix.m[8] + p.y * h->matrix.m[5] + p.x * h->matrix.m[2];
    r.x = r.x + h->boneOffset[0];
    r.y = r.y + h->boneOffset[1];
    r.z = r.z + h->boneOffset[2];
    *out = r;
    return out;
}
HOOK(0x48A340, Human_GetBonePos);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 5412A4, 4) const int store_5412A4[58] INIT({12, 12, 12, 12, 16, 16, 16, 16, 11, 10, 9, 9, 9, 9, 5, 4, 3, 3, 3, 3, 8, 7, 6, 6, 6, 2, 1, 0, 0, 0, 1596993584, 828322096, 0, 1094930252, 1701657164, 2037609838, 1835365491, 1634692143, 1735289188, 779318623, 6515056, 1094930252, 1701657164, 2037609838, 1835365491, 1634692143, 1735289188, 1667854382, 0, 1094930252, 1701657164, 2037609838, 1835365491, 1634692143, 1735289188, 1701995379, 1915645541, 29541});
ALIAS(gHitZones, store_5412A4, 0);
