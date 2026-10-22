#include <string.h>

#include "hook.h"
#include "igi.h"
#include "menu/menu.h"

ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));
ORIG(0x4B4A00, void, Draw_RectA, (float x, float y, float w, float h, float r, float g, float b, float a, int flags,
                                  int unk));
ORIG(0x4B6E90, void, Text_SetString, (Text *t, const char *s));
ORIG(0x4B6EC0, Text *, Text_New, (Sprite *s, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (Text *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));
ORIG(0x4B7DF0, int, Text_GetWidth, (Text *t));
ORIG(0x4B7E00, int, Text_GetHeight, (Text *t));
ORIG(0x424980, void, MenuWindow_DrawEnd, (void *w, float unk, float alpha, int unk2));
ORIG(0x4C1780, void, sub_4C1780_1, (int unk));
ORIG(0x4F0FA0, void *, Param_Int32, (void));

/* one line of the scrolling credits style text */
typedef struct TypeLine {
    int x;     /* 0 = centred, < 0 = from the right */
    int y;     /* -1 = blank line */
    int color;
    int font;
    int pause; /* frames after the line, -1 = fade out */
    int newPage;
    int localized;
    const char *key;
    const char *text;
} TypeLine;

/* types lines one character at a time */
typedef struct TypeWriterBox {
    MenuWindow w;
    int active;
    TypeLine *lines;
    int lineCount;
    int line;
    int firstLine;
    int chars;
    int delay;
    int frame;
    int fading;
    Text *fonts[4];
} TypeWriterBox;

_Static_assert(sizeof(TypeWriterBox) == 0x88, "TypeWriterBox");

typedef struct Color {
    float r, g, b;
} Color;

#define gTypeWriterBoxType VAR(uint16_t, 0x539C40)
#define gTypeLines ((TypeLine *)0x539C48)
#define gTypeColors ((const Color *)0x53B778)
#define gTypeWriterBoxParams VAR(void *, 0x57BB90)
#define gKeys ((unsigned char *)0xC28C62)

void TypeWriterBox_Run(TypeWriterBox *tw);
void TypeWriterBox_DrawHandler(TypeWriterBox *tw);
void TypeWriterBox_Delete(TypeWriterBox *tw);
void TypeWriterBox_Create(TypeWriterBox *tw, void *arg);
void TypeWriterBox_GetParams(void *task, void **out);
void TypeWriterBox_Changed(TypeWriterBox *tw, void *arg);
void TypeWriterBox_Restart(TypeWriterBox *tw);
void TypeWriterBox_Activate(TypeWriterBox *tw);
void TypeWriterBox_Deactivate(TypeWriterBox *tw);
void TypeWriterBox_UpdateText(TypeWriterBox *tw);

void TypeWriterBox_Init(void)
{
    TypeWriterBox tw;

    gTypeWriterBoxType = TaskType_New("TypeWriterBox", sizeof(TypeWriterBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gTypeWriterBoxType, 1, TypeWriterBox_Run);
    TaskType_SetHandler2(gTypeWriterBoxType, Screen_GetDrawEvent(), TypeWriterBox_DrawHandler);
    TaskType_SetHandler2(gTypeWriterBoxType, 2, TypeWriterBox_Delete);
    TaskType_SetHandler(gTypeWriterBoxType, 0, TypeWriterBox_Create);
    TaskType_SetHandler(gTypeWriterBoxType, Param_GetTableEvent(), TypeWriterBox_GetParams);
    TaskType_SetHandler(gTypeWriterBoxType, Param_GetChangedEvent(), TypeWriterBox_Changed);
    TaskType_SetHandler2(gTypeWriterBoxType, MenuWindow_GetEvent914(), TypeWriterBox_Restart);
    TaskType_SetHandler2(gTypeWriterBoxType, MenuWindow_GetEvent918(), MenuWindow_ForwardEvent918);
    TaskType_SetHandler2(gTypeWriterBoxType, Level_GetEventDA8(), TypeWriterBox_Activate);
    TaskType_SetHandler2(gTypeWriterBoxType, Level_GetEventDAC(), TypeWriterBox_Deactivate);
    TaskType_SetHandler2(gTypeWriterBoxType, MenuWindow_GetEvent91C(), TypeWriterBox_UpdateText);
    gTypeWriterBoxParams = ParamTable_New(2);
    ParamTable_Add(gTypeWriterBoxParams, &tw, &tw.w.w, Param_Int32(), 7, NULL);
    ParamTable_Add(gTypeWriterBoxParams, &tw, &tw.w.h, Param_Int32(), 7, NULL);
}
HOOK(0x41A1E0, TypeWriterBox_Init);

void TypeWriterBox_Create(TypeWriterBox *tw, void *arg)
{
    HANDLER(0, MenuWindow_GetType())(tw, arg);
    tw->w.w = 100;
    tw->w.h = 100;
    tw->w.unk3C = 1;
    tw->w.unk40 = 1;
    tw->fonts[0] = NULL;
    tw->fonts[1] = NULL;
    tw->fonts[2] = NULL;
    tw->fonts[3] = NULL;
    tw->active = 0;
}
HOOK(0x41A370, TypeWriterBox_Create);

void TypeWriterBox_Delete(TypeWriterBox *tw)
{
    int i;

    for (i = 0; i < 4; i++)
        if (tw->fonts[i])
            Text_Free(tw->fonts[i]);
    ((void (*)(void *))HANDLER(Level_GetEventDAC(), tw->w.task.type))(tw);
}
HOOK(0x41A3C0, TypeWriterBox_Delete);

static void resolve_lines(TypeWriterBox *tw)
{
    int i;

    for (i = 0; i < tw->lineCount; i++) {
        TypeLine *l = &tw->lines[i];

        if (l->localized)
            l->text = MenuManager_GetString(tw->w.menu, l->key);
        else
            l->text = l->key;
    }
}

void TypeWriterBox_UpdateText(TypeWriterBox *tw)
{
    CALL_CHILDREN(&tw->w.task, MenuWindow_GetEvent91C);
    if (tw->w.menu)
        resolve_lines(tw);
}
HOOK(0x41A410, TypeWriterBox_UpdateText);

void TypeWriterBox_Changed(TypeWriterBox *tw, void *arg)
{
    static const char *const names[4] = {"font1.fnt", "font2.fnt", "font3.fnt", "font4.fnt"};
    MenuManager *mm = tw->w.menu;
    int i;

    tw->lines = gTypeLines;
    tw->lineCount = 130;
    for (i = 0; i < 4; i++)
        if (tw->fonts[i])
            Text_Free(tw->fonts[i]);
    if (mm) {
        for (i = 0; i < 4; i++) {
            Sprite *s = MenuManager_FindFont(mm, names[i]);

            if (s) {
                tw->fonts[i] = Text_New(s, 0, 0, gEmptyString);
                tw->fonts[i]->unk20 = 0;
            }
        }
        resolve_lines(tw);
    }
    {
        uint8_t ev = Param_GetChangedEvent();

        HANDLER(ev, MenuWindow_GetType())(tw, arg);
    }
}
HOOK(0x41A4D0, TypeWriterBox_Changed);

void TypeWriterBox_Run(TypeWriterBox *tw)
{
    if (tw->delay) {
        tw->delay--;
    } else {
        TypeLine *lines = tw->lines;
        int line = tw->line;
        int len = strlen(lines[line].text);

        tw->fading = 0;
        tw->delay = 3;
        if (len == tw->chars) {
            if (++line == tw->lineCount)
                line = 0;
            if (lines[line].pause == -1) {
                tw->delay = 253;
                tw->fading = 1;
            } else {
                tw->delay = lines[line].pause + 3;
            }
            tw->line = line;
            tw->chars = 0;
            if (lines[line].newPage)
                tw->firstLine = line;
        } else {
            tw->chars++;
        }
    }
    tw->frame++;
    ((void (*)(void *))HANDLER(1, MenuWindow_GetType()))(tw);
    MenuManager_MarkBusy(tw->w.menu);
    if ((gKeys[0x1D] & 0x80) && (gKeys[0x2A] & 0x80) && (gKeys[0x18] & 0x80))
        sub_4C1780_1(1);
}
HOOK(0x41A600, TypeWriterBox_Run);

static int round_int(float f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

void TypeWriterBox_DrawHandler(TypeWriterBox *tw)
{
    long double fade = (long double)(MenuManager_GetFade(tw->w.menu) - 8) * 0.125f;
    float alpha = fade > 0.0f ? rf(fade) : 0.0f;
    int i = tw->firstLine;
    int k = i - 1;
    uint16_t base;

    if (k % tw->lineCount != tw->line) {
        do {
            TypeLine *l = &tw->lines[i % tw->lineCount];
            Text *t = tw->fonts[l->font];

            if (t && l->y != -1) {
                int x = l->x;
                float r = gTypeColors[l->color].r;
                float g = gTypeColors[l->color].g;
                float b = gTypeColors[l->color].b;
                float a = alpha;

                if (tw->fading) {
                    int d = tw->delay;

                    if (d < 16)
                        a = rf((long double)d * alpha * 0.0625f);
                    if (d < 32) {
                        long double f = (long double)(d - 16 < 0 ? 0 : d - 16) * 0.0625f;

                        r = rf(((long double)r - 128.0f) * f + 128.0f);
                        g = rf(((long double)g - 128.0f) * f + 128.0f);
                        b = rf(((long double)b - 128.0f) * f + 128.0f);
                    }
                }
                Text_SetString(t, l->text);
                if (x == 0)
                    x = (tw->w.w - Text_GetWidth(t)) >> 1;
                else if (x < 0)
                    x = x + (tw->w.w - Text_GetWidth(t)) - 8;
                else
                    x += 8;
                t->x = rf((long double)tw->w.x + x);
                t->flags |= 0x80000;
                t->alpha = a;
                t->color[0] = r;
                t->color[1] = g;
                t->color[2] = b;
                t->y = rf((long double)tw->w.y + l->y);
                if (i != tw->line) {
                    Text_DrawAlt(t);
                } else {
                    char buf[256];

                    crt_strncpy(buf, l->text, tw->chars);
                    buf[tw->chars] = 0;
                    Text_SetString(t, buf);
                    Text_DrawAlt(t);
                    /* blinking cursor */
                    if (tw->frame & 8) {
                        int w = Text_GetWidth(t);
                        int cx = w + round_int(t->x);
                        int cy = round_int(t->y);
                        int h = Text_GetHeight(t);

                        Draw_RectA(cx, cy, h * 3 >> 2, h, r, g, b, a, 0x80000, 0);
                    }
                }
            }
            i++;
            k++;
        } while (k % tw->lineCount != tw->line);
    }
    base = MenuWindow_GetType();
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(tw);
    MenuWindow_DrawEnd(tw, 0.5f, alpha, 0);
}
HOOK(0x41A6D0, TypeWriterBox_DrawHandler);

void TypeWriterBox_GetParams(void *task, void **out)
{
    *out = gTypeWriterBoxParams;
}
HOOK(0x41A9E0, TypeWriterBox_GetParams);

void TypeWriterBox_Restart(TypeWriterBox *tw)
{
    CALL_CHILDREN(&tw->w.task, MenuWindow_GetEvent914);
    tw->chars = 0;
    tw->line = 0;
    tw->firstLine = 0;
    tw->frame = 0;
    tw->fading = 0;
    tw->delay = 16;
}
HOOK(0x41A9F0, TypeWriterBox_Restart);

void TypeWriterBox_Activate(TypeWriterBox *tw)
{
    if (!tw->active)
        tw->active = 1;
}
HOOK(0x41AA80, TypeWriterBox_Activate);

void TypeWriterBox_Deactivate(TypeWriterBox *tw)
{
    if (tw->active)
        tw->active = 0;
}
HOOK(0x41AAA0, TypeWriterBox_Deactivate);

void TypeWriterBox_Shutdown(void)
{
    QTaskList_Free(gTypeWriterBoxParams);
    TaskType_Free(gTypeWriterBoxType);
    gTypeWriterBoxType = 0x181;
}
HOOK(0x41AAC0, TypeWriterBox_Shutdown);
