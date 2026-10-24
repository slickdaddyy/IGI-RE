/* Point, spot and directional lights, and the fog amount of a point. */
#pragma GCC optimize("excess-precision=standard")

#include "sector/sector.h"
#include "hook.h"

ORIG(0x4DA180, void, Haze_GetVisible, (int **layers, int *count));
ORIG(0x4DA300, void, Haze_GetFog, (float *color, float *amount, const Vec3d *from, const Vec3d *to, int *layers,
                                  int count));
ORIG(0x499FF0, int, Float_Log2, (float f));

/* a point light pushed while drawing (0x28 bytes) */
typedef struct PointLight {
    Vec3d pos;
    Vec3f color;
    float range;
} PointLight;

/* a spot light (0x38 bytes) */
typedef struct SpotLight {
    Vec3d pos;
    Vec3f color;
    float range;
    Vec3f dir;
    float cosHalf;
} SpotLight;

/* a directional light of the level (0x28 bytes incl. the pool index) */
typedef struct Light {
    Vec3f dir;
    Vec3f color;
    Vec3f unk18;
    char unk24;
    char on;
} Light;

_Static_assert(sizeof(PointLight) == 0x28, "PointLight");
_Static_assert(sizeof(SpotLight) == 0x38, "SpotLight");

#define gLightFrame ((int *)0xB01A00)
#define gLightStackCount VAR(int, 0xB03DF0)
#define gPointLights ((PointLight *)0xB03E00)
#define gLightNest VAR(int, 0xB03F90)
#define gSpotLights ((SpotLight *)0xB03FA0)
#define gLightFrameUsed VAR(int, 0xB041D0)
#define gLightPool VAR(Pool, 0xB041E0)
#define gLightStack ((int *)0xB04200)
#define gSpotCount VAR(int, 0xB0425C)
#define gPointCount VAR(int, 0xB04260)
#define gViewPos VAR(Vec3d, 0xBCAB08)
#define gViewFov VAR(float, 0xBCAB20)

#define kFovScale VAR(const float, 0x534B28)    /* 0.83 */
#define kFovBase VAR(const float, 0x534B24)     /* 0.17 */
#define kFogScale VAR(const float, 0x534B20)    /* 1/65536 */
#define kFogBias VAR(const float, 0x534B1C)     /* 130 */
#define kHalf VAR(const float, 0x533504)

static inline long double x87_cos(long double x)
{
    __asm__("fcos" : "+t"(x));
    return x;
}

void Fog_Compute(float *out, const Vec3d *pos, float depth, float scale, int *layers, int count)
{
    float color[3];
    long double f, v;

    if (!layers)
        Haze_GetVisible(&layers, &count);
    Haze_GetFog(color, out, &gViewPos, pos, layers, count);
    f = gViewFov * kFovScale + kFovBase;
    if (f < kOne)
        depth = f * depth;
    *out = *out * kFogScale;
    v = ((long double)Float_Log2(depth) - kFogBias) * scale * kFogScale + *out;
    *out = v;
    if (kZero > v)
        *out = 0.0f;
    else if (kOne < v)
        *out = 1.0f;
    else
        *out = v;
}
HOOK(0x4DA790, Fog_Compute);

void Light_Open(void)
{
    Pool_Init(&gLightPool, 0x28, 3, 4);
}
HOOK(0x4DA880, Light_Open);

void Light_Close(void)
{
    Pool_Free(&gLightPool);
}
HOOK(0x4DA8A0, Light_Close);

void Light_ResetFrame(void)
{
    gLightFrameUsed = 0;
    gPointCount = 0;
    gSpotCount = 0;
}
HOOK(0x4DA8B0, Light_ResetFrame);

void Light_PushPoint(const Vec3d *pos, const Vec3f *color, float range)
{
    PointLight *l;

    gLightNest++;
    if (gPointCount >= 10 || gLightNest > 0x17)
        return;
    gLightStack[gLightStackCount++] = gPointCount + 0x10000;
    l = &gPointLights[gPointCount++];
    l->pos = *pos;
    l->color = *color;
    l->range = range;
}
HOOK(0x4DA8D0, Light_PushPoint);

void Light_PushSpot(const Vec3d *pos, const Vec3f *color, float range, const Vec3f *dir, float angle)
{
    SpotLight *l;

    gLightNest++;
    if (gSpotCount >= 10 || gLightNest > 0x17)
        return;
    gLightStack[gLightStackCount++] = gSpotCount + 0x20000;
    l = &gSpotLights[gSpotCount++];
    l->pos = *pos;
    l->color = *color;
    l->range = range;
    l->dir = *dir;
    l->cosHalf = x87_cos(angle * kHalf);
}
HOOK(0x4DA950, Light_PushSpot);

void Light_Pop(void)
{
    if (gLightNest == gLightStackCount)
        gLightStackCount--;
    gLightNest--;
}
HOOK(0x4DAA00, Light_Pop);

/* copies the pushed light ids into the frame list; returns where they start */
int *Light_GetActive(int *count)
{
    int *out = &gLightFrame[gLightFrameUsed];
    int i;

    for (i = 0; i < gLightStackCount; i++)
        out[i] = gLightStack[i];
    gLightFrameUsed += gLightStackCount;
    *count = gLightStackCount;
    return out;
}
HOOK(0x4DAA20, Light_GetActive);

Light *Light_Add(const Vec3f *dir, const Vec3f *color, const Vec3f *unk18, char unk24, char on)
{
    Light *l = pool_take(&gLightPool);

    l->dir = *dir;
    l->color = *color;
    l->unk18 = *unk18;
    l->unk24 = unk24;
    l->on = on;
    return l;
}
HOOK(0x4DAA60, Light_Add);

void Light_Remove(Light *l)
{
    pool_release(&gLightPool, l);
}
HOOK(0x4DAAF0, Light_Remove);
