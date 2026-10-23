/* BoneObj: bone hierarchies built from a loaded skeleton, and root motion. */
#pragma GCC optimize("excess-precision=standard")
#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "core/pool.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4B0B00, void, Pool_Init_, (Pool *pool, int size, int count, unsigned int align));
ORIG(0x4B0B70, void, Pool_Free_, (Pool *pool));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4D3160, void, sub_4D3160, (float *out, const float *a, const float *b, float t));
ORIG(0x4D31C0, int, sub_4D31C0, (float *pos, float *rot, void *track));
ORIG(0x4D34A0, void, sub_4D34A0, (void *blend));
ORIG(0x4D3720, void *, sub_4D3720, (void *blend, int i));
ORIG(0x4D38A0, void, sub_4D38A0, (int a, int b));
ORIG(0x4D65D0, void, sub_4D65D0, (void));

/* A loaded skeleton: per bone its parent index and offset from the parent */
typedef struct BoneHierarchy {
    int count;
    int *parents;
    float (*offsets)[3];
} BoneHierarchy;

typedef struct Bone {
    int index;
    int unk4;
    float offset[3];
    float length;
    char unk18[0x5C];
    struct Bone *parent;
    struct Bone *child;
    struct Bone *sibling;
    void *blend;
} Bone;

typedef struct BoneObj {
    int type;
    int unk4;
    Bone *bones;
} BoneObj;

/* What sits at BoneObj bone +0x80: two tracks mixed by a weight function */
typedef struct BoneBlend {
    char unk0[0xC];
    long double (*weight)(void *arg);
    char unk10[8];
    void *arg;
} BoneBlend;

_Static_assert(sizeof(Bone) == 0x84, "Bone");
_Static_assert(sizeof(BoneBlend) == 0x1C, "BoneBlend");

#define gBoneHierarchies ((BoneHierarchy *)0xA542F0)
#define gBoneSlots ((int (*)[10])0xA538F0)
#define gBoneBuild VAR(BoneHierarchy *, 0xA54638)
#define gBoneBuildBones VAR(Bone *, 0xA5467C)
#define gBoneTicksPerFrame VAR(float, 0xA54658)
#define gBoneObjReady VAR(int, 0xA54678)

static const float kTicksPerSecond = 4800.0f;

static inline long double x87_sqrt(long double x)
{
    __asm__("fsqrt" : "+t"(x));
    return x;
}

void BoneObj_Init(void)
{
    int i;

    sub_4D38A0(0, 0);
    gBoneObjReady = 1;
    for (i = 0; i < 64; i++) {
        gBoneHierarchies[i].count = 0;
        gBoneHierarchies[i].parents = NULL;
        gBoneSlots[i][9] = 0;
        gBoneHierarchies[i].offsets = NULL;
        gBoneSlots[i][0] = 0;
        gBoneSlots[i][1] = 0;
    }
    Pool_Init_(&VAR(Pool, 0xA54640), 0x10, 0x80, 4);
    Pool_Init_(&VAR(Pool, 0xA54620), 0x34, 0x80, 4);
    Pool_Init_(&VAR(Pool, 0xA54608), 0x1C, 0x80, 4);
    Pool_Init_(&VAR(Pool, 0xA54660), 0x2C, 0x80, 4);
    Pool_Init_(&VAR(Pool, 0xA545F0), 0x14, 0x100, 4);
    VAR(int, 0xA54684) = 0;
    VAR(int, 0xA54688) = 0;
    gBoneTicksPerFrame = 0;
}
HOOK(0x4D2C30, BoneObj_Init);

void BoneObj_Shutdown(void)
{
    sub_4D65D0();
    Pool_Free_(&VAR(Pool, 0xA54640));
    Pool_Free_(&VAR(Pool, 0xA54620));
    Pool_Free_(&VAR(Pool, 0xA54608));
    Pool_Free_(&VAR(Pool, 0xA54660));
    Pool_Free_(&VAR(Pool, 0xA545F0));
}
HOOK(0x4D2CF0, BoneObj_Shutdown);

void BoneObj_SetFrameRate(float fps)
{
    gBoneTicksPerFrame = kTicksPerSecond / fps;
}
HOOK(0x4D2D30, BoneObj_SetFrameRate);

float BoneObj_GetTicksPerFrame(void)
{
    return gBoneTicksPerFrame;
}
HOOK(0x4D2D50, BoneObj_GetTicksPerFrame);

void BoneObj_Build(Bone *parent, int index);

void BoneObj_Create(BoneObj *o, int type)
{
    BoneHierarchy *h = &gBoneHierarchies[type];
    Bone root, *b;
    char mode;

    o->type = type;
    o->unk4 = 0;
    root.child = NULL;
    root.sibling = NULL;
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
    for (b = root.child; b; b = b->sibling)
        b->parent = NULL;
}
HOOK(0x4D2D60, BoneObj_Create);

void BoneObj_Free(BoneObj *o)
{
    int i;

    for (i = 0; i < gBoneHierarchies[o->type].count; i++) {
        if (o->bones[i].blend)
            sub_4D34A0(o->bones[i].blend);
        o->bones[i].blend = NULL;
    }
    if (o->bones) {
        Mem_Free(o->bones);
        o->bones = NULL;
    }
    o->type = 0;
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
            v = gBoneBuild->offsets[i];
            b->offset[0] = v[0];
            b->offset[1] = v[1];
            b->offset[2] = v[2];
            b->length = x87_sqrt((long double)b->offset[0] * b->offset[0] + (long double)b->offset[1] * b->offset[1] +
                                 (long double)b->offset[2] * b->offset[2]);
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
    b->sibling = parent->child;
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
    BoneBlend *bl = o->bones->blend;

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
    int a = sub_4D31C0(ap, ar, sub_4D3720(bl, 0));
    int b = sub_4D31C0(bp, br, sub_4D3720(bl, 1));

    if (!a && !b)
        return 0;
    if (a && b) {
        float t = bl->weight(bl->arg);

        if (pos)
            sub_4D3160(pos, ap, bp, t);
        if (rot)
            sub_4D3160(rot, ar, br, t);
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
