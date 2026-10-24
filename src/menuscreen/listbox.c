#include "hook.h"
#include "igi.h"
#include "menuscreen/menuscreen.h"

ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4B7050, int, Sprite_StringWidth, (Sprite *s, const char *str));

typedef struct ListBoxItem {
    int id;
    char text[0x28];
} ListBoxItem;

/* shows one of up to 64 items with an arrow button on each side; the script
   reads the shown item's id as "<name>.nActiveID" */
typedef struct ListBox {
    MenuWindow w;
    char name[0x40]; /* arrow sprites are <name>_1.spr and <name>_2.spr */
    char font[0x40];
    MenuButton *prev;
    MenuButton *next;
    int unkDC;
    ListBoxItem items[64];
    int count;
    int index;
    int activeId;
    int width;
    char registered;
    char padBF1[3];
    Text *text;
    VarString fill;
    VarString select;
    VarString forward;
    VarString action;
} ListBox;

_Static_assert(sizeof(ListBox) == 0xD38, "ListBox");
_Static_assert(offsetof(ListBox, count) == 0xBE0, "ListBox.count");

extern uint16_t gListBoxType;
extern void *gListBoxParams;
extern ListBox *gListBoxFilling;

void ListBox_Run(ListBox *lb);
void ListBox_Draw(ListBox *lb);
void ListBox_Delete(ListBox *lb);
void ListBox_Create(ListBox *lb, void *arg);
void ListBox_GetParams(void *task, void **out);
void ListBox_Changed(ListBox *lb, void *arg);
void ListBox_Layout(ListBox *lb);
void ListBox_Forward918(ListBox *lb);
void ListBox_Activate(ListBox *lb);
void ListBox_Deactivate(ListBox *lb);
void ListBox_Clear(ListBox *lb);

void ListBox_Init(void)
{
    ListBox lb;

    gListBoxType = TaskType_New("ListBox", sizeof(ListBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gListBoxType, 1, ListBox_Run);
    TaskType_SetHandler2(gListBoxType, Screen_GetDrawEvent(), ListBox_Draw);
    TaskType_SetHandler2(gListBoxType, 2, ListBox_Delete);
    TaskType_SetHandler(gListBoxType, 0, ListBox_Create);
    TaskType_SetHandler(gListBoxType, Param_GetTableEvent(), ListBox_GetParams);
    TaskType_SetHandler(gListBoxType, Param_GetChangedEvent(), ListBox_Changed);
    TaskType_SetHandler2(gListBoxType, MenuWindow_GetEvent914(), ListBox_Layout);
    TaskType_SetHandler2(gListBoxType, MenuWindow_GetEvent918(), ListBox_Forward918);
    TaskType_SetHandler2(gListBoxType, Level_GetEventDA8(), ListBox_Activate);
    TaskType_SetHandler2(gListBoxType, Level_GetEventDAC(), ListBox_Deactivate);
    gListBoxParams = ParamTable_New(7);
    ParamTable_Add(gListBoxParams, &lb, lb.name, Param_String32(), 7, NULL);
    ParamTable_Add(gListBoxParams, &lb, lb.font, Param_String32(), 7, NULL);
    ParamTable_Add(gListBoxParams, &lb, &lb.width, Param_Int32(), 7, NULL);
    ParamTable_Add(gListBoxParams, &lb, &lb.fill, Param_VarString(), 7, NULL);
    ParamTable_Add(gListBoxParams, &lb, &lb.select, Param_VarString(), 7, NULL);
    ParamTable_Add(gListBoxParams, &lb, &lb.forward, Param_VarString(), 7, NULL);
    ParamTable_Add(gListBoxParams, &lb, &lb.action, Param_VarString(), 7, NULL);
}
HOOK(0x41E690, ListBox_Init);

void ListBox_Create(ListBox *lb, void *arg)
{
    CALL_BASE(0, lb, arg);
    lb->width = 100;
    lb->name[0] = 0;
    lb->font[0] = 0;
    lb->prev = MenuButton_New(NULL);
    lb->next = MenuButton_New(NULL);
    lb->next->w.unk40 = 0;
    lb->prev->w.unk40 = 0;
    lb->next->w.unk3C = 0;
    lb->prev->w.unk3C = 0;
    lb->count = 0;
    ListBox_Clear(lb);
    VarString_Init(&lb->fill);
    VarString_Init(&lb->select);
    VarString_Init(&lb->forward);
    VarString_Init(&lb->action);
    lb->registered = 0;
    lb->w.unk3C = 1;
    lb->w.unk40 = 1;
    lb->text = NULL;
}
HOOK(0x41E8C0, ListBox_Create);

void ListBox_Clear(ListBox *lb)
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
HOOK(0x41E990, ListBox_Clear);

void ListBox_Delete(ListBox *lb)
{
    if (lb->text) {
        Text_Free(lb->text);
        lb->text = NULL;
    }
    ListBox_Clear(lb);
    MenuManager_Free(lb->prev);
    MenuManager_Free(lb->next);
    VarString_Free(&lb->fill);
    VarString_Free(&lb->select);
    VarString_Free(&lb->forward);
    VarString_Free(&lb->action);
    CALL_OWN(Level_GetEventDAC(), lb);
}
HOOK(0x41E9D0, ListBox_Delete);

void ListBox_Changed(ListBox *lb, void *arg)
{
    MenuButton *next = lb->next;
    MenuButton *prev = lb->prev;
    Sprite *font = MenuManager_FindFont(lb->w.menu, lb->font);
    uint8_t ev;

    if (lb->text) {
        Text_Free(lb->text);
        lb->text = NULL;
    }
    if (font) {
        lb->text = Text_New(font, 0, 0, gEmptyString);
        lb->text->unk20 = 0;
    }
    VarString_Bind(&lb->fill, lb);
    VarString_Bind(&lb->select, lb);
    VarString_Bind(&lb->forward, lb);
    VarString_Bind(&lb->action, lb);
    if (lb->w.menu) {
        char buf[64];

        crt_sprintf(buf, "%s_1.spr", lb->name);
        str_copy(lb->prev->sprite, buf);
        crt_sprintf(buf, "%s_2.spr", lb->name);
        str_copy(lb->next->sprite, buf);
    }
    lb->prev->w.menu = lb->w.menu;
    lb->next->w.menu = lb->w.menu;
    ev = Param_GetChangedEvent();
    HANDLER(ev, lb->prev->w.task.type)(lb->prev, arg);
    ev = Param_GetChangedEvent();
    HANDLER(ev, lb->next->w.task.type)(lb->next, arg);
    lb->w.w = lb->width + next->w.w + prev->w.w + 6;
    lb->w.h = (prev->w.h > next->w.h ? prev->w.h : next->w.h) + 4;
    CALL_BASE(Param_GetChangedEvent(), lb, arg);
}
HOOK(0x41EA70, ListBox_Changed);

/* the arrows (or left / right keys) step through the items, wrapping round */
void ListBox_Run(ListBox *lb)
{
    MenuButton *prev = lb->prev;
    int old = lb->activeId;
    MenuButton *next = lb->next;

    ((void (*)(void *))HANDLER(1, prev->w.task.type))(prev);
    ((void (*)(void *))HANDLER(1, next->w.task.type))(next);
    if (lb->count) {
        if (prev->w.clicked || lb->w.key == -1) {
            if (--lb->index == -1)
                lb->index = lb->count - 1;
        } else if (next->w.clicked || lb->w.key == -2) {
            lb->index = (lb->index + 1) % lb->count;
        }
        lb->activeId = lb->items[lb->index].id;
    }
    lb->w.key = 0;
    if (old != lb->activeId) {
        Sound_PlayName("menu_high", 0, 0);
        VarString_EvalReal(&lb->action, lb);
    }
    CALL_BASE(1, lb);
}
HOOK(0x41EC70, ListBox_Run);

void ListBox_Draw(ListBox *lb)
{
    MenuButton *prev = lb->prev;
    MenuButton *next = lb->next;
    float alpha = FADE_ALPHA(lb->w.menu);
    uint16_t base = MenuWindow_GetType();
    Text *t;
    int x, y;

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(lb);
    x = lb->w.x + 2;
    y = lb->w.y + 2;
    prev->w.x = x;
    prev->w.y = y;
    next->w.y = y;
    next->w.x = prev->w.w + x + 2 + lb->width;
    prev = lb->prev;
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), prev->w.task.type))(prev);
    next = lb->next;
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), next->w.task.type))(next);
    if (lb->count && (t = lb->text) != NULL) {
        const char *s = lb->items[lb->index].text;
        char buf[128];
        int th, tw;

        if (Sprite_StringWidth(t->grid->sprite, s) > lb->width - 4) {
            int n = 0;

            str_copy(buf, s);
            while (buf[n])
                n++;
            while (n && Sprite_StringWidth(t->grid->sprite, buf) > lb->width - 12)
                buf[--n] = 0;
            str_copy(buf + n, "...");
            s = buf;
        }
        Text_SetString(t, s);
        th = Text_GetHeight(t);
        y = (lb->w.h - th) / 2 + lb->w.y + 1;
        tw = Text_GetWidth(t);
        x = (lb->w.w - tw) / 2 + lb->w.x;
        x = SCREEN_X(x);
        y = SCREEN_Y(y);
        t->color[0] = 0;
        t->color[2] = 0;
        t->x = x;
        t->color[1] = 128.0f;
        t->flags |= 0x80000;
        t->y = y;
        t->alpha = alpha;
        Text_DrawAlt(t);
    }
    MenuWindow_DrawEnd(lb, 0.5f, alpha, lb->w.focused);
}
HOOK(0x41ED70, ListBox_Draw);

void ListBox_GetParams(void *task, void **out)
{
    *out = gListBoxParams;
}
HOOK(0x41F020, ListBox_GetParams);

void ListBox_Layout(ListBox *lb)
{
    ListBox_Clear(lb);
    gListBoxFilling = lb;
    EVAL(&lb->fill, lb);
    gListBoxFilling = NULL;
    if (VarString_HasExpr(&lb->select)) {
        int id = round_int(VarString_EvalReal(&lb->select, lb));
        int i;

        for (i = 0; i < lb->count; i++)
            if (id == lb->items[i].id) {
                lb->index = i;
                lb->activeId = id;
                break;
            }
    }
    CALL_CHILDREN(&lb->w.task, MenuWindow_GetEvent914);
}
HOOK(0x41F030, ListBox_Layout);

void ListBox_Forward918(ListBox *lb)
{
    EVAL(&lb->forward, lb);
    CALL_CHILDREN(&lb->w.task, MenuWindow_GetEvent918);
}
HOOK(0x41F140, ListBox_Forward918);

void ListBox_Activate(ListBox *lb)
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
HOOK(0x41F1E0, ListBox_Activate);

void ListBox_Deactivate(ListBox *lb)
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
HOOK(0x41F240, ListBox_Deactivate);

void ListBox_Shutdown(void)
{
    QTaskList_Free(gListBoxParams);
    TaskType_Free(gListBoxType);
    gListBoxType = 0x181;
}
HOOK(0x41F2A0, ListBox_Shutdown);

/* called by script functions while a ListBox runs its fill expression */
void ListBox_AddItem(const char *text, int id)
{
    ListBox *lb = gListBoxFilling;
    ListBoxItem *it = &lb->items[lb->count];
    int i;

    for (i = 0; i < lb->count; i++)
        if (lb->items[i].id == id)
            return;
    it->id = id;
    str_copy(it->text, text);
    lb->count++;
}
HOOK(0x41F2D0, ListBox_AddItem);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B694) uint16_t gListBoxType INIT(385);
PAD(igd, 53B694, 53B696, 0x2);
BSS(57BBD8) ListBox *gListBoxFilling;
BSS(57BBD8) void *gListBoxParams;
