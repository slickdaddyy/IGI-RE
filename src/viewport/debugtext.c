#include <stdarg.h>
#include <string.h>

#include "hook.h"
#include "viewport/viewport.h"

/* A text window on a Grid with a cursor, and the "DebugText" task that
   draws one with the built in font LOCAL:debug.fnt when debug text is on. */

typedef struct TextWin {
    void *grid;
    int visible;
    int x, y;
    int cols, rows;
    int curX, curY;
    int unk20; /* last argument of Grid_Draw, never set here */
} TextWin;

typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
} DisplayMode;

extern char gDebugTextOwnScene;
extern char gDebugFontData[];
extern Task *gDebugTextTask;
extern char gDebugTextDirty;
extern char gDebugTextEnabled;
extern uint16_t gDebugTextType;
extern char gDebugTextBuf[];
extern Scene gDebugTextScene;
extern char gDebugTextReady;
extern TextWin *gDebugTextWin;
extern void *gDebugTextFont;

ORIG(0x48F1F0, char, AppContext_GetDebugText, (void));
ORIG(0x491630, void, Display_AddSetModeCallback, (void (*fn)(void)));
ORIG(0x4916A0, void, Display_RemoveSetModeCallback, (void (*fn)(void)));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x497570, void, Scene_Init, (Scene *s, int queues));
ORIG(0x4975F0, void, Scene_Free, (Scene *s));
ORIG(0x497620, void, Scene_Select, (Scene *s, int add));
ORIG(0x497680, void, Scene_SetRect, (Scene *s, const float *rect));
ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));
ORIG(0x4B0C60, void *, Mem_Alloc, (int size, int align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B6420, void *, Resource_Register, (const char *name, int size, void *data));
ORIG(0x4B64B0, void, Resource_Release, (const char *name));
ORIG(0x4B7000, void *, Sprite_New, (const char *name));
ORIG(0x4B7030, void, Sprite_Delete, (void *sprite));
ORIG(0x4B7630, void *, Grid_New, (int cols, int rows, void *sprite));
ORIG(0x4B76D0, void, Grid_Free, (void *grid));
ORIG(0x4B77F0, void, Grid_Print, (void *grid, const char *text, int x, int y));
ORIG(0x4B7850, void, Grid_Scroll, (void *grid, int n));
ORIG(0x4B7930, void, Grid_Draw,
     (void *grid, float x, float y, float r, float g, float b, float alpha, float z, unsigned flags, int unk20));
ORIG(0x4B7C40, void, Grid_Clear, (void *grid));

static const char kDebugFont[] = "LOCAL:debug.fnt";

void TextWin_SetCursor(TextWin *w, int x, int y)
{
    w->curX = x;
    w->curY = y;
}
HOOK(0x4E75C0, TextWin_SetCursor);

void TextWin_Clear(TextWin *w)
{
    if (w->grid) {
        Grid_Clear(w->grid);
        TextWin_SetCursor(w, 0, 0);
    }
}
HOOK(0x4E75E0, TextWin_Clear);

TextWin *TextWin_New(void *sprite, int cols, int rows)
{
    TextWin *w = Mem_Alloc(sizeof(TextWin), 4);

    w->grid = sprite ? Grid_New(cols, rows, sprite) : NULL;
    w->curX = 0;
    w->curY = 0;
    w->x = 0;
    w->y = 0;
    w->visible = 0;
    w->cols = cols;
    w->rows = rows;
    return w;
}
HOOK(0x4E7600, TextWin_New);

void TextWin_Free(TextWin *w)
{
    if (w->grid) {
        Grid_Free(w->grid);
        w->grid = NULL;
    }
    Mem_Free(w);
}
HOOK(0x4E7660, TextWin_Free);

void TextWin_Draw(TextWin *w)
{
    if (w->visible && w->grid)
        Grid_Draw(w->grid, (float)w->x, (float)w->y, 128.0f, 128.0f, 128.0f, 1.0f, 1.0f, 0, w->unk20);
}
HOOK(0x4E7690, TextWin_Draw);

void TextWin_Print(TextWin *w, char *text);

void TextWin_VPrintf(TextWin *w, const char *fmt, va_list ap)
{
    crt_vsprintf(gDebugTextBuf, fmt, ap);
    TextWin_Print(w, gDebugTextBuf);
}
HOOK(0x4E76E0, TextWin_VPrintf);

void TextWin_NewLine(TextWin *w, int n);
void TextWin_Advance(TextWin *w, int n);

/* Cuts text at each newline in place. */
void TextWin_Print(TextWin *w, char *text)
{
    if (!w->grid)
        return;
    while (text) {
        int i, nl = 0;

        for (i = 0; text[i]; i++) {
            if (text[i] == '\n') {
                text[i] = 0;
                nl = 1;
                break;
            }
        }
        Grid_Print(w->grid, text, w->curX, w->curY);
        if (!nl) {
            TextWin_Advance(w, strlen(text));
            return;
        }
        TextWin_NewLine(w, 1);
        text += i + 1;
    }
}
HOOK(0x4E7710, TextWin_Print);

void TextWin_Scroll(TextWin *w);

void TextWin_NewLine(TextWin *w, int n)
{
    for (; n; n--) {
        int y = w->curY + 1;

        w->curX = 0;
        w->curY = y;
        if (y >= *(int16_t *)((char *)w->grid + 6)) {
            w->curY = y - 1;
            TextWin_Scroll(w);
        }
    }
}
HOOK(0x4E7790, TextWin_NewLine);

void TextWin_Scroll(TextWin *w)
{
    Grid_Scroll(w->grid, 1);
}
HOOK(0x4E77D0, TextWin_Scroll);

void TextWin_Advance(TextWin *w, int n)
{
    for (; n; n--) {
        if (++w->curX >= w->cols)
            TextWin_NewLine(w, 1);
    }
}
HOOK(0x4E77F0, TextWin_Advance);

void TextWin_SetVisible(TextWin *w, char on)
{
    w->visible = on;
}
HOOK(0x4E7830, TextWin_SetVisible);

void DebugText_OnSetMode(void);
void DebugText_Create(void);
void DebugText_Delete(void);
void DebugText_Draw(void);

void DebugText_Init(void)
{
    gDebugTextReady = 1;
    gDebugTextWin = TextWin_New(NULL, 80, 60);
    TextWin_SetVisible(gDebugTextWin, 1);
    gDebugTextDirty = 0;
    gDebugTextEnabled = 0;
    gDebugTextFont = NULL;
    gDebugTextTask = NULL;
    Display_AddSetModeCallback(DebugText_OnSetMode);
    gDebugTextType = TaskType_New("DebugText", 0x20, Task_GetRootType(), 0);
    TaskType_SetHandler(gDebugTextType, 0, DebugText_Create);
    TaskType_SetHandler2(gDebugTextType, 2, DebugText_Delete);
    TaskType_SetHandler2(gDebugTextType, Screen_GetDrawEvent(), DebugText_Draw);
    Resource_Register(kDebugFont, 0x507C, gDebugFontData);
}
HOOK(0x4E7840, DebugText_Init);

/* the font is reloaded on the next draw */
void DebugText_OnSetMode(void)
{
    if (!gDebugTextEnabled)
        return;
    gDebugTextDirty = 1;
    if (gDebugTextFont) {
        Grid_Free(gDebugTextWin->grid);
        Sprite_Delete(gDebugTextFont);
        gDebugTextFont = NULL;
        gDebugTextWin->grid = NULL;
    }
}
HOOK(0x4E7900, DebugText_OnSetMode);

void DebugText_Create(void)
{
    gDebugTextDirty = 1;
}
HOOK(0x4E7950, DebugText_Create);

void DebugText_DrawText(void);

void DebugText_Draw(void)
{
    if (gDebugTextOwnScene)
        Scene_Select(&gDebugTextScene, 1);
    if (gDebugTextEnabled)
        DebugText_DrawText();
    if (gDebugTextOwnScene)
        Debug_Log();
}
HOOK(0x4E7960, DebugText_Draw);

void DebugText_DrawText(void)
{
    if (gDebugTextDirty) {
        int mode = Mem_GetMode();

        if (!mode)
            Mem_SetDefaultMode();
        if (gDebugTextOwnScene) {
            float rect[4];

            rect[0] = 0;
            rect[1] = 0;
            rect[2] = (float)Display_GetMode()->width;
            rect[3] = (float)Display_GetMode()->height;
            Scene_SetRect(&gDebugTextScene, rect);
        }
        gDebugTextFont = Sprite_New(kDebugFont);
        gDebugTextWin->grid = Grid_New(gDebugTextWin->cols, gDebugTextWin->rows, gDebugTextFont);
        gDebugTextDirty = 0;
        if (!mode)
            Mem_ClearMode();
    }
    TextWin_Draw(gDebugTextWin);
}
HOOK(0x4E79A0, DebugText_DrawText);

void DebugText_Delete(void)
{
    if (gDebugTextFont) {
        Grid_Free(gDebugTextWin->grid);
        Sprite_Delete(gDebugTextFont);
        gDebugTextFont = NULL;
        gDebugTextWin->grid = NULL;
    }
    if (gDebugTextOwnScene)
        Scene_Free(&gDebugTextScene);
    gDebugTextTask = NULL;
}
HOOK(0x4E7A60, DebugText_Delete);

/* ownScene: draw into a full screen scene of its own */
Task *DebugText_New(void *parent, char ownScene)
{
    gDebugTextTask = Task_New(parent, gDebugTextType, NULL);
    gDebugTextEnabled = AppContext_GetDebugText();
    gDebugTextOwnScene = ownScene;
    if (ownScene) {
        float rect[4] = {0, 0, 0, 0};

        Scene_Init(&gDebugTextScene, 1);
        Scene_SetRect(&gDebugTextScene, rect);
    }
    return gDebugTextTask;
}
HOOK(0x4E7AC0, DebugText_New);

void DebugText_Shutdown(void)
{
    gDebugTextReady = 0;
    TaskType_Free(gDebugTextType);
    Resource_Release(kDebugFont);
    Display_RemoveSetModeCallback(DebugText_OnSetMode);
    TextWin_Free(gDebugTextWin);
}
HOOK(0x4E7B40, DebugText_Shutdown);

void DebugText_SetCursor(int x, int y)
{
    if (gDebugTextEnabled)
        TextWin_SetCursor(gDebugTextWin, x, y);
}
HOOK(0x4E7B80, DebugText_SetCursor);

void DebugText_Clear(void)
{
    if (gDebugTextEnabled)
        TextWin_Clear(gDebugTextWin);
}
HOOK(0x4E7BB0, DebugText_Clear);

void Debug_printf(const char *fmt, ...)
{
    va_list ap;

    if (!gDebugTextEnabled)
        return;
    va_start(ap, fmt);
    TextWin_VPrintf(gDebugTextWin, fmt, ap);
    va_end(ap);
}
HOOK(0x4E7BD0, Debug_printf);

void DebugText_VPrintf(const char *fmt, va_list ap)
{
    if (gDebugTextEnabled)
        TextWin_VPrintf(gDebugTextWin, fmt, ap);
}
HOOK(0x4E7C00, DebugText_VPrintf);

char DebugText_IsEnabled(void)
{
    return gDebugTextEnabled;
}
HOOK(0x4E7C30, DebugText_IsEnabled);

void *DebugText_GetFont(void)
{
    return gDebugTextFont;
}
HOOK(0x4E7C40, DebugText_GetFont);
