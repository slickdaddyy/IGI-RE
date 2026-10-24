/* Glows: a 32x32 block of the depth buffer is read when the glow is queued
   and again when it is drawn; the matching pixels inside a disc give the
   visible part and its centre. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "glowmesh/glowmesh.h"
#include "types.h"

ORIG(0x497800, void *, Frame_Alloc, (int size));
ORIG(0x4978B0, void, RenderQueue_Add, (int queue, void *node));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x4978E0, void, Primitive_Unregister, (int id));
ORIG(0x4978F0, void, Primitive_SetHandler, (int id, void *fn));
ORIG(0x4D3710, void, Task_Nothing, (void *buf, int x, int y, int w, int h));
ORIG(0x5182B0, void, LensFlare_Draw, (int unk, float *pos, float *color));

typedef struct Glow {
    int unk0;
    float x, y;
    float color[3];
    uint16_t *buffer;
} Glow;

typedef struct GlowReadPrim {
    void *next;
    int type;
    int unk8;
    float x, y;
    uint16_t *buffer;
} GlowReadPrim;

typedef struct GlowInstancePrim {
    void *next;
    int type;
    int unk8;
    Glow glow;
} GlowInstancePrim;

_Static_assert(sizeof(Glow) == 0x1C, "Glow");
_Static_assert(sizeof(GlowInstancePrim) == 0x28, "GlowInstancePrim");

extern int gGlowReadPrim;
extern float gGlowDirs[][2];
extern int gGlowInstancePrim;
extern char gGlowMask[];
extern int gGlowCount;
extern Glow gGlows[];
extern Pool gGlowPool;

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(k16, 0x41800000);
F32(k32, 0x42000000);
F32(kPi8, 0x3EC90FDB);
F32(k1024th, 0x3A800000);
F64(kPi16, 0x3FC921FB60000000ULL);

static inline long double x87_fcos(long double x)
{
    __asm__("fcos" : "+t"(x));
    return x;
}

static inline long double x87_fsin(long double x)
{
    __asm__("fsin" : "+t"(x));
    return x;
}

static char *pool_alloc(Pool *p)
{
    int i = p->used;
    char *e = p->data + p->free[i] * p->stride;

    *(int *)(e + p->size) = i;
    p->used = i + 1;
    return e;
}

/* the last used slot takes the place of the freed one */
static void pool_release(Pool *p, void *elem)
{
    int slot = *(int *)((char *)elem + p->size);
    int last = --p->used;

    if (slot != last) {
        char *le = p->data + p->free[last] * p->stride;
        int tmp = p->free[slot];
        int other = *(int *)(le + p->size);

        p->free[slot] = p->free[other];
        p->free[other] = tmp;
        *(int *)(le + p->size) = slot;
    }
}

void Glow_ReadBufferHandler(GlowReadPrim *p)
{
    int y = x87_fistp(p->y - k16.f);
    int x = x87_fistp(p->x - k16.f);

    Task_Nothing(p->buffer, x, y, 32, 32);
}
HOOK(0x5127E0, Glow_ReadBufferHandler);

void Glow_InstanceHandler(GlowInstancePrim *p);

void Glow_Init(void)
{
    long double u, v, dv, du, k, a;
    int i = 0;

    v = kZero.f;
    do {
        dv = v - k16.f;
        dv = dv * dv;
        u = kZero.f;
        do {
            du = u - k16.f;
            gGlowMask[i] = __builtin_sqrtl(du * du + dv) < k16.f;
            u += kOne.f;
            i++;
        } while (u < k32.f);
        v += kOne.f;
    } while (v < k32.f);
    k = kOne.f / x87_fcos(kPi16.d);
    for (i = 0; i < 16; i++) {
        a = (long double)i * kPi8.f;
        gGlowDirs[i][0] = x87_fcos(a) * k;
        gGlowDirs[i][1] = x87_fsin(a) * k;
    }
    gGlowReadPrim = Primitive_Register("GlowReadBufferPrimitive");
    Primitive_SetHandler(gGlowReadPrim, Glow_ReadBufferHandler);
    gGlowInstancePrim = Primitive_Register("GlowInstancePrimitive");
    Primitive_SetHandler(gGlowInstancePrim, Glow_InstanceHandler);
}
HOOK(0x512820, Glow_Init);

void Glow_Shutdown(void)
{
    Primitive_Unregister(gGlowReadPrim);
    Primitive_Unregister(gGlowInstancePrim);
}
HOOK(0x512920, Glow_Shutdown);

/* Compares the depth block now with the one read when queued; the share of
   unchanged pixels inside the disc scales the colour and their centre moves
   the glow. */
void Glow_InstanceHandler(GlowInstancePrim *p)
{
    Glow *g = &p->glow;
    uint16_t *buf;
    long double n, sx, sy, w;
    float fy, c[3];
    int i, j, k = 0, x, y;

    if (!gQCubeActive) {
        pool_release(&gGlowPool, g->buffer);
        return;
    }
    buf = (uint16_t *)pool_alloc(&gGlowPool);
    y = x87_fistp(g->y - k16.f);
    x = x87_fistp(g->x - k16.f);
    Task_Nothing(buf, x, y, 32, 32);
    n = sx = sy = kZero.f;
    for (j = -16; j < 16; j++) {
        for (i = -16; i < 16; i++, k++) {
            if (gGlowMask[k] && buf[k] == g->buffer[k]) {
                n += kOne.f;
                sx += i;
                sy += j;
            }
        }
    }
    fy = sy;
    if (n != kZero.f) {
        g->x = sx / n + g->x;
        g->y = fy / n + g->y;
    }
    w = n * k1024th.f;
    c[0] = w * g->color[0];
    c[1] = w * g->color[1];
    c[2] = w * g->color[2];
    LensFlare_Draw(g->unk0, &g->x, c);
    pool_release(&gGlowPool, buf);
    pool_release(&gGlowPool, g->buffer);
}
HOOK(0x512940, Glow_InstanceHandler);

/* queues an instance primitive for every glow added this frame */
void Glow_QueueAll(void)
{
    int i;

    for (i = 0; i < gGlowCount; i++) {
        GlowInstancePrim *p = Frame_Alloc(sizeof(GlowInstancePrim));

        p->type = gGlowInstancePrim;
        memcpy(&p->glow, &gGlows[i], sizeof(Glow));
        RenderQueue_Add(0, p);
    }
    gGlowCount = 0;
}
HOOK(0x512BA0, Glow_QueueAll);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A7DBD8) int gGlowReadPrim;
PAD(igb, A7DBD8, A7DBDC, 0x4);
STORE(igb, A7DBD8, 4) float store_A7DBE0[16][2];
ALIAS(gGlowDirs, store_A7DBE0, 0);
BSS(A7DBD8) int gGlowInstancePrim;
STORE(igb, A7DBD8, 1) char store_A7DC64[1028];
ALIAS(gGlowMask, store_A7DC64, 0);
BSS(A800EC) int gGlowCount;
STORE(igb, AFA840, 4) __typeof__(gGlows[0]) store_AFA840[1000];
ALIAS(gGlows, store_AFA840, 0);
BSS(AFA840) Pool gGlowPool;
