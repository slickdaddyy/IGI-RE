/* Anim: playing animations on the bones of a BoneObj. Times are in ticks, 4800 a second. */
#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "boneobj/boneobj.h"
#include "lod/lodvars.h"

ORIG(0x4D3710, void, Task_Nothing, (void *p));

static const float kTicksPerSecond = 4800.0f;

void Anim_Play(BoneObj *o, int track, int id, unsigned int flags, float blend);

void Anim_PlayNow(BoneObj *o, int track, int id, unsigned int flags)
{
    Anim_Play(o, track, id, flags, 0);
}
HOOK(0x4D6110, Anim_PlayNow);

/* Plays id (or keeps the parent's motion when id < 0) under a fixed orientation */
void Anim_PlayOriented(BoneObj *o, int track, int mode, const Quat *q, int id, unsigned int flags, float blend)
{
    float t = blend * kTicksPerSecond / gTicksPerFrame;
    Bone *b = &o->bones[track];
    BlendSource *src, *os;
    BlendNode *n;

    if (id < 0)
        src = BoneObj_NewParentSource(b);
    else
        src = BoneObj_NewAnimSource(o->skel, track, id, flags);
    os = BoneObj_NewOrientationSource(track, mode, q);
    n = BoneObj_NewBlendNode(BRANCH_SOURCE, os, BRANCH_SOURCE, src, 0, NULL, NULL, NULL, NULL, NULL);
    BoneObj_BlendTo(b, n, t, 0);
    b->flags |= BONE_ANIMATED;
}
HOOK(0x4D6130, Anim_PlayOriented);

void Anim_Play(BoneObj *o, int track, int id, unsigned int flags, float blend)
{
    float t = blend * kTicksPerSecond / gTicksPerFrame;
    Bone *b = &o->bones[track];
    BlendNode *old = b->blend, *n;
    BlendSource *src = BoneObj_NewAnimSource(o->skel, track, id, flags);

    if (old || b->parent) {
        if (!(t < 1.0f)) {
            BoneObj_FadeIn(b, src, t, 0);
            b->flags |= BONE_ANIMATED;
            return;
        }
        if (old)
            BoneObj_DeleteBlendNode(old);
    }
    n = BoneObj_AllocBlendNode();
    BoneObj_SetBlendNode(n, NULL, BRANCH_SOURCE, src, BRANCH_NONE, NULL, 0, NULL, NULL, NULL, NULL, NULL);
    b->blend = n;
    b->flags = (b->flags & 0xFFFAFEFF) | BONE_ANIMATED;
}
HOOK(0x4D61D0, Anim_Play);

long double BoneObj_FadeWeightCB(Fade *f);
int BoneObj_FadeSelectCB(Fade *f);
void BoneObj_FadeFreeCB(Fade *f);
Fade *BoneObj_AllocFade(void);
void BoneObj_InitFade(Fade *f, float duration, int result);
BlendSource *BoneObj_AllocBlendSource(void);
void BoneObj_SetBlendSource(BlendSource *s, void *getPosition, void *getOrientation, void *getEvents, void *free,
                            void *data);
int BoneObj_ParentOrientationCB(Quat *out, Bone *bone, Bone *self);

/* Fades the track back to its parent's motion */
void Anim_FadeOut(BoneObj *o, int track, float time)
{
    long double tt = time * kTicksPerSecond / gTicksPerFrame;
    float t = tt;
    Bone *b = &o->bones[track];
    BlendNode *old = b->blend, *n;
    BlendSource *s;
    Fade *f;

    if (tt < 1.0f) {
        if (old) {
            BoneObj_DeleteBlendNode(old);
            b->blend = NULL;
        }
        return;
    }
    if (!old)
        return;
    Task_Nothing(old);
    f = BoneObj_AllocFade();
    BoneObj_InitFade(f, t, 2);
    b->flags |= BONE_FADING;
    s = BoneObj_AllocBlendSource();
    BoneObj_SetBlendSource(s, NULL, BoneObj_ParentOrientationCB, NULL, NULL, b);
    n = old;
    if (old->branch[1].type != BRANCH_NONE) {
        n = BoneObj_AllocBlendNode();
        old->parent = n;
        n->branch[0].ptr = old;
        n->branch[0].type = BRANCH_NODE;
    }
    n->branch[1].ptr = s;
    n->branch[1].type = BRANCH_SOURCE;
    n->weight = (long double (*)(void *))BoneObj_FadeWeightCB;
    n->free = (void (*)(void *))BoneObj_FadeFreeCB;
    n->select = (int (*)(void *))BoneObj_FadeSelectCB;
    n->data = f;
    b->blend = n;
}
HOOK(0x4D62B0, Anim_FadeOut);

/* The animation instance the track is playing, the newest one while fading */
AnimInst *Anim_GetInst(BoneObj *o, int track)
{
    Bone *b = &o->bones[track];
    BlendNode *n;

    if (!(b->flags & BONE_ANIMATED))
        return NULL;
    n = b->blend;
    if (!n)
        return NULL;
    if (b->flags & BONE_FADING) {
        for (;;) {
            if (n->branch[1].type == BRANCH_SOURCE)
                return ((BlendSource *)n->branch[1].ptr)->data;
            n = n->branch[1].ptr;
            if (!n)
                return NULL;
        }
    }
    return ((BlendSource *)n->branch[0].ptr)->data;
}
HOOK(0x4D6460, Anim_GetInst);

int Anim_GetId(BoneObj *o, int track)
{
    return Anim_GetInst(o, track)->id;
}
HOOK(0x4D64C0, Anim_GetId);

long double Anim_GetLength(BoneObj *o, int track)
{
    return Anim_GetInst(o, track)->desc->length;
}
HOOK(0x4D64E0, Anim_GetLength);

long double Anim_GetTime(BoneObj *o, int track)
{
    return Anim_GetInst(o, track)->time;
}
HOOK(0x4D6500, Anim_GetTime);

long double Anim_GetPhase(BoneObj *o, int track)
{
    float t = Anim_GetTime(o, track);

    return t / Anim_GetLength(o, track);
}
HOOK(0x4D6520, Anim_GetPhase);

void BoneObj_UpdateAnimDirection(AnimInst *inst)
{
    if (inst->speed < 0.0f)
        inst->flags |= ANIM_REVERSE;
    else
        inst->flags &= ~ANIM_REVERSE;
}
HOOK(0x4D6550, BoneObj_UpdateAnimDirection);

void Anim_SetSpeed(BoneObj *o, int track, float speed)
{
    AnimInst *inst = Anim_GetInst(o, track);

    inst->speed = gTicksPerFrame * speed;
    BoneObj_UpdateAnimDirection(inst);
}
HOOK(0x4D6580, Anim_SetSpeed);

void Anim_SetTime(BoneObj *o, int track, float time)
{
    Anim_GetInst(o, track)->time = time;
}
HOOK(0x4D65B0, Anim_SetTime);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A54658) float gTicksPerFrame;
