#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "igi.h"
#include "sound/sound.h"

ORIG(0x49A010, void, SplineCache_Remove, (void *obj));
ORIG(0x49A050, float *, SplineMesh_GetVerts, (void *obj, int model, int part, int unk3, int unk4, int *count));
ORIG(0x4B2D00, void, Vec3_ToAngles, (const float *v, float *pitch, float *yaw));
ORIG(0x4B32B0, long double, Vec3d_Distance, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B3360, char, Vec3d_Equal, (const Vec3d *a, const Vec3d *b));
ORIG(0x4B38E0, void, Matrix_SetEuler, (float *m, float ax, float ay, float az));
ORIG(0x4B3BE0, void, Matrix_SetRotZ, (float *m, float a));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const float *m, float *ax, float *ay, float *az));
ORIG(0x4B4390, void, Matrix_Invert, (float *out, const float *m));
ORIG(0x4C6940, void, DynCube_Move, (void *tree, void *o, Vec3d *pos, float radius));
ORIG(0x4CEC10, char **, VirModel_Find, (const char *name));
ORIG(0x4E1440, void, SplineObj_BendVerts, (void *o, void *list));
ORIG(0x4E51C0, void, SplineObjSegment_GrowRadius, (void));
ORIG(0x518130, void, Hermite_Init, (double *h, double p0, double p1, double m0, double m1));
ORIG(0x518190, long double, Hermite_Eval, (const double *h, double u));
ORIG(0x5181C0, long double, Hermite_Slope, (const double *h, double u));

typedef struct SegBuffer {
    void *data;
    int count;
} SegBuffer;

/* "SplineObjSegment" (0x5B8 bytes), the fields set up here */
typedef struct SegObj {
    Task task;
    Vec3d pos;
    char unk38[0x34];
    char **models;          /* 0x6C */
    float rot[10];          /* 0x70, the rigid case */
    char unk98[0xC];
    int lod;                /* 0xA4 */
    char unkA8[0x48];
    void *tree;             /* 0xF0 */
    float radius;           /* 0xF4 */
    float length;           /* 0xF8 */
    float minX;             /* 0xFC */
    float maxX;             /* 0x100 */
    float segPos[21][3];    /* 0x104, relative to the middle of the ends */
    float segRot[20][10];   /* 0x200 */
    float invRot[9];        /* 0x520, inverse of the rigid rotation */
    float segLength[22];    /* 0x544, [0] is invRot's spare word */
    int segments;           /* 0x59C */
    SegBuffer buffers[2];   /* 0x5A0 */
    char **lastModel;       /* 0x5B0 */
    char rigid;
    char reversed;
} SegObj;

_Static_assert(offsetof(SegObj, segRot) == 0x200, "segRot");
_Static_assert(offsetof(SegObj, segLength) == 0x544, "segLength");
_Static_assert(offsetof(SegObj, segments) == 0x59C, "segments");
_Static_assert(offsetof(SegObj, rigid) == 0x5B4, "rigid");

#define gSphereVerts VAR(float *, 0xA5E3EC)
#define gSphereNodes VAR(SegBuffer *, 0xA5E3F0)
#define gSphereMesh VAR(char *, 0xA5E3F8)
#define gSphereStack ((void **)0xA5E374)
#define gSphereDepth VAR(int, 0xA5E400)

static const double kHalfD = 0.5;
static const float kZero = 0.0f;
static const float kOne = 1.0f;
static const float kPi = 0x1.921fb6p+1f;
static const float kTwoPi = 0x1.921fb6p+2f;
static const float kMinusPi = -0x1.921fb6p+1f;
static const float kRadiusScale = 0x1.19999ap+0f;     /* 1.1 */

/* a segment rotation, 10 words; the last one may run into invRot */
#define SEG_ROT(o, i) ((float *)((char *)(o) + 0x200 + (i) * 0x28))

/* the LOD info block of a model */
#define LOD_INFO(m) (*(char **)(*(char **)((m) + 0xC) + 0xC))
#define LOD_COUNT(models) (*(short *)((char *)(models) + 0x3C))

/* transposes a 3x3 matrix word by word, copying the tenth word */
static void transpose10(uint32_t *dst, const uint32_t *src)
{
    uint32_t t[10];
    int r, c;

    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            t[r * 3 + c] = src[c * 3 + r];
    t[9] = src[9];
    memcpy(dst, t, sizeof(t));
}

static void setup_segments(SegObj *o, Vec3d *from, Vec3d *to, float *rotFrom, float *rotTo, char rigid,
                           Vec3d *mid)
{
    double h[3][4];
    double dist;
    float ax0, ay0, az0, ax1, ay1, az1;
    float inv;
    int i;

    dist = Vec3d_Distance(from, to);
    Hermite_Init(h[0], from->x, to->x, rotFrom[0] * dist, rotTo[0] * dist);
    Hermite_Init(h[1], from->y, to->y, rotFrom[3] * dist, rotTo[3] * dist);
    Hermite_Init(h[2], from->z, to->z, rotFrom[6] * dist, rotTo[6] * dist);
    Matrix_GetEuler(rotFrom, &ax0, &ay0, &az0);
    Matrix_GetEuler(rotTo, &ax1, &ay1, &az1);
    mid->x = (from->x + to->x) * kHalfD;
    mid->y = (to->y + from->y) * kHalfD;
    mid->z = (to->z + from->z) * kHalfD;
    o->length = 0;
    inv = kOne / (long double)o->segments;
    for (i = 0; i <= o->segments; i++) {
        long double t = i * (long double)inv;
        float tf = t;
        float *pos = o->segPos[i];
        float *rot = SEG_ROT(o, i);

        if (rigid) {
            long double px = t * ((long double)to->x - from->x) + from->x;
            double py = (to->y - from->y) * tf + from->y;
            double pz = (to->z - from->z) * tf + from->z;
            long double dx;
            float d[3], pitch, yaw;

            pos[0] = px - mid->x;
            pos[1] = py - mid->y;
            pos[2] = pz - mid->z;
            memset(rot, 0, 40);
            dx = (long double)to->x - from->x;
            d[0] = dx;
            d[1] = to->y - from->y;
            d[2] = to->z - from->z;
            if (dx == kZero && d[1] == kZero && d[2] == kZero) {
                pitch = 0;
                yaw = 0;
            } else {
                Vec3_ToAngles(d, &pitch, &yaw);
            }
            Matrix_SetRotZ(rot, yaw);
            if (i == 0) {
                long double len = __builtin_sqrtl((long double)d[0] * d[0] + (long double)d[1] * d[1] +
                                                  (long double)d[2] * d[2]) * inv;

                o->segLength[1] = len;
                o->length = len + o->length;
            }
        } else {
            long double a0 = ax0, a1 = ax1, len;
            float a0f = ax0, a1f = ax1;
            double td;

            if (ax1 - a0 > kPi) {
                do
                    a0 += kTwoPi;
                while (ax1 - a0 > kPi);
                a0f = a0;
            }
            if (!(ax1 - a0 >= kMinusPi)) {
                do
                    a1 += kTwoPi;
                while (!(a1 - a0 >= kMinusPi));
                a1f = a1;
            }
            td = t;
            pos[0] = Hermite_Eval(h[0], td) - mid->x;
            pos[1] = Hermite_Eval(h[1], td) - mid->y;
            pos[2] = Hermite_Eval(h[2], td) - mid->z;
            if (i == 0) {
                memcpy(rot, rotFrom, 40);
            } else if (i == o->segments) {
                long double dx = (long double)pos[0] - pos[-3];
                double dy = (long double)pos[1] - pos[-2];
                double dz = (long double)pos[2] - pos[-1];

                memcpy(rot, rotTo, 40);
                len = __builtin_sqrtl(dx * dx + dy * (long double)dy + dz * (long double)dz);
                o->segLength[i] = len;
                o->length = len + o->length;
            } else {
                float s[3], pitch, yaw, ax, d0, d1;
                long double dz;

                s[0] = Hermite_Slope(h[0], td);
                s[1] = Hermite_Slope(h[1], td);
                s[2] = Hermite_Slope(h[2], td);
                if (s[0] == kZero && s[1] == kZero && s[2] == kZero) {
                    pitch = 0;
                    yaw = 0;
                } else {
                    Vec3_ToAngles(s, &pitch, &yaw);
                }
                ax = (a1f - a0f) * tf + a0f;
                Matrix_SetEuler(rot, ax, pitch, yaw);
                d0 = pos[0] - pos[-3];
                d1 = pos[1] - pos[-2];
                dz = (long double)pos[2] - pos[-1];
                len = __builtin_sqrtl((long double)d0 * d0 + (long double)d1 * d1 + dz * dz);
                o->segLength[i] = len;
                o->length = len + o->length;
            }
        }
    }
}

/* rigid: the rotation scales the model's x range onto from..to; returns the
   largest axis length */
static float setup_rigid(SegObj *o, Vec3d *from, Vec3d *to)
{
    long double dx, k, n0;
    float d[3], r0, r1, n1, n2, radius = kOne;

    if (Vec3d_Equal(from, to))
        return radius;
    dx = (long double)to->x - from->x;
    d[0] = dx;
    d[1] = to->y - from->y;
    d[2] = to->z - from->z;
    if (dx == kZero && d[1] == kZero && d[2] == kZero) {
        d[0] = 1.0f;
        d[1] = 0;
        d[2] = 0;
    }
    k = kOne / ((long double)o->maxX - o->minX);
    transpose10((uint32_t *)o->rot, (uint32_t *)SEG_ROT(o, 0));
    o->rot[0] = d[0];
    o->rot[1] = d[1];
    o->rot[2] = d[2];
    r0 = k * o->rot[0];
    o->rot[0] = r0;
    r1 = k * o->rot[1];
    o->rot[1] = r1;
    o->rot[2] = k * o->rot[2];
    if (o->reversed) {
        o->rot[0] = -r0;
        o->rot[1] = -r1;
        o->rot[2] = -o->rot[2];
    }
    transpose10((uint32_t *)o->rot, (uint32_t *)o->rot);
    n0 = __builtin_sqrtl((long double)o->rot[0] * o->rot[0] + (long double)o->rot[1] * o->rot[1] +
                         (long double)o->rot[2] * o->rot[2]);
    n1 = __builtin_sqrtl((long double)o->rot[3] * o->rot[3] + (long double)o->rot[4] * o->rot[4] +
                         (long double)o->rot[5] * o->rot[5]);
    n2 = __builtin_sqrtl((long double)o->rot[6] * o->rot[6] + (long double)o->rot[7] * o->rot[7] +
                         (long double)o->rot[8] * o->rot[8]);
    radius = n0;
    if (!(n0 >= n1))
        radius = n1;
    if (!(radius >= n2))
        radius = n2;
    transpose10((uint32_t *)o->invRot, (uint32_t *)o->rot);
    Matrix_Invert(o->invRot, o->invRot);
    return radius;
}

/* Places the segments of a spline object between two frames: a Hermite
   curve through from and to (or a straight line when rigid), then the model's
   x range, the bounding radius and the bent sphere trees of both meshes. */
void SplineObjSegment_Setup(SegObj *o, Vec3d *from, Vec3d *to, float *rotFrom, float *rotTo, char rigid,
                            char reversed, int segments, int unk, const char *model)
{
    Vec3d mid;
    char **models;
    float radius;
    int k, j;

    (void)unk;
    o->rigid = rigid;
    o->reversed = reversed;
    o->lod = 0;
    o->segments = segments;
    if (rigid)
        o->segments = 1;
    setup_segments(o, from, to, rotFrom, rotTo, rigid, &mid);
    o->segLength[o->segments + 1] = 0;
    models = VirModel_Find(model);
    o->models = models;
    if (!models)
        return;
    o->minX = 0;
    o->maxX = 0;
    for (k = 0; k < LOD_COUNT(models); k++) {
        char *info = LOD_INFO(models[k]);

        if (*(int *)(info + 0x20) == 0) {
            char *p = *(char **)(info + 0x24);
            float *v = *(float **)(p + 0x10);

            for (j = 0; j < *(int *)(p + 0xC); j++, v += 8) {
                if (!(*v >= o->minX))
                    o->minX = *v;
                if (*v > o->maxX)
                    o->maxX = *v;
            }
        }
    }
    if (o->rigid) {
        long double best = kZero;

        radius = setup_rigid(o, from, to);
        models = o->models;
        for (k = LOD_COUNT(models); k > 0; k--, models++) {
            float r = *(float *)(LOD_INFO(*models) + 0x78);

            if (!(best >= r))
                best = r;
        }
        radius = best * radius * kRadiusScale;
    } else {
        int count = (int)(intptr_t)models;
        int part;

        radius = 0;
        for (part = 0; part < 2; part++) {
            const float *v = SplineMesh_GetVerts(o, 0, part, 1, 0, &count);

            for (j = count; j > 0; j--, v += 3) {
                long double d = __builtin_sqrtl((long double)v[0] * v[0] + (long double)v[2] * v[2] +
                                                (long double)v[1] * v[1]);

                if (!(radius > d))
                    radius = d;
            }
            if (!(radius > kOne))
                radius = 1.0f;
        }
    }
    DynCube_Move(o->tree, o, &mid, radius);
    o->radius = radius;
    SplineCache_Remove(o);
    if (o->lastModel != o->models || rigid) {
        for (k = 0; k < 2; k++) {
            if (o->buffers[k].data) {
                Mem_Free(o->buffers[k].data);
                o->buffers[k].data = NULL;
            }
        }
        if (rigid) {
            o->lastModel = o->models;
            return;
        }
    }
    if (o->models) {
        int lod = o->lod;
        int part;

        for (part = 0; part < 2; part++) {
            SegBuffer *buf = &o->buffers[part];
            char *mesh = *(char **)(LOD_INFO(o->models[lod]) + 0x2C) + part * 0x20;

            gSphereMesh = mesh;
            if (*(int *)(mesh + 0xC)) {
                gSphereVerts = SplineMesh_GetVerts(o, lod, part, 1, 0, NULL);
                if (!buf->data) {
                    buf->count = *(int *)(mesh + 0xC);
                    buf->data = Mem_Alloc(buf->count * 0x18, 4);
                }
                memcpy(buf->data, *(void **)(mesh + 0x1C), buf->count * 0x18);
                gSphereNodes = buf;
                SplineObj_BendVerts(o, buf);
                gSphereStack[0] = buf->data;
                gSphereDepth = 0;
                SplineObjSegment_GrowRadius();
            }
        }
    }
    o->lastModel = o->models;
}
HOOK(0x4E4530, SplineObjSegment_Setup);
