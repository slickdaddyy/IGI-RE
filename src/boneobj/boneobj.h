#ifndef BONEOBJ_BONEOBJ_H
#define BONEOBJ_BONEOBJ_H

#include "igi.h"
#include "core/pool.h"
#include "math/vecmath.h"
#include "boneobj/boneobjtypes.h"

typedef struct Bone Bone;
typedef struct BlendNode BlendNode;

/* An animation event handed to the game: sound or effect at an offset from the bone */
typedef struct BoneEvent {
    int bone;
    int id;
    int anim;
    float time;
    float pos[3];
} BoneEvent;

enum { BRANCH_NONE = -1, BRANCH_NODE = 0, BRANCH_SOURCE = 1 };

typedef struct BlendBranch {
    int type;
    void *ptr;
} BlendBranch;

/* A leaf of a blend tree. The getters return -1 for nothing, 0 for an
   orientation relative to the parent bone and 1 for an absolute one. */
typedef struct BlendSource {
    int (*getPosition)(float *pos, float *delta, void *data);
    int (*getOrientation)(Quat *out, Bone *bone, void *data);
    int (*getEvents)(BoneEvent *events, int count, Bone *bone, void *data);
    void (*free)(void *data);
    void *data;
} BlendSource;

struct BlendNode {
    BlendNode *parent;
    int unk04;
    int (*blend)(Quat *out, int t0, Quat *q0, int t1, Quat *q1, float w);
    long double (*weight)(void *data);
    void (*free)(void *data);
    int (*select)(void *data);
    void *data;
    BlendBranch branch[2];
};

#define BONE_EVENTS_DONE 0x100
#define BONE_FADING 0x10000
#define BONE_ANIMATED 0x20000
#define BONE_ORIENTED 0x40000

struct Bone {
    int index;
    unsigned int flags;
    Vec3f offset;
    float unk14;
    Vec3f pos;
    Matrix matrix;
    Quat world;
    Quat local;
    Bone *parent;
    Bone *child;
    Bone *next;
    BlendNode *blend;
};

typedef struct BoneObj {
    int skel;
    int unk4;
    Bone *bones;
} BoneObj;

typedef struct EventKey {
    int id;
    int unk4;
    float time;
    float pos[3];
} EventKey;

typedef struct EventTrack {
    int count;
    EventKey *keys;
} EventTrack;

#define ANIM_LOOPS 0x80000000u

typedef struct AnimDesc {
    float length;
    unsigned int flags;
    PosTrack *pos;
    RotTrack *rot; /* one per bone */
    EventTrack *events;
} AnimDesc;

/* Last key used per bone, so playback does not search from the start */
typedef struct KeyCache {
    int skel;
    float time;
    int unk8;
    int *bones;
} KeyCache;

typedef struct AnimSet {
    AnimDesc *descs;
    AnimDesc **byId;
    Pool caches;
    int hasCaches;
    int count;
} AnimSet;

#define ANIM_LOOP 1
#define ANIM_REVERSE 2
#define ANIM_FIRST 0x8000

typedef struct AnimInst {
    int bone;
    int id;
    unsigned int flags;
    AnimDesc *desc;
    float pos[3];
    float delta[3];
    float speed;
    float time;
    KeyCache *cache;
} AnimInst;

typedef struct Fade {
    float time;
    float duration;
    float weight;
    int result;
} Fade;

typedef struct OrientData {
    int bone;
    int mode;
    Quat q;
} OrientData;

extern Skeleton gSkeletons[];
extern AnimSet gAnimSets[];
extern Quat *gRootQuat;
extern Matrix *gRootMatrix;
extern Pool gSourcePool;
extern Pool gOrientPool;
extern Pool gInstPool;
extern Pool gFadePool;
extern Pool gNodePool;
extern float gTicksPerFrame;
extern int gBoneSmoothAnims;

static inline void *pool_get(Pool *p)
{
    char *s = p->data + p->free[p->used] * p->stride;

    *(int *)(s + p->size) = p->used;
    p->used++;
    return s;
}

static inline void pool_put(Pool *p, void *slot)
{
    int i = *(int *)((char *)slot + p->size);

    p->used--;
    if (i != p->used) {
        char *last = p->data + p->free[p->used] * p->stride;
        int j = *(int *)(last + p->size);
        int t = p->free[i];

        p->free[i] = p->free[j];
        p->free[j] = t;
        *(int *)(last + p->size) = i;
    }
}

void BoneObj_DeleteBlendNode(BlendNode *n);
BlendNode *BoneObj_AllocBlendNode(void);
void BoneObj_SetBlendNode(BlendNode *n, BlendNode *parent, int t0, void *p0, int t1, void *p1, int unk04,
                          void *blend, void *weight, void *free, void *select, void *data);
BlendNode *BoneObj_NewBlendNode(int t0, void *p0, int t1, void *p1, int unk04, void *blend, void *weight, void *free,
                                void *select, void *data);
void BoneObj_Slerp(Quat *out, const Quat *a, const Quat *b, float t);
BlendSource *BoneObj_NewParentSource(Bone *bone);
BlendSource *BoneObj_NewAnimSource(int skel, int bone, int id, unsigned int flags);
BlendSource *BoneObj_NewOrientationSource(int bone, int mode, const Quat *q);
void BoneObj_FadeIn(Bone *bone, BlendSource *src, float duration, int result);
void BoneObj_BlendTo(Bone *bone, BlendNode *node, float duration, int result);
void BoneObj_UpdateAnimDirection(AnimInst *inst);

#endif
