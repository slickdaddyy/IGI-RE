/* Sectors: the rooms of buildings, the objects inside them, their
   lightmap references, and drawing only the rooms seen through portals. */
#pragma GCC optimize("excess-precision=standard")

#include <float.h>

#include "sector/sector.h"
#include "hook.h"
#include "litmap/litmaptypes.h"
#include "bonemesh/bonemeshtypes.h"
#include "transctx/transctxtypes.h"
#include "glowmesh/glowmeshtypes.h"

FN_EXTERN(SectorInteriorObject_WorldAdd);
FN_EXTERN(SectorInteriorObject_WorldRemove);
FN_EXTERN(SectorObject_Create);
FN_EXTERN(SectorObject_Delete);
FN_EXTERN(SectorObject_NearClip);
FN_EXTERN(SectorObject_Run);
FN_EXTERN(SectorObject_Slot10);
FN_EXTERN(SectorObject_Slot11);
FN_EXTERN(SectorObject_Slot8);
FN_EXTERN(SectorObject_Slot9);
FN_EXTERN(SectorObject_WorldAdd);
FN_EXTERN(SectorObject_WorldRemove);
FN_EXTERN(Sector_Create);
FN_EXTERN(Sector_Delete);
FN_EXTERN(Sector_WorldAdd);
FN_EXTERN(Sector_WorldRemove);
FN_EXTERN(Task_Nothing);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x497520, void, Scene_SetBounds, (void *scene, const float *min, const float *max));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B2C70, int, Vec3_RayPlane, (float *out, const float *p, const float *dir, const float *normal));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4CE090, void, Coll_SendEvent, (int world, uint8_t slot, void *arg, const Vec3d *p, unsigned int mask, int tree));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CFC30, int, Mesh3D_GetDrawRigidMethod, (void));
ORIG(0x4CFC70, uint8_t, Mesh3D_GetEvent1, (void));
ORIG(0x4CFC80, uint8_t, Mesh3D_GetEvent2, (void));
ORIG(0x4CFC90, uint8_t, Mesh3D_GetEvent3, (void));
ORIG(0x4CFCB0, int, ModelObj_GetLOD, (void *obj));
ORIG(0x4D0950, long double, VirModel_GetRadius, (VirModel *model));
ORIG(0x4D14C0, uint8_t, QCube_GetEnclosingEvent, (void));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4DA180, void, Haze_GetVisible, (int **layers, int *count));
ORIG(0x4DEE80, uint16_t, SectorObject_GetType, (void));
ORIG(0x4E55B0, uint8_t, CellMask_GetDrawEvent, (void));

/* handlers of the next range */
#define Sector_Create (FN(0x4DE6B0, Sector_Create))
#define Sector_Delete (FN(0x4DE740, Sector_Delete))
#define Sector_WorldAdd (FN(0x4DE770, Sector_WorldAdd))
#define Sector_WorldRemove (FN(0x4DE800, Sector_WorldRemove))
#define SectorObject_Create (FN(0x4DDF60, SectorObject_Create))
#define SectorObject_Delete (FN(0x4DDFF0, SectorObject_Delete))
#define SectorObject_Handler10 (FN(0x4DE240, SectorObject_Slot10))
#define SectorObject_Handler9 (FN(0x4DE2D0, SectorObject_Slot9))
#define SectorObject_Handler8 (FN(0x4DE360, SectorObject_Slot8))
#define SectorObject_Handler11 (FN(0x4DE3F0, SectorObject_Slot11))
#define SectorObject_Run (FN(0x4DE4E0, SectorObject_Run))
#define SectorObject_NearZHandler (FN(0x4DE8A0, SectorObject_NearClip))
#define SectorObject_WorldAdd (FN(0x4DE970, SectorObject_WorldAdd))
#define SectorObject_WorldRemove (FN(0x4DEA80, SectorObject_WorldRemove))
#define SectorInteriorObject_WorldAdd (FN(0x4DEB90, SectorInteriorObject_WorldAdd))
#define SectorInteriorObject_WorldRemove (FN(0x4DED30, SectorInteriorObject_WorldRemove))
#define sub_4D3710 (FN(0x4D3710, Task_Nothing))

/* argument of the locate event: which sector holds pos */
typedef struct LocateArg {
    Vec3d *pos;
    int raise;          /* start 409.6 above pos */
    Sector *sector;
    SectorObject *owner;
    Vec3d floor;        /* 0x10 */
    float radius;
    int unk2C;
} LocateArg;

/* line query sent to slot 9 of each sector */
typedef struct LineHit {
    RigidDynCubeObj *obj;
    int unk4;
    Vec3d point;
    char unk20[0x38];
    int found;          /* 0x58 */
    char unk5C[0x14];
} LineHit;

typedef struct LineQuery {
    Vec3d *from;
    Vec3d *to;
    int unk8;
    uint8_t unkC;
    int unk10;
    int unk14;
    int unk18;
    int tag;
    LineHit *hit;
} LineQuery;

/* a sector to draw this frame, with the screen rectangle it is seen through */
typedef struct VisSector {
    float min[2];
    float max[2];
    int unk10;
    int lod;
    Sector *sector;
    int unk1C;
} VisSector;

_Static_assert(sizeof(LocateArg) == 0x30, "LocateArg");
_Static_assert(sizeof(LineQuery) == 0x24, "LineQuery");
_Static_assert(sizeof(LineHit) == 0x70, "LineHit");

extern void (*gRenderFuncs[])(void *);
extern int gQCubeInside;
extern SectorObject *gQCubeEnclosing;
extern Sector *gViewSector;
extern int gSectorFogCount;
extern Pool gSectorLinks;
extern int *gSectorFog;
extern int gSectorLightCount;
extern Pool gVisSectors;
extern int gPortalClipDepth;
extern SectorObject *gCurrentSectorObj;
extern int gVisFrame;
extern int *gSectorLights;
extern int gQCubeDrawOutside;
extern uint16_t gSectorType;
extern uint16_t gSectorObjectType;
extern uint16_t gSectorInteriorObjectType;
extern float gQCubeOutsideMin[];
extern float gQCubeOutsideMax[];
extern TransCtx gView;
extern char gScene[];
extern SceneArea gSceneArea;

extern const double kOneD;
extern const float kHalf;
extern const float kRaise;       /* 409.6 */
extern const float kNearZ;       /* 0.04096 */
extern const float kPortalDist2;

void Portal_ClipTriangle(ClipVertex a, ClipVertex b, ClipVertex c, float **rect, float nearZ);
void Sector_FloodPortals(Sector *s, float *min, float *max, int lod);

/* x87 "equal" as tested with C3: true when unordered */
static inline int feq0(long double x)
{
    return !(x < 0 || x > 0);
}

/* screen outcode: 1 left, 2 top, 4 right, 8 bottom, 0x10 in front of the near plane */
static int outcode(float sx, float sy)
{
    return (sy > gSceneArea.maxY) << 3 | (sx > gSceneArea.maxX) << 2 | (sy < gSceneArea.minY) << 1 |
           (sx < gSceneArea.minX);
}

/* the other vertices of the triangle: o is the one outside, p and q follow it */
static void pick_one(ClipVertex *v[3], int ma, int mb, ClipVertex **o, ClipVertex **p, ClipVertex **q)
{
    int i = ma ? 0 : mb ? 1 : 2;

    *o = v[i];
    *p = v[(i + 1) % 3];
    *q = v[(i + 2) % 3];
}

/* the vertex inside and the two outside */
static void pick_two(ClipVertex *v[3], int ma, int mb, ClipVertex **in, ClipVertex **o1, ClipVertex **o2)
{
    int i = !ma ? 0 : !mb ? 1 : 2;

    *in = v[i];
    *o1 = v[(i + 1) % 3];
    *o2 = v[(i + 2) % 3];
}

/* Clips a view space triangle against the near plane and the screen and
   grows the screen rectangle {rect[0] min, rect[1] max} by what is left. Vertices cut off by a
   plane move onto it; a triangle with one vertex outside becomes two. */
void Portal_ClipTriangle(ClipVertex a, ClipVertex b, ClipVertex c, float **rect, float nearZ)
{
    ClipVertex *v[3] = {&a, &b, &c};
    ClipVertex *in, *o, *p, *q, tmp;
    long double t, d, z, r, sx, sy;
    float tf;
    int any, all, ma, mb, mc, i;

    gPortalClipDepth++;
    any = a.code | b.code | c.code;
    all = a.code & b.code & c.code;
    if (all)
        goto done;

    if (any & 0x10) {
        ma = a.code & 0x10;
        mb = b.code & 0x10;
        mc = c.code & 0x10;
        if (ma + mb + mc == 0x20) {
            pick_two(v, ma, mb, &in, &o, &p);
            t = (nearZ - in->z) / (o->z - in->z);
            o->x = (o->x - in->x) * t + in->x;
            o->y = (o->y - in->y) * t + in->y;
            z = (o->z - in->z) * t + in->z;
            o->z = z;
            r = kOne / z;
            o->rhw = r;
            o->sx = r * gView.scaleX * o->x + gView.centerX;
            o->sy = o->rhw * o->y * gView.scaleY + gView.centerY;
            o->code = outcode(o->sx, o->sy);
            t = (nearZ - in->z) / (p->z - in->z);
            p->x = (p->x - in->x) * t + in->x;
            p->y = (p->y - in->y) * t + in->y;
            z = (p->z - in->z) * t + in->z;
            p->z = z;
            r = kOne / z;
            p->rhw = r;
            p->sx = r * p->x * gView.scaleX + gView.centerX;
            p->sy = gView.scaleY * p->rhw * p->y + gView.centerY;
            p->code = outcode(p->sx, p->sy);
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (nearZ - p->z) / (o->z - p->z);
            tmp = *o;
            o->x = (o->x - p->x) * t + p->x;
            o->y = (o->y - p->y) * t + p->y;
            z = (o->z - p->z) * t + p->z;
            o->z = z;
            r = kOne / z;
            o->rhw = r;
            o->sx = r * o->x * gView.scaleX + gView.centerX;
            o->sy = gView.scaleY * o->y * o->rhw + gView.centerY;
            o->code = outcode(o->sx, o->sy);
            d = tmp.z - q->z;
            t = (nearZ - q->z) / d;
            tmp.x = (tmp.x - q->x) * t + q->x;
            tmp.y = (tmp.y - q->y) * t + q->y;
            tmp.z = d * t + q->z;
            r = kOne / tmp.z;
            tmp.rhw = r;
            sx = r * tmp.x * gView.scaleX + gView.centerX;
            tmp.sx = sx;
            sy = tmp.rhw * tmp.y * gView.scaleY + gView.centerY;
            tmp.sy = sy;
            tmp.code = (tmp.sy > gSceneArea.maxY) << 3 | (sx > gSceneArea.maxX) << 2 |
                       (sy < gSceneArea.minY) << 1 | (sx < gSceneArea.minX);
            Portal_ClipTriangle(*q, tmp, *o, rect, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 1) {
        ma = a.code & 1;
        mb = b.code & 1;
        mc = c.code & 1;
        if (ma + mb + mc == 2) {
            pick_two(v, ma, mb, &in, &o, &p);
            d = o->sx - in->sx;
            tf = (gSceneArea.minX - in->sx) / d;
            o->sx = d * tf + in->sx;
            o->sy = (o->sy - in->sy) * tf + in->sy;
            o->rhw = (o->rhw - in->rhw) * tf + in->rhw;
            o->code = outcode(o->sx, o->sy) & ~1;
            d = p->sx - in->sx;
            tf = (gSceneArea.minX - in->sx) / d;
            p->sx = d * tf + in->sx;
            p->sy = (p->sy - in->sy) * tf + in->sy;
            p->rhw = (p->rhw - in->rhw) * tf + in->rhw;
            p->code = outcode(p->sx, p->sy) & ~1;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            d = o->sx - p->sx;
            tf = (gSceneArea.minX - p->sx) / d;
            tmp = *o;
            o->sx = d * tf + p->sx;
            o->sy = (o->sy - p->sy) * tf + p->sy;
            o->rhw = (o->rhw - p->rhw) * tf + p->rhw;
            o->code = outcode(o->sx, o->sy) & ~1;
            d = tmp.sx - q->sx;
            tf = (gSceneArea.minX - q->sx) / d;
            sx = d * tf + q->sx;
            tmp.sx = sx;
            sy = (tmp.sy - q->sy) * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (tmp.sy > gSceneArea.maxY) << 3 | (sx > gSceneArea.maxX) << 2 | (sy < gSceneArea.minY) << 1;
            Portal_ClipTriangle(*q, tmp, *o, rect, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 2) {
        ma = a.code & 2;
        mb = b.code & 2;
        mc = c.code & 2;
        if (ma + mb + mc == 4) {
            pick_two(v, ma, mb, &in, &o, &p);
            t = (gSceneArea.minY - in->sy) / (o->sy - in->sy);
            o->sx = (o->sx - in->sx) * t + in->sx;
            o->sy = (o->sy - in->sy) * t + in->sy;
            o->rhw = (o->rhw - in->rhw) * t + in->rhw;
            o->code = outcode(o->sx, o->sy) & ~3;
            t = (gSceneArea.minY - in->sy) / (p->sy - in->sy);
            p->sx = (p->sx - in->sx) * t + in->sx;
            p->sy = (p->sy - in->sy) * t + in->sy;
            p->rhw = (p->rhw - in->rhw) * t + in->rhw;
            p->code = outcode(p->sx, p->sy) & ~3;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (gSceneArea.minY - p->sy) / (o->sy - p->sy);
            tmp = *o;
            o->sx = (o->sx - p->sx) * t + p->sx;
            o->sy = (o->sy - p->sy) * t + p->sy;
            o->rhw = (o->rhw - p->rhw) * t + p->rhw;
            o->code = outcode(o->sx, o->sy) & ~3;
            d = tmp.sy - q->sy;
            tf = (gSceneArea.minY - q->sy) / d;
            tmp.sx = (tmp.sx - q->sx) * tf + q->sx;
            sy = d * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (sy > gSceneArea.maxY) << 3 | (tmp.sx > gSceneArea.maxX) << 2;
            Portal_ClipTriangle(*q, tmp, *o, rect, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 4) {
        ma = a.code & 4;
        mb = b.code & 4;
        mc = c.code & 4;
        if (ma + mb + mc == 8) {
            pick_two(v, ma, mb, &in, &o, &p);
            d = o->sx - in->sx;
            tf = (gSceneArea.maxX - in->sx) / d;
            o->sx = d * tf + in->sx;
            o->sy = (o->sy - in->sy) * tf + in->sy;
            o->rhw = (o->rhw - in->rhw) * tf + in->rhw;
            o->code = (o->sy > gSceneArea.maxY) << 3;
            d = p->sx - in->sx;
            tf = (gSceneArea.maxX - in->sx) / d;
            p->sx = d * tf + in->sx;
            p->sy = (p->sy - in->sy) * tf + in->sy;
            p->rhw = (p->rhw - in->rhw) * tf + in->rhw;
            p->code = (p->sy > gSceneArea.maxY) << 3;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            d = o->sx - p->sx;
            tf = (gSceneArea.maxX - p->sx) / d;
            tmp = *o;
            o->sx = d * tf + p->sx;
            o->sy = (o->sy - p->sy) * tf + p->sy;
            o->rhw = (o->rhw - p->rhw) * tf + p->rhw;
            o->code = (o->sy > gSceneArea.maxY) << 3;
            d = tmp.sx - q->sx;
            tf = (gSceneArea.maxX - q->sx) / d;
            tmp.sx = d * tf + q->sx;
            sy = (tmp.sy - q->sy) * tf + q->sy;
            tmp.sy = sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            tmp.code = (sy > gSceneArea.maxY) << 3;
            Portal_ClipTriangle(*q, tmp, *o, rect, nearZ);
        }
        any = a.code | b.code | c.code;
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    if (any & 8) {
        ma = a.code & 8;
        mb = b.code & 8;
        mc = c.code & 8;
        if (ma + mb + mc == 0x10) {
            pick_two(v, ma, mb, &in, &o, &p);
            t = (gSceneArea.maxY - in->sy) / (o->sy - in->sy);
            o->sx = (o->sx - in->sx) * t + in->sx;
            o->sy = (o->sy - in->sy) * t + in->sy;
            o->code = 0;
            o->rhw = (o->rhw - in->rhw) * t + in->rhw;
            t = (gSceneArea.maxY - in->sy) / (p->sy - in->sy);
            p->sx = (p->sx - in->sx) * t + in->sx;
            p->sy = (p->sy - in->sy) * t + in->sy;
            p->code = 0;
            p->rhw = (p->rhw - in->rhw) * t + in->rhw;
        } else {
            pick_one(v, ma, mb, &o, &p, &q);
            t = (gSceneArea.maxY - p->sy) / (o->sy - p->sy);
            tmp = *o;
            tmp.code = 0;
            o->sx = (o->sx - p->sx) * t + p->sx;
            o->sy = (o->sy - p->sy) * t + p->sy;
            o->code = 0;
            o->rhw = (o->rhw - p->rhw) * t + p->rhw;
            d = tmp.sy - q->sy;
            tf = (gSceneArea.maxY - q->sy) / d;
            tmp.sx = (tmp.sx - q->sx) * tf + q->sx;
            tmp.sy = d * tf + q->sy;
            tmp.rhw = (tmp.rhw - q->rhw) * tf + q->rhw;
            Portal_ClipTriangle(*q, tmp, *o, rect, nearZ);
        }
        all = a.code & b.code & c.code;
    }
    if (all)
        goto done;

    for (i = 0; i < 3; i++) {
        float *mn = rect[0], *mx = rect[1];

        mx[0] = v[i]->sx < mx[0] ? mx[0] : v[i]->sx;
        mx[1] = v[i]->sy < mx[1] ? mx[1] : v[i]->sy;
        mn[0] = v[i]->sx > mn[0] ? mn[0] : v[i]->sx;
        mn[1] = v[i]->sy > mn[1] ? mn[1] : v[i]->sy;
    }
done:
    gPortalClipDepth--;
}
HOOK(0x4DAB50, Portal_ClipTriangle);
/* event from 0x4D14C0: casts a line down from arg->pos through the sector
   object and its sectors; a hit names the sector and the floor point */
void SectorObject_LocateHandler(SectorObject *so, LocateArg *arg)
{
    LineQuery q;
    LineHit hit;
    Vec3d from, to;
    RigidDynCubeObj *o;
    long double r, rx;
    float scale, ry, rz;
    float *m = so->s.o.matrix.m;
    Vec3f *v = &so->center;

    if (!so->sectors)
        return;
    from = *arg->pos;
    to = from;
    from.z = (arg->raise ? kRaise : kZero) + from.z;
    scale = so->s.o.scale[2] < so->s.o.scale[1] ? so->s.o.scale[1] : so->s.o.scale[2];
    if (so->s.o.scale[0] > scale)
        scale = so->s.o.scale[0];
    r = VirModel_GetRadius((VirModel *)so->s.o.model) * scale;
    hit.obj = NULL;
    hit.found = 0;
    q.hit = &hit;
    q.to = &to;
    to.z = to.z - (r + r);
    hit.point = to;
    q.from = &from;
    q.unkC = 0xFF;
    q.unk10 = 0;
    q.unk8 = 1;
    q.unk14 = 0;
    q.unk18 = 1;
    q.tag = 0x41424344;
    o = &so->s.o;
    do {
        gTaskHandlers[9][o->task.type](o, &q);
        if (o == &so->s.o) {
            if (!so->sectors)
                break;
            o = (RigidDynCubeObj *)so->sectors->head;
            if (!o->task.next)
                break;
        } else {
            o = (RigidDynCubeObj *)o->task.next;
            if (!o || !o->task.next)
                break;
        }
    } while (o);
    if (!hit.found)
        return;
    arg->sector = (Sector *)hit.obj;
    arg->owner = so;
    rx = (long double)m[2] * v->z + (long double)m[1] * v->y + (long double)m[0] * v->x;
    ry = m[5] * v->z + m[4] * v->y + m[3] * v->x;
    rz = m[8] * v->z + m[7] * v->y + m[6] * v->x;
    arg->floor.x = rx + hit.obj->pos.x;
    arg->floor.y = ry + hit.obj->pos.y;
    arg->floor.z = rz + hit.obj->pos.z;
    arg->radius = so->radius;
}
HOOK(0x4DBCC0, SectorObject_LocateHandler);

/* event from 0x4CFC90: is this lightmap set made for our model */
void SectorObject_MatchLightmaps(SectorObject *so, int *arg)
{
    LightmapSet *set = (LightmapSet *)arg[0];

    arg[1] = set ? set->model == (VirModel *)so->s.o.model : 0;
}
HOOK(0x4DBF00, SectorObject_MatchLightmaps);

static int lightmapped(RigidDynCubeObj *o)
{
    return o->model && lod_mesh((VirModel *)o->model, 0)->format == 3;
}

/* adds the LODs of lightmapped MagicObjs in the list t and below */
void SectorObject_CountMagicLods(int *count, Task *t)
{
    for (; t; t = t->next) {
        if (TaskType_IsA(t->type, MagicObj_GetType())) {
            if (lightmapped((RigidDynCubeObj *)t))
                *count += ((VirModel *)((RigidDynCubeObj *)t)->model)->lod_count;
            SectorObject_CountMagicLods(count, first_child(t));
        }
        if (!t->next || !t->next->next)
            break;
    }
}
HOOK(0x4DBF30, SectorObject_CountMagicLods);

/* the same for rigid objects that are not sector objects, and the tasks
   attached to them */
void SectorObject_CountRigidLods(int *count, Task *t)
{
    for (; t; t = t->next) {
        if (!TaskType_IsA(t->type, SectorObject_GetType())) {
            if (TaskType_IsA(t->type, RigidDynCubeObj_GetType())) {
                RigidDynCubeObj *o = (RigidDynCubeObj *)t;

                if (lightmapped(o))
                    *count += ((VirModel *)o->model)->lod_count;
                if (o->attached)
                    SectorObject_CountRigidLods(count, first_child(o->attached));
            }
            SectorObject_CountRigidLods(count, first_child(t));
        }
        if (!t->next || !t->next->next)
            break;
    }
}
HOOK(0x4DBFA0, SectorObject_CountRigidLods);

static void fill_refs(int *n, LightmapSet *set, RigidDynCubeObj *o)
{
    int i;

    o->lightmapRefs = &set->refs[*n];
    for (i = 0; i < ((VirModel *)o->model)->lod_count; i++) {
        LightmapRef *r = &set->refs[*n];

        r->lightmaps = NULL;
        r->obj = o;
        r->matrix = &o->matrix;
        r->pos = &o->pos;
        r->unk14 = o->unkD8;
        r->lodIndex = i;
        r->lodCount = ((VirModel *)o->model)->lod_count;
        r->lod = ((VirModel *)o->model)->lods[i];
        r->uploaded = 0;
        (*n)++;
    }
}

void SectorObject_FillMagicRefs(int *n, LightmapSet *set, Task *t, int top)
{
    for (; t; t = t->next) {
        if (TaskType_IsA(t->type, MagicObj_GetType()) && lightmapped((RigidDynCubeObj *)t)) {
            fill_refs(n, set, (RigidDynCubeObj *)t);
            SectorObject_FillMagicRefs(n, set, first_child(t), top);
        }
        if (!t->next || !t->next->next)
            break;
    }
}
HOOK(0x4DC050, SectorObject_FillMagicRefs);

void SectorObject_FillRigidRefs(int *n, LightmapSet *set, Task *t)
{
    for (; t; t = t->next) {
        if (!TaskType_IsA(t->type, SectorObject_GetType())) {
            if (TaskType_IsA(t->type, RigidDynCubeObj_GetType())) {
                RigidDynCubeObj *o = (RigidDynCubeObj *)t;

                if (lightmapped(o))
                    fill_refs(n, set, o);
                if (o->attached)
                    SectorObject_FillRigidRefs(n, set, first_child(o->attached));
            }
            SectorObject_FillRigidRefs(n, set, first_child(t));
        }
        if (!t->next || !t->next->next)
            break;
    }
}
HOOK(0x4DC190, SectorObject_FillRigidRefs);

void SectorObject_ClearMagicRefs(Task *t)
{
    for (; t; t = t->next) {
        if (TaskType_IsA(t->type, MagicObj_GetType())) {
            ((RigidDynCubeObj *)t)->lightmapRefs = NULL;
            SectorObject_ClearMagicRefs(first_child(t));
        }
        if (!t->next || !t->next->next)
            break;
    }
}
HOOK(0x4DC2F0, SectorObject_ClearMagicRefs);

void SectorObject_ClearRigidRefs(Task *t)
{
    for (; t; t = t->next) {
        if (!TaskType_IsA(t->type, SectorObject_GetType())) {
            if (TaskType_IsA(t->type, RigidDynCubeObj_GetType())) {
                RigidDynCubeObj *o = (RigidDynCubeObj *)t;

                o->lightmapRefs = NULL;
                if (o->attached)
                    SectorObject_ClearRigidRefs(first_child(o->attached));
            }
            SectorObject_ClearRigidRefs(first_child(t));
        }
        if (!t->next || !t->next->next)
            break;
    }
}
HOOK(0x4DC340, SectorObject_ClearRigidRefs);

/* the sector object, then its sectors */
static RigidDynCubeObj *next_part(SectorObject *so, RigidDynCubeObj *o)
{
    if (o == &so->s.o) {
        if (!so->sectors)
            return NULL;
        o = (RigidDynCubeObj *)so->sectors->head;
    } else {
        o = (RigidDynCubeObj *)o->task.next;
        if (!o)
            return NULL;
    }
    return o->task.next ? o : NULL;
}

void SectorObject_GetBounds(SectorObject *so, Vec3f *center, float *radius);

/* event from 0x4CFC70: one LightmapRef per LOD of every lightmapped part */
void SectorObject_LoadLightmaps(SectorObject *so, LightmapSet **arg)
{
    LightmapSet *old = *arg, *set;
    int count = 0, n = 0;
    RigidDynCubeObj *o;

    HANDLER(Mesh3D_GetEvent2(), so->s.o.task.type)(so, &old);
    if (so->sectors) {
        for (o = &so->s.o; o; o = next_part(so, o)) {
            if (o->model) {
                count += ((VirModel *)o->model)->lod_count;
                SectorObject_CountMagicLods(&count, first_child(o->attached));
            }
        }
    }
    SectorObject_CountRigidLods(&count, first_child(&so->s.o.task));
    if (!count)
        return;
    set = Mem_Alloc(count * sizeof(LightmapRef) + sizeof(LightmapSet), 4);
    set->count = count;
    set->model = (VirModel *)so->s.o.model;
    if (so->sectors) {
        for (o = &so->s.o; o; o = next_part(so, o)) {
            if (o->model) {
                fill_refs(&n, set, o);
                SectorObject_FillMagicRefs(&n, set, first_child(o->attached), o == &so->s.o);
            }
        }
    }
    SectorObject_FillRigidRefs(&n, set, first_child(&so->s.o.task));
    SectorObject_GetBounds(so, &set->center, &set->radius);
    *arg = set;
}
HOOK(0x4DC3D0, SectorObject_LoadLightmaps);

typedef struct Sphere {
    Vec3f c;
    float r;
} Sphere;

static void add_sphere(Pool *pool, RigidDynCubeObj *o)
{
    SectorMesh *m = lod_mesh((VirModel *)o->model, 0);
    Sphere *s = pool_take(pool);

    s->c = m->center;
    s->r = m->radius;
}

void SectorObject_AddChildSpheres(Pool *pool, Task *t);

/* a sphere around all the sectors: merges the two nearest spheres until
   one is left */
void SectorObject_GetBounds(SectorObject *so, Vec3f *center, float *radius)
{
    Pool pool;
    Sphere *a, *b, *last;
    RigidDynCubeObj *o;
    float min, dx, dy, dz, p1x, p1y, p1z, p2x, p2y, cx, cy, cz;
    long double d, len, s, z1, z2;
    int i, j;

    if (!so->sectors || !so->sectors->head->next) {
        SectorMesh *m;

        if (!so->s.o.model)
            return;
        m = lod_mesh((VirModel *)so->s.o.model, 0);
        *center = m->center;
        *radius = m->radius2;
        return;
    }
    Pool_Init(&pool, sizeof(Sphere), 1000, 0);
    for (o = &so->s.o; o; o = next_part(so, o)) {
        add_sphere(&pool, o);
        if (o->attached)
            SectorObject_AddChildSpheres(&pool, o->attached);
    }
    a = b = (Sphere *)center;
    while (pool.used > 1) {
        min = FLT_MAX;
        for (i = 0; i < pool.used; i++) {
            for (j = 0; j < pool.used; j++) {
                Sphere *p, *q;

                if (i == j)
                    continue;
                p = pool_get(&pool, i);
                q = pool_get(&pool, j);
                {
                    long double ez = (long double)q->c.z - p->c.z;
                    long double ey = (long double)q->c.y - p->c.y;
                    long double ex = (long double)q->c.x - p->c.x;

                    d = x87_sqrt(ex * ex + ey * ey + ez * ez);
                }
                if (d < min) {
                    min = d;
                    a = p;
                    b = q;
                }
            }
        }
        if (min == 0) {
            a->r = b->r < a->r ? a->r : b->r;
            pool_release(&pool, b);
        } else if (min + (long double)b->r < a->r) {
            pool_release(&pool, b);
        } else if (min + (long double)a->r < b->r) {
            pool_release(&pool, a);
        } else {
            dx = b->c.x - a->c.x;
            dy = b->c.y - a->c.y;
            dz = b->c.z - a->c.z;
            len = x87_sqrt((long double)dx * dx + (long double)dy * dy + (long double)dz * dz);
            s = a->r / len;
            p1x = dx * s;
            p1y = dy * s;
            z1 = s * dz;
            p1x = -p1x;
            p1y = -p1y;
            p1x = p1x + a->c.x;
            p1y = p1y + a->c.y;
            p1z = -z1 + a->c.z;
            s = b->r / len;
            p2x = dx * s;
            p2y = dy * s;
            z2 = s * dz;
            p2x = p2x + b->c.x;
            p2y = p2y + b->c.y;
            z2 = z2 + b->c.z;
            cx = (p2x - p1x) * kHalf + p1x;
            a->c.x = cx;
            cy = (p2y - p1y) * kHalf + p1y;
            a->c.y = cy;
            cz = (z2 - p1z) * kHalf + p1z;
            a->c.z = cz;
            {
                long double ez = z2 - cz;
                long double ey = (long double)p2y - cy;
                long double ex = (long double)p2x - cx;

                a->r = x87_sqrt(ex * ex + ey * ey + ez * ez);
            }
            pool_release(&pool, b);
        }
    }
    last = pool_get(&pool, 0);
    *center = last->c;
    *radius = last->r;
    pool_release(&pool, last);
    Pool_Free(&pool);
}
HOOK(0x4DC640, SectorObject_GetBounds);

void SectorObject_AddChildSpheres(Pool *pool, Task *t)
{
    Task *c = t->head;

    if (!c->next || !c)
        return;
    for (;;) {
        add_sphere(pool, (RigidDynCubeObj *)c);
        SectorObject_AddChildSpheres(pool, c);
        c = c->next;
        if (!c || !c->next)
            break;
    }
}
HOOK(0x4DCBA0, SectorObject_AddChildSpheres);

/* event from 0x4CFC80 */
void SectorObject_FreeLightmaps(SectorObject *so, void **arg)
{
    RigidDynCubeObj *o;

    if (*arg)
        Mem_Free(*arg);
    SectorObject_ClearRigidRefs(first_child(&so->s.o.task));
    if (!so->s.o.model)
        return;
    for (o = &so->s.o; o; o = next_part(so, o)) {
        o->lightmapRefs = NULL;
        SectorObject_ClearMagicRefs(first_child(o->attached));
    }
}
HOOK(0x4DCC20, SectorObject_FreeLightmaps);

/* puts obj into the sector holding pos; returns 1 when it found one */
int Sector_AddObject(int world, RigidDynCubeObj *obj, Vec3d *pos)
{
    LocateArg q;
    Sector *s;

    q.pos = pos;
    q.raise = 1;
    q.sector = NULL;
    q.owner = NULL;
    Coll_SendEvent(world, QCube_GetEnclosingEvent(), &q, pos, 0xFFFFFFFF, 5);
    s = q.sector;
    if (s && s != &q.owner->s && s->objects.used < s->objects.count && gSectorLinks.used < gSectorLinks.count) {
        SectorLink *l = pool_take(&gSectorLinks);

        l->sector = s;
        l->owner = q.owner;
        l->slot = pool_take(&s->objects);
        *l->slot = obj;
        obj->inSector = 1;
        return 1;
    }
    obj->inSector = 0;
    return 0;
}
HOOK(0x4DCCB0, Sector_AddObject);

void Sector_RemoveObject(RigidDynCubeObj *obj)
{
    int i;

    if (!obj->inSector)
        return;
    for (i = 0; i < gSectorLinks.used; i++) {
        SectorLink *l = pool_get(&gSectorLinks, i);

        if (*l->slot == obj) {
            pool_release(&l->sector->objects, l->slot);
            l->slot = NULL;
            pool_release(&gSectorLinks, l);
            obj->inSector = 0;
            return;
        }
    }
}
HOOK(0x4DCDD0, Sector_RemoveObject);

int Sector_MoveObject(int world, RigidDynCubeObj *obj, Vec3d *pos)
{
    Sector_RemoveObject(obj);
    return Sector_AddObject(world, obj, pos);
}
HOOK(0x4DCF00, Sector_MoveObject);

void Sector_Render(Sector *s);
void SectorObject_Render(SectorObject *so);

void Sector_Open(void)
{
    gSectorType = TaskType_New("Sector", sizeof(Sector), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gSectorType, 4, Sector_Render);
    TaskType_SetHandler(gSectorType, 0, Sector_Create);
    TaskType_SetHandler2(gSectorType, 2, Sector_Delete);
    TaskType_SetHandler(gSectorType, World_GetEvent0(), Sector_WorldAdd);
    TaskType_SetHandler(gSectorType, World_GetEvent1(), Sector_WorldRemove);
    gSectorObjectType = TaskType_New("SectorObject", sizeof(SectorObject), gSectorType, 0);
    TaskType_SetHandler(gSectorObjectType, 4, SectorObject_Render);
    TaskType_SetHandler(gSectorObjectType, CellMask_GetDrawEvent(), sub_4D3710);
    TaskType_SetHandler(gSectorObjectType, 0, SectorObject_Create);
    TaskType_SetHandler2(gSectorObjectType, 2, SectorObject_Delete);
    TaskType_SetHandler(gSectorObjectType, 10, SectorObject_Handler10);
    TaskType_SetHandler(gSectorObjectType, 9, SectorObject_Handler9);
    TaskType_SetHandler(gSectorObjectType, 8, SectorObject_Handler8);
    TaskType_SetHandler(gSectorObjectType, 11, SectorObject_Handler11);
    TaskType_SetHandler(gSectorObjectType, QCube_GetEnclosingEvent(), SectorObject_LocateHandler);
    TaskType_SetHandler2(gSectorObjectType, 1, SectorObject_Run);
    TaskType_SetHandler(gSectorObjectType, Mesh3D_GetEvent1(), SectorObject_LoadLightmaps);
    TaskType_SetHandler(gSectorObjectType, Mesh3D_GetEvent2(), SectorObject_FreeLightmaps);
    TaskType_SetHandler(gSectorObjectType, Mesh3D_GetEvent3(), SectorObject_MatchLightmaps);
    TaskType_SetHandler(gSectorObjectType, QCube_GetNearClipEvent(), SectorObject_NearZHandler);
    TaskType_SetHandler(gSectorObjectType, World_GetEvent0(), SectorObject_WorldAdd);
    TaskType_SetHandler(gSectorObjectType, World_GetEvent1(), SectorObject_WorldRemove);
    Pool_Init(&gVisSectors, sizeof(VisSector), 20, 0);
    Pool_Init(&gSectorLinks, sizeof(SectorLink), 1000, 0);
    gSectorInteriorObjectType = TaskType_New("SectorInteriorObject", sizeof(RigidDynCubeObj), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gSectorInteriorObjectType, World_GetEvent0(), SectorInteriorObject_WorldAdd);
    TaskType_SetHandler(gSectorInteriorObjectType, World_GetEvent1(), SectorInteriorObject_WorldRemove);
}
HOOK(0x4DCF20, Sector_Open);

/* draws the sector with the lights of its sector object, places its
   children and draws the objects inside */
void Sector_Render(Sector *s)
{
    MeshDrawArg d;
    Frame a;
    Task *c;
    int i;

    d.obj = (struct BoneDynCubeObj *)&s->o;
    d.lights = gSectorLights;
    d.lightCount = gSectorLightCount;
    d.haze = gSectorFog;
    d.hazeCount = gSectorFogCount;
    gRenderFuncs[Mesh3D_GetDrawRigidMethod()](&d);
    a.pos = s->o.pos;
    a.matrix = s->o.matrix;
    c = s->o.attached->head;
    if (c->next && c) {
        do {
            HANDLER(RigidDynCubeObj_GetEvent2(), c->type)(c, &a);
            c = c->next;
        } while (c && c->next);
    }
    c = s->o.task.head;
    if (c->next) {
        while (c) {
            TaskFn fn = HANDLER(RigidDynCubeObj_GetEvent2(), c->type);

            if (fn)
                fn(c, &a);
            c = c->next;
            if (!c || !c->next)
                break;
        }
    }
    for (i = 0; i < s->objects.used; i++) {
        RigidDynCubeObj *o = *(RigidDynCubeObj **)pool_get(&s->objects, i);
        TaskFn fn = HANDLER(4, o->task.type);

        if (fn)
            fn(o, 0);
    }
}
HOOK(0x4DD1C0, Sector_Render);

void Sector_AddVisible(Sector *s, float *min, float *max, int lod);
void Sector_DrawVisible(Sector *s, float *min, float *max, int lod);

/* draws the sector the camera sees and every sector visible through its
   portals, each clipped to its portal rectangle */
void SectorObject_Render(SectorObject *so)
{
    float savedMin[2], savedMax[2], min[2], max[2];
    Sector *s = &so->s;
    int lod, i;

    savedMin[0] = gSceneArea.minX;
    savedMax[0] = gSceneArea.maxX;
    savedMin[1] = gSceneArea.minY;
    savedMax[1] = gSceneArea.maxY;
    gCurrentSectorObj = so;
    if (!so->s.o.model)
        return;
    if (so == gQCubeEnclosing) {
        if (gQCubeInside)
            return;
        s = gViewSector;
    }
    gSectorLights = Light_GetActive(&gSectorLightCount);
    Haze_GetVisible(&gSectorFog, &gSectorFogCount);
    min[0] = gSceneArea.minX;
    min[1] = gSceneArea.minY;
    max[0] = gSceneArea.maxX;
    max[1] = gSceneArea.maxY;
    lod = ModelObj_GetLOD(s);
    if (lod != -1)
        Sector_AddVisible(s, min, max, lod);
    for (i = 0; i < gVisSectors.used; i++) {
        VisSector *v = pool_get(&gVisSectors, i);

        Sector_DrawVisible(v->sector, v->min, v->max, v->lod);
    }
    while (gVisSectors.used)
        pool_release(&gVisSectors, pool_get(&gVisSectors, 0));
    Scene_SetBounds(gScene, savedMin, savedMax);
}
HOOK(0x4DD310, SectorObject_Render);

/* adds a sector to this frame's list, or grows its rectangle when it is
   already there; a new one floods on through its portals */
void Sector_AddVisible(Sector *s, float *min, float *max, int lod)
{
    VisSector *v;
    int i;

    for (i = 0; i < gVisSectors.used; i++) {
        v = pool_get(&gVisSectors, i);
        if (v->sector == s) {
            v->min[0] = v->min[0] < min[0] ? v->min[0] : min[0];
            v->min[1] = min[1] > v->min[1] ? v->min[1] : min[1];
            v->max[0] = v->max[0] > max[0] ? v->max[0] : max[0];
            v->max[1] = max[1] < v->max[1] ? v->max[1] : max[1];
            return;
        }
    }
    v = pool_take(&gVisSectors);
    v->sector = s;
    v->lod = lod;
    v->min[0] = min[0];
    v->min[1] = min[1];
    v->max[0] = max[0];
    v->max[1] = max[1];
    v->unk10 = gVisFrame + 1;
    v->unk1C = 0;
    Sector_FloodPortals(s, min, max, lod);
}
HOOK(0x4DD4B0, Sector_AddVisible);

void Sector_DrawVisible(Sector *s, float *min, float *max, int lod)
{
    if (s == &gCurrentSectorObj->s && gCurrentSectorObj == gQCubeEnclosing) {
        gQCubeOutsideMin[0] = min[0];
        gQCubeOutsideMin[1] = min[1];
        gQCubeOutsideMax[0] = max[0];
        gQCubeOutsideMax[1] = max[1];
        gQCubeDrawOutside = 1;
    }
    Scene_SetBounds(gScene, min, max);
    HANDLER(4, gSectorType)(s, 0);
}
HOOK(0x4DD5D0, Sector_DrawVisible);

int Portal_GetScreenRect(Sector *s, int lod, int k, float *clipMin, float *clipMax, float *outMin, float *outMax);

/* every open portal of the sector seen inside min..max makes the sectors
   with the same portal visible */
void Sector_FloodPortals(Sector *s, float *min, float *max, int lod)
{
    SectorMesh *m = lod_mesh((VirModel *)s->o.model, lod);
    float rmin[2], rmax[2];
    int k, j;

    for (k = 0; k < m->portalCount; k++) {
        int id = m->portals[k].id;
        RigidDynCubeObj *o;

        if (!Portal_GetScreenRect(s, lod, k, min, max, rmin, rmax))
            continue;
        if (!(rmin[0] > min[0]))
            rmin[0] = min[0];
        if (min[1] >= rmin[1])
            rmin[1] = min[1];
        if (rmax[0] >= max[0])
            rmax[0] = max[0];
        if (!(max[1] > rmax[1]))
            rmax[1] = max[1];
        for (o = &gCurrentSectorObj->s.o; o; o = next_part(gCurrentSectorObj, o)) {
            int l;
            SectorMesh *m2;

            if (o == &s->o)
                continue;
            l = ModelObj_GetLOD(o);
            if (l == -1)
                continue;
            m2 = lod_mesh((VirModel *)o->model, l);
            for (j = 0; j < m2->portalCount; j++)
                if (m2->portals[j].id == id)
                    Sector_AddVisible((Sector *)o, rmin, rmax, l);
        }
    }
}
HOOK(0x4DD650, Sector_FloodPortals);

/* the screen rectangle of portal k seen within clipMin..clipMax; 0 when it
   is closed, faces away too far, or is off screen */
int Portal_GetScreenRect(Sector *s, int lod, int k, float *clipMin, float *clipMax, float *outMin, float *outMax)
{
    SectorMesh *m = lod_mesh((VirModel *)s->o.model, lod);
    Portal *p = &m->portals[k];
    float *mx = s->o.matrix.m;
    float min[2], max[2], *rect[2];
    ClipVertex vtx[99];
    Vec3f n, *v0, *v1, *v2;
    long double e1x, e1y, e1z, e2z, nz, px, dx, dy, dz, dot;
    float e2x, e2y, nx, ny, wx, wy, wz, py, pz, dxf, dyf, dzf, cx, cy, cz;
    double wpy, wpz;
    int *tri, i, any = 0;

    min[0] = FLT_MAX;
    min[1] = FLT_MAX;
    max[0] = -FLT_MAX;
    max[1] = -FLT_MAX;
    if (!gCurrentSectorObj->portalOpen[p->id])
        return 0;
    tri = m->tris[p->firstTri];
    v0 = &m->verts[tri[0]];
    v1 = &m->verts[tri[1]];
    v2 = &m->verts[tri[2]];
    e1x = (long double)v1->x - v0->x;
    e1y = (long double)v1->y - v0->y;
    e1z = (long double)v1->z - v0->z;
    e2x = v2->x - v0->x;
    e2y = v2->y - v0->y;
    e2z = (long double)v2->z - v0->z;
    nx = e2z * e1y - e2y * e1z;
    ny = -(e2z * e1x - e2x * e1z);
    nz = e1x * e2y - e2x * e1y;
    wx = nx * mx[0] + nz * mx[2] + ny * mx[1];
    wy = nx * mx[3] + nz * mx[5] + ny * mx[4];
    wz = nx * mx[6] + nz * mx[8] + ny * mx[7];
    n.x = wx;
    n.y = wy;
    n.z = wz;
    if (!feq0(wx) || !feq0(wy) || !feq0(wz)) {
        long double inv = kOneD / x87_sqrt((long double)wx * wx + (long double)wy * wy + (long double)wz * wz);

        n.x = wx * inv;
        n.y = n.y * inv;
        n.z = n.z * inv;
    }
    px = (long double)mx[0] * v0->x + (long double)mx[2] * v0->z + (long double)mx[1] * v0->y;
    py = v0->y * mx[4] + v0->z * mx[5] + v0->x * mx[3];
    pz = mx[6] * v0->x + mx[8] * v0->z + mx[7] * v0->y;
    px = px + s->o.pos.x;
    wpy = py + s->o.pos.y;
    wpz = pz + s->o.pos.z;
    dx = px - gView.pos.x;
    dxf = dx;
    dy = wpy - gView.pos.y;
    dyf = dy;
    dz = wpz - gView.pos.z;
    dzf = dz;
    if (!feq0(dx) || !feq0(dy) || !feq0(dz)) {
        float len = x87_sqrt(dx * dx + dy * dy + dz * dz);
        long double inv = kOneD / len;

        dx = dx * inv;
        dy = dy * inv;
        dz = dz * inv;
    }
    dot = dz * n.z + dy * n.y + dx * n.x;
    if (dot < kZero) {
        Vec3f neg, d;
        float out[3];
        long double ex, ey, ez;

        neg.x = -n.x;
        neg.y = -n.y;
        neg.z = -n.z;
        d.x = dxf;
        d.y = dyf;
        d.z = dzf;
        Vec3_RayPlane(out, &d.x, &neg.x, &n.x);
        ez = (long double)d.z - out[2];
        ey = (long double)d.y - out[1];
        ex = (long double)d.x - out[0];
        if (ex * ex + ey * ey + ez * ez > kPortalDist2)
            return 0;
    }
    cx = s->o.pos.x - gView.pos.x;
    cy = s->o.pos.y - gView.pos.y;
    cz = s->o.pos.z - gView.pos.z;
    if (p->vertCount <= 0)
        return 0;
    for (i = 0; i < p->vertCount; i++) {
        Vec3f *v = &m->verts[p->firstVert + i];
        ClipVertex *c = &vtx[i];
        float rx = v->y * mx[1] + mx[2] * v->z + mx[0] * v->x;
        float ry = v->y * mx[4] + mx[3] * v->x + v->z * mx[5];
        float rz = v->y * mx[7] + mx[8] * v->z + mx[6] * v->x;
        long double X = (long double)rx + cx, Y = (long double)ry + cy, Z = (long double)rz + cz;

        c->x = Z * gView.matrix.m[2] + Y * gView.matrix.m[1] + X * gView.matrix.m[0];
        c->y = Z * gView.matrix.m[5] + Y * gView.matrix.m[4] + X * gView.matrix.m[3];
        c->z = Z * gView.matrix.m[8] + Y * gView.matrix.m[7] + X * gView.matrix.m[6];
        if (!(c->z >= kNearZ)) {
            c->code = 0x10;
        } else {
            long double sx, sy;
            float rhw = kOne / c->z, syf;

            c->rhw = rhw;
            any = 1;
            sx = rhw * c->x * gView.scaleX + gView.centerX;
            c->sx = sx;
            sy = rhw * c->y * gView.scaleY + gView.centerY;
            syf = sy;
            c->sy = syf;
            c->code = (syf > gSceneArea.maxY) << 3 | (sx > gSceneArea.maxX) << 2 |
                      !(sy >= gSceneArea.minY) << 1 | !(sx >= gSceneArea.minX);
        }
    }
    if (!any)
        return 0;
    rect[0] = min;
    rect[1] = max;
    for (i = 0; i < p->triCount; i++) {
        int *t = m->tris[p->firstTri + i];

        Portal_ClipTriangle(vtx[t[0] - p->firstVert], vtx[t[1] - p->firstVert], vtx[t[2] - p->firstVert], rect,
                            kNearZ);
    }
    if (min[0] > clipMax[0] || min[1] > clipMax[1])
        return 0;
    if (!(max[0] >= clipMin[0]) || !(max[1] >= clipMin[1]))
        return 0;
    outMin[0] = min[0];
    outMin[1] = min[1];
    outMax[0] = max[0];
    outMax[1] = max[1];
    return 1;
}
HOOK(0x4DD7F0, Portal_GetScreenRect);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(534B2C) const float kNearZ INIT(0.04096f);
DATA(534B2C) const float kPortalDist2 INIT(167772.17f);
DATA(548694) uint16_t gSectorType INIT(385);
DATA(548694) uint16_t gSectorObjectType INIT(385);
DATA(548694) uint16_t gSectorInteriorObjectType INIT(385);
PAD(igd, 548694, 54869A, 0x2);
BSS(A5E310) int gSectorFogCount;
BSS(A5E330) int *gSectorFog;
BSS(A5E330) int gSectorLightCount;
BSS(A5E330) Pool gVisSectors;
BSS(A5E330) int gPortalClipDepth;
BSS(A5E330) SectorObject *gCurrentSectorObj;
BSS(A5E330) int gVisFrame;
BSS(A5E330) int *gSectorLights;
PAD(igb, B12B78, B12B78, 0x8);
ALIAS(gQCubeOutsideMin, pad_B12B78, 0);
PAD(igb, B16C60, B16C60, 0x8);
ALIAS(gQCubeOutsideMax, pad_B16C60, 0);
