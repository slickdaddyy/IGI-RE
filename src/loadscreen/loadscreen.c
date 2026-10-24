#include <stddef.h>
#include <stdint.h>

#include "hook.h"
#include "igi.h"

typedef struct Bitmap {
    int w;
    int h;
    uint32_t *pixels;
} Bitmap;

typedef struct DisplayMode {
    int unk0;
    int w;
    int h;
} DisplayMode;

typedef struct Text {
    void *grid;
    float x, y;
    float z;
    float alpha;
    float color[3];
    int unk20;
} Text;

/* a pixel block handed to Display_DrawPrimitive */
typedef struct BlitPrim {
    int unk0;
    int prim;
    int unk8;
    int pitch;
    int x, y;
    int w, h;
    uint32_t *pixels;
    int unk24;
} BlitPrim;

/* the green progress bar under the loading picture */
typedef struct LoadingScreen {
    int total;
    int done;
    int x, y;
    Bitmap bar;
} LoadingScreen;

#define gClearR VAR(int, 0xBCADF0)
#define gClearG VAR(int, 0xBCADF4)
#define gClearB VAR(int, 0xBCADF8)

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x491D70, void, Display_DrawPrimitive, (BlitPrim *prim));
ORIG(0x491DC0, int, Display_GetBlitPrim, (void));
ORIG(0x491E70, void, Display_SetClearColor, (int r, int g, int b));
ORIG(0x491A30, void, Display_Clear, (void));
ORIG(0x498370, int, Offscreen_GetMode, (void));
ORIG(0x498380, void, OffscreenBuffer_New, (Bitmap *b, int w, int h));
ORIG(0x4983D0, void, OffscreenBuffer_Free, (Bitmap *b));
ORIG(0x498410, void, OffscreenBuffer_SetTarget, (Bitmap *b));
ORIG(0x4B0FD0, void, RenderMode_Push, (int mode));
ORIG(0x4B0F90, void, RenderMode_Pop, (void));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int flags));
ORIG(0x4B63D0, void, Resource_Flush, (void *res));
ORIG(0x48F2D0, int, AppContext_IsUSA, (void));
ORIG(0x4B6DB0, Text *, Text_Load, (const char *name));
ORIG(0x4B6E60, void, Text_Draw, (Text *t));
ORIG(0x4B6E30, void, Text_Delete, (Text *t));

void LoadingScreen_Step(LoadingScreen *ls, int steps);

static void blit(Bitmap *b, int x, int y)
{
    BlitPrim p = {0};

    p.prim = Display_GetBlitPrim();
    p.pixels = b->pixels;
    p.w = b->w;
    p.x = x;
    p.pitch = b->w * 4;
    p.y = y;
    p.h = b->h;
    p.unk24 = 1;
    Display_DrawPrimitive(&p);
}

/* draws the loading picture once and returns a bar for `steps` steps */
LoadingScreen *LoadingScreen_New(int steps)
{
    LoadingScreen *ls = Mem_Alloc(sizeof(LoadingScreen), 4);
    DisplayMode *mode = Display_GetMode();
    Bitmap screen;
    void *res;
    Text *pic;
    int i;

    ls->done = 0;
    ls->total = steps;
    ls->x = (mode->w - 640) / 2 + 40;
    ls->y = (mode->h - 480) / 2 + 440;
    OffscreenBuffer_New(&ls->bar, 560, 10);
    for (i = 0; i < 560 * 10; i++)
        ls->bar.pixels[i] = 0x7F00;
    OffscreenBuffer_New(&screen, mode->w, mode->h);
    RenderMode_Push(Offscreen_GetMode());
    OffscreenBuffer_SetTarget(&screen);
    res = Resource_Load("LOCAL:menusystem/loadingscreen.res", 0);
    pic = Text_Load(AppContext_IsUSA() ? "LOCAL:menusystem/loading_us.pic" : "LOCAL:menusystem/loading.pic");
    pic->unk20 = -1;
    pic->x = (mode->w - 640) / 2;
    pic->y = (mode->h - 480) / 2;
    Text_Draw(pic);
    RenderMode_Pop();
    blit(&screen, 0, 0);
    OffscreenBuffer_Free(&screen);
    Text_Delete(pic);
    Resource_Flush(res);
    LoadingScreen_Step(ls, 0);
    return ls;
}
HOOK(0x48A440, LoadingScreen_New);

void LoadingScreen_Delete(LoadingScreen *ls)
{
    int r = gClearR, g = gClearG, b = gClearB;

    Display_SetClearColor(0, 0, 0);
    Display_Clear();
    Display_SetClearColor(r, g, b);
    OffscreenBuffer_Free(&ls->bar);
    Mem_Free(ls);
}
HOOK(0x48A5D0, LoadingScreen_Delete);

void LoadingScreen_Step(LoadingScreen *ls, int steps)
{
    int filled, x, y;

    ls->done += steps;
    filled = ls->bar.w * ls->done / ls->total;
    for (y = 0; y < ls->bar.h; y++) {
        uint32_t *p = ls->bar.pixels + ls->bar.w * y;

        for (x = 0; x < filled; x++)
            p[x] = 0xFF00;
    }
    blit(&ls->bar, ls->x, ls->y);
}
HOOK(0x48A620, LoadingScreen_Step);
