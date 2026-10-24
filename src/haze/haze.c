/* "Hazinglayers": horizontal fog boxes, and the fog seen along a line. */
#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "haze/haze.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4012A0, HazeLayer *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B2A10, long double, Vec3_FastLength, (const float *v));
ORIG(0x4C6E70, void, DynCube_MoveBox, (void *world, void *obj, Vec3d *center, const Vec3d *min, const Vec3d *max));
ORIG(0x4C7350, void, DynCube_AddBox, (void *world, void *obj, Vec3d *center, const Vec3d *min, const Vec3d *max,
                                  int unk));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));

extern uint16_t gQObjType;
extern uint16_t gHazeType;
extern HazeLayer *gHazeVisible[];
extern int gHazeVisibleCount;
extern HazeLayer *gHazeFrame[];
extern int gHazeFrameUsed;

static const double kHalfD = 0.5;
static const float kZero = 0.0f;
static const double kZeroD = 0.0;
static const float kOne = 1.0f;
static const float kFade = 0x1.333334p-2f; /* 0.3 */

void HazeLayer_Push(HazeLayer *t);
void HazeLayer_Enter(HazeLayer *t);
void HazeLayer_Pop(HazeLayer *t);
void HazeLayer_Leave(HazeLayer *t);

static void set_fields(HazeLayer *t, const float *color, float density, double bottom, double top)
{
    t->density = density;
    if (color) {
        t->color[0] = color[0];
        t->color[1] = color[1];
        t->color[2] = color[2];
    } else {
        memset(t->color, 0, sizeof(t->color));
    }
    t->bottom = bottom;
    t->top = top;
}

static void center(Vec3d *c, const Vec3d *min, const Vec3d *max)
{
    c->x = (max->x - min->x) * kHalfD + min->x;
    c->y = (max->y - min->y) * kHalfD + min->y;
    c->z = (max->z - min->z) * kHalfD + min->z;
}

HazeLayer *HazeLayer_New(const Vec3d *min, const Vec3d *max, const float *color, float density, double bottom,
                         double top)
{
    HazeLayer *t = Task_New(NULL, gHazeType, NULL);
    Vec3d c;

    set_fields(t, color, density, bottom, top);
    t->world = gCurrentWorld;
    t->visible = 0;
    center(&c, min, max);
    DynCube_AddBox(t->world, t, &c, min, max, 4);
    return t;
}
HOOK(0x4D9FC0, HazeLayer_New);

void HazeLayer_Set(HazeLayer *t, const Vec3d *min, const Vec3d *max, const float *color, float density,
                   double bottom, double top)
{
    Vec3d c;

    set_fields(t, color, density, bottom, top);
    center(&c, min, max);
    DynCube_MoveBox(t->world, t, &c, min, max);
}
HOOK(0x4DA0A0, HazeLayer_Set);

void HazeLayer_Delete(HazeLayer *t)
{
    DynCube_Remove(t->world, t);
    Task_Delete(t);
}
HOOK(0x4DA160, HazeLayer_Delete);

/* Hands out this frame's copy of the visible layer list */
void Haze_GetVisible(HazeLayer ***list, int *count)
{
    HazeLayer **out = &gHazeFrame[gHazeFrameUsed];

    *list = out;
    memcpy(out, gHazeVisible, gHazeVisibleCount * sizeof(*out));
    *count = gHazeVisibleCount;
    gHazeFrameUsed += gHazeVisibleCount;
}
HOOK(0x4DA180, Haze_GetVisible);

void HazeLayer_Open(void)
{
    gHazeType = TaskType_New("Hazinglayers", sizeof(HazeLayer), gQObjType, 0, 0);
    TaskType_SetHandler(gHazeType, 0x11, HazeLayer_Push);
    TaskType_SetHandler(gHazeType, 0x12, HazeLayer_Pop);
    TaskType_SetHandler(gHazeType, 4, HazeLayer_Enter);
    TaskType_SetHandler(gHazeType, 5, HazeLayer_Leave);
}
HOOK(0x4DA1D0, HazeLayer_Open);

void HazeLayer_Push(HazeLayer *t)
{
    gHazeVisible[gHazeVisibleCount++] = t;
    t->visible = 1;
}
HOOK(0x4DA240, HazeLayer_Push);

void HazeLayer_Enter(HazeLayer *t)
{
    if (!t->visible)
        gHazeVisible[gHazeVisibleCount++] = t;
}
HOOK(0x4DA270, HazeLayer_Enter);

void HazeLayer_Pop(HazeLayer *t)
{
    gHazeVisibleCount--;
    t->visible = 0;
}
HOOK(0x4DA2A0, HazeLayer_Pop);

void HazeLayer_Leave(HazeLayer *t)
{
    if (!t->visible)
        gHazeVisibleCount--;
}
HOOK(0x4DA2C0, HazeLayer_Leave);

void HazeLayer_Close(void)
{
    TaskType_Free(gHazeType);
}
HOOK(0x4DA2E0, HazeLayer_Close);

void Haze_ResetFrame(void)
{
    gHazeFrameUsed = 0;
}
HOOK(0x4DA2F0, Haze_ResetFrame);

static float clamp01(float f)
{
    if (f < kZero)
        return 0.0f;
    if (!(f <= kOne))
        return 1.0f;
    return f;
}

/* Distance from z to the layer edge it is nearest in the other point's direction */
static double edge_dist(const HazeLayer *l, double z, double other)
{
    long double e = (other > l->bottom ? l->top : l->bottom) - z;

    return !(e >= kZeroD) ? -e : e;
}

/* Fog colour and density along the line from..to through the given layers */
void Haze_GetFog(float *color, float *density, const Vec3d *from, const Vec3d *to, HazeLayer **layers, int count)
{
    float d[3];
    float wsum;
    int i;

    d[0] = to->x - from->x;
    d[1] = to->y - from->y;
    d[2] = to->z - from->z;
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;
    *density = 0;
    /* the original never sets the starting weight either */
    __asm__("" : "=m"(wsum));
    for (i = 0; i < count; i++) {
        HazeLayer *l = layers[i];
        double fz = from->z, tz = to->z, b = l->bottom, t = l->top;
        float frac = 1.0f, inv, len, s;
        long double r, v, sum;

        inv = kOne / ((t - b) * kFade);
        if (d[2] > kZero && tz > t)
            frac = clamp01((t - fz) * inv);
        if (!(d[2] >= kZero) && !(tz >= b))
            frac = clamp01((fz - b) * inv);
        r = Vec3_FastLength(d);
        len = r;
        if (!(r > kZero))
            continue;
        if (!(fz > b) && !(tz > b))
            continue;
        if (fz >= t && tz >= t)
            continue;
        s = d[2];
        if (!(s >= kZero))
            s = -s;
        if ((!(fz > b) && tz >= t) || (fz >= t && !(tz > b))) {
            long double h = t - b;

            if (!(h >= kZeroD))
                h = -h;
            v = h * len / s;
        } else if (fz > b && !(tz >= t) && !(fz >= t) && tz > b) {
            v = len;
        } else if (fz > b && !(fz >= t) && (!(tz > b) || tz >= t)) {
            v = len * edge_dist(l, fz, tz) / s;
        } else if (tz > b && !(tz >= t) && (!(fz > b) || fz >= t)) {
            v = len * edge_dist(l, tz, fz) / s;
        } else {
            Error_Show("Fog layer logic error");
            for (;;)
                ;
        }
        v = v * l->density;
        *density = v * frac + *density;
        if (!(v < kZero || v > kZero))
            continue;
        sum = v + wsum;
        color[0] = (v * l->color[0] + wsum * color[0]) / sum;
        color[1] = (v * l->color[1] + wsum * color[1]) / sum;
        color[2] = (v * l->color[2] + wsum * color[2]) / sum;
        wsum = sum;
    }
}
HOOK(0x4DA300, Haze_GetFog);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, A54694, 4) __typeof__(gHazeVisible[0]) store_A54694[10];
ALIAS(gHazeVisible, store_A54694, 0);
STORE(igb, A54694, 4) __typeof__(gHazeFrame[0]) store_A546BC[10000];
ALIAS(gHazeFrame, store_A546BC, 0);
BSS(A54694) int gHazeVisibleCount;
BSS(A54694) int gHazeFrameUsed;
BSS(A54694) uint16_t gHazeType;
PAD(igb, A54694, A5E306, 0x2);
