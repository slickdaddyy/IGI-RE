#include "hook.h"
#include "igi.h"
#include "ui/ui.h"
#include "ui/uitypes.h"
#include "ui/uivars.h"

ORIG(0x4B4AF0, void, Draw_RectAZ, (float x, float y, float w, float h, float r, float g, float b, float a, float z,
                                   int flags, int unk));
ORIG(0x41E5A0, void, ToggleBox_Activate, (void *tb));
ORIG(0x41E600, void, ToggleBox_Deactivate, (void *tb));

void ToggleBox_Create(ToggleBox *tb, void *arg);
void ToggleBox_Delete(ToggleBox *tb);
void ToggleBox_Changed(ToggleBox *tb, void *arg);
void ToggleBox_Run(ToggleBox *tb);
void ToggleBox_Draw(ToggleBox *tb);
void ToggleBox_GetParams(void *task, void **out);
void ToggleBox_Layout(ToggleBox *tb);
void ToggleBox_Save(ToggleBox *tb);

void ToggleBox_Init(void)
{
    ToggleBox tb;

    gToggleBoxType = TaskType_New("ToggleBox", sizeof(ToggleBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gToggleBoxType, 1, ToggleBox_Run);
    TaskType_SetHandler2(gToggleBoxType, Screen_GetDrawEvent(), ToggleBox_Draw);
    TaskType_SetHandler2(gToggleBoxType, 2, ToggleBox_Delete);
    TaskType_SetHandler(gToggleBoxType, 0, ToggleBox_Create);
    TaskType_SetHandler(gToggleBoxType, Param_GetTableEvent(), ToggleBox_GetParams);
    TaskType_SetHandler(gToggleBoxType, Param_GetChangedEvent(), ToggleBox_Changed);
    TaskType_SetHandler2(gToggleBoxType, MenuWindow_GetLoadEvent(), ToggleBox_Layout);
    TaskType_SetHandler2(gToggleBoxType, MenuWindow_GetSaveEvent(), ToggleBox_Save);
    TaskType_SetHandler2(gToggleBoxType, Level_GetActivateEvent(), ToggleBox_Activate);
    TaskType_SetHandler2(gToggleBoxType, Level_GetDeactivateEvent(), ToggleBox_Deactivate);
    gToggleBoxParams = ParamTable_New(2);
    ParamTable_Add(gToggleBoxParams, &tb, &tb.value, Param_VarString(), 7, NULL);
    ParamTable_Add(gToggleBoxParams, &tb, &tb.forward, Param_VarString(), 7, NULL);
}
HOOK(0x41DC50, ToggleBox_Init);

void ToggleBox_Create(ToggleBox *tb, void *arg)
{
    CALL_BASE(0, tb, arg);
    tb->left = (MenuWindow *)Task_New(NULL, MenuWindow_GetType(), NULL);
    tb->right = (MenuWindow *)Task_New(NULL, MenuWindow_GetType(), NULL);
    tb->left->menu = tb->w.menu;
    tb->right->menu = tb->w.menu;
    tb->unk54[0] = 0;
    tb->right->centreY = 0;
    tb->left->centreY = 0;
    tb->right->centreX = 0;
    tb->left->centreX = 0;
    tb->isOn = 1;
    tb->w.w = 50;
    tb->w.h = 15;
    tb->yes = NULL;
    tb->no = NULL;
    tb->w.centreX = 1;
    tb->w.centreY = 1;
    VarString_Init(&tb->value);
    VarString_Init(&tb->forward);
    tb->registered = 0;
}
HOOK(0x41DDD0, ToggleBox_Create);

void ToggleBox_Delete(ToggleBox *tb)
{
    if (tb->yes)
        Text_Free(tb->yes);
    if (tb->no)
        Text_Free(tb->no);
    Task_Delete(tb->left);
    Task_Delete(tb->right);
    VarString_Free(&tb->value);
    VarString_Free(&tb->forward);
    CALL_OWN(Level_GetDeactivateEvent(), tb);
}
HOOK(0x41DEB0, ToggleBox_Delete);

void ToggleBox_Changed(ToggleBox *tb, void *arg)
{
    Sprite *font = MenuManager_FindFont(tb->w.menu, "font4.fnt");
    void *menu = tb->w.menu;

    if (tb->yes)
        Text_Free(tb->yes);
    if (tb->no)
        Text_Free(tb->no);
    if (font) {
        tb->yes = Text_New(font, 0, 0, MenuManager_GetString(menu, "Yes"));
        tb->no = Text_New(font, 0, 0, MenuManager_GetString(menu, "No"));
        tb->yes->unk20 = 0;
        tb->no->unk20 = 0;
    }
    VarString_Bind(&tb->value, tb);
    VarString_Bind(&tb->forward, tb);
    CALL_BASE(Param_GetChangedEvent(), tb, arg);
}
HOOK(0x41DF30, ToggleBox_Changed);

/* the left half switches on, the right half off */
void ToggleBox_Run(ToggleBox *tb)
{
    MenuWindow *l = tb->left, *r = tb->right;
    int half = tb->w.w >> 1;

    l->y = tb->w.y;
    l->x = tb->w.x;
    l->w = half;
    l->h = tb->w.h;
    r->x = tb->w.x + half;
    r->y = tb->w.y;
    r->w = tb->w.w - half;
    r->h = tb->w.h;
    MenuWindow_HitTest(l, l->x, l->y, l->w, l->h, NULL, NULL);
    MenuWindow_HitTest(r, r->x, r->y, r->w, r->h, NULL, NULL);
    if ((l->clicked || tb->w.key == -1) && !tb->isOn) {
        Sound_PlayName("menu_high", 0, NULL);
        tb->isOn = 1;
    } else if ((r->clicked || tb->w.key == -2) && tb->isOn) {
        Sound_PlayName("menu_high", 0, NULL);
        tb->isOn = 0;
    }
    CALL_BASE(1, tb);
}
HOOK(0x41E020, ToggleBox_Run);

void ToggleBox_Draw(ToggleBox *tb)
{
    MenuWindow *sel = tb->isOn ? tb->left : tb->right;
    float alpha = ui_alpha(tb->w.menu);
    float c;

    {
        uint16_t base = MenuWindow_GetType();

        ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(tb);
    }
    if (tb->yes && tb->no) {
        int ty = (tb->w.h - Text_GetHeight(tb->yes)) / 2 + tb->w.y;
        int xYes = tb->w.w / 4 - Text_GetWidth(tb->yes) / 2 + tb->w.x;
        int xNo = tb->w.w * 3 / 4 - Text_GetWidth(tb->no) / 2 + tb->w.x;
        int x1 = (Display_GetMode()->width >> 1) - 320 + xYes;
        int x2 = (Display_GetMode()->width >> 1) - 320 + xNo;
        float y = (Display_GetMode()->height >> 1) - 240 + ty;

        tb->yes->color[0] = 0;
        tb->yes->color[1] = 0;
        tb->yes->color[2] = 0;
        tb->yes->flags |= 0x80000;
        tb->yes->alpha = alpha;
        tb->yes->x = x1;
        tb->yes->y = y;
        Text_DrawAlt(tb->yes);
        tb->no->color[0] = 0;
        tb->no->color[1] = 0;
        tb->no->color[2] = 0;
        tb->no->flags |= 0x80000;
        tb->no->alpha = alpha;
        tb->no->x = x2;
        tb->no->y = y;
        Text_DrawAlt(tb->no);
    }
    c = tb->w.focused ? 64.0f : 0.0f;
    Draw_RectAZ((Display_GetMode()->width >> 1) - 320 + sel->x, (Display_GetMode()->height >> 1) - 240 + sel->y, sel->w,
                sel->h, c, 128.0f, c, alpha, 1.0f, 0x80000, 0);
    MenuWindow_DrawEnd(tb, 0.5f, alpha, tb->w.focused);
}
HOOK(0x41E130, ToggleBox_Draw);

void ToggleBox_GetParams(void *task, void **out)
{
    *out = gToggleBoxParams;
}
HOOK(0x41E410, ToggleBox_GetParams);

void ToggleBox_Layout(ToggleBox *tb)
{
    void *menu = tb->w.menu;

    if (VarString_HasExpr(&tb->value))
        tb->isOn = MenuExpr_Eval(&tb->value, tb) != 0;
    Text_SetString(tb->yes, MenuManager_GetString(menu, "Yes"));
    Text_SetString(tb->no, MenuManager_GetString(menu, "No"));
    CALL_CHILDREN(&tb->w.task, MenuWindow_GetLoadEvent);
}
HOOK(0x41E420, ToggleBox_Layout);

void ToggleBox_Save(ToggleBox *tb)
{
    if (VarString_HasExpr(&tb->forward))
        MenuExpr_Eval(&tb->forward, tb);
    CALL_CHILDREN(&tb->w.task, MenuWindow_GetSaveEvent);
}
HOOK(0x41E500, ToggleBox_Save);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B674) uint16_t gToggleBoxType INIT(385);
PAD(igd, 53B674, 53B676, 0x2);
BSS(57BBD0) void *gToggleBoxParams;
