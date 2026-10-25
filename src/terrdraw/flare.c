/* Hermite curves, viewport slots and lens flares. */
#pragma GCC optimize("excess-precision=standard")

#include <stdint.h>

#include "hook.h"
#include "igi.h"
#include "terrdraw/terrdraw.h"
#include "transctx/transctxtypes.h"
#include "glowmesh/glowmeshvars.h"

/* A lens flare element; the list is drawn from its last element back. */
typedef struct Flare {
    struct Flare *next;
    struct Flare *prev;
    float pos; /* 0 at the light, 1 at the mirrored point across the screen centre */
    int unkC;
    float color[3];
    int unk1C;
    void (*draw)(float *color, float *pos, struct Flare *f);
} Flare;

_Static_assert(sizeof(Flare) == 0x24, "Flare");

ORIG(0x4B0B00, void, Pool_Init, (Pool *pool, int size, int count, unsigned int align));
ORIG(0x4B0B70, void, Pool_Free, (Pool *pool));
ORIG(0x512820, void, Glow_Init, (void));
ORIG(0x512920, void, Glow_Shutdown, (void));

extern int gViewSlotCurrent;
extern char gViewSlotUsed[];
extern Pool gFlarePool;
extern Scene gScene;

static const union {
    uint64_t u;
    double d;
} kThree = { 0x4008000000000000ULL };

static const union {
    uint32_t u;
    float f;
} kFlareMargin = { 0x41800000 };

/* h(u) = h0 u + h3 u^2 + h2 u^3 + h1 for end points p0, p1 and tangents m0, m1. */
void Hermite_Init(double *h, double p0, double p1, double m0, double m1)
{
    h[0] = m0;
    h[1] = p0;
    h[2] = p0 + p0 - (p1 + p1) + m0 + m1;
    h[3] = p1 * kThree.d - p0 * kThree.d - (m0 + m0) - m1;
}
HOOK(0x518130, Hermite_Init);

long double Hermite_Eval(const double *h, double u)
{
    long double uu = (long double)u * u;

    return u * h[0] + (uu * u * h[2] + uu * h[3]) + h[1];
}
HOOK(0x518190, Hermite_Eval);

long double Hermite_Slope(const double *h, double u)
{
    long double t = (long double)u * h[3];

    return (long double)u * h[2] * u * kThree.d + (t + t) + h[0];
}
HOOK(0x5181C0, Hermite_Slope);

void ViewSlot_Alloc(int *slot)
{
    int i = 0;

    while (gViewSlotUsed[i])
        i++;
    gViewSlotUsed[i] = 1;
    *slot = i;
}
HOOK(0x5181F0, ViewSlot_Alloc);

void ViewSlot_SetCurrent(int slot)
{
    gViewSlotCurrent = slot;
}
HOOK(0x518230, ViewSlot_SetCurrent);

int ViewSlot_GetCurrent(void)
{
    return gViewSlotCurrent;
}
HOOK(0x518240, ViewSlot_GetCurrent);

void ViewSlot_Free(int slot)
{
    gViewSlotUsed[slot] = 0;
}
HOOK(0x518250, ViewSlot_Free);

void LensFlare_Init(void)
{
    Pool_Init(&gGlowPool, 0x1000, 10, 0);
    Pool_Init(&gFlarePool, sizeof(Flare), 10, 4);
    Glow_Init();
}
HOOK(0x518260, LensFlare_Init);

void LensFlare_Shutdown(void)
{
    Glow_Shutdown();
    Pool_Free(&gFlarePool);
    Pool_Free(&gGlowPool);
}
HOOK(0x518290, LensFlare_Shutdown);

/* Draws the elements along the line from the light at pos through the
   screen centre, when pos is near the viewport. */
void LensFlare_Draw(Flare *f, const float *pos, const float *color)
{
    long double tx = pos[0] - gScene.x, ty = pos[1] - gScene.y;
    float dx = gScene.w - (tx + tx);
    float dy = gScene.h - (ty + ty);
    float p[2], c[3];

    if (!(gScene.x + gScene.w > pos[0] - kFlareMargin.f) || !(pos[0] + kFlareMargin.f > gScene.x) ||
        !(gScene.y + gScene.h > pos[1] - kFlareMargin.f) || !(pos[1] + kFlareMargin.f > gScene.y))
        return;
    while (f->next)
        f = f->next;
    do {
        p[0] = dx * f->pos + pos[0];
        p[1] = dy * f->pos + pos[1];
        c[0] = f->color[0] * color[0];
        c[1] = f->color[1] * color[1];
        c[2] = f->color[2] * color[2];
        f->draw(c, p, f);
        f = f->prev;
    } while (f);
}
HOOK(0x5182B0, LensFlare_Draw);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A800BC) int gViewSlotCurrent;
STORE(igb, A800BC, 1) char store_A800C0[16];
ALIAS(gViewSlotUsed, store_A800C0, 0);
BSS(A800BC) Pool gFlarePool;
