#include <string.h>

#include "hook.h"
#include "igi.h"
#include "menu/menu.h"
#include "config/configvars.h"
#include "menu/menuvars.h"

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
ORIG(0x4C1780, void, Debug_Log, (int unk));
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

extern uint16_t gTypeWriterBoxType;
extern TypeLine gTypeLines[];
extern void *gTypeWriterBoxParams;
extern uint8_t gKeys[];

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
    TaskType_SetHandler2(gTypeWriterBoxType, MenuWindow_GetLoadEvent(), TypeWriterBox_Restart);
    TaskType_SetHandler2(gTypeWriterBoxType, MenuWindow_GetSaveEvent(), MenuWindow_ForwardSave);
    TaskType_SetHandler2(gTypeWriterBoxType, Level_GetActivateEvent(), TypeWriterBox_Activate);
    TaskType_SetHandler2(gTypeWriterBoxType, Level_GetDeactivateEvent(), TypeWriterBox_Deactivate);
    TaskType_SetHandler2(gTypeWriterBoxType, MenuWindow_GetUpdateTextEvent(), TypeWriterBox_UpdateText);
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
    tw->w.centreX = 1;
    tw->w.centreY = 1;
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
    ((void (*)(void *))HANDLER(Level_GetDeactivateEvent(), tw->w.task.type))(tw);
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
    CALL_CHILDREN(&tw->w.task, MenuWindow_GetUpdateTextEvent);
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
        Debug_Log(1);
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
                float r = gMenuColors[l->color].r;
                float g = gMenuColors[l->color].g;
                float b = gMenuColors[l->color].b;
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
    CALL_CHILDREN(&tw->w.task, MenuWindow_GetLoadEvent);
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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
REF_EXTERN(dat_53B084);
REF_EXTERN(dat_53B094);
REF_EXTERN(gEmptyString);
REF_EXTERN(str_53AE90);
REF_EXTERN(str_53AEB0);
REF_EXTERN(str_53AEC8);
REF_EXTERN(str_53AEDC);
REF_EXTERN(str_53AEEC);
REF_EXTERN(str_53AF08);
REF_EXTERN(str_53AF1C);
REF_EXTERN(str_53AF30);
REF_EXTERN(str_53AF50);
REF_EXTERN(str_53AF60);
REF_EXTERN(str_53AF7C);
REF_EXTERN(str_53AF8C);
REF_EXTERN(str_53AFA4);
REF_EXTERN(str_53AFB4);
REF_EXTERN(str_53AFC4);
REF_EXTERN(str_53AFD4);
REF_EXTERN(str_53AFE0);
REF_EXTERN(str_53AFE4);
REF_EXTERN(str_53AFFC);
REF_EXTERN(str_53B00C);
REF_EXTERN(str_53B028);
REF_EXTERN(str_53B034);
REF_EXTERN(str_53B050);
REF_EXTERN(str_53B058);
REF_EXTERN(str_53B068);
REF_EXTERN(str_53B078);
REF_EXTERN(str_53B0A8);
REF_EXTERN(str_53B0B8);
REF_EXTERN(str_53B0C8);
REF_EXTERN(str_53B0D8);
REF_EXTERN(str_53B0E8);
REF_EXTERN(str_53B10C);
REF_EXTERN(str_53B11C);
REF_EXTERN(str_53B130);
REF_EXTERN(str_53B148);
REF_EXTERN(str_53B158);
REF_EXTERN(str_53B164);
REF_EXTERN(str_53B174);
REF_EXTERN(str_53B184);
REF_EXTERN(str_53B190);
REF_EXTERN(str_53B198);
REF_EXTERN(str_53B1A4);
REF_EXTERN(str_53B1B0);
REF_EXTERN(str_53B1BC);
REF_EXTERN(str_53B1DC);
REF_EXTERN(str_53B1E8);
REF_EXTERN(str_53B200);
REF_EXTERN(str_53B20C);
REF_EXTERN(str_53B220);
REF_EXTERN(str_53B230);
REF_EXTERN(str_53B244);
REF_EXTERN(str_53B254);
REF_EXTERN(str_53B260);
REF_EXTERN(str_53B270);
REF_EXTERN(str_53B284);
REF_EXTERN(str_53B294);
REF_EXTERN(str_53B2A4);
REF_EXTERN(str_53B2B4);
REF_EXTERN(str_53B2C0);
REF_EXTERN(str_53B2D0);
REF_EXTERN(str_53B2F0);
REF_EXTERN(str_53B310);
REF_EXTERN(str_53B318);
REF_EXTERN(str_53B32C);
REF_EXTERN(str_53B330);
REF_EXTERN(str_53B34C);
REF_EXTERN(str_53B35C);
REF_EXTERN(str_53B364);
REF_EXTERN(str_53B36C);
REF_EXTERN(str_53B37C);
REF_EXTERN(str_53B384);
REF_EXTERN(str_53B394);
REF_EXTERN(str_53B3A0);
REF_EXTERN(str_53B3B4);
REF_EXTERN(str_53B3B8);
REF_EXTERN(str_53B3C8);
REF_EXTERN(str_53B3D8);
REF_EXTERN(str_53B3EC);
REF_EXTERN(str_53B400);
REF_EXTERN(str_53B410);
REF_EXTERN(str_53B420);
REF_EXTERN(str_53B430);
REF_EXTERN(str_53B448);
REF_EXTERN(str_53B458);
REF_EXTERN(str_53B468);
REF_EXTERN(str_53B478);
REF_EXTERN(str_53B488);
REF_EXTERN(str_53B49C);
REF_EXTERN(str_53B4AC);
REF_EXTERN(str_53B4B8);
REF_EXTERN(str_53B4C4);
REF_EXTERN(str_53B4D4);
REF_EXTERN(str_53B4E4);
REF_EXTERN(str_53B4F8);
REF_EXTERN(str_53B504);
REF_EXTERN(str_53B514);
REF_EXTERN(str_53B524);
REF_EXTERN(str_53B534);
DATA(539C40) uint16_t gTypeWriterBoxType INIT(385);
PAD(igd, 539C40, 539C42, 0x6);
STORE(igd, 539C40, 4) __typeof__(gTypeLines[0]) store_539C48[130] INIT({{.y = 4, .color = 1, .font = 2, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[0].key))ref_str_53B534}, {.y = 34, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[1].key))ref_str_53B524}, {.x = 1, .y = 54, .color = 2, .font = 2, .key = (__typeof__(store_539C48[2].key))ref_str_53B514}, {.x = -1, .y = 54, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[3].key))ref_str_53B504}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[4].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[5].key))ref_str_53B4F8}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[6].key))ref_str_53B4E4}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[7].key))ref_str_53B4D4}, {.x = 1, .y = 44, .color = 2, .font = 2, .key = (__typeof__(store_539C48[8].key))ref_str_53B4C4}, {.x = -1, .y = 44, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[9].key))ref_str_53B4B8}, {.x = 1, .y = 64, .color = 2, .font = 2, .key = (__typeof__(store_539C48[10].key))ref_str_53B4AC}, {.x = -1, .y = 64, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[11].key))ref_str_53B4B8}, {.x = 1, .y = 84, .color = 2, .font = 2, .key = (__typeof__(store_539C48[12].key))ref_str_53B49C}, {.x = -1, .y = 84, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[13].key))ref_str_53B4B8}, {.x = 1, .y = 104, .color = 2, .font = 2, .key = (__typeof__(store_539C48[14].key))ref_str_53B488}, {.x = -1, .y = 104, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[15].key))ref_str_53B4B8}, {.x = 1, .y = 124, .color = 2, .font = 2, .key = (__typeof__(store_539C48[16].key))ref_str_53B478}, {.x = -1, .y = 124, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[17].key))ref_str_53B4B8}, {.x = 1, .y = 144, .color = 2, .font = 2, .key = (__typeof__(store_539C48[18].key))ref_str_53B468}, {.x = -1, .y = 144, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[19].key))ref_str_53B4B8}, {.x = 1, .y = 164, .color = 2, .font = 2, .key = (__typeof__(store_539C48[20].key))ref_str_53B458}, {.x = -1, .y = 164, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[21].key))ref_str_53B4B8}, {.x = 1, .y = 184, .color = 2, .font = 2, .key = (__typeof__(store_539C48[22].key))ref_str_53B448}, {.x = -1, .y = 184, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[23].key))ref_str_53B4B8}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[24].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[25].key))ref_str_53B430}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[26].key))ref_str_53B420}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[27].key))ref_str_53B4B8}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[28].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[29].key))ref_str_53B410}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[30].key))ref_str_53B400}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[31].key))ref_str_53B3EC}, {.x = 1, .y = 44, .color = 2, .font = 2, .key = (__typeof__(store_539C48[32].key))ref_str_53B3D8}, {.x = -1, .y = 44, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[33].key))ref_str_53B3C8}, {.x = 1, .y = 64, .color = 2, .font = 2, .key = (__typeof__(store_539C48[34].key))ref_str_53B3B8}, {.x = -1, .y = 64, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[35].key))ref_str_53B3C8}, {.y = 84, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[36].key))ref_str_53B3B4}, {.x = 1, .y = 104, .color = 2, .font = 2, .key = (__typeof__(store_539C48[37].key))ref_str_53B3A0}, {.x = -1, .y = 104, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[38].key))ref_str_53B394}, {.x = 1, .y = 124, .color = 2, .font = 2, .key = (__typeof__(store_539C48[39].key))ref_str_53B384}, {.x = -1, .y = 124, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[40].key))ref_str_53B37C}, {.x = 1, .y = 144, .color = 2, .font = 2, .key = (__typeof__(store_539C48[41].key))ref_str_53B36C}, {.x = -1, .y = 144, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[42].key))ref_str_53B364}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[43].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[44].key))ref_str_53B35C}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[45].key))ref_str_53B34C}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[46].key))ref_str_53B330}, {.y = 44, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[47].key))ref_str_53B32C}, {.x = 1, .y = 64, .color = 2, .font = 2, .key = (__typeof__(store_539C48[48].key))ref_str_53B318}, {.x = -1, .y = 64, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[49].key))ref_str_53B310}, {.y = 84, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[50].key))ref_str_53B2F0}, {.y = 104, .color = 2, .font = 2, .key = (__typeof__(store_539C48[51].key))ref_str_53B2D0}, {.y = 124, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[52].key))ref_str_53B2C0}, {.y = 144, .color = 2, .font = 2, .key = (__typeof__(store_539C48[53].key))ref_str_53B2B4}, {.y = 164, .color = 2, .font = 2, .key = (__typeof__(store_539C48[54].key))ref_str_53B2A4}, {.y = 184, .color = 2, .font = 2, .key = (__typeof__(store_539C48[55].key))ref_str_53B294}, {.y = 204, .color = 2, .font = 2, .key = (__typeof__(store_539C48[56].key))ref_str_53B284}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[57].key))ref_gEmptyString}, {.y = 4, .color = 1, .font = 2, .newPage = 1, .key = (__typeof__(store_539C48[58].key))ref_str_53B270}, {.x = 1, .y = 34, .color = 2, .font = 2, .key = (__typeof__(store_539C48[59].key))ref_str_53B260}, {.x = -1, .y = 34, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[60].key))ref_str_53B254}, {.x = 1, .y = 54, .color = 2, .font = 2, .key = (__typeof__(store_539C48[61].key))ref_str_53B244}, {.x = -1, .y = 54, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[62].key))ref_str_53B230}, {.x = 1, .y = 74, .color = 2, .font = 2, .key = (__typeof__(store_539C48[63].key))ref_str_53B220}, {.x = -1, .y = 74, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[64].key))ref_str_53B20C}, {.x = 1, .y = 94, .color = 2, .font = 2, .key = (__typeof__(store_539C48[65].key))ref_str_53B200}, {.x = -1, .y = 94, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[66].key))ref_str_53B1E8}, {.x = 1, .y = 114, .color = 2, .font = 2, .key = (__typeof__(store_539C48[67].key))ref_str_53B1DC}, {.x = -1, .y = 114, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[68].key))ref_str_53B1BC}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[69].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[70].key))ref_str_53B32C}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[71].key))ref_str_53B1B0}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[72].key))ref_str_53B1A4}, {.x = 1, .y = 44, .color = 2, .font = 2, .key = (__typeof__(store_539C48[73].key))ref_str_53B198}, {.x = -1, .y = 44, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[74].key))ref_str_53B190}, {.x = 1, .y = 64, .color = 2, .font = 2, .key = (__typeof__(store_539C48[75].key))ref_str_53B184}, {.x = -1, .y = 64, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[76].key))ref_str_53B190}, {.x = 1, .y = 84, .color = 2, .font = 2, .key = (__typeof__(store_539C48[77].key))ref_str_53B174}, {.x = -1, .y = 84, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[78].key))ref_str_53B190}, {.x = 1, .y = 104, .color = 2, .font = 2, .key = (__typeof__(store_539C48[79].key))ref_str_53B164}, {.x = -1, .y = 104, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[80].key))ref_str_53B158}, {.x = 1, .y = 124, .color = 2, .font = 2, .key = (__typeof__(store_539C48[81].key))ref_str_53B148}, {.x = -1, .y = 124, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[82].key))ref_str_53B130}, {.x = 1, .y = 144, .color = 2, .font = 2, .key = (__typeof__(store_539C48[83].key))ref_str_53B11C}, {.x = -1, .y = 144, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[84].key))ref_str_53B130}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[85].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[86].key))ref_str_53B10C}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[87].key))ref_str_53B148}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[88].key))ref_str_53B0E8}, {.x = 1, .y = 44, .color = 2, .font = 2, .key = (__typeof__(store_539C48[89].key))ref_str_53B0D8}, {.x = -1, .y = 44, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[90].key))ref_str_53B1A4}, {.x = 1, .y = 64, .color = 2, .font = 2, .key = (__typeof__(store_539C48[91].key))ref_str_53B0C8}, {.x = -1, .y = 64, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[92].key))ref_str_53B190}, {.x = 1, .y = 84, .color = 2, .font = 2, .key = (__typeof__(store_539C48[93].key))ref_str_53B0B8}, {.x = -1, .y = 84, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[94].key))ref_str_53B190}, {.x = 1, .y = 104, .color = 2, .font = 2, .key = (__typeof__(store_539C48[95].key))ref_str_53B0A8}, {.x = -1, .y = 104, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[96].key))ref_str_53B190}, {.x = 1, .y = 124, .color = 2, .font = 2, .key = (__typeof__(store_539C48[97].key))ref_dat_53B094}, {.x = -1, .y = 124, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[98].key))ref_str_53B190}, {.x = 1, .y = 144, .color = 2, .font = 2, .key = (__typeof__(store_539C48[99].key))ref_dat_53B084}, {.x = -1, .y = 144, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[100].key))ref_str_53B190}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[101].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[102].key))ref_str_53B078}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[103].key))ref_str_53B068}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[104].key))ref_str_53B058}, {.x = 1, .y = 44, .color = 2, .font = 2, .key = (__typeof__(store_539C48[105].key))ref_str_53B050}, {.x = -1, .y = 44, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[106].key))ref_str_53B034}, {.x = 1, .y = 64, .color = 2, .font = 2, .key = (__typeof__(store_539C48[107].key))ref_str_53B028}, {.x = -1, .y = 64, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[108].key))ref_str_53B00C}, {.x = 1, .y = 84, .color = 2, .font = 2, .key = (__typeof__(store_539C48[109].key))ref_str_53AFFC}, {.x = -1, .y = 84, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[110].key))ref_str_53AFE4}, {.y = 104, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[111].key))ref_str_53AFE0}, {.x = 1, .y = 124, .color = 2, .font = 2, .key = (__typeof__(store_539C48[112].key))ref_str_53AFD4}, {.x = -1, .y = 124, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[113].key))ref_str_53AFC4}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[114].key))ref_gEmptyString}, {.y = 4, .color = 3, .font = 3, .newPage = 1, .localized = 1, .key = (__typeof__(store_539C48[115].key))ref_str_53AFB4}, {.x = 1, .y = 24, .color = 2, .font = 2, .key = (__typeof__(store_539C48[116].key))ref_str_53AFA4}, {.x = -1, .y = 24, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[117].key))ref_str_53AF8C}, {.x = 1, .y = 44, .color = 2, .font = 2, .key = (__typeof__(store_539C48[118].key))ref_str_53AF7C}, {.x = -1, .y = 44, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[119].key))ref_str_53AF60}, {.x = 1, .y = 64, .color = 2, .font = 2, .key = (__typeof__(store_539C48[120].key))ref_str_53AF50}, {.x = -1, .y = 64, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[121].key))ref_str_53AF30}, {.y = 84, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[122].key))ref_str_53AF1C}, {.x = 1, .y = 104, .color = 2, .font = 2, .key = (__typeof__(store_539C48[123].key))ref_str_53AF08}, {.x = -1, .y = 104, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[124].key))ref_str_53AEEC}, {.x = 1, .y = 124, .color = 2, .font = 2, .key = (__typeof__(store_539C48[125].key))ref_str_53AEDC}, {.x = -1, .y = 124, .color = 2, .font = 2, .localized = 1, .key = (__typeof__(store_539C48[126].key))ref_str_53AEC8}, {.y = 144, .color = 3, .font = 3, .localized = 1, .key = (__typeof__(store_539C48[127].key))ref_str_53AEB0}, {.y = 164, .color = 2, .font = 2, .key = (__typeof__(store_539C48[128].key))ref_str_53AE90}, {.x = -100, .y = -1, .color = 2, .font = 2, .pause = -1, .key = (__typeof__(store_539C48[129].key))ref_gEmptyString}});
ALIAS(gTypeLines, store_539C48, 0);
DATA(539C40) char str_53AE90[31] INIT("A Creative Experience (LONDON)");
PAD(igd, 539C40, 53AEAF, 0x1);
DATA(539C40) char str_53AEB0[23] INIT("Manual Design & Layout");
PAD(igd, 539C40, 53AEC7, 0x1);
DATA(539C40) char str_53AEC8[18] INIT("Creative Designer");
PAD(igd, 539C40, 53AEDA, 0x2);
DATA(539C40) char str_53AEDC[15] INIT("Caroline Simon");
PAD(igd, 539C40, 53AEEB, 0x1);
DATA(539C40) char str_53AEEC[26] INIT("Creative Services Manager");
PAD(igd, 539C40, 53AF06, 0x2);
DATA(539C40) char str_53AF08[20] INIT("Matt Carter-Johnson");
DATA(539C40) char str_53AF1C[18] INIT("Creative Services");
PAD(igd, 539C40, 53AF2E, 0x2);
DATA(539C40) char str_53AF30[30] INIT("Printed Materials Coordinator");
PAD(igd, 539C40, 53AF4E, 0x2);
DATA(539C40) char str_53AF50[15] INIT("Jacqui Ralston");
PAD(igd, 539C40, 53AF5F, 0x1);
DATA(539C40) char str_53AF60[27] INIT("Group Localisation Manager");
PAD(igd, 539C40, 53AF7B, 0x1);
DATA(539C40) char str_53AF7C[13] INIT("Flavia Grant");
PAD(igd, 539C40, 53AF89, 0x3);
DATA(539C40) char str_53AF8C[21] INIT("Localisation Manager");
PAD(igd, 539C40, 53AFA1, 0x3);
DATA(539C40) char str_53AFA4[14] INIT("Holly Andrews");
PAD(igd, 539C40, 53AFB2, 0x2);
DATA(539C40) char str_53AFB4[13] INIT("Localisation");
PAD(igd, 539C40, 53AFC1, 0x3);
DATA(539C40) char str_53AFC4[14] INIT("PR Specialist");
PAD(igd, 539C40, 53AFD2, 0x2);
DATA(539C40) char str_53AFD4[12] INIT("Eva Whitlow");
DATA(539C40) char str_53AFE0[3] INIT("PR");
PAD(igd, 539C40, 53AFE3, 0x1);
DATA(539C40) char str_53AFE4[21] INIT("Product Manager (US)");
PAD(igd, 539C40, 53AFF9, 0x3);
DATA(539C40) char str_53AFFC[14] INIT("Dave DePaulis");
PAD(igd, 539C40, 53B00A, 0x2);
DATA(539C40) char str_53B00C[25] INIT("Product Manager (France)");
PAD(igd, 539C40, 53B025, 0x3);
DATA(539C40) char str_53B028[9] INIT("Christel");
PAD(igd, 539C40, 53B031, 0x3);
DATA(539C40) char str_53B034[26] INIT("Product Manager (Germany)");
PAD(igd, 539C40, 53B04E, 0x2);
DATA(539C40) char str_53B050[7] INIT("Herman");
PAD(igd, 539C40, 53B057, 0x1);
DATA(539C40) char str_53B058[16] INIT("Product Manager");
DATA(539C40) char str_53B068[13] INIT("Rebecca West");
PAD(igd, 539C40, 53B075, 0x3);
DATA(539C40) char str_53B078[10] INIT("Marketing");
PAD(igd, 539C40, 53B082, 0x2);
DATA(539C40) uint8_t dat_53B084[16] INIT({77, 97, 105, 107, 101, 32, 75, 104, 246, 108, 101, 114});
DATA(539C40) uint8_t dat_53B094[20] INIT({74, 252, 114, 103, 101, 110, 32, 76, 111, 116, 116, 101, 114, 109, 111, 115, 101, 114});
DATA(539C40) char str_53B0A8[14] INIT("Sam Byung Kil");
PAD(igd, 539C40, 53B0B6, 0x2);
DATA(539C40) char str_53B0B8[15] INIT("Marco Vernetti");
PAD(igd, 539C40, 53B0C7, 0x1);
DATA(539C40) char str_53B0C8[13] INIT("Iain Willows");
PAD(igd, 539C40, 53B0D5, 0x3);
DATA(539C40) char str_53B0D8[15] INIT("Alex Lepoureau");
PAD(igd, 539C40, 53B0E7, 0x1);
DATA(539C40) char str_53B0E8[34] INIT("Assistant QA Localisation Manager");
PAD(igd, 539C40, 53B10A, 0x2);
DATA(539C40) char str_53B10C[16] INIT("QA Localisation");
DATA(539C40) char str_53B11C[19] INIT("James Featherstone");
PAD(igd, 539C40, 53B12F, 0x1);
DATA(539C40) char str_53B130[21] INIT("Assistant QA Manager");
PAD(igd, 539C40, 53B145, 0x3);
DATA(539C40) char str_53B148[16] INIT("Jean-Yves Duret");
DATA(539C40) char str_53B158[11] INIT("QA Manager");
PAD(igd, 539C40, 53B163, 0x1);
DATA(539C40) char str_53B164[13] INIT("Chris Rowley");
PAD(igd, 539C40, 53B171, 0x3);
DATA(539C40) char str_53B174[13] INIT("Lawrence Day");
PAD(igd, 539C40, 53B181, 0x3);
DATA(539C40) char str_53B184[11] INIT("Steve Didd");
PAD(igd, 539C40, 53B18F, 0x1);
DATA(539C40) char str_53B190[7] INIT("Tester");
PAD(igd, 539C40, 53B197, 0x1);
DATA(539C40) char str_53B198[11] INIT("Chris Ince");
PAD(igd, 539C40, 53B1A3, 0x1);
DATA(539C40) char str_53B1A4[12] INIT("Lead Tester");
DATA(539C40) char str_53B1B0[12] INIT("Daryl Bibby");
DATA(539C40) char str_53B1BC[29] INIT("External Development Manager");
PAD(igd, 539C40, 53B1D9, 0x3);
DATA(539C40) char str_53B1DC[11] INIT("David Rose");
PAD(igd, 539C40, 53B1E7, 0x1);
DATA(539C40) char str_53B1E8[21] INIT("Shadow Producer (US)");
PAD(igd, 539C40, 53B1FD, 0x3);
DATA(539C40) char str_53B200[10] INIT("Frank Hom");
PAD(igd, 539C40, 53B20A, 0x2);
DATA(539C40) char str_53B20C[19] INIT("Assistant Producer");
PAD(igd, 539C40, 53B21F, 0x1);
DATA(539C40) char str_53B220[14] INIT("Andrew Norman");
PAD(igd, 539C40, 53B22E, 0x2);
DATA(539C40) char str_53B230[19] INIT("Executive Producer");
PAD(igd, 539C40, 53B243, 0x1);
DATA(539C40) char str_53B244[14] INIT("Ed Bainbridge");
PAD(igd, 539C40, 53B252, 0x2);
DATA(539C40) char str_53B254[9] INIT("Producer");
PAD(igd, 539C40, 53B25D, 0x3);
DATA(539C40) char str_53B260[15] INIT("Richard Carter");
PAD(igd, 539C40, 53B26F, 0x1);
DATA(539C40) char str_53B270[18] INIT("EIDOS Interactive");
PAD(igd, 539C40, 53B282, 0x2);
DATA(539C40) char str_53B284[15] INIT("Larissa Murray");
PAD(igd, 539C40, 53B293, 0x1);
DATA(539C40) char str_53B294[13] INIT("Danny McCall");
PAD(igd, 539C40, 53B2A1, 0x3);
DATA(539C40) char str_53B2A4[15] INIT("Amanda Mealing");
PAD(igd, 539C40, 53B2B3, 0x1);
DATA(539C40) char str_53B2B4[12] INIT("Kerry Shale");
DATA(539C40) char str_53B2C0[13] INIT("Voice Talent");
PAD(igd, 539C40, 53B2CD, 0x3);
DATA(539C40) char str_53B2D0[30] INIT("Philip Morris at AllintheGame");
PAD(igd, 539C40, 53B2EE, 0x2);
DATA(539C40) char str_53B2F0[29] INIT("Casting and Voice Production");
PAD(igd, 539C40, 53B30D, 0x3);
DATA(539C40) char str_53B310[7] INIT("IGI QA");
PAD(igd, 539C40, 53B317, 0x1);
DATA(539C40) char str_53B318[18] INIT("Henning Strandvik");
PAD(igd, 539C40, 53B32A, 0x2);
DATA(539C40) char str_53B32C[3] INIT("QA");
PAD(igd, 539C40, 53B32F, 0x1);
DATA(539C40) char str_53B330[26] INIT("Musician & Sound Designer");
PAD(igd, 539C40, 53B34A, 0x2);
DATA(539C40) char str_53B34C[14] INIT("Kim M. Jensen");
PAD(igd, 539C40, 53B35A, 0x2);
DATA(539C40) char str_53B35C[6] INIT("Music");
PAD(igd, 539C40, 53B362, 0x2);
DATA(539C40) char str_53B364[8] INIT("Artist2");
DATA(539C40) char str_53B36C[15] INIT("Mikael Noguchi");
PAD(igd, 539C40, 53B37B, 0x1);
DATA(539C40) char str_53B37C[8] INIT("Artist1");
DATA(539C40) char str_53B384[15] INIT("Joachim Barrum");
PAD(igd, 539C40, 53B393, 0x1);
DATA(539C40) char str_53B394[12] INIT("Lead Artist");
DATA(539C40) char str_53B3A0[19] INIT("Olav-Rasmus Vorren");
PAD(igd, 539C40, 53B3B3, 0x1);
DATA(539C40) char str_53B3B4[4] INIT("Art");
DATA(539C40) char str_53B3B8[15] INIT("Jolyon Leonard");
PAD(igd, 539C40, 53B3C7, 0x1);
DATA(539C40) char str_53B3C8[15] INIT("Level Designer");
PAD(igd, 539C40, 53B3D7, 0x1);
DATA(539C40) char str_53B3D8[19] INIT("Ole Marius Kohmann");
PAD(igd, 539C40, 53B3EB, 0x1);
DATA(539C40) char str_53B3EC[20] INIT("Lead Level Designer");
DATA(539C40) char str_53B400[14] INIT("Gavin Skinner");
PAD(igd, 539C40, 53B40E, 0x2);
DATA(539C40) char str_53B410[13] INIT("Level Design");
PAD(igd, 539C40, 53B41D, 0x3);
DATA(539C40) char str_53B420[13] INIT("Thomas Hagen");
PAD(igd, 539C40, 53B42D, 0x3);
DATA(539C40) char str_53B430[23] INIT("Additional Programming");
PAD(igd, 539C40, 53B447, 0x1);
DATA(539C40) char str_53B448[16] INIT("Henrik Holmdahl");
DATA(539C40) char str_53B458[14] INIT("Paul Endresen");
PAD(igd, 539C40, 53B466, 0x2);
DATA(539C40) char str_53B468[14] INIT("Johan Overbye");
PAD(igd, 539C40, 53B476, 0x2);
DATA(539C40) char str_53B478[15] INIT("Stein Pedersen");
PAD(igd, 539C40, 53B487, 0x1);
DATA(539C40) char str_53B488[17] INIT("Tim van Klooster");
PAD(igd, 539C40, 53B499, 0x3);
DATA(539C40) char str_53B49C[14] INIT("Paal Holmberg");
PAD(igd, 539C40, 53B4AA, 0x2);
DATA(539C40) char str_53B4AC[12] INIT("Martin Gram");
DATA(539C40) char str_53B4B8[11] INIT("Programmer");
PAD(igd, 539C40, 53B4C3, 0x1);
DATA(539C40) char str_53B4C4[15] INIT("Anders Dybdahl");
PAD(igd, 539C40, 53B4D3, 0x1);
DATA(539C40) char str_53B4D4[16] INIT("Lead Programmer");
DATA(539C40) char str_53B4E4[17] INIT("Ole Marius Liabo");
PAD(igd, 539C40, 53B4F5, 0x3);
DATA(539C40) char str_53B4F8[12] INIT("Programming");
DATA(539C40) char str_53B504[15] INIT("Project Leader");
PAD(igd, 539C40, 53B513, 0x1);
DATA(539C40) char str_53B514[15] INIT("Andrew Wensley");
PAD(igd, 539C40, 53B523, 0x1);
DATA(539C40) char str_53B524[13] INIT("Game Concept");
PAD(igd, 539C40, 53B531, 0x3);
DATA(539C40) char str_53B534[10] INIT("Innerloop");
PAD(igd, 539C40, 53B53E, 0x2);
STORE(igd, 53B778, 4) __typeof__(gMenuColors[0]) store_53B778[10] INIT({{.r = 0.0f, .g = 0.0f, .b = 0.0f}, {.r = 128.0f, .g = 128.0f, .b = 128.0f}, {.r = 0.0f, .g = 128.0f, .b = 0.0f}, {.r = 128.0f, .g = 95.0f, .b = 0.0f}, {.r = 64.0f, .g = 128.0f, .b = 64.0f}, {.r = 3.7155043e+12f, .g = 13.144378f, .b = 6.362229e-39f}, {.r = 3.7155043e+12f, .g = 3364.4607f, .b = 2.80778e-41f}, {.r = 3.7155043e+12f, .g = 820597.94f, .b = 2.48702e-41f}, {.r = 3.7155043e+12f, .g = 12.766204f, .b = 2.69988e-41f}, {.r = 3.0220246e+32f, .g = 7.8719775e+31f, .b = 0.0f}});
ALIAS(gMenuColors, store_53B778, 0);
BSS(57BB90) void *gTypeWriterBoxParams;
