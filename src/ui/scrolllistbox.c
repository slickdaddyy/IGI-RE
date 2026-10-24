#include "hook.h"
#include "igi.h"
#include "ui/ui.h"

ORIG(0x4B4A00, void, Draw_RectA, (float x, float y, float w, float h, float r, float g, float b, float a, int flags,
                                  int unk));
ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));

typedef struct ListItem {
    int id;
    char text[0x40];
    const char *str;
} ListItem;

/* a list of up to 64 strings with a scroll bar; the script reads the
   selected item's id as "<name>.nActiveID" */
typedef struct ScrollListBox {
    MenuWindow w;
    ListItem items[64];
    int count;
    char unk1258[4];
    char font[0x20];
    char unk127C[0x20];
    int index;
    int activeId;
    int lastClick;
    char registered;
    char hasScroll;
    char keyboard;
    char pad12AB;
    Text *text;
    VarString fill;
    VarString select;
    VarString forward;
    VarString action;
    int lineHeight;
    MenuWindow *scrollBar;
    MenuWindow *frame;
} ScrollListBox;

_Static_assert(sizeof(ScrollListBox) == 0x13FC, "ScrollListBox");

typedef struct Color {
    float r, g, b;
} Color;

#define gScrollListBoxType VAR(uint16_t, 0x53B59C)
#define gScrollListBoxParams VAR(void *, 0x57BBB4)
#define gScrollListBoxFilling VAR(ScrollListBox *, 0x57BBB0)
#define gMenuColors ((const Color *)0x53B778)

void ScrollListBox_Create(ScrollListBox *lb, void *arg);
void ScrollListBox_Clear(ScrollListBox *lb);
void ScrollListBox_Delete(ScrollListBox *lb);
void ScrollListBox_Changed(ScrollListBox *lb, void *arg);
void ScrollListBox_Run(ScrollListBox *lb);
void ScrollListBox_Draw(ScrollListBox *lb);
void ScrollListBox_GetParams(void *task, void **out);
void ScrollListBox_Layout(ScrollListBox *lb);
void ScrollListBox_Forward918(ScrollListBox *lb);
void ScrollListBox_Activate(ScrollListBox *lb);
void ScrollListBox_Deactivate(ScrollListBox *lb);

void ScrollListBox_Init(void)
{
    ScrollListBox lb;

    gScrollListBoxType = TaskType_New("ScrollListBox", sizeof(ScrollListBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gScrollListBoxType, 1, ScrollListBox_Run);
    TaskType_SetHandler2(gScrollListBoxType, Screen_GetDrawEvent(), ScrollListBox_Draw);
    TaskType_SetHandler2(gScrollListBoxType, 2, ScrollListBox_Delete);
    TaskType_SetHandler(gScrollListBoxType, 0, ScrollListBox_Create);
    TaskType_SetHandler(gScrollListBoxType, Param_GetTableEvent(), ScrollListBox_GetParams);
    TaskType_SetHandler(gScrollListBoxType, Param_GetChangedEvent(), ScrollListBox_Changed);
    TaskType_SetHandler2(gScrollListBoxType, MenuWindow_GetEvent914(), ScrollListBox_Layout);
    TaskType_SetHandler2(gScrollListBoxType, MenuWindow_GetEvent918(), ScrollListBox_Forward918);
    TaskType_SetHandler2(gScrollListBoxType, Level_GetEventDA8(), ScrollListBox_Activate);
    TaskType_SetHandler2(gScrollListBoxType, Level_GetEventDAC(), ScrollListBox_Deactivate);
    gScrollListBoxParams = ParamTable_New(7);
    ParamTable_Add(gScrollListBoxParams, &lb, lb.font, Param_String32(), 7, NULL);
    ParamTable_Add(gScrollListBoxParams, &lb, &lb.fill, Param_VarString(), 7, NULL);
    ParamTable_Add(gScrollListBoxParams, &lb, &lb.select, Param_VarString(), 7, NULL);
    ParamTable_Add(gScrollListBoxParams, &lb, &lb.forward, Param_VarString(), 7, NULL);
    ParamTable_Add(gScrollListBoxParams, &lb, &lb.w.w, Param_Int32(), 7, NULL);
    ParamTable_Add(gScrollListBoxParams, &lb, &lb.w.h, Param_Int32(), 7, NULL);
    ParamTable_Add(gScrollListBoxParams, &lb, &lb.action, Param_VarString(), 7, NULL);
}
HOOK(0x41B8E0, ScrollListBox_Init);

void ScrollListBox_Create(ScrollListBox *lb, void *arg)
{
    CALL_BASE(0, lb, arg);
    lb->count = 0;
    ScrollListBox_Clear(lb);
    VarString_Init(&lb->fill);
    VarString_Init(&lb->select);
    VarString_Init(&lb->forward);
    VarString_Init(&lb->action);
    lb->registered = 0;
    lb->w.unk3C = 1;
    lb->w.unk40 = 1;
    lb->font[0] = 0;
    lb->w.w = 100;
    lb->w.h = 100;
    lb->text = NULL;
    lb->scrollBar = SlideBar_NewVertical(NULL, lb->w.menu);
    lb->frame = MenuWindow_New(NULL, lb->w.menu);
    lb->keyboard = 0;
}
HOOK(0x41BB10, ScrollListBox_Create);

void ScrollListBox_Clear(ScrollListBox *lb)
{
    int i;

    for (i = 0; i < 64; i++) {
        lb->items[i].text[0] = 0;
        lb->items[i].id = 0;
    }
    lb->count = 0;
    lb->index = 0;
    lb->activeId = 0;
}
HOOK(0x41BBD0, ScrollListBox_Clear);

void ScrollListBox_Delete(ScrollListBox *lb)
{
    MenuManager_Free(lb->scrollBar);
    MenuManager_Free(lb->frame);
    if (lb->text) {
        Text_Free(lb->text);
        lb->text = NULL;
    }
    ScrollListBox_Clear(lb);
    VarString_Free(&lb->fill);
    VarString_Free(&lb->select);
    VarString_Free(&lb->forward);
    VarString_Free(&lb->action);
    CALL_OWN(Level_GetEventDAC(), lb);
}
HOOK(0x41BC00, ScrollListBox_Delete);

void ScrollListBox_Changed(ScrollListBox *lb, void *arg)
{
    Sprite *font = MenuManager_FindFont(lb->w.menu, lb->font);
    MenuWindow *bar;

    if (lb->text) {
        Text_Free(lb->text);
        lb->text = NULL;
    }
    if (font) {
        lb->text = Text_New(font, 0, 0, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
        lb->text->unk20 = 0;
        lb->lineHeight = Text_GetHeight(lb->text) + 1;
    }
    lb->hasScroll = 0;
    lb->frame->w = lb->w.w;
    lb->frame->h = lb->w.h;
    lb->frame->x = lb->w.x;
    lb->frame->y = lb->w.y;
    VarString_Bind(&lb->fill, lb);
    VarString_Bind(&lb->select, lb);
    VarString_Bind(&lb->forward, lb);
    VarString_Bind(&lb->action, lb);
    bar = lb->scrollBar;
    HANDLER(Param_GetChangedEvent(), bar->task.type)(bar, arg);
    CALL_BASE(Param_GetChangedEvent(), lb, arg);
}
HOOK(0x41BCA0, ScrollListBox_Changed);

void ScrollListBox_Run(ScrollListBox *lb)
{
    void *menu = lb->w.menu;
    MenuWindow *frame = lb->frame;
    MenuInput *in = MenuManager_GetInput(menu);
    int rows = (lb->w.h - 4) / lb->lineHeight;
    int pos;
    int mx, my;

    if (lb->hasScroll) {
        CALL_OWN(1, lb->scrollBar);
        pos = SlideBar_GetPos(lb->scrollBar);
    } else {
        pos = 0;
    }
    MenuWindow_HitTest(frame, frame->x, frame->y, frame->w, frame->h, &mx, &my);
    if (frame->clicked) {
        int row = (my - frame->y) / lb->lineHeight + pos;

        if (row != lb->index && row < lb->count) {
            lb->index = row;
            lb->lastClick = Flow_GetFrames();
            Sound_PlayName("menu_sel2", 0, NULL);
        } else {
            int now = Flow_GetFrames();
            int half = round_int((long double)Flow_GetFps() * 0.5);

            /* a second click on the same item */
            if (lb->lastClick != -1 && now - lb->lastClick < half && VarString_HasExpr(&lb->action))
                VarString_EvalReal(&lb->action, lb);
            lb->lastClick = now;
        }
    }
    if (lb->keyboard) {
        while (Keys_GetCount()) {
            switch (Keys_Pop()) {
            case -11:
                MenuManager_EnableInput(menu);
                lb->keyboard = 0;
                break;
            case -7:
                if (VarString_HasExpr(&lb->action))
                    VarString_EvalReal(&lb->action, lb);
                break;
            case -4:
                if (lb->index < lb->count - 1) {
                    lb->index++;
                    if (lb->index > pos + rows)
                        SlideBar_SetPos(lb->scrollBar, pos + 1);
                    Sound_PlayName("menu_sel2", 0, NULL);
                }
                break;
            case -3:
                if (lb->index > 0) {
                    lb->index--;
                    if (lb->index < pos)
                        SlideBar_SetPos(lb->scrollBar, pos - 1);
                    Sound_PlayName("menu_sel2", 0, NULL);
                }
                break;
            }
        }
    }
    if (in->select) {
        lb->keyboard = 1;
        MenuManager_DisableInput(menu);
    }
    lb->activeId = lb->items[lb->index].id;
    CALL_BASE(1, lb);
}
HOOK(0x41BDD0, ScrollListBox_Run);

void ScrollListBox_Draw(ScrollListBox *lb)
{
    float alpha = ui_alpha(lb->w.menu);
    int rows = (lb->w.h - 4) / lb->lineHeight;
    int shown = lb->count < rows ? lb->count : rows;
    int x = (Display_GetMode()->width >> 1) - 320 + lb->w.x;
    int y = (Display_GetMode()->height >> 1) - 238 + lb->w.y;
    int pos = rows < lb->count ? SlideBar_GetPos(lb->scrollBar) : 0;
    int sel = lb->index - pos;
    int i;

    lb->frame->x = lb->w.x;
    lb->frame->y = lb->w.y;
    if (shown > 0) {
        float tx = x + 4;

        for (i = 0; i < shown; i++) {
            Text *t;
            const Color *c;
            float ty;

            Text_SetString(lb->text, lb->items[pos + i].str);
            t = lb->text;
            t->x = tx;
            t->y = ty = y;
            c = i == sel ? &gMenuColors[4] : &gMenuColors[2];
            t->color[0] = c->r;
            lb->text->color[1] = c->g;
            lb->text->color[2] = c->b;
            lb->text->flags |= 0x80000;
            lb->text->alpha = alpha;
            Text_DrawAlt(lb->text);
            if (i == sel)
                Draw_RectA((float)x + 2.0f, ty, (float)lb->frame->w - 4.0f, lb->lineHeight, 0, 80.0f, 0, alpha,
                           0x80000, 0);
            y += lb->lineHeight;
        }
    }
    if (lb->hasScroll) {
        MenuWindow *bar = lb->scrollBar;

        ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), bar->task.type))(bar);
    }
    MenuWindow_DrawEnd(lb->frame, 0.5f, alpha, lb->keyboard);
}
HOOK(0x41C080, ScrollListBox_Draw);

void ScrollListBox_GetParams(void *task, void **out)
{
    *out = gScrollListBoxParams;
}
HOOK(0x41C300, ScrollListBox_GetParams);

void ScrollListBox_Layout(ScrollListBox *lb)
{
    int i, rows, sw, h;

    ScrollListBox_Clear(lb);
    gScrollListBoxFilling = lb;
    if (VarString_HasExpr(&lb->fill))
        VarString_EvalReal(&lb->fill, lb);
    gScrollListBoxFilling = NULL;
    for (i = 0; i < lb->count; i++)
        lb->items[i].str = MenuManager_GetString(lb->w.menu, lb->items[i].text);
    if (VarString_HasExpr(&lb->select)) {
        int id = round_int(VarString_EvalReal(&lb->select, lb));

        for (i = 0; i < lb->count; i++)
            if (id == lb->items[i].id) {
                lb->index = i;
                lb->activeId = id;
                goto found;
            }
    }
    lb->index = 0;
    lb->activeId = lb->count ? lb->items[0].id : 0;
found:
    h = lb->w.h;
    rows = (h - 4) / lb->lineHeight;
    lb->hasScroll = rows < lb->count;
    if (lb->hasScroll) {
        sw = lb->scrollBar->w;
        SlideBar_SetRange(lb->scrollBar, lb->w.w + lb->w.x - sw, lb->w.y, 0, h, lb->count - rows + 1);
        sw += 2;
    } else {
        sw = 0;
    }
    lb->frame->x = lb->w.x;
    lb->frame->y = lb->w.y;
    lb->frame->h = lb->w.h;
    lb->frame->w = lb->w.w - sw;
    lb->keyboard = 0;
    lb->lastClick = -1;
    CALL_CHILDREN(&lb->w.task, MenuWindow_GetEvent914);
}
HOOK(0x41C310, ScrollListBox_Layout);

void ScrollListBox_Forward918(ScrollListBox *lb)
{
    if (VarString_HasExpr(&lb->forward))
        VarString_EvalReal(&lb->forward, lb);
    CALL_CHILDREN(&lb->w.task, MenuWindow_GetEvent918);
}
HOOK(0x41C500, ScrollListBox_Forward918);

void ScrollListBox_Activate(ScrollListBox *lb)
{
    char name[64];
    char buf[128];

    if (!lb->registered) {
        lb->registered = 1;
        Task_GetName(name, lb);
        crt_sprintf(buf, "%s.nActiveID", name);
        Script_RegisterInt2(buf, &lb->activeId);
    }
}
HOOK(0x41C5A0, ScrollListBox_Activate);

void ScrollListBox_Deactivate(ScrollListBox *lb)
{
    char name[64];
    char buf[128];

    if (lb->registered) {
        Task_GetName(name, lb);
        crt_sprintf(buf, "%s.nActiveID", name);
        Script_RemoveSymbol(buf);
        lb->registered = 0;
    }
}
HOOK(0x41C600, ScrollListBox_Deactivate);

void ScrollListBox_Shutdown(void)
{
    QTaskList_Free(gScrollListBoxParams);
    TaskType_Free(gScrollListBoxType);
    gScrollListBoxType = 0x181;
}
HOOK(0x41C660, ScrollListBox_Shutdown);

/* called by script functions while a ScrollListBox runs its fill expression */
void ScrollListBox_AddItem(const char *text, int id)
{
    ScrollListBox *lb = gScrollListBoxFilling;
    ListItem *it = &lb->items[lb->count];
    int i;

    for (i = 0; i < lb->count; i++)
        if (lb->items[i].id == id)
            return;
    it->id = id;
    for (i = 0; (it->text[i] = text[i]) != 0; i++)
        ;
    lb->count++;
}
HOOK(0x41C690, ScrollListBox_AddItem);
