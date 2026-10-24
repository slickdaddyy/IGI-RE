#include <string.h>

#include "hook.h"
#include "transctx/transctx.h"

FN_EXTERN(Task_Nothing);

ORIG(0x491DB0, int, Display_GetClipWindowPrim, (void));
ORIG(0x491DC0, int, Display_GetBlitPrim, (void));
ORIG(0x491DF0, int, Display_GetOpenMethod, (void));
ORIG(0x491E00, int, Display_GetCloseMethod, (void));
ORIG(0x491E40, int, Display_GetSwapBuffersMethod, (void));
ORIG(0x491E50, int, Display_GetClearMethod, (void));
ORIG(0x4B0E00, int, RenderMode_Create, (const char *name));
ORIG(0x4B0E50, void, RenderMode_Delete, (int mode));
ORIG(0x4B0EC0, void, RenderMode_SetMethod, (int mode, int method, void *fn));

/* the shared do-nothing function */
#define NOTHING (FN(0x4D3710, Task_Nothing))

/* What the OffscreenBuffer render mode draws into: 32 bit pixels in memory. */
extern int gOffscreenWidth;
extern int gOffscreenHeight;
extern uint32_t *gOffscreenPixels;
extern float gOffscreenClip[];
extern int gOffscreenMode;
extern char gOffscreenReady;

F32(kZero, 0x00000000);

typedef struct ClipWindowPrim {
    RenderNode node;
    float x, y, w, h;
} ClipWindowPrim;

typedef struct BlitPrim {
    RenderNode node;
    int pitch; /* bytes */
    int x, y, w, h;
    uint32_t *pixels;
} BlitPrim;

int Offscreen_Open(void);
void Offscreen_Clear(void);
void Offscreen_Blit(BlitPrim *p);
void Offscreen_ClipWindow(ClipWindowPrim *p);

void Offscreen_Init(void)
{
    gOffscreenMode = RenderMode_Create("OffscreenBuffer");
    RenderMode_SetMethod(gOffscreenMode, Display_GetOpenMethod(), Offscreen_Open);
    RenderMode_SetMethod(gOffscreenMode, Display_GetCloseMethod(), NOTHING);
    RenderMode_SetMethod(gOffscreenMode, Display_GetClearMethod(), Offscreen_Clear);
    RenderMode_SetMethod(gOffscreenMode, Display_GetSwapBuffersMethod(), NOTHING);
    RenderMode_SetMethod(gOffscreenMode, Display_GetBlitPrim(), Offscreen_Blit);
    RenderMode_SetMethod(gOffscreenMode, Display_GetClipWindowPrim(), Offscreen_ClipWindow);
    gOffscreenReady = 1;
}
HOOK(0x498080, Offscreen_Init);

/* The width is clamped against x rather than 0, the height against y. */
void Offscreen_ClipWindow(ClipWindowPrim *p)
{
    long double t;

    if (!(p->x >= kZero.f)) {
        gOffscreenClip[0] = 0;
    } else {
        t = gOffscreenWidth;
        gOffscreenClip[0] = !(t >= p->x) ? t : p->x;
    }
    if (!(p->y >= kZero.f)) {
        gOffscreenClip[1] = 0;
    } else {
        t = gOffscreenHeight;
        gOffscreenClip[1] = !(t >= p->y) ? t : p->y;
    }
    if (!(p->w >= p->x)) {
        gOffscreenClip[2] = p->x;
    } else {
        t = gOffscreenWidth - p->x;
        gOffscreenClip[2] = !(t >= p->w) ? t : p->w;
    }
    if (!(p->h >= p->y)) {
        gOffscreenClip[3] = p->y;
    } else {
        t = gOffscreenHeight - p->y;
        gOffscreenClip[3] = !(t >= p->h) ? t : p->h;
    }
}
HOOK(0x498140, Offscreen_ClipWindow);

int Offscreen_Open(void)
{
    gOffscreenWidth = 0;
    gOffscreenHeight = 0;
    memset(gOffscreenClip, 0, 6 * 4);
    gOffscreenPixels = NULL;
    return 1;
}
HOOK(0x498230, Offscreen_Open);

void Offscreen_Clear(void)
{
    memset(gOffscreenPixels, 0, (unsigned)(gOffscreenHeight * gOffscreenWidth) * 4);
}
HOOK(0x498260, Offscreen_Clear);

void Offscreen_Blit(BlitPrim *p)
{
    int w, h;
    uint32_t *dst, *src;

    if (p->w < 0) {
        w = 0;
    } else {
        w = gOffscreenWidth - p->x;
        if (w >= p->w)
            w = p->w;
    }
    if (p->h < 0) {
        h = 0;
    } else {
        h = gOffscreenHeight - p->y;
        if (h >= p->h)
            h = p->h;
    }
    dst = gOffscreenPixels + p->y * gOffscreenWidth + p->x;
    src = p->pixels;
    for (; h > 0; h--) {
        memcpy(dst, src, (unsigned)w * 4);
        dst += gOffscreenWidth;
        src += p->pitch / 4;
    }
}
HOOK(0x498290, Offscreen_Blit);

void Offscreen_Shutdown(void)
{
    RenderMode_Delete(gOffscreenMode);
    gOffscreenReady = 0;
}
HOOK(0x498350, Offscreen_Shutdown);

int Offscreen_GetMode(void)
{
    return gOffscreenMode;
}
HOOK(0x498370, Offscreen_GetMode);
