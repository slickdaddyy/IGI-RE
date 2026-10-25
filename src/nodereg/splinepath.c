/* SplinePathDynCubeObj and its SplinePathNodeQTask children: a Hermite
   spline through the node positions, with the tangents, segment lengths
   and node rotations worked out whenever a node changes. */
#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <string.h>

#include "hook.h"
#include "nodereg/splinepath.h"
#include "sound/soundvars.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B3320, char, Vec3d_IsZero, (const Vec3d *v));
ORIG(0x4B44D0, void, Quat_SlerpPrepare, (char *flip, float *angle, float *inv_sin, const float *a, const float *b));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F50, int, Param_Real32, (void));
ORIG(0x4F0F70, int, Param_Real32x9, (void));
ORIG(0x4F1000, int, Param_Bool8, (void));
ORIG(0x4F1010, int, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, int type, int flags, void *extra));

extern uint16_t gSplinePathNodeType;
extern uint16_t gSplinePathType;
extern void *gSplinePathNodeParams;
extern void *gSplinePathParams;

static const double kOne = 1.0;
static const double kHalf = 0.5;
static const float kZeroF = 0.0f;
static const float kOneF = 1.0f;
static const float kHalfF = 0.5f;
static const float kTwoNegF = -2.0f;
static const float kThreeF = 3.0f;
static const float kFourF = 4.0f;
static const float kSixF = 6.0f;
static const float kSixNegF = -6.0f;
static const float kTenF = 10.0f;
static const float kSlerpMin = 0x1.47ae14p-7f;     /* 0.01 */
static const float kSpeedScale = 0x1.1c71c8p-2f;   /* 1 / 3.6 */
static const float kTimeScale = 0x1p-12f;
static const float kTenth = 0x1.99999ap-4f;
static const float kStepFine = 0x1.0624dep-10f;    /* 0.001 */
static const float kStepFirst = 0x1.0624dep-9f;    /* 0.002 */
static const float kNodeRadius = 20480.0f;

static const char kNotOnPath[] = "Parent must be of type SplinePathDynCubeObj";

/* nonzero and not NaN, like the fcom / test ah,0x40 checks */
static int nonzero(long double v)
{
    return v < 0 || v > 0;
}

static long double x87_sin(long double x)
{
    long double r;

    __asm__("fsin" : "=t"(r) : "0"(x));
    return r;
}

/* fchs, so gcc can't turn -(a - b) into b - a (they round apart in round-down) */
static long double x87_neg(long double x)
{
    __asm__("fchs" : "+t"(x));
    return x;
}

void SplinePathNode_Create(SplinePathNode *n);
void SplinePathNode_Delete(SplinePathNode *n);
void SplinePathNode_ParamsChanged(SplinePathNode *n);
void SplinePathNode_GetParamTable(SplinePathNode *n, ParamTable **out);
void SplinePathNode_GetInfo(SplinePathNode *n, ObjInfo *info);

void SplinePathNode_Open(void)
{
    SplinePathNode p;
    uint16_t t = TaskType_New("SplinePathNodeQTask", sizeof(SplinePathNode), Task_GetRootType(), 0, 0);

    gSplinePathNodeType = t;
    TaskType_SetHandler(t, 0, SplinePathNode_Create);
    TaskType_SetHandler2(gSplinePathNodeType, 2, SplinePathNode_Delete);
    TaskType_SetHandler(gSplinePathNodeType, Param_GetChangedEvent(), SplinePathNode_ParamsChanged);
    TaskType_SetHandler(gSplinePathNodeType, Param_GetTableEvent(), SplinePathNode_GetParamTable);
    TaskType_SetHandler(gSplinePathNodeType, 7, SplinePathNode_GetInfo);
    gSplinePathNodeParams = ParamTable_New(6);
    ParamTable_Add(gSplinePathNodeParams, &p, &p.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSplinePathNodeParams, &p, &p.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gSplinePathNodeParams, &p, &p.speed, Param_Real32(), 7, NULL);
    ParamTable_Add(gSplinePathNodeParams, &p, &p.time, Param_Real32(), 7, NULL);
    ParamTable_Add(gSplinePathNodeParams, &p, &p.tangent, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gSplinePathNodeParams, &p, &p.autoTangent, Param_Bool8(), 7, NULL);
}
HOOK(0x519850, SplinePathNode_Open);

void SplinePathNode_GetInfo(SplinePathNode *n, ObjInfo *info)
{
    info->pos = n->pos;
    info->velocity = (Vec3f){0};
    info->matrix = n->matrix;
    info->radius = kNodeRadius;
    if (info->name)
        strcpy(info->name, "SplinePathNode");
}
HOOK(0x5199D0, SplinePathNode_GetInfo);

void SplinePathNode_GetParamTable(SplinePathNode *n, ParamTable **out)
{
    *out = gSplinePathNodeParams;
}
HOOK(0x519A50, SplinePathNode_GetParamTable);

void SplinePathNode_ParamsChanged(SplinePathNode *n)
{
    SplinePath *p = (SplinePath *)n->task.parent;

    if (p->task.type != gSplinePathType)
        Warning_Show(kNotOnPath);
    n->speed = n->speed > kZeroF ? n->speed : kZeroF;
    SplinePath_CollectNodes(p);
    SplinePath_UpdateLengths(p);
    SplinePath_UpdateRotations(p);
}
HOOK(0x519A60, SplinePathNode_ParamsChanged);

void SplinePathNode_Create(SplinePathNode *n)
{
    int i;

    if (n->task.parent->type != gSplinePathType)
        Warning_Show(kNotOnPath);
    n->matrix.m[0] = 1;
    n->matrix.m[4] = 1;
    n->matrix.m[8] = 1;
    n->quat.w = 1;
    n->pos.x = n->pos.y = n->pos.z = 0;
    n->tangent.x = n->tangent.y = n->tangent.z = 0;
    n->matrix.m[1] = 0;
    n->matrix.m[2] = 0;
    n->matrix.m[3] = 0;
    n->matrix.m[5] = 0;
    n->matrix.m[6] = 0;
    n->matrix.m[7] = 0;
    n->matrix.count = 0;
    n->quat.x = 0;
    n->quat.y = 0;
    n->quat.z = 0;
    n->quat.unk10 = 0;
    n->length = 0;
    n->speed = 0;
    n->time = 0;
    n->autoTangent = 1;
    for (i = 0; i < SPLINE_SUBSEGS; i++)
        n->segLength[i] = 0;
}
HOOK(0x519AD0, SplinePathNode_Create);

void SplinePathNode_Delete(SplinePathNode *n)
{
    SplinePath_RemoveNode((SplinePath *)n->task.parent, n);
}
HOOK(0x519B80, SplinePathNode_Delete);

void SplinePathNode_Close(void)
{
    QTaskList_Free(gSplinePathNodeParams);
    TaskType_Free(gSplinePathNodeType);
    gSplinePathNodeType = TASK_NO_TYPE;
}
HOOK(0x519BA0, SplinePathNode_Close);

/* builds the matrix from the tangent (column 1) and the old up vector
   (column 2) */
void SplinePathNode_UpdateMatrix(SplinePathNode *n)
{
    float *m = n->matrix.m;
    long double x, y, z, inv;
    float yf, zf, s0, s1, s2, f0, f1, f2;

    if (Vec3d_IsZero(&n->tangent)) {
        m[0] = 1;
        m[1] = 0;
        m[2] = 0;
        m[3] = 0;
        m[4] = 1;
        m[5] = 0;
        m[6] = 0;
        m[7] = 0;
        m[8] = 1;
        n->matrix.count = 0;
        return;
    }
    x = n->tangent.x;
    y = n->tangent.y;
    z = n->tangent.z;
    yf = y;
    zf = z;
    s0 = y * m[8] - z * m[5];
    s1 = -(x * m[8] - z * m[2]);
    s2 = x * m[5] - y * m[2];
    if (!nonzero(s0) && !nonzero(s1) && !nonzero(s2)) {
        f0 = 0;
        f1 = -zf;
        f2 = yf;
        s0 = yf * f2 - zf * f1;
        s1 = -(x * f2 - zf * f0);
        s2 = x * f1 - yf * f0;
    } else {
        f0 = s1 * z - s2 * y;
        f1 = -(s0 * z - s2 * x);
        f2 = s0 * y - s1 * x;
    }
    if (nonzero(f0) || nonzero(f1) || nonzero(f2)) {
        inv = kOne / sqrtl((long double)f0 * f0 + (long double)f1 * f1 + (long double)f2 * f2);
        f0 = f0 * inv;
        f1 = f1 * inv;
        f2 = f2 * inv;
    }
    if (nonzero(s0) || nonzero(s1) || nonzero(s2)) {
        inv = kOne / sqrtl((long double)s0 * s0 + (long double)s1 * s1 + (long double)s2 * s2);
        s0 = s0 * inv;
        s1 = s1 * inv;
        s2 = s2 * inv;
    }
    if (nonzero(x) || nonzero(yf) || nonzero(zf)) {
        inv = kOne / sqrtl(x * x + (long double)yf * yf + (long double)zf * zf);
        x = x * inv;
        yf = yf * inv;
        zf = zf * inv;
    }
    m[0] = s0;
    m[3] = s1;
    m[6] = s2;
    m[4] = yf;
    m[7] = zf;
    m[2] = f0;
    m[1] = x;
    m[5] = f1;
    m[8] = f2;
}
HOOK(0x519BD0, SplinePathNode_UpdateMatrix);

/* 1 / (10 * length) of the tenth of the segment that u falls in */
long double SplinePathNode_GetStepScale(SplinePathNode *n, float u)
{
    int i = x87_fistp(u * kTenF);

    if (i < 0)
        i = 0;
    else if (i > SPLINE_SUBSEGS - 1)
        i = SPLINE_SUBSEGS - 1;
    return kOneF / (n->segLength[i] * kTenF);
}
HOOK(0x51A0C0, SplinePathNode_GetStepScale);

/* the node children in list order, at most SPLINE_NODES */
void SplinePath_CollectNodes(SplinePath *p)
{
    Task *c = p->task.head;
    Task *next;

    p->count = 0;
    for (next = c->next; next; next = next->next) {
        if (c->type == gSplinePathNodeType && p->count < SPLINE_NODES)
            p->nodes[p->count++] = (SplinePathNode *)c;
        c = next;
    }
}
HOOK(0x51A110, SplinePath_CollectNodes);

void SplinePath_RemoveNode(SplinePath *p, SplinePathNode *n)
{
    int i;

    for (i = 0; i < p->count; i++)
        if (p->nodes[i] == n)
            break;
    if (i >= p->count)
        return;
    for (; i < p->count - 1; i++)
        p->nodes[i] = p->nodes[i + 1];
    p->nodes[p->count - 1] = NULL;
    p->count--;
    SplinePath_UpdateLengths(p);
}
HOOK(0x51A160, SplinePath_RemoveNode);

void SplinePath_Create(SplinePath *p);
void SplinePath_GetParamTable(SplinePath *p, ParamTable **out);

void SplinePath_Open(void)
{
    uint16_t t = TaskType_New("SplinePathDynCubeObj", sizeof(SplinePath), gDynCubeObjType, 0, 0);

    gSplinePathType = t;
    TaskType_SetHandler(t, 0, SplinePath_Create);
    TaskType_SetHandler2(gSplinePathType, 2, TASK_NOTHING);
    TaskType_SetHandler(gSplinePathType, Param_GetTableEvent(), SplinePath_GetParamTable);
}
HOOK(0x51A1D0, SplinePath_Open);

void SplinePath_GetParamTable(SplinePath *p, ParamTable **out)
{
    *out = gSplinePathParams;
}
HOOK(0x51A230, SplinePath_GetParamTable);

void SplinePath_Create(SplinePath *p)
{
    memset(p->nodes, 0, sizeof(p->nodes));
    p->count = 0;
    p->unk1FC = 0;
}
HOOK(0x51A240, SplinePath_Create);

void SplinePath_Close(void)
{
    TaskType_Free(gSplinePathType);
    gSplinePathType = TASK_NO_TYPE;
}
HOOK(0x51A260, SplinePath_Close);

/* the point at u in [0, 1] on segment i */
Vec3d *SplinePath_GetPos(Vec3d *out, SplinePath *p, int i, float u)
{
    SplinePathNode *a = p->nodes[i];
    SplinePathNode *b = p->nodes[i + 1];
    long double u2 = u * u;
    float u3 = u2 * u;
    float h10 = u3 - (u2 + u2) + u;
    float u23 = u2 * kThreeF;
    float h00 = u3 + u3 - u23 + kOneF;
    long double h11 = u3 - u2;
    long double h01 = u3 * kTwoNegF + u23;
    Vec3d r;

    r.x = h01 * b->pos.x + h11 * b->tangent.x + h00 * a->pos.x + h10 * a->tangent.x;
    r.y = h01 * b->pos.y + h11 * b->tangent.y + h00 * a->pos.y + h10 * a->tangent.y;
    r.z = h01 * b->pos.z + h11 * b->tangent.z + h00 * a->pos.z + h10 * a->tangent.z;
    *out = r;
    return out;
}
HOOK(0x51A280, SplinePath_GetPos);

/* the derivative at u on segment i */
float *SplinePath_GetTangent(float *out, SplinePath *p, int i, float u)
{
    SplinePathNode *a = p->nodes[i];
    SplinePathNode *b = p->nodes[i + 1];
    long double u2 = u * u;
    float u23 = u2 * kThreeF;
    float h00 = u23 - u * kFourF + kOneF;
    long double u6 = u * kSixF;
    float h01 = u2 * kSixNegF + u6;
    long double h11 = u2 * kSixF - u6;
    float h10 = u23 - (u + u);
    float x, y, z;

    x = h11 * a->pos.x + h01 * b->pos.x + b->tangent.x * h10 + a->tangent.x * h00;
    y = h11 * a->pos.y + h01 * b->pos.y + b->tangent.y * h10 + a->tangent.y * h00;
    z = h11 * a->pos.z + h01 * b->pos.z + b->tangent.z * h10 + a->tangent.z * h00;
    out[0] = x;
    out[1] = y;
    out[2] = z;
    return out;
}
HOOK(0x51A370, SplinePath_GetTangent);

/* the rotation at u on segment i: the node quaternions slerped, with
   column 1 turned to follow the tangent */
void SplinePath_GetMatrix(SplinePath *p, float *out, int i, float u)
{
    SplinePathNode *a = p->nodes[i];
    SplinePathNode *b = p->nodes[i + 1];
    char flip;
    float angle, invSin, at;
    float q1[5], q[5], c0, c1, c2, ty, tz, u0, u1, u2, v0, v1, v2;
    long double w, s0, s1, tx, inv, uz, vz;
    float *t;

    Quat_SlerpPrepare(&flip, &angle, &invSin, &a->quat.x, &b->quat.x);
    if (flip) {
        q1[0] = -b->quat.x;
        q1[1] = -b->quat.y;
        q1[2] = -b->quat.z;
        q1[3] = -b->quat.w;
    } else {
        memcpy(q1, &b->quat, sizeof(q1));
    }
    if (!nonzero(angle)) {
        memcpy(q, &a->quat, sizeof(q));
        w = q[3];
    } else {
        if (!(angle >= kSlerpMin)) {
            s0 = kOneF - u;
            s1 = u;
        } else {
            at = angle * u;
            s0 = x87_sin(angle - at) * invSin;
            s1 = x87_sin(at) * invSin;
        }
        q[0] = s0 * a->quat.x + s1 * q1[0];
        q[1] = s0 * a->quat.y + s1 * q1[1];
        q[2] = s0 * a->quat.z + s1 * q1[2];
        w = s0 * a->quat.w + s1 * q1[3];
    }
    c0 = w * q[1] + w * q[1] + (q[2] * q[0] + q[2] * q[0]);
    c1 = q[2] * q[1] + q[2] * q[1] - (w * q[0] + w * q[0]);
    c2 = kOneF - (q[0] * q[0] + q[0] * q[0]) - (q[1] * q[1] + q[1] * q[1]);
    t = SplinePath_GetTangent(q, p, i, u);
    tx = t[0];
    ty = t[1];
    tz = t[2];
    if (nonzero(tx) || nonzero(ty) || nonzero(tz)) {
        inv = kOne / sqrtl(tx * tx + (long double)ty * ty + (long double)tz * tz);
        tx = tx * inv;
        ty = ty * inv;
        tz = tz * inv;
    }
    u0 = ty * c2 - tz * c1;
    u1 = x87_neg(tx * c2 - tz * c0);
    u2 = tx * c1 - ty * c0;
    if (nonzero(u0) || nonzero(u1) || nonzero(u2)) {
        inv = kOne / sqrtl((long double)u0 * u0 + (long double)u1 * u1 + (long double)u2 * u2);
        u0 = u0 * inv;
        u1 = u1 * inv;
        uz = inv * u2;
    } else {
        uz = u2;
    }
    v0 = u1 * tz - uz * ty;
    v1 = x87_neg(u0 * tz - uz * tx);
    v2 = u0 * ty - u1 * tx;
    if (nonzero(v0) || nonzero(v1) || nonzero(v2)) {
        inv = kOne / sqrtl((long double)v0 * v0 + (long double)v1 * v1 + (long double)v2 * v2);
        v0 = v0 * inv;
        v1 = v1 * inv;
        vz = inv * v2;
    } else {
        vz = v2;
    }
    out[0] = u0;
    out[6] = uz;
    out[3] = u1;
    out[1] = tx;
    out[4] = ty;
    out[8] = vz;
    out[7] = tz;
    out[2] = v0;
    out[5] = v1;
}
HOOK(0x51A4A0, SplinePath_GetMatrix);

static long double step_length(const Vec3d *a, const Vec3d *b)
{
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    long double dz = a->z - b->z;

    return sqrtl((long double)dx * dx + (long double)dy * dy + dz * dz);
}

/* sets the automatic tangents, then measures each segment by stepping
   along it */
void SplinePath_UpdateLengths(SplinePath *p)
{
    int n = p->count;
    float step = kStepFirst;
    int i, j, k;

    if (n < 3)
        return;
    if (p->nodes[0]->autoTangent) {
        SplinePathNode *a = p->nodes[0], *b = p->nodes[1];

        a->tangent.x = b->pos.x - a->pos.x;
        a->tangent.y = b->pos.y - a->pos.y;
        a->tangent.z = b->pos.z - a->pos.z;
    }
    if (p->nodes[n - 1]->autoTangent) {
        SplinePathNode *a = p->nodes[n - 2], *b = p->nodes[n - 1];

        b->tangent.x = b->pos.x - a->pos.x;
        b->tangent.y = b->pos.y - a->pos.y;
        b->tangent.z = b->pos.z - a->pos.z;
    }
    for (i = 1; i < n - 1; i++) {
        SplinePathNode *a = p->nodes[i - 1], *b = p->nodes[i], *c = p->nodes[i + 1];

        if (!b->autoTangent)
            continue;
        b->tangent.x = c->pos.x - a->pos.x;
        b->tangent.y = c->pos.y - a->pos.y;
        b->tangent.z = c->pos.z - a->pos.z;
        b->tangent.x = b->tangent.x * kHalf;
        b->tangent.y = b->tangent.y * kHalf;
        b->tangent.z = b->tangent.z * kHalf;
    }
    p->nodes[n - 1]->length = 0;
    p->nodes[0]->time = 0;
    for (i = 0; i < n - 1; i++) {
        SplinePathNode *a, *b;
        Vec3d pt[2], tmp;
        float u = 0;
        long double speed;

        p->nodes[i]->length = 0;
        pt[0] = p->nodes[i]->pos;
        pt[1] = p->nodes[i]->pos;
        for (k = 0; k < 500; k++) {
            u = u + step;
            pt[k & 1] = *SplinePath_GetPos(&tmp, p, i, u);
            p->nodes[i]->length = step_length(&pt[0], &pt[1]) + p->nodes[i]->length;
        }
        a = p->nodes[i];
        b = p->nodes[i + 1];
        speed = (b->speed + a->speed) * kHalfF * kSpeedScale;
        if (speed > 0)
            b->time = a->length * kTimeScale / speed + a->time;
        else
            b->time = 0;
        /* every segment after the first is only measured to its middle */
        step = kStepFine;
        for (j = 0; j < SPLINE_SUBSEGS; j++) {
            p->nodes[i]->segLength[j] = 0;
            u = j * kTenth;
            pt[0] = *SplinePath_GetPos(&tmp, p, i, u);
            pt[1] = pt[0];
            for (k = 0; k < 100; k++) {
                u = u + kStepFine;
                pt[k & 1] = *SplinePath_GetPos(&tmp, p, i, u);
                p->nodes[i]->segLength[j] = step_length(&pt[0], &pt[1]) + p->nodes[i]->segLength[j];
            }
        }
    }
}
HOOK(0x51A8C0, SplinePath_UpdateLengths);

/* the node matrices from their tangents, and quaternions from those */
void SplinePath_UpdateRotations(SplinePath *p)
{
    static const int next[3] = {1, 2, 0};
    int i;

    for (i = 0; i < p->count; i++) {
        SplinePathNode *n = p->nodes[i];
        float *m;
        float *q;
        long double tr, s;

        SplinePathNode_UpdateMatrix(n);
        n = p->nodes[i];
        m = n->matrix.m;
        q = &n->quat.x;
        tr = m[8] + m[4] + m[0];
        if (tr > 0) {
            s = sqrtl(tr + kOneF);
            n->quat.w = s * kHalfF;
            s = kHalfF / s;
            n->quat.x = -((m[5] - m[7]) * s);
            n->quat.y = -((m[6] - m[2]) * s);
            n->quat.z = -((m[1] - m[3]) * s);
        } else {
            int a = m[4] > m[0];
            int b, c;

            if (m[8] > m[a * 4])
                a = 2;
            b = next[a];
            c = next[b];
            s = sqrtl(m[a * 4] - m[b * 4] - m[c * 4] + kOneF);
            q[a] = s * kHalfF;
            if (nonzero(s))
                s = kHalfF / s;
            q[b] = (m[b * 3 + a] + m[a * 3 + b]) * s;
            q[c] = (m[c * 3 + a] + m[a * 3 + c]) * s;
            n->quat.w = -((m[b * 3 + c] - m[c * 3 + b]) * s);
        }
        n = p->nodes[i];
        n->quat.unk10 = n->matrix.count;
    }
}
HOOK(0x51ACB0, SplinePath_UpdateRotations);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54ED4C) uint16_t gSplinePathNodeType INIT(385);
DATA(54ED4C) uint16_t gSplinePathType INIT(385);
BSS(A8374C) void *gSplinePathNodeParams;
BSS(A8374C) void *gSplinePathParams;
