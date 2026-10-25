#include "hook.h"
#include "igi.h"
#include "menuscreen/menuscreen.h"
#include "menu/menuvars.h"

ORIG(0x4B51F0, void, QVertex_DrawQuad, (Sprite *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                        float x3, float y3, float r, float g, float b, int frame, int flags, int unk));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));

/* a line of menu text that runs its action when clicked; color is an index
   into a color table, bit 4 draws it at half alpha */
typedef struct MenuText {
    MenuWindow w;
    char text[0x40]; /* string name, looked up with MenuManager_GetString */
    char font[0x40];
    char sound[0x40]; /* played on select, "menu_sel" when empty */
    Text *label;
    int color;
    VarString action;
    Sprite *glow;
} MenuText;

/* MenuText that can be disabled or hidden by expressions */
typedef struct MenuTextConditional {
    MenuWindow w;
    char text[0x40];
    char font[0x40];
    char sound[0x40];
    Text *label;
    int color;
    VarString action;
    VarString enabledExpr;
    VarString visibleExpr;
    Sprite *glow;
    int enabled;
    int visible;
} MenuTextConditional;

_Static_assert(sizeof(MenuText) == 0x170, "MenuText");
_Static_assert(sizeof(MenuTextConditional) == 0x218, "MenuTextConditional");

extern uint16_t gMenuTextCondType;
extern void *gMenuTextCondParams;
extern const Color gMenuTextCondColors[];
extern uint16_t gMenuTextType;
extern void *gMenuTextParams;

void MenuTextConditional_Run(MenuTextConditional *mt);
void MenuTextConditional_Draw(MenuTextConditional *mt);
void MenuTextConditional_Delete(MenuTextConditional *mt);
void MenuTextConditional_Create(MenuTextConditional *mt, void *arg);
void MenuTextConditional_GetParams(void *task, void **out);
void MenuTextConditional_Changed(MenuTextConditional *mt, void *arg);
void MenuTextConditional_Place(MenuTextConditional *mt, void *arg);
void MenuText_UpdateText(MenuText *mt);

void MenuTextConditional_Init(void)
{
    MenuTextConditional mt;

    gMenuTextCondType = TaskType_New("MenuTextConditional", sizeof(MenuTextConditional), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gMenuTextCondType, 1, MenuTextConditional_Run);
    TaskType_SetHandler2(gMenuTextCondType, Screen_GetDrawEvent(), MenuTextConditional_Draw);
    TaskType_SetHandler2(gMenuTextCondType, 2, MenuTextConditional_Delete);
    TaskType_SetHandler(gMenuTextCondType, 0, MenuTextConditional_Create);
    TaskType_SetHandler(gMenuTextCondType, Param_GetTableEvent(), MenuTextConditional_GetParams);
    TaskType_SetHandler(gMenuTextCondType, Param_GetChangedEvent(), MenuTextConditional_Changed);
    TaskType_SetHandler2(gMenuTextCondType, MenuWindow_GetUpdateTextEvent(), MenuText_UpdateText);
    TaskType_SetHandler(gMenuTextCondType, MenuWindow_GetPlaceEvent(), MenuTextConditional_Place);
    gMenuTextCondParams = ParamTable_New(7);
    ParamTable_Add(gMenuTextCondParams, &mt, mt.text, Param_String32(), 7, NULL);
    ParamTable_Add(gMenuTextCondParams, &mt, mt.font, Param_String32(), 7, NULL);
    ParamTable_Add(gMenuTextCondParams, &mt, &mt.color, Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuTextCondParams, &mt, &mt.enabledExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gMenuTextCondParams, &mt, &mt.visibleExpr, Param_VarString(), 7, NULL);
    ParamTable_Add(gMenuTextCondParams, &mt, &mt.action, Param_VarString(), 7, NULL);
    ParamTable_Add(gMenuTextCondParams, &mt, mt.sound, Param_String32(), 7, NULL);
}
HOOK(0x420300, MenuTextConditional_Init);

/* hidden texts take no room in the line up */
void MenuTextConditional_Place(MenuTextConditional *mt, void *arg)
{
    if (mt->visible) {
        uint8_t ev = MenuWindow_GetPlaceEvent();

        CALL_BASE(ev, mt, arg);
    }
}
HOOK(0x420500, MenuTextConditional_Place);

void MenuTextConditional_Create(MenuTextConditional *mt, void *arg)
{
    CALL_BASE(0, mt, arg);
    mt->text[0] = 0;
    mt->font[0] = 0;
    mt->sound[0] = 0;
    mt->label = NULL;
    mt->glow = NULL;
    mt->w.x = 300;
    mt->w.y = 230;
    mt->w.centreX = 1;
    mt->w.centreY = 1;
    VarString_Init(&mt->action);
    VarString_Init(&mt->enabledExpr);
    VarString_Init(&mt->visibleExpr);
    mt->enabled = 0;
    mt->visible = 1;
}
HOOK(0x420550, MenuTextConditional_Create);

void MenuTextConditional_Delete(MenuTextConditional *mt)
{
    if (mt->label) {
        Text_Free(mt->label);
        mt->label = NULL;
    }
    VarString_Free(&mt->action);
    VarString_Free(&mt->enabledExpr);
    VarString_Free(&mt->visibleExpr);
}
HOOK(0x4205F0, MenuTextConditional_Delete);

/* makes the label from the font and string; the window is the text's size */
#define MAKE_LABEL(mt)                                                          \
    do {                                                                        \
        (mt)->w.w = 0;                                                          \
        (mt)->w.h = 0;                                                          \
        if ((mt)->w.menu) {                                                     \
            Sprite *font_ = MenuManager_FindFont((mt)->w.menu, (mt)->font);    \
                                                                                \
            (mt)->glow = MenuManager_FindSprite((mt)->w.menu, "glow.spr");     \
            if ((mt)->label) {                                                  \
                Text_Free((mt)->label);                                         \
                (mt)->label = NULL;                                             \
            }                                                                   \
            if (font_) {                                                        \
                int x_ = SCREEN_X((mt)->w.x);                                   \
                int y_ = SCREEN_Y((mt)->w.y);                                   \
                const char *s_ = MenuManager_GetString((mt)->w.menu, (mt)->text); \
                                                                                \
                (mt)->label = Text_New(font_, x_, y_, s_);                      \
                (mt)->label->unk20 = 0;                                         \
                (mt)->w.w = Text_GetWidth((mt)->label);                         \
                (mt)->w.h = Text_GetHeight((mt)->label);                        \
            }                                                                   \
        }                                                                       \
    } while (0)

void MenuTextConditional_Changed(MenuTextConditional *mt, void *arg)
{
    MAKE_LABEL(mt);
    VarString_Bind(&mt->action, mt);
    VarString_Bind(&mt->enabledExpr, mt);
    VarString_Bind(&mt->visibleExpr, mt);
    if (mt->action.text)
        mt->w.selectable = 1;
    if (VarString_HasExpr(&mt->enabledExpr)) {
        mt->enabled = round_int(VarString_EvalReal(&mt->enabledExpr, mt));
        mt->w.selectable = mt->enabled;
    }
    if (VarString_HasExpr(&mt->visibleExpr))
        mt->visible = round_int(VarString_EvalReal(&mt->visibleExpr, mt));
    CALL_BASE(Param_GetChangedEvent(), mt, arg);
}
HOOK(0x420640, MenuTextConditional_Changed);

/* hover focuses the text, a click or the select key runs its action */
#define RUN_ACTION(mt)                                                          \
    do {                                                                        \
        MenuWindow_HitTest(mt, (mt)->w.x, (mt)->w.y, (mt)->w.w, (mt)->w.h, NULL, NULL); \
        if ((mt)->w.hover && !(mt)->w.wasHover)                                   \
            MenuManager_FocusItem((mt)->w.menu, mt);                            \
        if (((mt)->w.clicked || (mt)->w.key == -7) && VarString_HasExpr(&(mt)->action)) { \
            VarString_EvalReal(&(mt)->action, mt);                              \
            Sound_PlayName((mt)->sound[0] ? (mt)->sound : "menu_sel", 0, 0);        \
        }                                                                       \
    } while (0)

void MenuTextConditional_Run(MenuTextConditional *mt)
{
    if (VarString_HasExpr(&mt->enabledExpr)) {
        mt->enabled = round_int(VarString_EvalReal(&mt->enabledExpr, mt));
        mt->w.selectable = mt->enabled;
    }
    if (mt->action.text && mt->enabled)
        RUN_ACTION(mt);
    mt->w.key = 0;
    CALL_BASE(1, mt);
}
HOOK(0x4207E0, MenuTextConditional_Run);

/* sets the label color and alpha and draws it, with a glow sprite behind
   the focused text once the menu has faded in */
#define DRAW_LABEL(mt, colors, fade, alpha, dim)                                \
    do {                                                                        \
        Text *t_ = (mt)->label;                                                 \
        int glow_ = (fade) == 16 && (mt)->w.focused;                            \
        const Color *c_ = &(colors)[(mt)->color & 0xF];                         \
                                                                                \
        if (glow_) {                                                            \
            t_->color[0] = c_->r * 0.5f + 64.0f;                                \
            t_->color[1] = c_->g * 0.5f + 64.0f;                                \
            t_->color[2] = c_->b * 0.5f + 64.0f;                                \
        } else {                                                                \
            t_->color[0] = c_->r;                                               \
            t_->color[1] = c_->g;                                               \
            t_->color[2] = c_->b;                                               \
        }                                                                       \
        if (dim)                                                                \
            alpha *= 0.5f;                                                      \
        t_->alpha = alpha;                                                      \
        t_->flags |= 0x80000;                                                   \
        (mt)->label->x = (Display_GetMode()->width >> 1) + ((mt)->w.x - 320);       \
        (mt)->label->y = (Display_GetMode()->height >> 1) + ((mt)->w.y - 240);      \
        Text_DrawAlt((mt)->label);                                              \
        if (glow_) {                                                            \
            float x0_, y0_, x1_, y1_, h_;                                       \
                                                                                \
            x0_ = rf((long double)SCREEN_X((mt)->w.x) - (long double)(mt)->w.w * 0.4f); \
            h_ = (mt)->w.h;                                                     \
            y0_ = SCREEN_Y((mt)->w.y);                                          \
            y0_ = rf((long double)y0_ - (long double)h_ * 0.2f);                \
            x1_ = rf((long double)(mt)->w.w * 1.8f + x0_);                      \
            y1_ = rf((long double)h_ * 1.4f + y0_);                             \
            QVertex_DrawQuad((mt)->glow, x0_, y0_, x1_, y0_, x0_, y1_, x1_, y1_, \
                             rf(c_->r * 0.2f), rf(c_->g * 0.2f), rf(c_->b * 0.2f), 0, 8, 0); \
        }                                                                       \
    } while (0)

void MenuTextConditional_Draw(MenuTextConditional *mt)
{
    Text *t = mt->label;
    int fade = MenuManager_GetFade(mt->w.menu);
    float alpha = fade_alpha(fade);

    if (mt->visible) {
        uint16_t base = MenuWindow_GetType();

        ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(mt);
        if (t)
            DRAW_LABEL(mt, gMenuTextCondColors, fade, alpha, !mt->enabled || (mt->color & 0x10));
    }
}
HOOK(0x4208F0, MenuTextConditional_Draw);

void MenuTextConditional_GetParams(void *task, void **out)
{
    *out = gMenuTextCondParams;
}
HOOK(0x420BB0, MenuTextConditional_GetParams);

/* language changed: look the string up again */
void MenuText_UpdateText(MenuText *mt)
{
    mt->w.w = 0;
    mt->w.h = 0;
    if (mt->w.menu && mt->label) {
        Text_SetString(mt->label, MenuManager_GetString(mt->w.menu, mt->text));
        mt->w.w = Text_GetWidth(mt->label);
        mt->w.h = Text_GetHeight(mt->label);
    }
    CALL_CHILDREN(&mt->w.task, MenuWindow_GetUpdateTextEvent);
}
HOOK(0x420BC0, MenuText_UpdateText);

void MenuTextConditional_Shutdown(void)
{
    QTaskList_Free(gMenuTextCondParams);
    TaskType_Free(gMenuTextCondType);
    gMenuTextCondType = 0x181;
}
HOOK(0x420C90, MenuTextConditional_Shutdown);

void MenuText_Run(MenuText *mt);
void MenuText_Draw(MenuText *mt);
void MenuText_Delete(MenuText *mt);
void MenuText_Create(MenuText *mt, void *arg);
void MenuText_GetParams(void *task, void **out);
void MenuText_Changed(MenuText *mt, void *arg);

void MenuText_Init(void)
{
    MenuText mt;

    gMenuTextType = TaskType_New("MenuText", sizeof(MenuText), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gMenuTextType, 1, MenuText_Run);
    TaskType_SetHandler2(gMenuTextType, Screen_GetDrawEvent(), MenuText_Draw);
    TaskType_SetHandler2(gMenuTextType, 2, MenuText_Delete);
    TaskType_SetHandler(gMenuTextType, 0, MenuText_Create);
    TaskType_SetHandler(gMenuTextType, Param_GetTableEvent(), MenuText_GetParams);
    TaskType_SetHandler(gMenuTextType, Param_GetChangedEvent(), MenuText_Changed);
    TaskType_SetHandler2(gMenuTextType, MenuWindow_GetUpdateTextEvent(), MenuText_UpdateText);
    gMenuTextParams = ParamTable_New(5);
    ParamTable_Add(gMenuTextParams, &mt, mt.text, Param_String32(), 7, NULL);
    ParamTable_Add(gMenuTextParams, &mt, mt.font, Param_String32(), 7, NULL);
    ParamTable_Add(gMenuTextParams, &mt, &mt.color, Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuTextParams, &mt, &mt.action, Param_VarString(), 7, NULL);
    ParamTable_Add(gMenuTextParams, &mt, mt.sound, Param_String32(), 7, NULL);
    Script_RegisterConstant("TEXT_BLACK", 0);
    Script_RegisterConstant("TEXT_WHITE", 1);
    Script_RegisterConstant("TEXT_GREEN", 2);
    Script_RegisterConstant("TEXT_ORANGE", 3);
}
HOOK(0x420CC0, MenuText_Init);

void MenuText_Create(MenuText *mt, void *arg)
{
    CALL_BASE(0, mt, arg);
    mt->w.x = 300;
    mt->text[0] = 0;
    mt->font[0] = 0;
    mt->sound[0] = 0;
    mt->label = NULL;
    mt->glow = NULL;
    mt->w.y = 230;
    mt->w.centreX = 1;
    mt->w.centreY = 1;
    VarString_Init(&mt->action);
}
HOOK(0x420E90, MenuText_Create);

void MenuText_Delete(MenuText *mt)
{
    if (mt->label) {
        Text_Free(mt->label);
        mt->label = NULL;
    }
    VarString_Free(&mt->action);
}
HOOK(0x420F00, MenuText_Delete);

void MenuText_Changed(MenuText *mt, void *arg)
{
    MAKE_LABEL(mt);
    VarString_Bind(&mt->action, mt);
    if (mt->action.text)
        mt->w.selectable = 1;
    CALL_BASE(Param_GetChangedEvent(), mt, arg);
}
HOOK(0x420F40, MenuText_Changed);

void MenuText_Run(MenuText *mt)
{
    if (mt->action.text)
        RUN_ACTION(mt);
    mt->w.key = 0;
    CALL_BASE(1, mt);
}
HOOK(0x421070, MenuText_Run);

void MenuText_Draw(MenuText *mt)
{
    Text *t = mt->label;
    int fade = MenuManager_GetFade(mt->w.menu);
    float alpha = fade_alpha(fade);
    uint16_t base = MenuWindow_GetType();

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(mt);
    if (t)
        DRAW_LABEL(mt, gMenuColors, fade, alpha, mt->color & 0x10);
}
HOOK(0x421130, MenuText_Draw);

void MenuText_GetParams(void *task, void **out)
{
    *out = gMenuTextParams;
}
HOOK(0x4213D0, MenuText_GetParams);

void MenuText_Shutdown(void)
{
    QTaskList_Free(gMenuTextParams);
    Script_UnregisterSymbol("TEXT_BLACK");
    Script_UnregisterSymbol("TEXT_WHITE");
    Script_UnregisterSymbol("TEXT_GREEN");
    Script_UnregisterSymbol("TEXT_ORANGE");
    TaskType_Free(gMenuTextType);
    gMenuTextType = 0x181;
}
HOOK(0x4213E0, MenuText_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B700) uint16_t gMenuTextCondType INIT(385);
PAD(igd, 53B700, 53B702, 0x2);
STORE(igd, 53B708, 4) __typeof__(gMenuTextCondColors[0]) store_53B708[8] INIT({{.r = 0.0f, .g = 0.0f, .b = 0.0f}, {.r = 128.0f, .g = 128.0f, .b = 128.0f}, {.r = 0.0f, .g = 128.0f, .b = 0.0f}, {.r = 128.0f, .g = 95.0f, .b = 0.0f}, {.r = 64.0f, .g = 128.0f, .b = 64.0f}, {.r = 3.0220246e+32f, .g = 7.8719775e+31f, .b = 1.7593369e+22f}, {.r = 7.225074e+28f, .g = 9.953185e-39f, .b = 4.8560844e+33f}, {.r = 4.7626013e+30f, .g = 0.0f, .b = 3.0220308e+32f}});
ALIAS(gMenuTextCondColors, store_53B708, 0);
PAD_CONST(igd, 53B708, 53B768, 0x8) INIT({0x5F, 0x73, 0x65, 0x6C});
DATA(53B770) uint16_t gMenuTextType INIT(385);
PAD(igd, 53B770, 53B772, 0x2);
BSS(57BBEC) void *gMenuTextCondParams;
PAD(igb, 57BBEC, 57BBF0, 0x4);
BSS(57BBEC) void *gMenuTextParams;
