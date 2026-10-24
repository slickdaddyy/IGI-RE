/* Animation queries and bone rotation (the end of the bone code). */
#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "igi.h"
#include "math/x87.h"
#include "haze/haze.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B44D0, void, Quat_SlerpPrepare_, (int *flip, float *angle, float *inv_sin, const Quat *a, const Quat *b));

static const float kTwoPi = 6.2831854820251465f;
static const float kHalf = 0.5f;
static const float kOne = 1.0f;
static const double kOneD = 1.0;
static const float kSecondsPerTick = 0x1.b4e81cp-13f;

static void slerp_prepare(SlerpInfo *s, const float *a, const float *b)
{
    Quat qa, qb;
    int flip = 0;

    qa.x = a[0];
    qa.y = a[1];
    qa.z = a[2];
    qa.w = a[3];
    qa.unk10 = 0;
    qb.x = b[0];
    qb.y = b[1];
    qb.z = b[2];
    qb.w = b[3];
    qb.unk10 = 0;
    Quat_SlerpPrepare_(&flip, &s->angle, &s->invSin, &qa, &qb);
    if (flip)
        s->angle = s->angle + kTwoPi;
}

/* Fills the slerp tables of every rotation track of a loaded animation set */
void Anim_PrepareSlerps(int skel)
{
    int bones = gSkeletons[skel].count;
    AnimSet *set = &gAnimSets[skel];
    AnimDesc **d;
    int n, b, k;

    if (bones <= 0 || set->count <= 0)
        return;
    d = set->byId;
    for (n = set->count; n; n--, d++) {
        AnimDesc *desc = *d;
        RotTrack *t;

        if (!desc)
            continue;
        t = desc->rot;
        for (b = bones; b; b--, t++) {
            int keys = t->count;

            if (keys <= 1 || t->slerp)
                continue;
            t->slerp = Mem_Alloc(keys * sizeof(SlerpInfo), 4);
            if (gBoneSmoothAnims)
                t->slerp2 = Mem_Alloc(keys * sizeof(SlerpInfo), 4);
            for (k = 0; k < keys; k++) {
                int next = (k + 1) % keys;

                if (next == 0 && !(desc->flags & ANIM_LOOPS)) {
                    t->slerp[k].angle = 0;
                    t->slerp[k].invSin = 0;
                    if (gBoneSmoothAnims) {
                        t->slerp2[k].angle = 0;
                        t->slerp2[k].invSin = 0;
                    }
                    continue;
                }
                slerp_prepare(&t->slerp[k], &t->keys[k].q.x, &t->keys[next].q.x);
                if (gBoneSmoothAnims)
                    slerp_prepare(&t->slerp2[k], t->keys[k].a, t->keys[next].b);
            }
        }
    }
}
HOOK(0x4D6D40, Anim_PrepareSlerps);

void Anim_GetPosKey(float *pos, float *time, BoneObj *obj, int id, int key)
{
    AnimDesc *desc = gAnimSets[obj->skel].byId[id];

    if (pos) {
        PosKey *p = &desc->pos->keys[key];

        pos[0] = p->pos[0];
        pos[1] = p->pos[1];
        pos[2] = p->pos[2];
    }
    if (time)
        *time = desc->pos->keys[key].time;
}
HOOK(0x4D6FC0, Anim_GetPosKey);

int Anim_GetPosKeyCount(int skel, int id)
{
    return gAnimSets[skel].byId[id]->pos->count;
}
HOOK(0x4D7020, Anim_GetPosKeyCount);

int Anim_GetLengthTicks(int skel, int id)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"((long double)gAnimSets[skel].byId[id]->length) : "st");
    return (int)r;
}
HOOK(0x4D7040, Anim_GetLengthTicks);

/* Rotates the bone and its children by q. Bones above rotDepth get the new
   orientation, bones above posDepth also a new position from their parent. */
void Bone_Rotate(Bone *b, const Quat *q, int depth, int rotDepth, int posDepth)
{
    Bone *p = b->parent;
    Quat *w = &b->world;

    if (depth < rotDepth) {
        long double m1, m2, m6, s, nw, ny;
        long double xx, xy, zw, one_xx, t;
        float t18, t10, t44, t40, nz;
        float xz, xw, yy, yz, yw, zz;

        m1 = (w->y - w->z) * (q->z - q->y);
        m2 = (q->w - q->x) * (w->z + w->y);
        t18 = (w->w - w->x) * (q->y + q->z);
        t10 = (q->x + q->z) * (w->y + w->x);
        t44 = (w->w - w->z) * (q->w + q->y);
        m6 = (q->w - q->y) * (w->w + w->z);
        s = m6 + t44 + t10;
        t40 = ((w->x - w->y) * (q->z - q->x) + s) * kHalf;
        w->x = (q->w + q->x) * (w->w + w->x) + t40 - s;
        ny = m2 + t40 - m6;
        w->y = ny;
        nz = t40 + t18 - t44;
        w->z = nz;
        nw = m1 + t40 - t10;
        w->w = nw;
        w->unk10 += q->unk10 + 1;
        if (w->unk10 > 1000) {
            float fy = ny;
            long double r;

            w->unk10 = 0;
            r = kOneD / x87_sqrt(nw * nw + nz * nz + fy * fy + w->x * w->x);
            w->x = w->x * r;
            w->y = fy * r;
            w->z = nz * r;
            w->w = nw * r;
        }
        if (p) {
            Quat *l = &b->local;
            float px = -p->world.x;
            float py = -p->world.y;
            float pz = -p->world.z;
            float pw = p->world.w;
            float n1, n2, h, lx, lz;
            long double n3, s2, ly, lw;

            n1 = (pz + px) * (w->y + w->x);
            n2 = (w->w - w->z) * (pw + py);
            n3 = (w->w + w->z) * (pw - py);
            s2 = n3 + n2 + n1;
            h = ((pz - px) * (w->x - w->y) + s2) * kHalf;
            lx = (pw + px) * (w->w + w->x) + h - s2;
            l->x = lx;
            ly = (w->z + w->y) * (pw - px) + h - n3;
            l->y = ly;
            lz = (w->w - w->x) * (pz + py) + h - n2;
            l->z = lz;
            lw = (w->y - w->z) * (pz - py) + h - n1;
            l->w = lw;
            l->unk10 = w->unk10 + p->world.unk10 + 1;
            if (l->unk10 > 1000) {
                long double r;

                l->unk10 = 0;
                r = kOneD / x87_sqrt(lw * lw + lz * lz + ly * ly + lx * lx);
                l->x = lx * r;
                l->y = ly * r;
                l->z = lz * r;
                l->w = lw * r;
            }
        }
        b->matrix.unk24 = w->unk10;
        xx = w->x * w->x;
        xx += xx;
        xy = w->y * w->x;
        xy += xy;
        t = w->z * w->x;
        xz = t + t;
        t = w->w * w->x;
        xw = t + t;
        t = w->y * w->y;
        yy = t + t;
        t = w->z * w->y;
        yz = t + t;
        t = w->w * w->y;
        yw = t + t;
        t = w->z * w->z;
        zz = t + t;
        zw = w->w * w->z;
        zw += zw;
        b->matrix.m[0] = kOne - yy - zz;
        b->matrix.m[1] = xy - zw;
        b->matrix.m[2] = yw + xz;
        b->matrix.m[3] = zw + xy;
        one_xx = kOne - xx;
        b->matrix.m[4] = one_xx - zz;
        b->matrix.m[5] = yz - xw;
        b->matrix.m[6] = xz - yw;
        b->matrix.m[7] = yz + xw;
        b->matrix.m[8] = one_xx - yy;
    }
    if (depth < posDepth) {
        const float *m = p->matrix.m;
        float r0 = m[2] * b->offset.z + m[1] * b->offset.y + m[0] * b->offset.x;
        float r1 = m[5] * b->offset.z + m[4] * b->offset.y + m[3] * b->offset.x;
        float r2 = m[8] * b->offset.z + m[7] * b->offset.y + m[6] * b->offset.x;
        Bone *c;

        b->pos.x = r0 + p->pos.x;
        b->pos.y = r1 + p->pos.y;
        b->pos.z = r2 + p->pos.z;
        if (depth < posDepth - 1)
            for (c = b->child; c; c = c->next)
                Bone_Rotate(c, q, depth + 1, rotDepth, posDepth);
    }
}
HOOK(0x4D7070, Bone_Rotate);

long double Anim_GetLengthSeconds(int skel, int id)
{
    return (long double)gAnimSets[skel].byId[id]->length * kSecondsPerTick;
}
HOOK(0x4D74D0, Anim_GetLengthSeconds);

int Anim_Exists(BoneObj *obj, int bone, int id)
{
    AnimSet *set;

    if (!obj || !obj->bones || bone < 0 || bone >= gSkeletons[obj->skel].count)
        return 0;
    set = &gAnimSets[obj->skel];
    if (!set->count || id >= set->count)
        return 0;
    return set->byId[id] != NULL;
}
HOOK(0x4D74F0, Anim_Exists);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igb, A538F0, A538F0, 0xA00);
ALIAS(gAnimSets, pad_A538F0, 0);
ALIAS(gBoneSlots, pad_A538F0, 0);
BSS(A54678) int gBoneSmoothAnims;
