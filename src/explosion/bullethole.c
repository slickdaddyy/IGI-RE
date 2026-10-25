#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "explosion/explosion.h"
#include "ammotype/ammotypevars.h"
#include "explosion/explosionvars.h"

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x490370, unsigned int, Timer_GetMs, (void));
ORIG(0x49A430, void, PolyList_SetPos, (void *mesh, DrawMsg *msg));
ORIG(0x49A450, void *, PolyList_New, (void *world, void *owner, const Vec3d *pos, const Matrix *m, HoleTri *tris,
                                    int count, int a, int b, int c, int d, int e));
ORIG(0x49A510, void, PolyList_SetUnusedMode, (void *mesh, int a));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x4B4010, void, Matrix_SetDirection, (Matrix *m, const Vec3f *dir));
ORIG(0x4CE410, char, Coll_LineOfSight, (void *world, const Vec3d *from, const Vec3d *to, int unk, int unk2,
                                        int flags, int trees, float *vis));
ORIG(0x4E0810, void, SectorObject_SetMatrix, (void *mesh, Matrix *m));

extern struct Terrain *gCurrentWorld;
extern const Vec3f gWhite[];

static const double kOne = 1.0;
static const float kZero = 0.0f;
static const float kReach = 1.5f;
static const float kFacing = 0.9f;
static const float kHoleZ = 0x1.89374cp+3f;     /* 12.288 */
static const float kHoleDepth = 0x1.89374cp+4f; /* 24.576 */

void BulletHole_Place(const Vec3d *pos, const Matrix *m, BulletHole *bh, Hole *h, HoleHit *hit, int state,
                      void *texture, float size);

static void *pool_take(Pool *p)
{
    char *item = p->data + p->free[p->used] * p->stride;

    *(int *)(item + p->size) = p->used;
    p->used++;
    return item;
}

static void pool_release(Pool *p, void *item)
{
    int index = *(int *)((char *)item + p->size);
    char *last;
    int last_index, tmp;

    p->used--;
    if (index == p->used)
        return;
    last = p->data + p->free[p->used] * p->stride;
    last_index = *(int *)(last + p->size);
    tmp = p->free[index];
    p->free[index] = p->free[last_index];
    p->free[last_index] = tmp;
    *(int *)(last + p->size) = index;
}

static int nonzero(const Vec3f *v)
{
    return v->x != kZero || v->y != kZero || v->z != kZero;
}

static void scale(Vec3f *v, long double len2)
{
    long double s = kOne / x87_sqrt(len2);

    v->x = (float)(v->x * s);
    v->y = (float)(v->y * s);
    v->z = (float)(v->z * s);
}

/* the summation orders of the four inlined copies of the "same hole" test */
enum { RIGID_NEW, RIGID_OLD, SPLINE_NEW, SPLINE_OLD };

/* a hole close to the hit and facing the same way is not added again */
static int is_same(Hole *h, HoleHit *hit, int kind)
{
    Vec3f *n = &h->normal, *hn = &hit->normal;
    long double dx, dy, dz, dot;
    float r;

    if (h->state == -1)
        return 0;
    dz = h->pos.z - hit->point.z;
    dy = h->pos.y - hit->point.y;
    dx = h->pos.x - hit->point.x;
    r = h->size * kReach;
    if (!((long double)r * r > (dx * dx + dy * dy) + dz * dz))
        return 0;
    if (nonzero(hn)) {
        if (kind == RIGID_NEW || kind == RIGID_OLD)
            scale(hn, (hn->y * hn->y + hn->z * hn->z) + hn->x * hn->x);
        else
            scale(hn, (hn->x * hn->x + hn->y * hn->y) + hn->z * hn->z);
    }
    if (nonzero(n)) {
        if (kind == RIGID_NEW || kind == RIGID_OLD)
            scale(n, (n->x * n->x + n->y * n->y) + n->z * n->z);
        else if (kind == SPLINE_NEW)
            scale(n, (n->y * n->y + n->z * n->z) + n->x * n->x);
        else
            scale(n, (n->z * n->z + n->x * n->x) + n->y * n->y);
    }
    if (kind == RIGID_NEW)
        dot = (hn->z * n->z + hn->y * n->y) + hn->x * n->x;
    else if (kind == RIGID_OLD || kind == SPLINE_NEW)
        dot = (hn->x * n->x + hn->z * n->z) + n->y * hn->y;
    else
        dot = (hn->z * n->z + hn->y * n->y) + n->x * hn->x;
    return dot > kFacing;
}

/* adds a hole to obj's BulletHole child, making it when needed */
static void add_hole(Task *obj, HoleMsg *msg, const Vec3d *pos, const Matrix *m, int spline)
{
    HoleHit *hit = msg->hit;
    BulletHole *bh;
    Hole *h;
    void **item;
    int i;

    for (i = 0; i < gHolePool.used; i++) {
        bh = *(BulletHole **)(gHolePool.data + gHolePool.free[i] * gHolePool.stride);
        if (bh->obj != obj)
            continue;
        for (i = bh->last; i >= 0; i--)
            if (is_same(&bh->holes[i], hit, spline ? SPLINE_NEW : RIGID_NEW))
                return;
        for (i = bh->count - 1; i > bh->last; i--)
            if (is_same(&bh->holes[i], hit, spline ? SPLINE_OLD : RIGID_OLD))
                return;
        if (bh->count == 200) {
            HolePoly *p, *next;

            i = bh->next;
            bh->last = i;
            h = &bh->holes[i];
            bh->next = i + 1;
            if (i + 1 >= 200)
                bh->next = i + 1 - 200;
            for (p = h->polys; p; p = next) {
                next = p->next;
                pool_release(&gHolePolyPool, p);
            }
        } else {
            i = bh->count;
            bh->last = i;
            bh->count = i + 1;
            h = &bh->holes[i];
        }
        BulletHole_Place(pos, m, bh, h, hit, 0, msg->texture, msg->size);
        bh->time = Timer_GetMs();
        return;
    }
    if (gHolePool.used == gHolePool.count) {
        BulletHole *oldest = NULL;
        unsigned int best = 0xFFFFFFFF;

        for (i = 0; i < gHolePool.used; i++) {
            bh = *(BulletHole **)(gHolePool.data + gHolePool.free[i] * gHolePool.stride);
            if (bh->time <= best) {
                oldest = bh;
                best = bh->time;
            }
        }
        Task_Delete(oldest);
    }
    bh = (BulletHole *)Task_New(obj, gBulletHoleType, NULL);
    item = pool_take(&gHolePool);
    bh->poolItem = item;
    *item = bh;
    bh->obj = obj;
    bh->mesh = PolyList_New(Game_GetWorld(), bh, pos, m, bh->tris, 0, 0, 0, 1, 0, 0);
    PolyList_SetUnusedMode(bh->mesh, 2);
    bh->count = 1;
    bh->last = 0;
    bh->next = 0;
    BulletHole_Place(pos, m, bh, &bh->holes[0], hit, 0, msg->texture, msg->size);
    bh->time = Timer_GetMs();
}

/* parent moved event: rebuilds the triangles when holes changed, then draws them */
void BulletHole_Draw(BulletHole *bh, DrawMsg *msg)
{
    int *mesh = bh->mesh;
    int i;

    if (bh->dirty) {
        mesh[0x27] = 0;
        for (i = 0; i < bh->count; i++) {
            Hole *h = &bh->holes[i];
            HoleTri *t;

            if (h->state == -1)
                continue;
            if (!h->polys) {
                if (mesh[0x27] + 1 >= 500)
                    continue;
                t = &bh->tris[mesh[0x27]];
                t[0].normal = h->normal;
                t[1].normal = h->normal;
                t[0].color[0] = t[0].color[1] = t[0].color[2] = gWhite;
                t[1].color[0] = t[1].color[1] = t[1].color[2] = gWhite;
                t[0].v[0] = &h->corner[0];
                t[0].v[1] = &h->corner[1];
                t[0].v[2] = &h->corner[2];
                t[1].v[0] = &h->corner[1];
                t[1].v[1] = &h->corner[3];
                t[1].v[2] = &h->corner[2];
                t[0].uv[0] = 0.0f;
                t[0].uv[1] = 0.0f;
                t[0].uv[2] = 0.0f;
                t[0].uv[3] = 1.0f;
                t[0].uv[4] = 1.0f;
                t[0].uv[5] = 0.0f;
                t[1].uv[0] = 0.0f;
                t[1].uv[1] = 1.0f;
                t[1].uv[2] = 1.0f;
                t[1].uv[3] = 1.0f;
                t[1].uv[4] = 1.0f;
                t[1].uv[5] = 0.0f;
                t[0].texture = h->texture;
                t[1].texture = h->texture;
                mesh[0x27] += 2;
            } else {
                HolePoly *p;

                for (p = h->polys; p; p = p->next) {
                    int k;

                    if (mesh[0x27] >= 500)
                        continue;
                    t = &bh->tris[mesh[0x27]];
                    t->normal = h->normal;
                    t->v[0] = &p->v[0];
                    t->v[1] = &p->v[1];
                    t->v[2] = &p->v[2];
                    t->color[0] = t->color[1] = t->color[2] = gWhite;
                    for (k = 0; k < 6; k++)
                        t->uv[k] = p->uv[k];
                    t->texture = h->texture;
                    mesh[0x27]++;
                }
            }
        }
        bh->dirty = 0;
    }
    PolyList_SetPos(mesh, msg);
    SectorObject_SetMatrix(mesh, &msg->matrix);
    HANDLER(4, ((Task *)mesh)->type)(mesh, NULL);
}
HOOK(0x47D0B0, BulletHole_Draw);

void BulletHole_Delete(BulletHole *bh)
{
    pool_release(&gHolePool, bh->poolItem);
}
HOOK(0x47D320, BulletHole_Delete);

/* bullet hole event of a RigidDynCubeObj */
void BulletHole_AddRigid(RigidDynCubeObj *obj, HoleMsg *msg)
{
    add_hole(&obj->task, msg, &obj->pos, &obj->matrix, 0);
}
HOOK(0x47D380, BulletHole_AddRigid);

/* Sets up hole h at the hit: a square of 2 * size around the hit point in
   the plane of the normal, in obj space. The hole is dropped (state -1)
   unless lines through all four corners hit something. */
void BulletHole_Place(const Vec3d *pos, const Matrix *m, BulletHole *bh, Hole *h, HoleHit *hit, int state,
                      void *texture, float size)
{
    Vec3f *c = h->corner;
    Matrix d;
    Vec3f dir;
    Vec3d w[4], to[4];
    double v[3];
    long double s, dz, w0x;
    float x, y, z, t[3];
    int i, clear;

    h->state = state;
    h->texture = texture;
    h->polys = NULL;
    h->size = size;
    h->pos = hit->point;
    h->normal = hit->normal;
    if (nonzero(&h->normal))
        scale(&h->normal, (h->normal.x * h->normal.x + h->normal.y * h->normal.y) + h->normal.z * h->normal.z);
    c[0].x = size;
    c[0].y = size;
    c[0].z = kHoleZ;
    c[1].x = -size;
    c[1].y = size;
    c[1].z = kHoleZ;
    c[2].x = size;
    c[2].y = -size;
    c[2].z = kHoleZ;
    c[3].x = -size;
    c[3].y = -size;
    c[3].z = kHoleZ;
    Matrix_SetDirection(&d, &hit->normal);
    for (i = 0; i < 4; i++) {
        x = c[i].x;
        y = c[i].y;
        z = c[i].z;
        if (i == 2) {
            t[0] = (d.m[6] * z + d.m[0] * x) + d.m[3] * y;
            t[1] = (d.m[7] * z + d.m[1] * x) + d.m[4] * y;
            t[2] = (d.m[8] * z + d.m[2] * x) + d.m[5] * y;
        } else {
            t[0] = (d.m[3] * y + d.m[6] * z) + d.m[0] * x;
            t[1] = (d.m[4] * y + d.m[7] * z) + d.m[1] * x;
            t[2] = (d.m[5] * y + d.m[8] * z) + d.m[2] * x;
        }
        c[i].x = t[0];
        c[i].y = t[1];
        c[i].z = t[2];
    }
    for (i = 0; i < 4; i++) {
        c[i].x = c[i].x + hit->point.x;
        c[i].y = c[i].y + hit->point.y;
        c[i].z = c[i].z + hit->point.z;
    }
    bh->dirty = 1;

    dir = hit->dir;
    s = kHoleDepth / x87_sqrt((dir.x * dir.x + dir.y * dir.y) + dir.z * dir.z);
    dir.x = (float)(dir.x * s);
    dir.y = (float)(dir.y * s);
    dz = dir.z * s;

    /* the corners in world space, lines from them dir deep */
    for (i = 0; i < 4; i++) {
        x = c[i].x;
        y = c[i].y;
        z = c[i].z;
        v[0] = (m->m[1] * y + m->m[2] * z) + m->m[0] * x;
        if (i == 0)
            v[1] = (m->m[3] * x + m->m[4] * y) + m->m[5] * z;
        else
            v[1] = (m->m[4] * y + m->m[5] * z) + m->m[3] * x;
        if (i == 3)
            v[2] = (m->m[8] * z + m->m[6] * x) + m->m[7] * y;
        else
            v[2] = (m->m[7] * y + m->m[8] * z) + m->m[6] * x;
        if (i == 0)
            w0x = v[0] + pos->x;
        w[i].x = v[0] + pos->x;
        w[i].y = v[1] + pos->y;
        w[i].z = v[2] + pos->z;
    }
    /* the first x is used before it is rounded */
    to[0].x = w0x - dir.x;
    for (i = 1; i < 4; i++)
        to[i].x = w[i].x - dir.x;
    for (i = 0; i < 4; i++) {
        to[i].y = w[i].y - dir.y;
        to[i].z = w[i].z - dz;
    }
    clear = Coll_LineOfSight(gCurrentWorld, &w[0], &to[0], 0xFF, 0, -1, -1, NULL) == 0;
    clear &= Coll_LineOfSight(gCurrentWorld, &w[1], &to[1], 0xFF, 0, -1, -1, NULL) == 0;
    clear &= Coll_LineOfSight(gCurrentWorld, &w[2], &to[2], 0xFF, 0, -1, -1, NULL) == 0;
    if (!(clear & (Coll_LineOfSight(gCurrentWorld, &w[3], &to[3], 0xFF, 0, -1, -1, NULL) == 0)))
        h->state = -1;
}
HOOK(0x47D920, BulletHole_Place);

/* bullet hole event of a SplineObjSegment, which has no matrix */
void BulletHole_AddSpline(Task *obj, HoleMsg *msg)
{
    Matrix id = {{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};

    add_hole(obj, msg, (Vec3d *)(obj + 1), &id, 1);
}
HOOK(0x47E0C0, BulletHole_AddSpline);

void BulletHole_Close(void)
{
    Pool_Free(&gHolePolyPool);
    Pool_Free(&gHolePool);
    TaskType_Free(gBulletHoleType);
    TaskEvent_Free(gBulletHoleEvent);
    gBulletHoleEvent = 0xFF;
}
HOOK(0x47E6B0, BulletHole_Close);

uint8_t BulletHole_GetEvent(void)
{
    return gBulletHoleEvent;
}
HOOK(0x47E6F0, BulletHole_GetEvent);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 5408F8, 4) __typeof__(gWhite[0]) store_5408F8[1] INIT({{.x = 1.0f, .y = 1.0f, .z = 1.0f}});
ALIAS(gWhite, store_5408F8, 0);
DATA(540904) uint8_t gBulletHoleEvent INIT(255);
PAD(igd, 540904, 540905, 0x3);
BSS(5C1108) Pool gHolePool;
BSS(5C1108) Pool gHolePolyPool;
