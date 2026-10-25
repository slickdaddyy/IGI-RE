/* BoneObj: bone hierarchies built from a loaded skeleton, and root motion. */
#pragma GCC optimize("excess-precision=standard")
#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "core/pool.h"
#include "boneobj/boneobjtypes.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0B00, void, Pool_Init_, (Pool *pool, int size, int count, unsigned int align));
ORIG(0x4B0B70, void, Pool_Free_, (Pool *pool));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4D3160, void, BoneObj_LerpVec, (float *out, const float *a, const float *b, float t));
ORIG(0x4D31C0, int, BoneObj_GetBranchPosition, (float *pos, float *rot, void *track));
ORIG(0x4D34A0, void, BoneObj_DeleteBlendNode, (void *blend));
ORIG(0x4D3720, void *, BoneObj_GetBlendBranch, (void *blend, int i));
ORIG(0x4D38A0, void, BoneObj_SetRoot, (int a, int b));
ORIG(0x4D65D0, void, BoneObj_FreeAll, (void));

/* What sits at BoneObj bone +0x80: two tracks mixed by a weight function */
typedef struct BoneBlend {
    char unk0[0xC];
    long double (*weight)(void *arg);
    char unk10[8];
    void *arg;
} BoneBlend;

_Static_assert(sizeof(BoneBlend) == 0x1C, "BoneBlend");

extern Skeleton gSkeletons[];
extern AnimSet gAnimSets[];
extern Skeleton *gBoneBuild;
extern Bone *gBoneBuildBones;
extern Pool gSourcePool;
extern Pool gOrientPool;
extern Pool gInstPool;
extern Pool gFadePool;
extern Pool gNodePool;
extern int gBoneUnused0;
extern int gBoneUnused1;
extern float gTicksPerFrame;
extern int gBoneSmoothAnims;

static const float kTicksPerSecond = 4800.0f;

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

void BoneObj_Init(void)
{
    int i;

    BoneObj_SetRoot(0, 0);
    gBoneSmoothAnims = 1;
    for (i = 0; i < 64; i++) {
        gSkeletons[i].count = 0;
        gSkeletons[i].parents = NULL;
        gAnimSets[i].count = 0;
        gSkeletons[i].offsets = NULL;
        gAnimSets[i].descs = NULL;
        gAnimSets[i].byId = NULL;
    }
    Pool_Init_(&gFadePool, 0x10, 0x80, 4);
    Pool_Init_(&gInstPool, 0x34, 0x80, 4);
    Pool_Init_(&gOrientPool, 0x1C, 0x80, 4);
    Pool_Init_(&gNodePool, 0x2C, 0x80, 4);
    Pool_Init_(&gSourcePool, 0x14, 0x100, 4);
    gBoneUnused0 = 0;
    gBoneUnused1 = 0;
    gTicksPerFrame = 0;
}
HOOK(0x4D2C30, BoneObj_Init);

void BoneObj_Shutdown(void)
{
    BoneObj_FreeAll();
    Pool_Free_(&gFadePool);
    Pool_Free_(&gInstPool);
    Pool_Free_(&gOrientPool);
    Pool_Free_(&gNodePool);
    Pool_Free_(&gSourcePool);
}
HOOK(0x4D2CF0, BoneObj_Shutdown);

void BoneObj_SetFrameRate(float fps)
{
    gTicksPerFrame = kTicksPerSecond / fps;
}
HOOK(0x4D2D30, BoneObj_SetFrameRate);

float BoneObj_GetTicksPerFrame(void)
{
    return gTicksPerFrame;
}
HOOK(0x4D2D50, BoneObj_GetTicksPerFrame);

void BoneObj_Build(Bone *parent, int index);

void BoneObj_Create(BoneObj *o, int type)
{
    Skeleton *h = &gSkeletons[type];
    Bone root, *b;
    char mode;

    o->skel = type;
    o->unk4 = 0;
    root.child = NULL;
    root.next = NULL;
    root.index = -1;
    if (!h->count) {
        o->bones = NULL;
        Error_Show("Failed to create boneobj of type %d, no hierarchy loaded", type);
        for (;;)
            ;
    }
    mode = Mem_GetMode();
    if (!mode)
        Mem_SetDefaultMode();
    o->bones = Mem_Alloc(h->count * sizeof(Bone), 4);
    if (!mode)
        Mem_ClearMode();
    gBoneBuildBones = o->bones;
    gBoneBuild = h;
    BoneObj_Build(&root, -1);
    for (b = root.child; b; b = b->next)
        b->parent = NULL;
}
HOOK(0x4D2D60, BoneObj_Create);

void BoneObj_Free(BoneObj *o)
{
    int i;

    for (i = 0; i < gSkeletons[o->skel].count; i++) {
        if (o->bones[i].blend)
            BoneObj_DeleteBlendNode(o->bones[i].blend);
        o->bones[i].blend = NULL;
    }
    if (o->bones) {
        Mem_Free(o->bones);
        o->bones = NULL;
    }
    o->skel = 0;
    o->unk4 = 0;
    o->bones = NULL;
}
HOOK(0x4D2E40, BoneObj_Free);

void Bone_Init(Bone *b, Bone *parent);

/* Adds the children of bone index under parent, depth first */
void BoneObj_Build(Bone *parent, int index)
{
    int i;

    for (i = 0; i < gBoneBuild->count; i++) {
        if (gBoneBuild->parents[i] == index) {
            Bone *b = &gBoneBuildBones[i];
            const float *v;

            Bone_Init(b, parent);
            b->index = i;
            v = &gBoneBuild->offsets[i].x;
            b->offset.x = v[0];
            b->offset.y = v[1];
            b->offset.z = v[2];
            b->length = x87_sqrt((long double)b->offset.x * b->offset.x + (long double)b->offset.y * b->offset.y +
                                 (long double)b->offset.z * b->offset.z);
            BoneObj_Build(b, i);
        }
    }
}
HOOK(0x4D2EC0, BoneObj_Build);

void Bone_Init(Bone *b, Bone *parent)
{
    int *p = (int *)b;
    int i;

    for (i = 0; i < 0x21; i++)
        p[i] = 0;
    b->child = NULL;
    b->parent = parent;
    b->next = parent->child;
    parent->child = b;
}
HOOK(0x4D2F80, Bone_Init);

int BoneBlend_GetMotion(float *pos, float *rot, BoneBlend *bl);

static void clear3(float *v)
{
    if (v) {
        v[0] = 0;
        v[1] = 0;
        v[2] = 0;
    }
}

/* Movement of the root bone this frame, zero when it has none */
void BoneObj_GetRootMotion(float *pos, float *rot, BoneObj *o)
{
    BoneBlend *bl = (BoneBlend *)o->bones->blend;

    if (bl && BoneBlend_GetMotion(pos, rot, bl))
        return;
    clear3(pos);
    clear3(rot);
}
HOOK(0x4D2FB0, BoneObj_GetRootMotion);

static void copy3(float *d, const float *s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

int BoneBlend_GetMotion(float *pos, float *rot, BoneBlend *bl)
{
    float ap[3], ar[3], bp[3], br[3];
    int a = BoneObj_GetBranchPosition(ap, ar, BoneObj_GetBlendBranch(bl, 0));
    int b = BoneObj_GetBranchPosition(bp, br, BoneObj_GetBlendBranch(bl, 1));

    if (!a && !b)
        return 0;
    if (a && b) {
        float t = bl->weight(bl->arg);

        if (pos)
            BoneObj_LerpVec(pos, ap, bp, t);
        if (rot)
            BoneObj_LerpVec(rot, ar, br, t);
    } else if (a) {
        if (pos)
            copy3(pos, ap);
        if (rot)
            copy3(rot, ar);
    } else {
        if (pos)
            copy3(pos, bp);
        if (rot)
            copy3(rot, br);
    }
    return 1;
}
HOOK(0x4D3020, BoneBlend_GetMotion);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, A542F0, 4) __typeof__(gSkeletons[0]) store_A542F0[64];
ALIAS(gSkeletons, store_A542F0, 0);
BSS(A542F0) Pool gSourcePool;
BSS(A542F0) Pool gOrientPool;
BSS(A542F0) Pool gInstPool;
BSS(A542F0) Skeleton *gBoneBuild;
PAD(igb, A542F0, A5463C, 0x4);
BSS(A542F0) Pool gFadePool;
BSS(A542F0) float gTicksPerFrame;
PAD(igb, A542F0, A5465C, 0x4);
BSS(A542F0) Pool gNodePool;
BSS(A5467C) Bone *gBoneBuildBones;
PAD(igb, A5467C, A54680, 0x4);
BSS(A5467C) int gBoneUnused0;
BSS(A5467C) int gBoneUnused1;
