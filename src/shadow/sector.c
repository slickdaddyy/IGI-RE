#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "math/x87.h"
#include "shadow/shadow.h"

ORIG(0x4015F0, void, Task_Delete, (Task *task));
ORIG(0x4012A0, Task *, Task_New, (Task *parent, uint16_t type, void *arg));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int stride, int count, int unk));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (Obj *o));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (Task *parts, Obj *o, Obj *owner));
ORIG(0x4C5700, uint8_t, RigidDynCubeObj_GetEvent3, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, Obj *o, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, float, VirModel_GetRadius, (void **model));
ORIG(0x4D0CF0, char *, ModelLod_GetAttach, (void *lod, int i));
ORIG(0x4D0D10, int, ModelLod_GetAttachCount, (void *lod));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D9FB0, uint8_t, MagicObj_GetMoveEvent, (void));
ORIG(0x4DCBA0, void, SectorObject_AddChildSpheres, (Pool *spheres, Task *sectors));
ORIG(0x4DCCB0, int, Sector_AddObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));

Task *MagicObjContainer_New(Task *parent, void *owner);
void SectorObject_CalcBounds(SectorObject *so, float *center, float *radius);
void Sector_CreateMagicObjs(Sector *s, void *owner);

void SectorObject_Create(SectorObject *so)
{
    RigidDynCubeObj_Init(gCurrentWorld, &so->s.o, NULL);
    so->s.o.matrix.m[0] = 1.0f;
    so->s.o.matrix.m[4] = 1.0f;
    so->s.o.matrix.m[8] = 1.0f;
    so->s.o.matrix.m[1] = 0;
    so->s.o.matrix.m[2] = 0;
    so->s.o.matrix.m[3] = 0;
    so->s.o.matrix.m[5] = 0;
    so->s.o.matrix.m[6] = 0;
    so->s.o.matrix.m[7] = 0;
    so->s.o.matrix.unk24 = 0;
    so->sectors = NULL;
    memset(so->unk14C, 1, sizeof(so->unk14C));
    so->built = 1;
    Pool_Init(&so->s.links, 4, 100, 0);
}
HOOK(0x4DDF60, SectorObject_Create);

void Sector_RemoveAllObjects(Sector *s);

void SectorObject_Delete(SectorObject *so)
{
    Sector_RemoveAllObjects(&so->s);
    Pool_Free(&so->s.links);
    if (so->s.o.parts)
        Task_Delete(so->s.o.parts);
    if (so->sectors)
        Task_Delete(so->sectors);
}
HOOK(0x4DDFF0, SectorObject_Delete);

/* Drops every object linked to s, then every link that still names s.
   Loops forever if one of its entries has no link, like the original. */
void Sector_RemoveAllObjects(Sector *s)
{
    int i;

    while (s->links.used > 0) {
        void **item = (void **)(s->links.data + s->links.slots[0] * s->links.stride);
        Obj *obj = *item;
        int found = 0;

        for (i = 0; i < gSectorLinks.used && !found; i++) {
            SectorLink *l = (SectorLink *)(gSectorLinks.data + gSectorLinks.slots[i] * gSectorLinks.stride);

            if (l->item == item) {
                pool_release(&l->sector->links, l->item);
                pool_release(&gSectorLinks, l);
                found = 1;
                obj->inSector = 0;
            }
        }
    }
    for (i = gSectorLinks.used - 1; i >= 0; i--) {
        SectorLink *l = (SectorLink *)(gSectorLinks.data + gSectorLinks.slots[i] * gSectorLinks.stride);

        if (l->sector == s)
            pool_release(&gSectorLinks, l);
    }
}
HOOK(0x4DE040, Sector_RemoveAllObjects);

/* slots 8..11: the Sector handler first, then each child sector */
void SectorObject_Slot10(SectorObject *so, void *arg)
{
    HANDLER(10, gSectorType)(so, arg);
    if (so->sectors)
        EACH_CHILD_NC(so->sectors, 10, arg);
}
HOOK(0x4DE240, SectorObject_Slot10);

void SectorObject_Slot9(SectorObject *so, void *arg)
{
    HANDLER(9, gSectorType)(so, arg);
    if (so->sectors)
        EACH_CHILD_NC(so->sectors, 9, arg);
}
HOOK(0x4DE2D0, SectorObject_Slot9);

void SectorObject_Slot8(SectorObject *so, void *arg)
{
    HANDLER(8, gSectorType)(so, arg);
    if (so->sectors)
        EACH_CHILD_NC(so->sectors, 8, arg);
}
HOOK(0x4DE360, SectorObject_Slot8);

typedef struct SectorQuery {
    int unk0;
    int count;
    int *flags;
    char unk0C[0x88];
    int unk94;
} SectorQuery;

/* each child gets a copy with its own flags; a flag the child leaves
   clear clears the caller's */
void SectorObject_Slot11(SectorObject *so, SectorQuery *q)
{
    Task *c;

    HANDLER(11, gSectorType)(so, q);
    if (!so->sectors || !(c = first_child(so->sectors)))
        return;
    do {
        SectorQuery copy = *q;
        int flags[64];
        int i;

        copy.flags = flags;
        copy.unk94 = 1;
        if (q->count > 0)
            memset(flags, 0, q->count * sizeof(int));
        HANDLER(11, c->type)(c, &copy);
        for (i = 0; i < q->count; i++)
            if (q->flags[i] && !flags[i])
                q->flags[i] = 0;
    } while ((c = next_task(c)) != NULL);
}
HOOK(0x4DE3F0, SectorObject_Slot11);

void SectorObject_Run(SectorObject *so)
{
    Task *s;

    EACH_CHILD(&so->s.o.task, 1);
    if (so->s.o.parts)
        EACH_CHILD(so->s.o.parts, 1);
    if (!so->s.o.model || !so->sectors || !(s = first_child(so->sectors)))
        return;
    do {
        EACH_CHILD(s, 1);
        if (((Obj *)s)->parts)
            EACH_CHILD(((Obj *)s)->parts, 1);
    } while ((s = next_task(s)) != NULL);
}
HOOK(0x4DE4E0, SectorObject_Run);

void Sector_Create(Sector *s, MagicArg *arg)
{
    RigidDynCubeObj_Init(arg->world, &s->o, NULL);
    s->o.pos = arg->pos;
    s->o.matrix = arg->matrix;
    s->o.model = VirModel_Find(arg->model);
    s->o.parts = MagicObjContainer_New(NULL, s);
    RigidDynCubeObj_CreateMagicObjs(s->o.parts, &s->o, &s->o);
    s->offset = arg->offset;
    s->local = arg->local;
    Pool_Init(&s->links, 4, 100, 0);
}
HOOK(0x4DE6B0, Sector_Create);

void Sector_Delete(Sector *s)
{
    Sector_RemoveAllObjects(s);
    Pool_Free(&s->links);
    Task_Delete(s->o.parts);
}
HOOK(0x4DE740, Sector_Delete);

void Sector_WorldAdd(Sector *s, void **arg)
{
    if (s->o.parts)
        EACH_CHILD(s->o.parts, World_GetEvent0(), arg);
}
HOOK(0x4DE770, Sector_WorldAdd);

void Sector_WorldRemove(Sector *s, void **arg)
{
    Sector_RemoveAllObjects(s);
    if (s->o.parts)
        EACH_CHILD(s->o.parts, World_GetEvent1(), arg);
}
HOOK(0x4DE800, Sector_WorldRemove);

/* the RigidDynCubeObj handler and the linked objects, for the object and
   each of its sectors */
void SectorObject_NearClip(SectorObject *so, void *arg)
{
    Sector *s = &so->s;

    while (s) {
        uint8_t slot = QCube_GetNearClipEvent();
        int i;

        HANDLER(slot, RigidDynCubeObj_GetType())(s, arg);
        for (i = 0; i < s->links.used; i++) {
            Task *obj = *(Task **)(s->links.data + s->links.slots[i] * s->links.stride);
            TaskFn fn = HANDLER(QCube_GetNearClipEvent(), obj->type);

            if (fn)
                fn(obj, arg);
        }
        if (s == &so->s) {
            if (!so->sectors)
                return;
            s = (Sector *)first_child(so->sectors);
        } else {
            s = (Sector *)next_task(&s->o.task);
        }
    }
}
HOOK(0x4DE8A0, SectorObject_NearClip);

void SectorObject_WorldAdd(SectorObject *so, void **arg)
{
    if (so->s.o.parts)
        EACH_CHILD(so->s.o.parts, World_GetEvent0(), arg);
    if (so->sectors)
        EACH_CHILD(so->sectors, World_GetEvent0(), arg);
}
HOOK(0x4DE970, SectorObject_WorldAdd);

void SectorObject_WorldRemove(SectorObject *so, void **arg)
{
    Sector_RemoveAllObjects(&so->s);
    if (so->sectors)
        EACH_CHILD(so->sectors, World_GetEvent1(), arg);
    if (so->s.o.parts)
        EACH_CHILD(so->s.o.parts, World_GetEvent1(), arg);
}
HOOK(0x4DEA80, SectorObject_WorldRemove);

/* arg[1] is the world */
void SectorInteriorObject_WorldAdd(Obj *o, void **arg)
{
    float scale = obj_scale(o);

    DynCube_Add(arg[1], o, &o->pos, VirModel_GetRadius(o->model) * scale, 1);
    Sector_AddObject(arg[1], o, DynCubeObj_GetPosition(o));
    EACH_CHILD(&o->task, World_GetEvent0(), arg);
    if (o->parts)
        EACH_CHILD(o->parts, World_GetEvent0(), arg);
}
HOOK(0x4DEB90, SectorInteriorObject_WorldAdd);

/* arg[0] is the world */
void SectorInteriorObject_WorldRemove(Obj *o, void **arg)
{
    Sector_RemoveObject(o);
    DynCube_Remove(arg[0], o);
    EACH_CHILD(&o->task, World_GetEvent1(), arg);
    if (o->parts)
        EACH_CHILD(o->parts, World_GetEvent1(), arg);
}
HOOK(0x4DED30, SectorInteriorObject_WorldRemove);

void Sector_Close(void)
{
    Pool_Free(&gSectorLinks);
    Pool_Free(&gVisSectors);
    TaskType_Free(gSectorObjectType);
    TaskType_Free(gSectorType);
    TaskType_Free(gSectorInteriorObjectType);
}
HOOK(0x4DEE40, Sector_Close);

uint16_t SectorObject_GetType(void)
{
    return gSectorObjectType;
}
HOOK(0x4DEE80, SectorObject_GetType);

void SectorObject_SetModel(SectorObject *so, const char *name)
{
    void **model = VirModel_Find(name);

    if (so->s.o.model != model) {
        so->s.o.model = model;
        so->built = 0;
    }
}
HOOK(0x4DEE90, SectorObject_SetModel);

typedef struct MoveArg {
    Vec3d *pos;
    Matrix *matrix;
    Sector *owner;
} MoveArg;

/* Places each child sector from its local frame, or builds the sectors and
   magic objects first when the model changed. Then tells the magic objects
   of the object and of each sector that they moved. */
void SectorObject_Update(SectorObject *so)
{
    const float *m = so->s.o.matrix.m;
    Sector *s;

    if (!so->s.o.model)
        return;
    if (!so->built) {
        if (so->s.o.parts)
            Task_Delete(so->s.o.parts);
        if (so->sectors)
            Task_Delete(so->sectors);
        so->s.o.parts = MagicObjContainer_New(NULL, so);
        so->sectors = MagicObjContainer_New(NULL, so);
        Sector_CreateMagicObjs(&so->s, so);
        SectorObject_CalcBounds(so, so->center, &so->radius);
        so->built = 1;
        return;
    }
    if (so->sectors && (s = (Sector *)first_child(so->sectors)) != NULL) {
        do {
            const float *l = s->local.m;
            float *r = s->o.matrix.m;
            long double x;
            double y, z;

            /* the original also re-orthonormalizes a copy once unk24
               passes 1000, but never uses it */
            r[0] = m[3] * l[1] + l[2] * m[6] + m[0] * l[0];
            r[1] = m[4] * l[1] + m[7] * l[2] + l[0] * m[1];
            r[2] = m[5] * l[1] + m[2] * l[0] + m[8] * l[2];
            r[3] = l[5] * m[6] + m[3] * l[4] + l[3] * m[0];
            r[4] = m[4] * l[4] + l[3] * m[1] + l[5] * m[7];
            r[5] = l[5] * m[8] + l[4] * m[5] + l[3] * m[2];
            r[6] = m[3] * l[7] + l[8] * m[6] + m[0] * l[6];
            r[7] = m[4] * l[7] + m[7] * l[8] + l[6] * m[1];
            r[8] = m[5] * l[7] + m[2] * l[6] + m[8] * l[8];
            s->o.matrix.unk24 = s->local.unk24 + so->s.o.matrix.unk24 + 1;
            x = m[2] * (long double)s->offset.z + m[0] * (long double)s->offset.x + m[1] * (long double)s->offset.y;
            y = m[4] * (long double)s->offset.y + m[3] * (long double)s->offset.x + m[5] * (long double)s->offset.z;
            z = m[7] * (long double)s->offset.y + m[6] * (long double)s->offset.x + m[8] * (long double)s->offset.z;
            s->o.pos.x = x + so->s.o.pos.x;
            s->o.pos.y = y + so->s.o.pos.y;
            s->o.pos.z = z + so->s.o.pos.z;
        } while ((s = (Sector *)next_task(&s->o.task)) != NULL);
    }
    s = &so->s;
    do {
        MoveArg arg;

        arg.pos = &s->o.pos;
        arg.matrix = &s->o.matrix;
        arg.owner = s;
        EACH_CHILD(s->o.parts, MagicObj_GetMoveEvent(), &arg);
        if (s == &so->s) {
            if (!so->sectors)
                return;
            s = (Sector *)first_child(so->sectors);
        } else {
            s = (Sector *)next_task(&s->o.task);
        }
    } while (s);
}
HOOK(0x4DEEC0, SectorObject_Update);

static inline char *model_info(void **model)
{
    char *lod = *(char **)((char *)model[0] + 0xC);

    return *(char **)(lod + 0xC);
}

/* the bounding sphere (center, radius) of a model's first LOD */
static inline const float *model_bounds(void **model)
{
    return (const float *)(model_info(model) + 0x50);
}

#define SPHERE(p, i) ((float *)((p)->data + (p)->slots[i] * (p)->stride))

/* Merges the bounding spheres of all sectors and their magic objects into
   one, always joining the two closest. */
void SectorObject_CalcBounds(SectorObject *so, float *center, float *radius)
{
    Pool sp;
    Task *t;
    float *a, *b, *first;
    int n;

    if (!so->sectors || !(t = first_child(so->sectors))) {
        const float *info;

        if (!so->s.o.model)
            return;
        info = model_bounds(so->s.o.model);
        center[0] = info[0];
        center[1] = info[1];
        center[2] = info[2];
        *radius = info[3];
        return;
    }
    Pool_Init(&sp, 0x10, 1000, 0);
    do {
        Sector *s = (Sector *)t;
        const float *info = model_bounds(s->o.model);
        float *item = SPHERE(&sp, sp.used);

        *(int *)((char *)item + sp.index_offset) = sp.used;
        sp.used++;
        item[0] = info[0];
        item[1] = info[1];
        item[2] = info[2];
        item[3] = info[3];
        if (s->o.parts)
            SectorObject_AddChildSpheres(&sp, s->o.parts);
    } while ((t = next_task(t)) != NULL);
    a = b = center;
    for (n = sp.used; n > 1; n = sp.used) {
        float best = 0x1.fffffep127f;
        int i, j;

        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                float *p, *q;
                long double dx, dy, dz, d;

                if (i == j)
                    continue;
                p = SPHERE(&sp, i);
                q = SPHERE(&sp, j);
                dz = q[2] - (long double)p[2];
                dy = q[1] - (long double)p[1];
                dx = q[0] - (long double)p[0];
                d = x87_sqrt(dx * dx + dy * dy + dz * dz);
                if (!(d >= best)) {
                    best = d;
                    a = p;
                    b = q;
                }
            }
        }
        if (!(best < kZero || best > kZero)) {
            a[3] = !(b[3] >= a[3]) ? a[3] : b[3];
            pool_release(&sp, b);
        } else if (!(best + (long double)b[3] >= a[3])) {
            pool_release(&sp, b);
        } else if (!(best + (long double)a[3] >= b[3])) {
            pool_release(&sp, a);
        } else {
            float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
            float p1x, p1y, p1z, p2x, p2y, cx, cy, cz;
            long double len, k, z;

            len = x87_sqrt(dx * dx + dy * dy + dz * dz);
            k = a[3] / len;
            p1x = dx * k;
            p1y = dy * k;
            z = dz * k;
            p1x = -p1x;
            p1y = -p1y;
            p1x = p1x + a[0];
            p1y = p1y + a[1];
            p1z = -z + a[2];
            k = b[3] / len;
            p2x = dx * k;
            p2y = dy * k;
            z = dz * k;
            p2x = p2x + b[0];
            p2y = p2y + b[1];
            z = z + b[2];
            cx = (p2x - p1x) * kHalf + p1x;
            a[0] = cx;
            cy = (p2y - p1y) * kHalf + p1y;
            a[1] = cy;
            cz = (z - p1z) * kHalf + p1z;
            a[2] = cz;
            {
                long double ez = z - cz, ey = p2y - (long double)cy, ex = p2x - (long double)cx;

                a[3] = x87_sqrt(ex * ex + ey * ey + ez * ez);
            }
            pool_release(&sp, b);
        }
    }
    first = SPHERE(&sp, 0);
    center[0] = first[0];
    center[1] = first[1];
    center[2] = first[2];
    *radius = first[3];
    pool_release(&sp, first);
    Pool_Free(&sp);
}
HOOK(0x4DF500, SectorObject_CalcBounds);

/* an attachment point of a model LOD; its name is a DefineMagicObj name */
typedef struct ModelAttach {
    char name[16];
    float pos[3];       /* 0x10 */
    float m[9];         /* 0x1C */
} ModelAttach;

typedef struct MagicObjDef {
    char name[16];
    char model[16];
    uint16_t type;
    uint16_t unk22;
    void *data;
} MagicObjDef;

extern int gMagicObjDefCount;
extern MagicObjDef gMagicObjDefs[];

static inline int is_zero3(float a, float b, float c)
{
    return !(a < kZero || a > kZero) && !(b < kZero || b > kZero) && !(c < kZero || c > kZero);
}

static inline void scale3(float *v, long double inv)
{
    v[0] = v[0] * inv;
    v[1] = v[1] * inv;
    v[2] = v[2] * inv;
}

/* Creates a magic object for each attachment point of the model: a child
   sector when the attached model has sectors itself, else the type given
   by DefineMagicObj. */
void Sector_CreateMagicObjs(Sector *s, void *owner)
{
    int count, i;

    if (!s->o.model)
        return;
    count = ModelLod_GetAttachCount(s->o.model[0]);
    for (i = 0; i < count; i++) {
        const float *M = s->o.matrix.m;
        ModelAttach *att = (ModelAttach *)ModelLod_GetAttach(s->o.model[0], i);
        const float *a = att->m;
        MagicObjDef *def = NULL;
        MagicArg arg, arg2;
        double rx, ry, rz;
        float c0, c1, e[3], f[3], W[3], E[3], F[3], *R, *A;
        long double c2;
        void **model;
        Task *t;
        TaskFn fn;
        int j;

        arg.offset.x = att->pos[0];
        arg.offset.y = att->pos[1];
        arg.offset.z = att->pos[2];
        rx = M[1] * att->pos[1] + M[2] * att->pos[2] + M[0] * att->pos[0];
        ry = M[4] * att->pos[1] + M[5] * att->pos[2] + M[3] * att->pos[0];
        rz = M[7] * att->pos[1] + M[8] * att->pos[2] + M[6] * att->pos[0];
        arg.pos.x = rx + s->o.pos.x;
        arg.pos.y = ry + s->o.pos.y;
        arg.pos.z = rz + s->o.pos.z;

        /* local frame: the first column of the attachment matrix and two
           cross products, normalized */
        c0 = a[7] * a[3] - a[4] * a[6];
        c1 = -(a[7] * a[0] - a[1] * a[6]);
        c2 = a[4] * (long double)a[0] - a[1] * (long double)a[3];
        W[0] = a[0];
        W[1] = a[3];
        W[2] = a[6];
        if (!is_zero3(a[0], a[3], a[6]))
            scale3(W, kOneD / x87_sqrt(a[0] * a[0] + a[3] * a[3] + a[6] * a[6]));
        e[0] = c1 * a[6] - c2 * a[3];
        e[1] = -(c0 * a[6] - c2 * a[0]);
        e[2] = c0 * a[3] - c1 * a[0];
        f[0] = e[2] * W[1] - e[1] * W[2];
        f[1] = -(e[2] * W[0] - e[0] * W[2]);
        f[2] = e[1] * W[0] - e[0] * W[1];
        E[0] = e[0];
        E[1] = e[1];
        E[2] = e[2];
        if (!is_zero3(e[0], e[1], e[2]))
            scale3(E, kOneD / x87_sqrt(e[2] * e[2] + e[1] * e[1] + e[0] * e[0]));
        F[0] = f[0];
        F[1] = f[1];
        F[2] = f[2];
        if (!is_zero3(f[0], f[1], f[2]))
            scale3(F, kOneD / x87_sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]));
        R = arg.local.m;
        R[0] = W[0];
        R[1] = W[1];
        R[2] = W[2];
        R[3] = E[0];
        R[4] = E[1];
        R[5] = E[2];
        R[6] = F[0];
        R[7] = F[1];
        R[8] = F[2];
        arg.local.unk24 = 0;

        /* world frame: the object's matrix times the local one, then
           re-orthonormalized (the original also does that a second time
           into a copy it never reads once unk24 passes 1000) */
        {
            float P[9], X[3], C[3], D[3], c[3], d[3];

            P[0] = R[3] * M[1] + R[6] * M[2] + R[0] * M[0];
            P[1] = R[4] * M[1] + R[7] * M[2] + R[1] * M[0];
            P[2] = R[5] * M[1] + R[8] * M[2] + R[2] * M[0];
            P[3] = R[3] * M[4] + R[6] * M[5] + R[0] * M[3];
            P[4] = R[4] * M[4] + R[7] * M[5] + R[1] * M[3];
            P[5] = R[5] * M[4] + R[8] * M[5] + R[2] * M[3];
            P[6] = R[3] * M[7] + R[6] * M[8] + R[0] * M[6];
            P[7] = R[4] * M[7] + R[7] * M[8] + R[1] * M[6];
            P[8] = R[5] * M[7] + R[8] * M[8] + R[2] * M[6];
            X[0] = P[0];
            X[1] = P[1];
            X[2] = P[2];
            if (!is_zero3(P[0], P[1], P[2]))
                scale3(X, kOneD / x87_sqrt(P[0] * P[0] + P[1] * P[1] + P[2] * P[2]));
            c[0] = P[2] * P[7] - P[1] * P[8];
            c[1] = -(P[6] * P[2] - P[0] * P[8]);
            c[2] = P[6] * P[1] - P[0] * P[7];
            d[0] = c[2] * X[1] - c[1] * X[2];
            d[1] = -(c[2] * X[0] - c[0] * X[2]);
            d[2] = c[1] * X[0] - c[0] * X[1];
            C[0] = c[0];
            C[1] = c[1];
            C[2] = c[2];
            if (!is_zero3(c[0], c[1], c[2]))
                scale3(C, kOneD / x87_sqrt(c[2] * c[2] + c[1] * c[1] + c[0] * c[0]));
            D[0] = d[0];
            D[1] = d[1];
            D[2] = d[2];
            if (!is_zero3(d[0], d[1], d[2]))
                scale3(D, kOneD / x87_sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
            A = arg.matrix.m;
            A[0] = X[0];
            A[1] = X[1];
            A[2] = X[2];
            A[3] = C[0];
            A[4] = C[1];
            A[5] = C[2];
            A[6] = D[0];
            A[7] = D[1];
            A[8] = D[2];
            arg.matrix.unk24 = 0;
        }

        for (j = 0; j < gMagicObjDefCount && !def; j++)
            if (strcmp(att->name, gMagicObjDefs[j].name) == 0)
                def = &gMagicObjDefs[j];
        if (!def) {
            Warning_Show("Magic object type \"%s\" not defined (Created from model \"%s\")", att->name,
                         (char *)s->o.model + 0x14);
            continue;
        }
        arg.data = def->data;
        arg.owner = owner;
        arg.model = def->model;
        arg.world = s->o.world;
        model = VirModel_Find(def->model);
        if (!model) {
            Warning_Show("Magic object model \"%s\" not found", def->model);
            continue;
        }
        if (*(int *)(model_info(model) + 0x98))
            t = Task_New(((SectorObject *)s)->sectors, gSectorType, &arg);
        else
            t = Task_New(s->o.parts, def->type, &arg);
        /* only these four fields are set, like the original */
        arg2.data = def->data;
        arg2.model = def->model;
        arg2.world = s->o.world;
        arg2.owner = owner;
        fn = HANDLER(RigidDynCubeObj_GetEvent3(), t->type);
        if (fn)
            fn(t, &arg2);
    }
}
HOOK(0x4DFA50, Sector_CreateMagicObjs);

void SectorObject_SetMatrix(SectorObject *so, const Matrix *m)
{
    so->s.o.matrix = *m;
}
HOOK(0x4E0810, SectorObject_SetMatrix);

uint16_t SectorInteriorObject_GetType(void)
{
    return gSectorInteriorObjectType;
}
HOOK(0x4E0830, SectorInteriorObject_GetType);
