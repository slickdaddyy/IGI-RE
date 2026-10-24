#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <string.h>

#include "hook.h"
#include "math/x87.h"
#include "tagitem/tagitem.h"

/* Points along a "SplineObj" and the "SplineObjWaypoint" handlers (types made by 0x502250). */

ORIG(0x401BE0, Task *, Task_FindChild, (Task *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (Task *task, uint16_t type));
ORIG(0x401C80, Task *, Task_FindPrev, (Task *task, uint16_t type));
ORIG(0x418D70, void, MenuManager_Free, (void *task));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B2760, long double, Math_ACosF, (float x));
ORIG(0x4B27D0, long double, Math_ASinF, (float x));
ORIG(0x4B32F0, long double, Vec3d_DistanceSq, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B44D0, void, Quat_SlerpPrepare, (char *flip, float *angle, float *inv_sin, const float *a, const float *b));
ORIG(0x4B2D00, void, Vec3_ToAngles, (const Vec3f *v, float *pitch, float *yaw));
ORIG(0x4B38E0, void, Matrix_SetEuler, (Matrix *m, float ax, float ay, float az));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4E43D0, SplineObjSegment *, SplineObjSegment_New, (void *parent, void *tree, Vec3d *from, Vec3d *to,
                                                         Matrix *rotFrom, Matrix *rotTo, int rigid, char reversed,
                                                         int segments, int unk, const char *model));
ORIG(0x4E4530, void, SplineObjSegment_Setup, (SplineObjSegment *o, Vec3d *from, Vec3d *to, Matrix *rotFrom,
                                              Matrix *rotTo, int rigid, char reversed, int segments, int unk,
                                              const char *model));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x501D70, void, SplineObjWaypoint_UpdateAttach, (SplineObjWaypoint *w));

extern uint16_t gSplineObjWaypointType;
extern void *gCurrentWorld;
extern char gEmptyString;
extern TaskFn gTaskHandlers[][0x180];

extern const float kDefaultSegLength;
extern const float kHalf;
extern const float kSmallAngle;
extern const float kMinusTwo;
extern const float kPi;
extern const double kTiny;

/* segment tables by raw offset: the original also reads index -1 (the word before segLength) */
#define SEG_POS(s, i) ((float *)((char *)(s) + 0x104 + (i) * 12))
#define SEG_ROT(s, i) ((float *)((char *)(s) + 0x200 + (i) * 40))
#define SEG_LEN(s, i) (*(float *)((char *)(s) + 0x548 + (i) * 4))

typedef SplineObjWaypoint Waypoint;

/* how the original's fcom / test ah,40h treats a comparison with zero */
static int IsZero(float x)
{
    return !islessgreater(x, 0.0f);
}

static int IsZeroL(long double x)
{
    return !islessgreater(x, 0.0L);
}

static Waypoint *Waypoint_Next(Waypoint *w)
{
    return (Waypoint *)Task_FindNext(&w->obj.task, gSplineObjWaypointType);
}

static Waypoint *Waypoint_Prev(Waypoint *w)
{
    return (Waypoint *)Task_FindPrev(&w->obj.task, gSplineObjWaypointType);
}

/* the segment of w runs from its previous waypoint to w */
static void Waypoint_SetupSegment(Waypoint *w, Waypoint *from, SplineObj *obj)
{
    SplineObjSegment_Setup(w->seg, &from->point, &w->point, &from->rot, &w->rot, w->rigid || w->straight,
                           w->reversed, w->segments, obj->unk20, w->segModel);
}

/* the quaternion of a rotation, {x, y, z, w, stamp} */
static void Rot_ToQuat(float *q, const float *e)
{
    static const int next[3] = {1, 2, 0};
    long double t = (e[8] + e[4]) + e[0];

    if (t > 0.0f) {
        long double s = x87_sqrt(t + 1.0f);

        q[3] = s * kHalf;
        s = kHalf / s;
        q[0] = -((e[5] - e[7]) * s);
        q[1] = -((e[6] - e[2]) * s);
        q[2] = -((e[1] - e[3]) * s);
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
        q[i] = s * kHalf;
        if (!IsZeroL(s))
            s = kHalf / s;
        q[j] = (e[j * 3 + i] + e[i * 3 + j]) * s;
        q[k] = (e[k * 3 + i] + e[i * 3 + k]) * s;
        q[3] = -((e[j * 3 + k] - e[k * 3 + j]) * s);
    }
    memcpy(&q[4], &e[9], 4);
}

/* m = the rotation between segment joints i and i + 1 at t */
static void Segment_BlendRot(Matrix *m, SplineObjSegment *seg, int i, float t)
{
    float q0[5], q1[5], r[5];
    float angle, invSin;
    char flip;
    long double s0, s1, a, b, iz, one;
    float c, d, e, f, g, h;

    Rot_ToQuat(q0, SEG_ROT(seg, i));
    Rot_ToQuat(q1, SEG_ROT(seg, i + 1));
    Quat_SlerpPrepare(&flip, &angle, &invSin, q0, q1);
    if (flip) {
        q1[0] = -q1[0];
        q1[1] = -q1[1];
        q1[2] = -q1[2];
        q1[3] = -q1[3];
    }
    if (IsZero(angle)) {
        memcpy(r, q0, sizeof(r));
    } else {
        int stamp0, stamp1;

        if (!(angle >= kSmallAngle)) {
            s0 = 1.0f - t;
            s1 = t;
        } else {
            float u = angle * t;

            s0 = x87_sin(angle - u) * invSin;
            s1 = x87_sin(u) * invSin;
        }
        r[0] = s1 * q1[0] + s0 * q0[0];
        r[1] = s1 * q1[1] + s0 * q0[1];
        r[2] = s1 * q1[2] + s0 * q0[2];
        r[3] = s1 * q1[3] + s0 * q0[3];
        memcpy(&stamp0, &q0[4], 4);
        memcpy(&stamp1, &q1[4], 4);
        stamp0 = stamp1 + stamp0 + 1;
        memcpy(&r[4], &stamp0, 4);
    }
    a = (long double)r[0] * r[0];
    a = a + a;
    b = (long double)r[1] * r[0];
    b = b + b;
    memcpy(&m->count, &r[4], 4);
    c = 2 * ((long double)r[2] * r[0]);
    d = 2 * ((long double)r[3] * r[0]);
    e = 2 * ((long double)r[1] * r[1]);
    f = 2 * ((long double)r[2] * r[1]);
    g = 2 * ((long double)r[3] * r[1]);
    h = 2 * ((long double)r[2] * r[2]);
    iz = (long double)r[3] * r[2];
    iz = iz + iz;
    m->m[0] = (1.0f - e) - h;
    m->m[1] = b - iz;
    m->m[2] = g + c;
    m->m[3] = iz + b;
    one = 1.0f - a;
    m->m[4] = one - h;
    m->m[5] = f - d;
    m->m[6] = c - g;
    m->m[7] = f + d;
    m->m[8] = one - e;
}

/* v = joint i of the segment in the world, moved by offset in the joint's frame */
static void Segment_GetJoint(Vec3d *v, SplineObjSegment *seg, int i, const float *o)
{
    const float *p = SEG_POS(seg, i);

    v->x = p[0] + seg->pos.x;
    v->y = p[1] + seg->pos.y;
    v->z = p[2] + seg->pos.z;
    if (o) {
        const float *r = SEG_ROT(seg, i);
        long double x = (r[2] * o[2] + r[1] * o[1]) + r[0] * o[0];
        float y = (r[4] * o[1] + r[3] * o[0]) + r[5] * o[2];
        float z = (r[7] * o[1] + r[6] * o[0]) + r[8] * o[2];

        v->x = x + v->x;
        v->y = y + v->y;
        v->z = z + v->z;
    }
}

static void Segment_CopyJoint(Vec3d *out, Matrix *m, SplineObjSegment *seg, int i)
{
    const float *p = SEG_POS(seg, i);

    out->x = p[0] + seg->pos.x;
    out->y = p[1] + seg->pos.y;
    out->z = p[2] + seg->pos.z;
    if (m)
        memcpy(m, SEG_ROT(seg, i), sizeof(Matrix));
}

/* Points along the spline with id: out[0] (and m[0]) at distance dist, wrapped to the
   length, then count more points, each spacing[k] (negative: backwards) in a straight
   line from the one before. offset is added in the local frame of each point. */
void SplineObj_GetPoints(int id, Matrix *m, Vec3d *out, char *found, float dist, int count, const float *spacing,
                         const float *offset)
{
    SplineObj *obj;
    Waypoint *cur;
    SplineObjSegment *seg;
    float acc = 0;
    int i = 0;
    int k;
    long double d;
    float t, target;

    obj = (SplineObj *)Task_FindById(id);
    if (!obj) {
        *found = 0;
        return;
    }
    cur = (Waypoint *)Task_FindChild(&obj->task, gSplineObjWaypointType);
    if (!cur) {
        *found = 0;
        return;
    }
    if (IsZero(obj->length))
        goto at_start;
    d = dist;
    if (!(d >= 0.0f)) {
        do
            d += obj->length;
        while (!(d >= 0.0f));
        dist = d;
    }
    if (d >= obj->length) {
        do
            d -= obj->length;
        while (d >= obj->length);
        dist = d;
    }
    if (IsZeroL(d))
        goto at_start;
    if (!(d > 0.0f)) {
        *found = 0;
        return;
    }

    do {
        cur = Waypoint_Next(cur);
        if (!cur) {
            *found = 0;
            return;
        }
        seg = cur->seg;
        d = (long double)acc + seg->length;
        acc = d;
    } while (!(d >= dist));
    if (!seg) {
        *found = 0;
        return;
    }

    d = (long double)acc - seg->length;
    if (!islessgreater(d, dist)) {
        for (k = count + 1; k > 0; k--)
            Segment_CopyJoint(out++, NULL, seg, 0);
        if (m)
            memcpy(m, SEG_ROT(seg, 0), sizeof(Matrix));
        *found = 1;
        return;
    }
    if (!(d >= dist)) {
        for (;;) {
            if (i >= seg->segments) {
                for (k = count + 1; k > 0; k--)
                    Segment_CopyJoint(out++, NULL, seg, seg->segments);
                if (m)
                    memcpy(m, SEG_ROT(seg, seg->segments), sizeof(Matrix));
                *found = 1;
                return;
            }
            d += SEG_LEN(seg, i);
            i++;
            if (d >= dist)
                break;
        }
    }
    d -= SEG_LEN(seg, i - 1);
    i--;
    acc = d;
    t = (dist - acc) / SEG_LEN(seg, i);
    {
        const float *p0 = SEG_POS(seg, i);
        const float *p1 = SEG_POS(seg, i + 1);
        long double x = (p1[0] - p0[0]) * t + p0[0];
        float y = (p1[1] - p0[1]) * t + p0[1];
        float z = (p1[2] - p0[2]) * t + p0[2];

        out->x = x + seg->pos.x;
        out->y = y + seg->pos.y;
        out->z = z + seg->pos.z;
    }
    if (m)
        Segment_BlendRot(m, seg, i, t);
    if (offset) {
        const float *r = m->m;
        long double x = (r[1] * offset[1] + r[2] * offset[2]) + r[0] * offset[0];
        float y = (r[4] * offset[1] + r[3] * offset[0]) + r[5] * offset[2];
        float z = (r[7] * offset[1] + r[6] * offset[0]) + r[8] * offset[2];

        out->x = x + out->x;
        out->y = y + out->y;
        out->z = z + out->z;
    }

    target = dist;
    for (k = 0; k < count; k++) {
        float sp = spacing[k];
        float sq = sp * sp;
        Vec3d *prev = out;
        Vec3d a, b;
        float d0, len, r, sinA, rest;
        long double d1, ang, c;

        out++;
        target = sp + target;
        if (m)
            m++;
        if (sp < 0.0f) {
            Segment_GetJoint(&a, seg, i, offset);
            if (Vec3d_DistanceSq(prev, &a) > sq) {
                t = (target - acc) / SEG_LEN(seg, i);
                Segment_GetJoint(&a, seg, i, offset);
                Segment_GetJoint(&b, seg, i + 1, offset);
                goto interpolate;
            }
            /* walk back to the joint pair the circle of radius -sp around prev crosses */
            for (;;) {
                if (i) {
                    d = acc - SEG_LEN(seg, i - 1);
                    i--;
                } else {
                    Waypoint *p = Waypoint_Prev(cur);
                    SplineObjSegment *s = p->seg;

                    if (!s)
                        goto off_end;
                    i = s->segments;
                    cur = p;
                    d = acc - SEG_LEN(s, i - 1);
                    i--;
                    seg = s;
                }
                acc = d;
                Segment_GetJoint(&a, seg, i, offset);
                Segment_GetJoint(&b, seg, i + 1, offset);
                d0 = Vec3d_DistanceSq(prev, &a);
                d1 = Vec3d_DistanceSq(prev, &b);
                if ((d0 > sq) != (d1 > sq))
                    break;
            }
            len = SEG_LEN(seg, i);
            r = x87_sqrt(d1);
            ang = Math_ACosF(((d0 - d1) - len * len) / (r * len * kMinusTwo));
            sp = -sp;
            sinA = x87_sin(ang);
            rest = kPi - ang;
            c = rest - Math_ASinF(sinA / sp * r);
            if (c > kTiny) {
                t = 1.0f - x87_sin(c) * sp / sinA / len;
            } else {
                long double r0 = x87_sqrt(d0);

                t = (sp - r0) / (r - r0);
            }
        } else {
            Segment_GetJoint(&b, seg, i + 1, offset);
            if (Vec3d_DistanceSq(prev, &b) > sq) {
                t = (target - acc) / SEG_LEN(seg, i);
                Segment_GetJoint(&a, seg, i, offset);
                Segment_GetJoint(&b, seg, i + 1, offset);
                goto interpolate;
            }
            for (;;) {
                float d1f;

                if (i < seg->segments - 1) {
                    acc = acc + SEG_LEN(seg, i);
                    i++;
                } else {
                    Waypoint *p = Waypoint_Next(cur);

                    if (!p || !p->seg)
                        goto off_end;
                    acc = acc + SEG_LEN(seg, i);
                    i = 0;
                    cur = p;
                    seg = p->seg;
                }
                Segment_GetJoint(&a, seg, i, offset);
                Segment_GetJoint(&b, seg, i + 1, offset);
                d0 = Vec3d_DistanceSq(prev, &a);
                d1f = Vec3d_DistanceSq(prev, &b);
                d1 = d1f;
                if ((d0 > sq) != (d1f > sq))
                    break;
            }
            len = SEG_LEN(seg, i);
            r = x87_sqrt(d0);
            ang = Math_ACosF(((d1 - d0) - len * len) / (r * len * kMinusTwo));
            sinA = x87_sin(ang);
            rest = kPi - ang;
            c = rest - Math_ASinF(sinA / sp * r);
            if (c > kTiny)
                t = x87_sin(c) * sp / sinA / len;
            else
                t = (sp - r) / (x87_sqrt(d1) - r);
        }
    interpolate:
        out->x = (b.x - a.x) * t + a.x;
        out->y = (b.y - a.y) * t + a.y;
        out->z = (b.z - a.z) * t + a.z;
        if (m)
            Segment_BlendRot(m, seg, i, t);
        continue;
    off_end:
        Segment_CopyJoint(out, m, seg, sp < 0.0f ? 0 : seg->segments);
    }
    *found = 1;
    return;

at_start:
    if (m)
        memcpy(m, &cur->rot, sizeof(Matrix));
    for (k = count + 1; k > 0; k--)
        memcpy(out++, &cur->point, sizeof(Vec3d));
    *found = 1;
}
HOOK(0x4FEC10, SplineObj_GetPoints);

/* a new waypoint copies its neighbour and is placed one segment length beyond it */
void SplineObjWaypoint_Create(Waypoint *w)
{
    SplineObj *obj = (SplineObj *)w->obj.task.parent;
    Waypoint *p = Waypoint_Prev(w);
    char notify = 0;

    if (p) {
        long double x;
        float y, z;

        strcpy(w->modelName, p->modelName);
        strcpy(w->segModel, p->segModel);
        memcpy(&w->point, &p->point, sizeof(Vec3d));
        memcpy(&w->rot, &p->rot, sizeof(Matrix));
        x = w->rot.m[0];
        w->straight = p->straight;
        w->autoOrient = p->autoOrient;
        y = w->rot.m[3];
        z = w->rot.m[6];
        if (!IsZero(w->rot.m[0]) || !IsZero(y) || !IsZero(z)) {
            long double inv = 1.0L / x87_sqrt(x * x + y * y + z * z);

            x = x * inv;
            y = y * inv;
            z = z * inv;
        }
        if (p->seg) {
            long double len = (long double)p->seg->end - p->seg->start;
            float d = len;

            x = x * len;
            y = d * y;
            z = d * z;
        } else {
            x = x * kDefaultSegLength;
            y = kDefaultSegLength * y;
            z = kDefaultSegLength * z;
        }
        w->point.x = x + w->point.x;
        w->point.y = y + w->point.y;
        w->point.z = z + w->point.z;
    } else if ((p = Waypoint_Next(w)) != NULL) {
        Waypoint *pn;
        long double dx;
        float x, y, z;

        strcpy(w->modelName, p->modelName);
        strcpy(w->segModel, p->segModel);
        memcpy(&w->point, &p->point, sizeof(Vec3d));
        memcpy(&w->rot, &p->rot, sizeof(Matrix));
        x = w->rot.m[0];
        z = w->rot.m[6];
        w->straight = p->straight;
        y = w->rot.m[3];
        if (!IsZero(x) || !IsZero(y) || !IsZero(z)) {
            long double inv = 1.0L / x87_sqrt(x * x + y * y + z * z);

            x = x * inv;
            y = y * inv;
            z = z * inv;
        }
        pn = Waypoint_Next(p);
        if (!pn) {
            dx = kDefaultSegLength * x;
            y = kDefaultSegLength * y;
            z = kDefaultSegLength * z;
        } else if (pn->seg) {
            long double len = (long double)pn->seg->end - pn->seg->start;
            float d = len;

            dx = len * x;
            y = d * y;
            z = d * z;
        } else {
            dx = x;
        }
        notify = 1;
        w->point.x = w->point.x - dx;
        w->point.y = w->point.y - y;
        w->point.z = w->point.z - z;
    } else {
        static const Matrix identity = {{1.0f, 0, 0, 0, 1.0f, 0, 0, 0, 1.0f}, 0};

        w->modelName[0] = gEmptyString;
        memset(&w->point, 0, sizeof(Vec3d));
        w->segModel[0] = gEmptyString;
        memcpy(&w->rot, &identity, sizeof(Matrix));
        w->straight = 0;
        w->autoOrient = 0;
    }
    w->seg = NULL;
    w->rigid = obj->rigid;
    w->segments = obj->maxSegments;
    w->reversed = 0;
    w->inTree = 0;
    RigidDynCubeObj_Init(gCurrentWorld, w, VirModel_Find(w->modelName));
    w->tree = gCurrentWorld;
    if (notify) {
        uint16_t type = Waypoint_Next(w)->obj.task.type;
        TaskFn fn = gTaskHandlers[Param_GetChangedEvent()][type];

        fn(Waypoint_Next(w), NULL);
    }
}
HOOK(0x5008F0, SplineObjWaypoint_Create);

void SplineObjWaypoint_Delete(Waypoint *w)
{
    SplineObj *obj = (SplineObj *)w->obj.task.parent;
    Waypoint *next = Waypoint_Next(w);
    Waypoint *prev = Waypoint_Prev(w);

    /* looks for a waypoint below this one, so it finds none */
    w->seg = (SplineObjSegment *)Task_FindChild(&w->obj.task, gSplineObjWaypointType);
    if (w->seg)
        MenuManager_Free(w->seg);
    if (prev) {
        if (next)
            Waypoint_SetupSegment(next, prev, obj);
    } else if (next) {
        MenuManager_Free(next->seg);
        next->seg = NULL;
    }
    if (w->inTree)
        DynCube_Remove(w->tree, w);
}
HOOK(0x500DE0, SplineObjWaypoint_Delete);

/* normalizes v when it is not zero, summing the squares in the order a, b, c */
static void Vec3f_NormalizeOrdered(float *v, int a, int b, int c)
{
    if (!IsZero(v[0]) || !IsZero(v[1]) || !IsZero(v[2])) {
        long double inv = 1.0L / x87_sqrt((long double)v[a] * v[a] + (long double)v[b] * v[b] + (long double)v[c] * v[c]);

        v[0] = v[0] * inv;
        v[1] = v[1] * inv;
        v[2] = v[2] * inv;
    }
}

/* builds the rotation from the direction prev -> next */
static void Waypoint_AutoOrient(Waypoint *w, Waypoint *prev, Waypoint *next)
{
    float *r = w->rot.m;
    float dir[3] = {0, 0, 0};
    float u[3], c[3], d[3];
    Matrix m;

    if (prev) {
        dir[0] = w->point.x - prev->point.x;
        dir[1] = w->point.y - prev->point.y;
        dir[2] = w->point.z - prev->point.z;
    }
    if (next) {
        float dy = next->point.y - w->point.y;
        float dz = next->point.z - w->point.z;

        dir[0] = (next->point.x - w->point.x) + dir[0];
        dir[1] = dy + dir[1];
        dir[2] = dz + dir[2];
    }
    if (IsZero(dir[0]) && IsZero(dir[1]) && IsZero(dir[2])) {
        r[1] = 0;
        r[2] = 0;
        r[0] = 1.0f;
    } else {
        memcpy(r, dir, sizeof(dir));
    }
    if (IsZero(r[0]) && IsZero(r[1])) {
        r[3] = 0;
        r[4] = 1.0f;
        r[5] = 0;
    } else {
        long double b = r[1];
        long double a;

        r[5] = 0;
        r[3] = b;
        a = -(long double)r[0];
        r[4] = a;
        if (!IsZero(b) || !IsZero(a)) {
            long double inv;

            r[5] = 0;
            inv = 1.0L / x87_sqrt(b * b + a * a);
            r[3] = b * inv;
            r[4] = a * inv;
        }
    }
    {
        float t0 = r[4] * r[2] - r[5] * r[1];
        float t1 = -(r[3] * r[2] - r[5] * r[0]);
        float t2 = r[3] * r[1] - r[4] * r[0];

        r[6] = t0;
        r[7] = t1;
        r[8] = t2;
    }
    memcpy(u, r, sizeof(u));
    Vec3f_NormalizeOrdered(u, 0, 1, 2);
    c[0] = r[2] * r[7] - r[8] * r[1];
    c[1] = -(r[2] * r[6] - r[8] * r[0]);
    c[2] = r[1] * r[6] - r[0] * r[7];
    d[0] = c[2] * u[1] - c[1] * u[2];
    d[1] = -(c[2] * u[0] - c[0] * u[2]);
    d[2] = c[1] * u[0] - c[0] * u[1];
    Vec3f_NormalizeOrdered(c, 2, 1, 0);
    Vec3f_NormalizeOrdered(d, 0, 1, 2);
    m.m[0] = u[0];
    m.m[1] = c[0];
    m.m[2] = d[0];
    m.m[3] = u[1];
    m.m[4] = c[1];
    m.m[5] = d[1];
    m.m[6] = u[2];
    m.m[7] = c[2];
    m.m[8] = d[2];
    m.count = 0;
    memcpy(&w->rot, &m, sizeof(Matrix));
}

static float Waypoint_MaxScale(Waypoint *w)
{
    float m = w->obj.scale[2] >= w->obj.scale[1] ? w->obj.scale[2] : w->obj.scale[1];

    return w->obj.scale[0] > m ? w->obj.scale[0] : m;
}

/* lines up a run of "straight" waypoints evenly between its two ends */
static void Waypoint_Straighten(Waypoint *w, Waypoint *next, SplineObj *obj)
{
    Waypoint *first, *last, *p, *cur;
    int count = 0;
    long double dx, inv;
    float invf, pitch, yaw;
    float step[3];
    Vec3d pos;
    Matrix m;

    if (w->straight)
        first = last = w;
    else
        first = last = next;
    p = Waypoint_Prev(first);
    if (first->straight) {
        while (p) {
            first = p;
            p = Waypoint_Prev(p);
            count++;
            if (!first->straight)
                break;
        }
    }
    for (p = Waypoint_Next(last); p && p->straight; p = Waypoint_Next(p)) {
        last = p;
        count++;
    }
    if (first == last)
        return;

    cur = Waypoint_Next(first);
    dx = last->point.x - first->point.x;
    step[1] = last->point.y - first->point.y;
    step[2] = last->point.z - first->point.z;
    inv = 1.0f / (long double)count;
    invf = inv;
    step[0] = inv * dx;
    step[1] = invf * step[1];
    step[2] = invf * step[2];
    Vec3_ToAngles((Vec3f *)step, &pitch, &yaw);
    Matrix_SetEuler(&m, 0.0f, pitch, yaw);
    memcpy(&pos, &first->point, sizeof(Vec3d));
    memcpy(&first->rot, &m, sizeof(Matrix));
    memcpy(&first->obj.matrix, &m, sizeof(Matrix));
    if (first->seg)
        Waypoint_SetupSegment(first, Waypoint_Prev(first), obj);

    for (; cur != last; cur = Waypoint_Next(cur)) {
        char inTree = cur->inTree;

        memcpy(&cur->rot, &m, sizeof(Matrix));
        pos.x = step[0] + pos.x;
        pos.y = step[1] + pos.y;
        pos.z = step[2] + pos.z;
        memcpy(&cur->point, &pos, sizeof(Vec3d));
        memcpy(&cur->obj.matrix, &m, sizeof(Matrix));
        if (inTree)
            DynCube_Move(cur->tree, cur, &cur->point, VirModel_GetRadius(cur->obj.model));
        if (cur->seg)
            Waypoint_SetupSegment(cur, Waypoint_Prev(cur), obj);
    }

    memcpy(&last->rot, &m, sizeof(Matrix));
    memcpy(&last->obj.matrix, &m, sizeof(Matrix));
    if (last->seg)
        Waypoint_SetupSegment(last, Waypoint_Prev(last), obj);
    p = Waypoint_Next(last);
    if (p && p->seg)
        Waypoint_SetupSegment(p, last, obj);
}

/* params changed: orient, clamp, snap, place in the world and rebuild the segments around it */
void SplineObjWaypoint_Changed(Waypoint *w)
{
    Waypoint *next = Waypoint_Next(w);
    Waypoint *prev = Waypoint_Prev(w);
    SplineObj *obj = (SplineObj *)w->obj.task.parent;
    Waypoint *p;
    int n;

    if ((obj->autoOrient || w->autoOrient) && (prev || next) && !obj->rigid)
        Waypoint_AutoOrient(w, prev, next);
    w->rigid = obj->rigid || w->straight;

    n = w->segments < obj->maxSegments ? w->segments : obj->maxSegments;
    w->segments = n > 20 ? 20 : n;

    if (obj->snap && prev && w->seg) {
        float dx = w->point.x - prev->point.x;
        float dy = w->point.y - prev->point.y;
        long double dz = w->point.z - prev->point.z;
        long double len = (long double)w->seg->end - w->seg->start;
        long double k = len / x87_sqrt(dx * dx + dy * dy + dz * dz);

        dx = dx * k;
        dy = dy * k;
        w->point.x = dx + prev->point.x;
        w->point.y = dy + prev->point.y;
        w->point.z = dz * k + prev->point.z;
    }

    if (w->inTree) {
        if (!obj->collide) {
            DynCube_Remove(w->obj.world, w);
            w->inTree = 0;
        }
    } else if (obj->collide) {
        float scale = Waypoint_MaxScale(w);

        DynCube_Add(gCurrentWorld, w, &w->point, VirModel_GetRadius(w->obj.model) * scale, 1);
        w->inTree = 1;
    }

    memcpy(&w->obj.matrix, &w->rot, sizeof(Matrix));
    if (w->modelName[0] != '*' && w->modelName[0]) {
        void **model = VirModel_Find(w->modelName);

        if (model) {
            w->obj.model = model;
        } else {
            char name[0x38];

            strcpy(name, w->modelName);
            crt_sprintf(w->modelName, "*%s", name);
        }
    }
    if (w->inTree)
        DynCube_Move(w->tree, w, &w->point, VirModel_GetRadius(w->obj.model));

    if (prev) {
        if (!w->seg)
            w->seg = SplineObjSegment_New(w, w->tree, &prev->point, &w->point, &prev->rot, &w->rot,
                                          w->rigid || w->straight, w->reversed, w->segments, obj->unk20,
                                          w->segModel);
        else
            Waypoint_SetupSegment(w, prev, obj);
    } else if (w->seg) {
        MenuManager_Free(w->seg);
        w->seg = NULL;
    }
    if (next && next->seg)
        Waypoint_SetupSegment(next, w, obj);

    if (w->straight || (next && next->straight))
        Waypoint_Straighten(w, next, obj);

    p = (Waypoint *)Task_FindChild(&obj->task, gSplineObjWaypointType);
    obj->length = 0;
    if (p) {
        for (p = Waypoint_Next(p); p; p = Waypoint_Next(p))
            if (p->seg)
                obj->length = p->seg->length + obj->length;
    }
    if (w->seg) {
        memcpy(&w->seg->color1, &obj->color1, sizeof(Vec3f));
        memcpy(&w->seg->color2, &obj->color2, sizeof(Vec3f));
    }
    SplineObjWaypoint_UpdateAttach(w);
}
HOOK(0x500EF0, SplineObjWaypoint_Changed);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533400) const float kPi INIT(3.1415927f);
DATA(533714) const float kSmallAngle INIT(0.01f);
ALIAS(kHundredth, kSmallAngle, 0);
DATA(534BA0) const double kTiny INIT(0.001);
DATA(534BA0) const float kMinusTwo INIT(-2.0f);
PAD_CONST(igd, 534BA0, 534BAC, 0x4);
DATA(54E5D4) const float kDefaultSegLength INIT(141314.9f);
