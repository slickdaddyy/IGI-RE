/* BoneObj: per bone blend trees of animation and orientation sources. */
#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "math/x87.h"
#include "boneobj/boneobj.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B0B70, void, Pool_Free_, (Pool *pool));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B44D0, void, Quat_SlerpPrepare_, (char *flip, float *angle, float *inv_sin, const Quat *a, const Quat *b));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int flags));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x4D3020, int, BoneBlend_GetMotion, (float *pos, float *delta, BlendNode *node));
void Task_Nothing(void *task);
ORIG(0x4D6D40, void, Anim_PrepareSlerps, (int skel));

typedef struct IffNode {
    struct IffNode *next;
    struct IffNode *parent;
    struct IffNode *child;
    int owned;
    int id;
    int type;
    int size;
    void *data;
} IffNode;

ORIG(0x517840, void, Iff_Free, (IffNode *n));
ORIG(0x517890, IffNode *, Iff_FindForm, (IffNode *n, int type));
ORIG(0x5178E0, IffNode *, Iff_FindNextForm, (IffNode *n, int type));
ORIG(0x517940, IffNode *, Iff_FindChunk, (IffNode *form, int id));
ORIG(0x517980, IffNode *, Iff_FindNextChunk, (IffNode *n, int id));
ORIG(0x5179C0, void *, Iff_GetData, (IffNode *n));
ORIG(0x517A00, IffNode *, Iff_Parse, (void *data));

static const float kTwoPi = 0x1.921fb6p+2f;
static const float kSlerpMin = 0.001f;
static const float kSlerpMax = 0.999f;
static const float kSmallAngle = 0.01f;

/* Rounding of the partial products of quat_mul to float, which differs per caller */
enum { RA = 1, RB = 2, RC = 4, RD = 8, RE = 16, RF = 32 };

/* o = A * B, the 8 multiply form. o[0] still has to be rounded by the caller. */
static void quat_mul(long double o[4], const float *A, const float *B, int round)
{
    long double a = (A[2] - A[1]) * (B[1] - B[2]);
    long double b = (A[3] - A[0]) * (B[2] + B[1]);
    long double c = (B[3] - B[0]) * (A[2] + A[1]);
    long double d = (A[2] + A[0]) * (B[0] + B[1]);
    long double e = (A[3] + A[1]) * (B[3] - B[2]);
    long double f = (A[3] - A[1]) * (B[2] + B[3]);
    long double s;
    float t;

    if (round & RA)
        a = (float)a;
    if (round & RB)
        b = (float)b;
    if (round & RC)
        c = (float)c;
    if (round & RD)
        d = (float)d;
    if (round & RE)
        e = (float)e;
    if (round & RF)
        f = (float)f;
    s = (f + e) + d;
    t = ((A[2] - A[0]) * (B[0] - B[1]) + s) * 0.5f;
    o[0] = ((A[3] + A[0]) * (B[0] + B[3]) + t) - s;
    o[1] = (b + t) - f;
    o[2] = (t + c) - e;
    o[3] = (a + t) - d;
}

static inline long double inv_len(long double sum)
{
    return 1.0 / x87_sqrt(sum);
}

void BoneObj_LerpVec(float *out, const float *a, const float *b, float t)
{
    long double d0 = a[0] - b[0];
    float d1 = a[1] - b[1];
    float d2 = a[2] - b[2];

    d1 = d1 * t;
    d2 = d2 * t;
    out[0] = d0 * t + b[0];
    out[1] = d1 + b[1];
    out[2] = d2 + b[2];
}
HOOK(0x4D3160, BoneObj_LerpVec);

int BoneObj_GetBranchPosition(float *pos, float *delta, BlendBranch *br)
{
    if (br->type == BRANCH_SOURCE) {
        BlendSource *s = br->ptr;

        if (s->getPosition)
            return s->getPosition(pos, delta, s->data);
    } else if (br->type == BRANCH_NODE) {
        return BoneBlend_GetMotion(pos, delta, br->ptr);
    }
    return 0;
}
HOOK(0x4D31C0, BoneObj_GetBranchPosition);

int BoneObj_GetNodeEvents(BoneEvent *ev, int n, Bone *bone, BlendNode *node);
int BoneObj_GetBranchEvents(BoneEvent *ev, int n, Bone *bone, BlendBranch *br);
void BoneObj_DeleteBlendNodeBranch(BlendNode *node, int i);
void BoneObj_ReleaseBlendNodeData(BlendNode *node);
void BoneObj_PruneBone(Bone *bone);

/* Collects this frame's animation events of all bones and drops finished blends */
int BoneObj_GetEvents(BoneEvent *ev, BoneObj *o)
{
    int count = gSkeletons[o->skel].count;
    int n = 0, i;

    for (i = 0; i < count; i++) {
        Bone *b = &o->bones[i];

        if (b->blend) {
            b->flags &= ~BONE_FADING;
            n = BoneObj_GetNodeEvents(ev, n, b, b->blend);
            BoneObj_PruneBone(b);
            if (!b->blend)
                b->flags &= 0xFFF87EFF;
        }
    }
    return n;
}
HOOK(0x4D3210, BoneObj_GetEvents);

int BoneObj_GetNodeEvents(BoneEvent *ev, int n, Bone *bone, BlendNode *node)
{
    BlendBranch *b0, *b1;
    int r;

    Task_Nothing(node);
    b0 = &node->branch[0];
    b1 = &node->branch[1];
    if (!node->select) {
        if (b0->type != BRANCH_NONE)
            n = BoneObj_GetBranchEvents(ev, n, bone, b0);
        if (b1->type != BRANCH_NONE)
            n = BoneObj_GetBranchEvents(ev, n, bone, b1);
        return n;
    }
    r = node->select(node->data);
    if (r != -1)
        BoneObj_ReleaseBlendNodeData(node);
    switch (r) {
    case 0:
        BoneObj_DeleteBlendNodeBranch(node, 0);
        return BoneObj_GetBranchEvents(ev, n, bone, b1);
    case 1:
        BoneObj_DeleteBlendNodeBranch(node, 1);
        return BoneObj_GetBranchEvents(ev, n, bone, b0);
    case 2:
        BoneObj_DeleteBlendNodeBranch(node, 0);
        BoneObj_DeleteBlendNodeBranch(node, 1);
        return n;
    }
    bone->flags |= BONE_FADING;
    n = BoneObj_GetBranchEvents(ev, n, bone, b0);
    return BoneObj_GetBranchEvents(ev, n, bone, b1);
}
HOOK(0x4D32A0, BoneObj_GetNodeEvents);

int BoneObj_GetBranchEvents(BoneEvent *ev, int n, Bone *bone, BlendBranch *br)
{
    if (br->type == BRANCH_SOURCE) {
        BlendSource *s = br->ptr;

        if (s->getEvents)
            return s->getEvents(ev, n, bone, s->data);
        return n;
    }
    if (br->type == BRANCH_NODE)
        return BoneObj_GetNodeEvents(ev, n, bone, br->ptr);
    Error_Show("BlendBranch to illegal target");
    for (;;)
        ;
}
HOOK(0x4D33D0, BoneObj_GetBranchEvents);

void BoneObj_DeleteBlendSource(BlendSource *s);

void BoneObj_DeleteBlendNodeBranch(BlendNode *node, int i)
{
    BlendBranch *br = &node->branch[i];

    if (br->type != BRANCH_NONE) {
        if (br->type == BRANCH_NODE) {
            BoneObj_DeleteBlendNode(br->ptr);
        } else if (br->type == BRANCH_SOURCE) {
            BoneObj_DeleteBlendSource(br->ptr);
        } else {
            Error_Show("BoneObj_DeleteBlendNodeBranch: Illegal child type");
            for (;;)
                ;
        }
    }
    br->type = BRANCH_NONE;
}
HOOK(0x4D3440, BoneObj_DeleteBlendNodeBranch);

void BoneObj_DeleteBlendNode(BlendNode *n)
{
    if (n->free) {
        BoneObj_ReleaseBlendNodeData(n);
        BoneObj_DeleteBlendNodeBranch(n, 0);
        BoneObj_DeleteBlendNodeBranch(n, 1);
    } else {
        if (n->branch[0].type != BRANCH_NONE)
            BoneObj_DeleteBlendNodeBranch(n, 0);
        if (n->branch[1].type != BRANCH_NONE)
            BoneObj_DeleteBlendNodeBranch(n, 1);
    }
    pool_put(&gNodePool, n);
}
HOOK(0x4D34A0, BoneObj_DeleteBlendNode);

BlendNode *BoneObj_NewBlendNode(int t0, void *p0, int t1, void *p1, int unk04, void *blend, void *weight, void *free,
                                void *select, void *data)
{
    BlendNode *n = BoneObj_AllocBlendNode();

    BoneObj_SetBlendNode(n, NULL, t0, p0, t1, p1, unk04, blend, weight, free, select, data);
    return n;
}
HOOK(0x4D3560, BoneObj_NewBlendNode);

BlendNode *BoneObj_AllocBlendNode(void)
{
    BlendNode *n = pool_get(&gNodePool);

    n->parent = NULL;
    n->branch[0].type = BRANCH_NONE;
    n->branch[1].type = BRANCH_NONE;
    return n;
}
HOOK(0x4D35B0, BoneObj_AllocBlendNode);

void BoneObj_SetBlendNode(BlendNode *n, BlendNode *parent, int t0, void *p0, int t1, void *p1, int unk04,
                          void *blend, void *weight, void *free, void *select, void *data)
{
    n->parent = parent;
    n->branch[0].type = t0;
    n->branch[0].ptr = p0;
    n->branch[1].type = t1;
    n->branch[1].ptr = p1;
    n->unk04 = unk04;
    n->blend = blend;
    n->weight = weight;
    n->free = free;
    n->select = select;
    n->data = data;
}
HOOK(0x4D3600, BoneObj_SetBlendNode);

void BoneObj_DeleteBlendSource(BlendSource *s)
{
    if (s->free)
        s->free(s->data);
    s->data = NULL;
    s->free = NULL;
    s->getPosition = NULL;
    s->getOrientation = NULL;
    s->getEvents = NULL;
    pool_put(&gSourcePool, s);
}
HOOK(0x4D3660, BoneObj_DeleteBlendSource);

void BoneObj_ReleaseBlendNodeData(BlendNode *node)
{
    Task_Nothing(node);
    if (node->free)
        node->free(node->data);
    node->data = NULL;
    node->free = NULL;
    node->weight = NULL;
    node->select = NULL;
}
HOOK(0x4D36E0, BoneObj_ReleaseBlendNodeData);

void Task_Nothing(void *task)
{
    (void)task;
}
HOOK(0x4D3710, Task_Nothing);

BlendBranch *BoneObj_GetBlendBranch(BlendNode *node, int i)
{
    return &node->branch[i];
}
HOOK(0x4D3720, BoneObj_GetBlendBranch);

BlendNode *BoneObj_PruneBlendNode(BlendNode *node);

void BoneObj_PruneBone(Bone *bone)
{
    if (bone->blend)
        bone->blend = BoneObj_PruneBlendNode(bone->blend);
}
HOOK(0x4D3730, BoneObj_PruneBone);

/* Collapses nodes left with one or no branch; returns what replaces node */
BlendNode *BoneObj_PruneBlendNode(BlendNode *node)
{
    BlendNode *parent = node->parent, *result = node;
    BlendBranch *slot = NULL, *b0, *b1;

    if (parent) {
        slot = &parent->branch[0];
        if (slot->type != BRANCH_NODE || slot->ptr != node)
            slot = &parent->branch[1];
    }
    b0 = &node->branch[0];
    b1 = &node->branch[1];
    if (b0->type == BRANCH_NONE) {
        if (b1->type == BRANCH_NONE) {
            BoneObj_DeleteBlendNode(node);
            if (parent) {
                BoneObj_ReleaseBlendNodeData(parent);
                slot->type = BRANCH_NONE;
            }
            return parent;
        }
        *b0 = *b1;
        b1->type = BRANCH_NONE;
        return BoneObj_PruneBlendNode(node);
    }
    if (b1->type == BRANCH_NONE) {
        if (b0->type == BRANCH_NODE || parent) {
            BlendNode *next = NULL;

            if (parent) {
                result = parent;
                *slot = *b0;
            }
            if (b0->type == BRANCH_NODE) {
                next = b0->ptr;
                next->parent = parent;
            }
            b0->type = BRANCH_NONE;
            BoneObj_DeleteBlendNode(node);
            if (next)
                return BoneObj_PruneBlendNode(next);
        }
        return result;
    }
    if (b0->type == BRANCH_NODE)
        BoneObj_PruneBlendNode(b0->ptr);
    if (b1->type == BRANCH_NODE)
        BoneObj_PruneBlendNode(b1->ptr);
    if (b0->type == BRANCH_NONE || b1->type == BRANCH_NONE)
        return BoneObj_PruneBlendNode(node);
    return result;
}
HOOK(0x4D3750, BoneObj_PruneBlendNode);

void BoneObj_SetRoot(Matrix *m, Quat *q)
{
    gRootMatrix = m;
    gRootQuat = q;
}
HOOK(0x4D38A0, BoneObj_SetRoot);

void BoneObj_UpdateBone(const Matrix *m, const Quat *pq, const Vec3f *ppos, Bone *b, BlendNode *inherited);

/* Poses all bones under the root transform m */
void BoneObj_Update(const Matrix *m, BoneObj *o)
{
    static const int next[3] = {1, 2, 0};
    Matrix mm = *m;
    const float *e = mm.m;
    Vec3f pos = {0, 0, 0};
    Quat q;
    float *qa = &q.x;
    long double t = (e[8] + e[4]) + e[0];
    Bone *root;

    if (t > 0.0f) {
        long double s = x87_sqrt(t + 1.0f);

        q.w = s * 0.5f;
        s = 0.5f / s;
        q.x = -((e[5] - e[7]) * s);
        q.y = -((e[6] - e[2]) * s);
        q.z = -((e[1] - e[3]) * s);
    } else {
        int i = 0, j, k;
        long double s;

        if (e[4] > e[0])
            i = 1;
        if (e[8] > e[i * 4])
            i = 2;
        j = next[i];
        k = next[j];
        s = x87_sqrt(((e[i * 4] - e[j * 4]) - e[k * 4]) + 1.0f);
        qa[i] = s * 0.5f;
        if (s != 0.0f)
            s = 0.5f / s;
        qa[j] = (e[j * 3 + i] + e[i * 3 + j]) * s;
        qa[k] = (e[k * 3 + i] + e[i * 3 + k]) * s;
        q.w = -((e[j * 3 + k] - e[k * 3 + j]) * s);
    }
    q.unk10 = mm.unk24;
    BoneObj_SetRoot(&mm, &q);
    root = o->bones;
    BoneObj_UpdateBone(&mm, &q, &pos, root, NULL);
    root->local.x = 0;
    root->local.y = 0;
    root->local.z = 0;
    root->local.w = 1.0f;
    root->local.unk10 = 0;
    BoneObj_SetRoot(NULL, NULL);
}
HOOK(0x4D38C0, BoneObj_Update);

int BoneObj_GetNodeOrientation(Quat *out, Bone *b, BlendNode *n);

void BoneObj_UpdateBone(const Matrix *m, const Quat *pq, const Vec3f *ppos, Bone *b, BlendNode *inherited)
{
    const float *e = m->m;
    BlendNode *eff = b->blend ? b->blend : inherited;
    long double x;
    float y, z;
    Quat q;
    int r;
    Bone *c;

    b->flags &= ~(BONE_ANIMATED | BONE_ORIENTED);
    x = (e[2] * b->offset.z + e[1] * b->offset.y) + b->offset.x * e[0];
    y = (e[5] * b->offset.z + e[3] * b->offset.x) + e[4] * b->offset.y;
    z = (e[8] * b->offset.z + e[6] * b->offset.x) + e[7] * b->offset.y;
    b->pos.x = x + ppos->x;
    b->pos.y = y + ppos->y;
    b->pos.z = z + ppos->z;
    r = BoneObj_GetNodeOrientation(&q, b, eff);
    if (r == -1) {
        unsigned int saved = b->flags;

        r = BoneObj_GetNodeOrientation(&q, b, inherited);
        b->flags = saved;
    }
    if (r == 0) {
        long double o[4];

        b->local = q;
        quat_mul(o, &pq->x, &q.x, RD);
        q.x = o[0];
        q.y = o[1];
        q.z = o[2];
        q.w = o[3];
        q.unk10 = q.unk10 + pq->unk10 + 1;
        if (q.unk10 > 1000) {
            long double inv;

            q.unk10 = 0;
            inv = inv_len((((long double)q.x * q.x + (long double)q.w * q.w) + (long double)q.z * q.z) +
                          (long double)q.y * q.y);
            q.x = q.x * inv;
            q.y = q.y * inv;
            q.z = q.z * inv;
            q.w = q.w * inv;
        }
    } else {
        float n[4] = {-pq->x, -pq->y, -pq->z, pq->w};
        long double o[4];
        float r0, r2;

        quat_mul(o, n, &q.x, RD | RE);
        r0 = o[0];
        b->local.x = r0;
        b->local.y = o[1];
        r2 = o[2];
        b->local.z = r2;
        b->local.w = o[3];
        b->local.unk10 = pq->unk10 + q.unk10 + 1;
        if (b->local.unk10 > 1000) {
            long double inv;

            b->local.unk10 = 0;
            inv = inv_len(((o[3] * o[3] + (long double)r2 * r2) + o[1] * o[1]) + (long double)r0 * r0);
            b->local.x = r0 * inv;
            b->local.y = o[1] * inv;
            b->local.z = r2 * inv;
            b->local.w = o[3] * inv;
        }
    }
    b->matrix.unk24 = q.unk10;
    b->world = q;
    {
        long double xx = (long double)q.x * q.x, xy = (long double)q.x * q.y, zw;
        float xz, xw, yy, yz, yw, zz;
        long double u;

        xx = xx + xx;
        xy = xy + xy;
        xz = (long double)q.x * q.z * 2;
        xw = (long double)q.x * q.w * 2;
        yy = (long double)q.y * q.y * 2;
        yz = (long double)q.z * q.y * 2;
        yw = (long double)q.w * q.y * 2;
        zz = (long double)q.z * q.z * 2;
        zw = (long double)q.w * q.z;
        zw = zw + zw;
        b->matrix.m[0] = (1.0f - yy) - zz;
        b->matrix.m[1] = xy - zw;
        b->matrix.m[2] = yw + xz;
        b->matrix.m[3] = zw + xy;
        u = 1.0f - xx;
        b->matrix.m[4] = u - zz;
        b->matrix.m[5] = yz - xw;
        b->matrix.m[6] = xz - yw;
        b->matrix.m[7] = yz + xw;
        b->matrix.m[8] = u - yy;
    }
    if (b->flags & BONE_ANIMATED)
        inherited = eff;
    for (c = b->child; c; c = c->next)
        BoneObj_UpdateBone(&b->matrix, &q, &b->pos, c, inherited);
}
HOOK(0x4D3A90, BoneObj_UpdateBone);

int BoneObj_GetBranchOrientation(Quat *out, Bone *bone, BlendBranch *br);
int BoneObj_BlendOrientations(Quat *out, int t0, Quat *q0, int t1, Quat *q1, float w);
void BoneObj_GetRoot(Matrix **m, Quat **q);

int BoneObj_GetNodeOrientation(Quat *out, Bone *b, BlendNode *n)
{
    Quat qa, qb;
    int r0, r1, r;
    float w;

    r0 = BoneObj_GetBranchOrientation(&qa, b, &n->branch[0]);
    r1 = BoneObj_GetBranchOrientation(&qb, b, &n->branch[1]);
    if (n->weight)
        w = n->weight(n->data);
    else
        w = 0;
    if (n->blend)
        r = n->blend(out, r0, &qa, r1, &qb, w);
    else
        r = BoneObj_BlendOrientations(out, r0, &qa, r1, &qb, w);
    if (r == 1) {
        const Quat *p;
        float nq[4];
        long double o[4];

        if (b->parent) {
            p = &b->parent->world;
        } else {
            Quat *root;

            BoneObj_GetRoot(NULL, &root);
            p = root;
        }
        nq[0] = -p->x;
        nq[1] = -p->y;
        nq[2] = -p->z;
        nq[3] = p->w;
        quat_mul(o, nq, &out->x, RA | RC | RD | RE);
        out->x = o[0];
        out->y = o[1];
        out->z = o[2];
        out->unk10 = out->unk10 + (p->unk10 + 1);
        out->w = o[3];
        if (out->unk10 > 1000) {
            long double inv;

            out->unk10 = 0;
            inv = inv_len(((o[3] * o[3] + o[2] * o[2]) + (long double)out->x * out->x) +
                          (long double)out->y * out->y);
            out->x = out->x * inv;
            out->y = out->y * inv;
            out->z = o[2] * inv;
            out->w = o[3] * inv;
        }
        return 0;
    }
    return r;
}
HOOK(0x4D3F50, BoneObj_GetNodeOrientation);

void BoneObj_GetRoot(Matrix **m, Quat **q)
{
    if (m)
        *m = gRootMatrix;
    if (q)
        *q = gRootQuat;
}
HOOK(0x4D4190, BoneObj_GetRoot);

/* Default node blend: slerp two orientations of the same kind, else combine them */
int BoneObj_BlendOrientations(Quat *out, int t0, Quat *q0, int t1, Quat *q1, float w)
{
    long double o[4], inv;

    if (t0 == -1) {
        if (t1 != -1) {
            *out = *q1;
            return t1;
        }
        return -1;
    }
    if (t1 == -1) {
        *out = *q0;
        return t0;
    }
    if (t0 == 0) {
        if (t1 == 0) {
            BoneObj_Slerp(out, q0, q1, w);
            return 0;
        }
        quat_mul(o, &q1->x, &q0->x, RC | RD | RE);
        out->x = o[0];
        out->y = o[1];
        out->z = o[2];
        out->w = o[3];
        out->unk10 = q0->unk10 + q1->unk10 + 1;
        if (out->unk10 > 1000) {
            out->unk10 = 0;
            inv = inv_len((((long double)out->x * out->x + o[3] * o[3]) + (long double)out->z * out->z) +
                          (long double)out->y * out->y);
            out->x = out->x * inv;
            out->y = out->y * inv;
            out->z = out->z * inv;
            out->w = o[3] * inv;
        }
        return 1;
    }
    if (t1 == 1) {
        BoneObj_Slerp(out, q0, q1, w);
        return 1;
    }
    quat_mul(o, &q0->x, &q1->x, RC | RD | RE);
    out->x = o[0];
    out->y = o[1];
    out->z = o[2];
    out->w = o[3];
    out->unk10 = q0->unk10 + q1->unk10 + 1;
    if (out->unk10 > 1000) {
        out->unk10 = 0;
        inv = inv_len(((o[3] * o[3] + (long double)out->z * out->z) + (long double)out->y * out->y) +
                      (long double)out->x * out->x);
        out->x = out->x * inv;
        out->y = out->y * inv;
        out->z = out->z * inv;
        out->w = o[3] * inv;
    }
    return 1;
}
HOOK(0x4D41C0, BoneObj_BlendOrientations);

/* t weights a; at the ends it copies instead of interpolating */
void BoneObj_Slerp(Quat *out, const Quat *a, const Quat *b, float t)
{
    int flip = 0;
    float angle, inv_sin, u;
    Quat c;
    long double s0, s1;

    if (!(t > kSlerpMin)) {
        *out = *b;
        return;
    }
    if (!(t < kSlerpMax)) {
        *out = *a;
        return;
    }
    Quat_SlerpPrepare_((char *)&flip, &angle, &inv_sin, a, b);
    if (flip) {
        c.x = -b->x;
        c.y = -b->y;
        c.z = -b->z;
        c.w = -b->w;
        c.unk10 = b->unk10;
    } else {
        c = *b;
    }
    u = 1.0f - t;
    if (angle == 0.0f) {
        *out = *a;
        return;
    }
    if (angle < kSmallAngle) {
        s0 = 1.0f - u;
        s1 = u;
    } else {
        u = u * angle;
        s0 = x87_sin(angle - u) * inv_sin;
        s1 = x87_sin(u) * inv_sin;
    }
    out->x = s0 * a->x + s1 * c.x;
    out->y = s0 * a->y + s1 * c.y;
    out->z = s0 * a->z + s1 * c.z;
    out->w = s0 * a->w + s1 * c.w;
    out->unk10 = a->unk10 + c.unk10 + 1;
}
HOOK(0x4D4500, BoneObj_Slerp);

int BoneObj_GetBranchOrientation(Quat *out, Bone *bone, BlendBranch *br)
{
    if (br->type == BRANCH_SOURCE) {
        BlendSource *s = br->ptr;

        if (s->getOrientation)
            return s->getOrientation(out, bone, s->data);
    } else if (br->type == BRANCH_NODE) {
        return BoneObj_GetNodeOrientation(out, bone, br->ptr);
    }
    return -1;
}
HOOK(0x4D4690, BoneObj_GetBranchOrientation);

BlendSource *BoneObj_AllocBlendSource(void);
void BoneObj_SetBlendSource(BlendSource *s, void *getPosition, void *getOrientation, void *getEvents, void *free,
                            void *data);
int BoneObj_ParentOrientationCB(Quat *out, Bone *bone, Bone *self);

/* A source that keeps the orientation of the nearest animated parent */
BlendSource *BoneObj_NewParentSource(Bone *bone)
{
    BlendSource *s = BoneObj_AllocBlendSource();

    BoneObj_SetBlendSource(s, NULL, BoneObj_ParentOrientationCB, NULL, NULL, bone);
    return s;
}
HOOK(0x4D46E0, BoneObj_NewParentSource);

BlendSource *BoneObj_AllocBlendSource(void)
{
    return pool_get(&gSourcePool);
}
HOOK(0x4D4710, BoneObj_AllocBlendSource);

void BoneObj_SetBlendSource(BlendSource *s, void *getPosition, void *getOrientation, void *getEvents, void *free,
                            void *data)
{
    s->getPosition = getPosition;
    s->getOrientation = getOrientation;
    s->getEvents = getEvents;
    s->free = free;
    s->data = data;
}
HOOK(0x4D4750, BoneObj_SetBlendSource);

Bone *BoneObj_FindBlendParent(Bone *b);

int BoneObj_ParentOrientationCB(Quat *out, Bone *bone, Bone *self)
{
    return BoneObj_GetNodeOrientation(out, bone, BoneObj_FindBlendParent(self)->blend);
}
HOOK(0x4D4780, BoneObj_ParentOrientationCB);

Bone *BoneObj_FindBlendParent(Bone *b)
{
    Bone *p = b->parent;

    while (p && !(p->flags & BONE_ANIMATED))
        p = p->parent;
    return p;
}
HOOK(0x4D47B0, BoneObj_FindBlendParent);

AnimInst *BoneObj_AllocAnimInst(void);
void BoneObj_InitAnimInst(AnimInst *inst, AnimDesc *desc, unsigned int flags, int skel, int bone, int id);
int BoneObj_AnimPositionCB(float *pos, float *delta, AnimInst *inst);
int BoneObj_AnimOrientationCB(Quat *out, Bone *bone, AnimInst *inst);
int BoneObj_AnimEventsCB(BoneEvent *ev, int n, Bone *bone, AnimInst *inst);
void BoneObj_AnimFreeCB(AnimInst *inst);

BlendSource *BoneObj_NewAnimSource(int skel, int bone, int id, unsigned int flags)
{
    BlendSource *s = BoneObj_AllocBlendSource();
    AnimDesc *desc = gAnimSets[skel].byId[id];
    AnimInst *inst = BoneObj_AllocAnimInst();

    BoneObj_InitAnimInst(inst, desc, flags | ANIM_FIRST, skel, bone, id);
    BoneObj_SetBlendSource(s, BoneObj_AnimPositionCB, BoneObj_AnimOrientationCB, BoneObj_AnimEventsCB,
                           BoneObj_AnimFreeCB, inst);
    return s;
}
HOOK(0x4D47D0, BoneObj_NewAnimSource);

int BoneObj_AnimPositionCB(float *pos, float *delta, AnimInst *inst)
{
    PosTrack *tr = inst->desc->pos;
    int n = tr->count;
    PosKey *keys = tr->keys;

    if (n == 1) {
        memcpy(inst->pos, keys[0].pos, sizeof(inst->pos));
        inst->delta[0] = 0;
        inst->delta[1] = 0;
        inst->delta[2] = 0;
    } else {
        union {
            AnimInst *p;
            float f;
        } stale = {inst};
        float tm = inst->time, kt = stale.f, p[3];
        int i = 0;

        for (; i < n; i++) {
            kt = keys[i].time;
            if (!(kt < tm))
                break;
        }
        if (tm == kt) {
            memcpy(p, keys[i].pos, sizeof(p));
        } else if (i == n) {
            memcpy(p, keys[n - 1].pos, sizeof(p));
        } else {
            PosKey *k = &keys[i], *pk = &keys[i - 1];

            if (gBoneSmoothAnims) {
                long double dt = kt - pk->time;
                long double s = 1.0f - (kt - tm) / dt;
                long double s2 = s * s, s3, h, in2;
                float s2f, s3f, h1, h2, h3, h4, o0, o1, o2, i0, i1;

                s2f = s2;
                s3 = s2 * s;
                s3f = s3;
                h = (s3 + s3) - s2f * 3.0f;
                h1 = h + 1.0f;
                h2 = -h;
                h3 = (s - (s2f + s2f)) + s3f;
                h4 = s2f - s3f;
                o0 = pk->out[0] * dt;
                o1 = pk->out[1] * dt;
                o2 = pk->out[2] * dt;
                i0 = k->in[0] * dt;
                i1 = k->in[1] * dt;
                in2 = dt * k->in[2];
                p[0] = ((k->pos[0] * h2 + i0 * h4) + pk->pos[0] * h1) + o0 * h3;
                p[1] = ((k->pos[1] * h2 + pk->pos[1] * h1) + i1 * h4) + o1 * h3;
                p[2] = ((k->pos[2] * h2 + pk->pos[2] * h1) + in2 * h4) + o2 * h3;
            } else {
                long double f = (kt - tm) / (kt - pk->time);
                long double d0 = pk->pos[0] - k->pos[0];
                float d1 = pk->pos[1] - k->pos[1];
                float d2 = pk->pos[2] - k->pos[2];
                float e0 = d0 * f;
                long double e2;

                d1 = d1 * f;
                e2 = f * d2;
                p[0] = e0 + k->pos[0];
                p[1] = d1 + k->pos[1];
                p[2] = e2 + k->pos[2];
            }
        }
        inst->delta[0] = p[0] - inst->pos[0];
        inst->delta[1] = p[1] - inst->pos[1];
        inst->delta[2] = p[2] - inst->pos[2];
        memcpy(inst->pos, p, sizeof(p));
    }
    if (pos)
        memcpy(pos, inst->pos, sizeof(inst->pos));
    if (delta)
        memcpy(delta, inst->delta, sizeof(inst->delta));
    return 1;
}
HOOK(0x4D4840, BoneObj_AnimPositionCB);

int BoneObj_AnimCollectEvents(BoneEvent *ev, int n, Bone *bone, AnimInst *inst, float from, float to);

/* Advances the animation one frame and reports the events passed */
int BoneObj_AnimEventsCB(BoneEvent *ev, int n, Bone *bone, AnimInst *inst)
{
    float old, now;
    int has = 0;
    AnimDesc *d;

    if (inst->cache)
        inst->cache->time = inst->time;
    inst->flags &= ~BONE_EVENTS_DONE;
    bone->flags &= ~BONE_EVENTS_DONE;
    old = inst->time;
    now = inst->speed + inst->time;
    inst->time = now;
    if (ev && inst->desc->events && inst->desc->events->count > 0) {
        has = 1;
        if ((inst->flags & ANIM_FIRST) && old != now)
            n = BoneObj_AnimCollectEvents(ev, n, bone, inst, old, old);
        n = BoneObj_AnimCollectEvents(ev, n, bone, inst, old, inst->time);
    }
    d = inst->desc;
    if ((!(inst->time < d->length) && !(inst->flags & ANIM_REVERSE)) ||
        (!(inst->time > 0.0f) && (inst->flags & ANIM_REVERSE))) {
        inst->flags |= BONE_EVENTS_DONE;
        bone->flags |= BONE_EVENTS_DONE;
        if (inst->flags & ANIM_LOOP) {
            float from;

            if (inst->flags & ANIM_REVERSE) {
                inst->time = inst->desc->length + inst->time;
                from = inst->desc->length + 1.0f;
            } else {
                inst->time = inst->time - inst->desc->length;
                from = -1.0f;
            }
            if (has)
                n = BoneObj_AnimCollectEvents(ev, n, bone, inst, from, inst->time);
            if (bone->index == 0 && !(inst->flags & ANIM_FIRST)) {
                PosTrack *tr = inst->desc->pos;
                float saved = inst->time;
                PosKey *k;

                if (inst->flags & ANIM_REVERSE) {
                    inst->time = 0;
                    BoneObj_AnimPositionCB(NULL, NULL, inst);
                    k = &tr->keys[tr->count - 1];
                } else {
                    inst->time = inst->desc->length;
                    BoneObj_AnimPositionCB(NULL, NULL, inst);
                    k = &tr->keys[0];
                }
                inst->pos[0] = k->pos[0] - inst->delta[0];
                inst->pos[1] = k->pos[1] - inst->delta[1];
                inst->pos[2] = k->pos[2] - inst->delta[2];
                inst->flags &= ~ANIM_FIRST;
                inst->time = saved;
                return n;
            }
        } else {
            if (inst->flags & ANIM_REVERSE) {
                inst->time = 0;
                inst->flags &= ~ANIM_FIRST;
                return n;
            }
            inst->time = inst->desc->length;
        }
    }
    inst->flags &= ~ANIM_FIRST;
    return n;
}
HOOK(0x4D4B60, BoneObj_AnimEventsCB);

int BoneObj_AnimCollectEvents(BoneEvent *ev, int n, Bone *bone, AnimInst *inst, float from, float to)
{
    EventTrack *et;
    EventKey *k;
    int count, back, i;
    float lo, hi;

    if (!inst)
        return n;
    et = inst->desc->events;
    count = et->count;
    k = et->keys;
    back = to < from;
    lo = from < to ? from : to;
    hi = from > to ? from : to;
    for (i = 0; i < count; i++, k++) {
        int add;

        if (k->time > hi)
            break;
        if (lo == hi && k->time == lo)
            add = 1;
        else if (back)
            add = !(k->time < lo) && k->time < hi;
        else
            add = k->time > lo;
        if (add) {
            ev[n].bone = bone->index;
            ev[n].id = k->id;
            ev[n].anim = inst->id;
            memcpy(&ev[n].time, &k->time, 4 * sizeof(float));
            n++;
        }
    }
    return n;
}
HOOK(0x4D4DC0, BoneObj_AnimCollectEvents);

AnimInst *BoneObj_AllocAnimInst(void)
{
    return pool_get(&gInstPool);
}
HOOK(0x4D4F20, BoneObj_AllocAnimInst);

void BoneObj_InitAnimInst(AnimInst *inst, AnimDesc *desc, unsigned int flags, int skel, int bone, int id)
{
    AnimSet *set = &gAnimSets[skel];

    if (set->hasCaches) {
        KeyCache *c = pool_get(&set->caches);
        int i;

        inst->cache = c;
        c->bones = (int *)(c + 1);
        c->skel = skel;
        *(int *)&c->time = 0;
        c->unk8 = 0;
        for (i = 0; i < gSkeletons[skel].count; i++)
            c->bones[i] = 0;
    } else {
        inst->cache = NULL;
    }
    inst->bone = bone;
    inst->id = id;
    inst->desc = desc;
    inst->delta[0] = 0;
    inst->delta[1] = 0;
    inst->delta[2] = 0;
    inst->flags = flags;
    if (flags & ANIM_REVERSE) {
        memcpy(inst->pos, desc->pos->keys[desc->pos->count - 1].pos, sizeof(inst->pos));
        inst->speed = -gTicksPerFrame;
        memcpy(&inst->time, &desc->length, sizeof(float));
    } else {
        memcpy(inst->pos, desc->pos->keys[0].pos, sizeof(inst->pos));
        *(int *)&inst->time = 0;
        memcpy(&inst->speed, &gTicksPerFrame, sizeof(float));
    }
}
HOOK(0x4D4F60, BoneObj_InitAnimInst);

void BoneObj_FreeAnimInst(AnimInst *inst);

void BoneObj_AnimFreeCB(AnimInst *inst)
{
    BoneObj_FreeAnimInst(inst);
}
HOOK(0x4D5080, BoneObj_AnimFreeCB);

void BoneObj_FreeAnimInst(AnimInst *inst)
{
    if (inst->cache)
        pool_put(&gAnimSets[inst->cache->skel].caches, inst->cache);
    pool_put(&gInstPool, inst);
}
HOOK(0x4D5090, BoneObj_FreeAnimInst);

/* Weights c0 for s0 and c for s1, after flipping c onto the short arc */
static void slerp_keys(Quat *out, const float *c0, const Quat *c, long double s0, long double s1, int count)
{
    out->x = s1 * c->x + s0 * c0[0];
    out->y = s1 * c->y + s0 * c0[1];
    out->z = s1 * c->z + s0 * c0[2];
    out->w = s1 * c->w + s0 * c0[3];
    out->unk10 = count + c->unk10 + 1;
}

/* Prepares the second key for slerp_keys: returns the angle, 0 means use c0 */
static long double slerp_angle(Quat *c, const float *key, const SlerpInfo *sl)
{
    long double angle = sl->angle;

    memcpy(c, key, sizeof(*c));
    if (!(sl->angle < kTwoPi)) {
        angle = angle - kTwoPi;
        c->x = -c->x;
        c->y = -c->y;
        c->z = -c->z;
        c->w = -c->w;
    }
    return angle;
}

static void slerp_weights(long double angle, float r, float inv_sin, long double *s0, long double *s1)
{
    if (angle < kSmallAngle) {
        *s0 = 1.0f - r;
        *s1 = r;
    } else {
        float t = angle * r;

        *s0 = x87_sin(angle - t) * inv_sin;
        *s1 = x87_sin(t) * inv_sin;
    }
}

int BoneObj_AnimOrientationCB(Quat *out, Bone *bone, AnimInst *inst)
{
    int id = bone->index;
    RotTrack *tr;
    RotKey *keys;
    int n, i = 0, a, b, has;
    float tm, kt = 0, r;
    KeyCache *cache;

    if (inst->bone == id)
        bone->flags |= BONE_ANIMATED;
    tr = &inst->desc->rot[id];
    n = tr->count;
    keys = tr->keys;
    if (n == 1) {
        memcpy(out, keys[0].q, 4 * sizeof(float));
        out->unk10 = 0;
        return 0;
    }
    tm = inst->time;
    cache = inst->cache;
    has = cache != NULL;
    if (has && !(tm - cache->time < 0.0f))
        i = cache->bones[id];
    for (; i < n; i++) {
        kt = keys[i].time;
        if (!(kt < tm))
            break;
    }
    if (tm == kt) {
        memcpy(out, keys[i].q, 4 * sizeof(float));
        out->unk10 = 0;
        a = i;
        goto done;
    }
    a = i - 1;
    if (i == n) {
        if (inst->desc->flags & ANIM_LOOPS) {
            r = (tm - kt) / (inst->desc->length - kt);
            b = 0;
            goto interp;
        }
        memcpy(out, keys[n - 1].q, 4 * sizeof(float));
        goto zero;
    }
    if (a < 0) {
        memcpy(out, keys[0].q, 4 * sizeof(float));
        out->unk10 = 0;
        goto done;
    }
    r = 1.0f - (kt - tm) / (kt - keys[i - 1].time);
    b = i;
interp:
    if (tr->slerp) {
        RotKey *ka = &keys[a], *kb = &keys[b];
        long double angle, s0, s1;
        Quat c;

        if (gBoneSmoothAnims) {
            Quat q1, q2;

            angle = slerp_angle(&c, kb->q, &tr->slerp[a]);
            if (angle == 0.0f) {
                memcpy(&q1, ka->q, sizeof(q1));
            } else {
                slerp_weights(angle, r, tr->slerp[a].invSin, &s0, &s1);
                slerp_keys(&q1, ka->q, &c, s0, s1, *(int *)&ka->time);
            }
            angle = slerp_angle(&c, kb->b, &tr->slerp2[a]);
            if (angle == 0.0f) {
                memcpy(&q2, ka->a, sizeof(q2));
            } else {
                slerp_weights(angle, r, tr->slerp2[a].invSin, &s0, &s1);
                slerp_keys(&q2, ka->a, &c, s0, s1, *(int *)&ka->b[0]);
            }
            BoneObj_Slerp(out, &q1, &q2, 1.0f - ((1.0f - r) * r + (1.0f - r) * r));
        } else {
            angle = slerp_angle(&c, kb->q, &tr->slerp[a]);
            if (angle == 0.0f) {
                memcpy(out, ka->q, sizeof(*out));
            } else {
                slerp_weights(angle, r, tr->slerp[a].invSin, &s0, &s1);
                slerp_keys(out, ka->q, &c, s0, s1, *(int *)&ka->time);
            }
        }
    }
zero:
    out->unk10 = 0;
done:
    if (has)
        inst->cache->bones[bone->index] = a;
    return 0;
}
HOOK(0x4D5180, BoneObj_AnimOrientationCB);

OrientData *BoneObj_AllocOrientation(void);
void BoneObj_SetOrientation(OrientData *d, int bone, int mode, const Quat *q);
int BoneObj_GetOrientationCB(Quat *out, Bone *bone, OrientData *d);
void BoneObj_FreeOrientation(OrientData *d);

BlendSource *BoneObj_NewOrientationSource(int bone, int mode, const Quat *q)
{
    BlendSource *s = BoneObj_AllocBlendSource();
    OrientData *d = BoneObj_AllocOrientation();

    BoneObj_SetOrientation(d, bone, mode, q);
    BoneObj_SetBlendSource(s, NULL, BoneObj_GetOrientationCB, NULL, BoneObj_FreeOrientation, d);
    return s;
}
HOOK(0x4D5700, BoneObj_NewOrientationSource);

OrientData *BoneObj_AllocOrientation(void)
{
    return pool_get(&gOrientPool);
}
HOOK(0x4D5750, BoneObj_AllocOrientation);

void BoneObj_SetOrientation(OrientData *d, int bone, int mode, const Quat *q)
{
    d->bone = bone;
    d->mode = mode;
    memcpy(&d->q, q, sizeof(d->q));
}
HOOK(0x4D5790, BoneObj_SetOrientation);

void BoneObj_FreeOrientation(OrientData *d)
{
    pool_put(&gOrientPool, d);
}
HOOK(0x4D57C0, BoneObj_FreeOrientation);

/* Modes 0-2 give an orientation relative to the parent, 3-5 an absolute one:
   0 and 4 against the parent or the root, 2 against the parent only, 5 against
   the root only, 1 and 3 as given. */
int BoneObj_GetOrientationCB(Quat *out, Bone *bone, OrientData *d)
{
    Bone *p;
    Quat *root;
    const Quat *Q = &d->q;
    long double o[4], inv;
    float A[4];
    int cnt;
    float o0, o1, o2;

    if (bone->index != d->bone)
        return -1;
    p = bone->parent;
    bone->flags |= BONE_ORIENTED;
    BoneObj_GetRoot(NULL, &root);
    switch (d->mode) {
    case 0:
    case 2:
        if (d->mode == 2 && !p) {
            memcpy(out, Q, sizeof(*out));
            return 0;
        }
        {
            const Quat *pq = p ? &p->world : root;

            A[0] = -pq->x;
            A[1] = -pq->y;
            A[2] = -pq->z;
            A[3] = pq->w;
            cnt = pq->unk10;
        }
        quat_mul(o, A, &Q->x, RA | RB | RC | RD | RE | RF);
        o0 = o[0];
        out->x = o0;
        out->y = o[1];
        o2 = o[2];
        out->z = o2;
        out->w = o[3];
        out->unk10 = Q->unk10 + cnt + 1;
        if (out->unk10 > 1000) {
            out->unk10 = 0;
            if (d->mode == 0)
                inv = inv_len((((long double)o0 * o0 + o[3] * o[3]) + (long double)o2 * o2) + o[1] * o[1]);
            else
                inv = inv_len(((o[3] * o[3] + (long double)o2 * o2) + o[1] * o[1]) + (long double)o0 * o0);
            out->x = out->x * inv;
            out->y = o[1] * inv;
            out->z = o2 * inv;
            out->w = o[3] * inv;
        }
        return 0;
    case 1:
        memcpy(out, Q, sizeof(*out));
        return 0;
    case 3:
        memcpy(out, Q, sizeof(*out));
        return 1;
    case 4:
    case 5:
        if (d->mode == 4 && p) {
            memcpy(A, &p->world.x, sizeof(A));
            cnt = p->world.unk10;
        } else if (d->mode == 4) {
            A[0] = -root->x;
            A[1] = -root->y;
            A[2] = -root->z;
            A[3] = root->w;
            cnt = root->unk10;
        } else {
            memcpy(A, &root->x, sizeof(A));
            cnt = root->unk10;
        }
        quat_mul(o, A, &Q->x, RC | RD | RE);
        o0 = o[0];
        out->x = o0;
        o1 = o[1];
        out->y = o1;
        o2 = o[2];
        out->z = o2;
        out->w = o[3];
        out->unk10 = Q->unk10 + cnt + 1;
        if (out->unk10 > 1000) {
            out->unk10 = 0;
            inv = inv_len((((o[3] * o[3] + (long double)o2 * o2) + (long double)o1 * o1) + (long double)o0 * o0));
            out->x = out->x * inv;
            out->y = o1 * inv;
            out->z = o2 * inv;
            out->w = o[3] * inv;
        }
        return 1;
    }
    Error_Show("BoneObj_GetOrientationCB: Illegal orientation type");
    for (;;)
        ;
}
HOOK(0x4D5820, BoneObj_GetOrientationCB);

Fade *BoneObj_AllocFade(void);
void BoneObj_InitFade(Fade *f, float duration, int result);
long double BoneObj_FadeWeightCB(Fade *f);
int BoneObj_FadeSelectCB(Fade *f);
void BoneObj_FadeFreeCB(Fade *f);

static void set_fade(BlendNode *n, Fade *f)
{
    n->weight = (long double (*)(void *))BoneObj_FadeWeightCB;
    n->free = (void (*)(void *))BoneObj_FadeFreeCB;
    n->select = (int (*)(void *))BoneObj_FadeSelectCB;
    n->data = f;
}

/* Fades from what the bone does now to src over duration ticks */
void BoneObj_FadeIn(Bone *bone, BlendSource *src, float duration, int result)
{
    BlendNode *n = bone->blend;
    Fade *f = BoneObj_AllocFade();

    BoneObj_InitFade(f, duration, result);
    if (n) {
        Task_Nothing(n);
        if (n->branch[1].type != BRANCH_NONE) {
            BlendNode *m = BoneObj_AllocBlendNode();

            n->parent = m;
            m->branch[0].ptr = n;
            m->branch[0].type = BRANCH_NODE;
            n = m;
        }
        n->branch[1].type = BRANCH_SOURCE;
        n->branch[1].ptr = src;
    } else {
        n = BoneObj_AllocBlendNode();
        n->branch[0].type = BRANCH_SOURCE;
        n->branch[1].type = BRANCH_SOURCE;
        n->branch[1].ptr = src;
        n->branch[0].ptr = BoneObj_NewParentSource(bone);
    }
    n->unk04 = 0;
    n->blend = NULL;
    set_fade(n, f);
    bone->blend = n;
    bone->flags |= BONE_FADING;
}
HOOK(0x4D5EC0, BoneObj_FadeIn);

Fade *BoneObj_AllocFade(void)
{
    return pool_get(&gFadePool);
}
HOOK(0x4D5F90, BoneObj_AllocFade);

void BoneObj_InitFade(Fade *f, float duration, int result)
{
    f->time = 0;
    f->duration = duration;
    f->weight = 1.0f;
    f->result = result;
}
HOOK(0x4D5FD0, BoneObj_InitFade);

/* Like FadeIn for a whole node; short durations switch at once */
void BoneObj_BlendTo(Bone *bone, BlendNode *node, float duration, int result)
{
    BlendNode *old = bone->blend;

    if (old || bone->parent) {
        if (!(duration < 1.0f)) {
            Fade *f = BoneObj_AllocFade();
            BlendNode *n = old;

            BoneObj_InitFade(f, duration, result);
            if (old) {
                Task_Nothing(old);
                if (old->branch[1].type != BRANCH_NONE) {
                    n = BoneObj_AllocBlendNode();
                    old->parent = n;
                    n->branch[0].ptr = old;
                    n->branch[0].type = BRANCH_NODE;
                }
                n->branch[1].type = BRANCH_NODE;
                n->branch[1].ptr = node;
            } else {
                n = BoneObj_AllocBlendNode();
                n->branch[0].type = BRANCH_SOURCE;
                n->branch[1].type = BRANCH_NODE;
                n->branch[1].ptr = node;
                n->branch[0].ptr = BoneObj_NewParentSource(bone);
            }
            n->unk04 = 0;
            n->blend = NULL;
            set_fade(n, f);
            bone->blend = n;
            bone->flags |= BONE_FADING;
            return;
        }
        if (old)
            BoneObj_DeleteBlendNode(old);
    }
    bone->blend = node;
    bone->flags = (bone->flags & 0xFFFAFEFF) | BONE_FADING;
}
HOOK(0x4D5FF0, BoneObj_BlendTo);

long double BoneObj_FadeWeightCB(Fade *f)
{
    return f->weight;
}
HOOK(0x4D63A0, BoneObj_FadeWeightCB);

/* -1 while fading, then the result the fade was made with */
int BoneObj_FadeSelectCB(Fade *f)
{
    long double t = f->time + 1.0f;

    f->time = t;
    if (t < f->duration) {
        f->weight = 1.0f - t / f->duration;
        return -1;
    }
    f->weight = 0;
    return f->result;
}
HOOK(0x4D63B0, BoneObj_FadeSelectCB);

void BoneObj_FreeFade(Fade *f);

void BoneObj_FadeFreeCB(Fade *f)
{
    BoneObj_FreeFade(f);
}
HOOK(0x4D63F0, BoneObj_FadeFreeCB);

void BoneObj_FreeFade(Fade *f)
{
    pool_put(&gFadePool, f);
}
HOOK(0x4D6400, BoneObj_FreeFade);

void BoneObj_FreeAnims(int skel);
void BoneObj_FreeSkeleton(int skel);

void BoneObj_FreeAll(void)
{
    int i;

    for (i = 0; i < 64; i++) {
        BoneObj_FreeAnims(i);
        BoneObj_FreeSkeleton(i);
    }
}
HOOK(0x4D65D0, BoneObj_FreeAll);

void BoneObj_FreeSkeleton(int skel)
{
    Skeleton *s = &gSkeletons[skel];

    if (s->count) {
        Mem_Free(s->parents);
        s->parents = NULL;
        Mem_Free(s->offsets);
        s->count = 0;
        s->parents = NULL;
        s->offsets = NULL;
    }
}
HOOK(0x4D65F0, BoneObj_FreeSkeleton);

void BoneObj_FreeAnims(int skel)
{
    AnimSet *set = &gAnimSets[skel];
    int n, bones, i, j;

    if (set->hasCaches) {
        Pool_Free_(&set->caches);
        set->hasCaches = 0;
    }
    n = set->count;
    if (n <= 0)
        return;
    bones = gSkeletons[skel].count;
    if (bones) {
        for (i = 0; i < n; i++) {
            AnimDesc *d = set->byId[i];
            RotTrack *r;

            if (!d)
                continue;
            set->byId[i] = NULL;
            if (d->events) {
                Mem_Free(d->events->keys);
                d->events->keys = NULL;
                Mem_Free(d->events);
            }
            Mem_Free(d->pos->keys);
            d->pos->keys = NULL;
            Mem_Free(d->pos);
            r = d->rot;
            for (j = 0; j < bones; j++) {
                Mem_Free(r[j].keys);
                r[j].keys = NULL;
                if (r[j].slerp) {
                    Mem_Free(r[j].slerp);
                    r[j].slerp = NULL;
                }
                if (r[j].slerp2) {
                    Mem_Free(r[j].slerp2);
                    r[j].slerp2 = NULL;
                }
            }
            Mem_Free(r);
        }
    }
    Mem_Free(set->descs);
    set->descs = NULL;
    Mem_Free(set->byId);
    set->descs = NULL;
    set->count = 0;
    set->byId = NULL;
}
HOOK(0x4D6640, BoneObj_FreeAnims);

#define ID(a, b, c, d) ((a) << 24 | (b) << 16 | (c) << 8 | (d))

static void *chunk_data(IffNode *form, int id)
{
    return Iff_GetData(Iff_FindChunk(form, id));
}

/* Loads a skeleton and its animations from a BOBJ file */
void BoneObj_Load(const char *name)
{
    IffNode *root = Iff_Parse(Resource_Load(name, 0));
    IffNode *form, *head;
    int *hdr, skel, count;

    if (!root) {
        Warning_Show("File %s is not IFF file.", name);
        return;
    }
    form = Iff_FindForm(root, ID('B', 'O', 'B', 'J'));
    if (!form) {
        Warning_Show("IFF File %s is not of correct type.", name);
        return;
    }
    head = Iff_FindForm(form, ID('B', 'O', 'B', 'H'));
    hdr = chunk_data(head, ID('B', 'O', 'S', 'H'));
    skel = hdr[0];
    count = hdr[1];
    if (count < 1) {
        Iff_FindNextForm(head, ID('B', 'O', 'B', 'H'));
    } else {
        Skeleton *s = &gSkeletons[skel];
        AnimSet *set = &gAnimSets[skel];
        IffNode *plst, *tlst, *list, *it;
        int *lh, anims, total, k;

        s->count = count;
        s->parents = Mem_Alloc(count * 4, 4);
        s->offsets = Mem_Alloc(count * 12, 4);
        plst = Iff_FindChunk(head, ID('P', 'L', 'S', 'T'));
        tlst = Iff_FindChunk(head, ID('T', 'L', 'S', 'T'));
        memcpy(s->parents, Iff_GetData(plst), count * 4);
        memcpy(s->offsets, Iff_GetData(tlst), count * 12);
        list = Iff_FindNextForm(head, ID('B', 'O', 'A', 'L'));
        Iff_FindNextForm(list, ID('B', 'O', 'B', 'H'));
        it = Iff_FindChunk(list, ID('B', 'A', 'L', 'H'));
        lh = Iff_GetData(it);
        anims = lh[0];
        total = lh[1];
        set->count = total;
        set->descs = Mem_Alloc(total * sizeof(AnimDesc), 4);
        set->byId = Mem_Alloc(total * 4, 4);
        memset(set->byId, 0, total * 4);
        for (k = anims; k > 0; k--) {
            IffNode *prev;
            int *ah, id, keys, size, j;
            AnimDesc *d;

            it = Iff_FindNextForm(it, ID('B', 'O', 'A', 'N'));
            ah = chunk_data(it, ID('B', 'O', 'A', 'H'));
            id = ah[2];
            d = set->byId[id] = &set->descs[id];
            memcpy(&d->length, &ah[0], 4);
            d->flags = ah[1];
            d->pos = Mem_Alloc(8, 4);
            d->rot = Mem_Alloc(count * sizeof(RotTrack), 4);
            d->events = NULL;
            prev = Iff_FindChunk(it, ID('B', 'O', 'T', 'H'));
            keys = *(int *)Iff_GetData(prev);
            d->pos->count = keys;
            if (keys > 0) {
                prev = Iff_FindChunk(it, ID('B', 'O', 'T', 'D'));
                size = keys > 1 ? keys * (int)sizeof(PosKey) : keys * 16;
                d->pos->keys = Mem_Alloc(size, 4);
                memcpy(d->pos->keys, Iff_GetData(prev), size);
            }
            for (j = 0; j < count; j++) {
                RotTrack *r = &d->rot[j];

                prev = Iff_FindNextChunk(prev, ID('B', 'O', 'R', 'H'));
                keys = *(int *)Iff_GetData(prev);
                r->count = keys;
                size = keys > 1 ? keys * (int)sizeof(RotKey) : keys * 0x14;
                r->keys = Mem_Alloc(size, 4);
                r->slerp = NULL;
                r->slerp2 = NULL;
                prev = Iff_FindNextChunk(prev, ID('B', 'O', 'R', 'D'));
                memcpy(r->keys, Iff_GetData(prev), size);
            }
            keys = *(int *)chunk_data(it, ID('B', 'O', 'E', 'H'));
            if (keys > 0) {
                d->events = Mem_Alloc(8, 4);
                d->events->count = keys;
                d->events->keys = Mem_Alloc(keys * sizeof(EventKey), 4);
                memcpy(d->events->keys, chunk_data(it, ID('B', 'O', 'E', 'D')), keys * sizeof(EventKey));
            }
        }
    }
    Anim_PrepareSlerps(skel);
    Iff_Free(root);
    Resource_Unload(name);
}
HOOK(0x4D67A0, BoneObj_Load);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A538E8) Quat *gRootQuat;
BSS(A538E8) Matrix *gRootMatrix;
