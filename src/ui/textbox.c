#include "hook.h"
#include "igi.h"
#include "ui/ui.h"

/* one page of text, picked by id */
typedef struct TextBoxItem {
    char text[0x100];
    int id;
    const char *str;
} TextBoxItem;

/* shows one of up to 32 texts, word wrapped */
typedef struct TextBox {
    MenuWindow w;
    char font[0x20];
    char unk74[0x20];
    Text *text;
    Sprite *sprite;
    TextBoxItem items[32];
    int index;
    int count;
    VarString fill;
    VarString select;
} TextBox;

_Static_assert(sizeof(TextBox) == 0x2244, "TextBox");

#define gTextBoxType VAR(uint16_t, 0x53B580)
#define gTextBoxParams VAR(void *, 0x57BB98)
#define gTextBoxFilling VAR(TextBox *, 0x57BBA0)

void TextBox_Create(TextBox *tb, void *arg)
{
    CALL_BASE(0, tb, arg);
    tb->w.w = 0x80;
    tb->w.h = 0x80;
    tb->font[0] = 0;
    tb->text = NULL;
    tb->sprite = NULL;
    tb->w.x = 300;
    tb->w.y = 230;
    tb->w.unk3C = 1;
    tb->w.unk40 = 1;
    tb->count = 0;
    tb->index = 0;
    VarString_Init(&tb->fill);
    VarString_Init(&tb->select);
}
HOOK(0x41ACA0, TextBox_Create);

void TextBox_Delete(TextBox *tb)
{
    if (tb->text) {
        Text_Free(tb->text);
        tb->text = NULL;
    }
    tb->count = 0;
    VarString_Free(&tb->fill);
    VarString_Free(&tb->select);
}
HOOK(0x41AD20, TextBox_Delete);

void TextBox_Changed(TextBox *tb, void *arg)
{
    VarString_Bind(&tb->fill, tb);
    VarString_Bind(&tb->select, tb);
    if (tb->text) {
        Text_Free(tb->text);
        tb->text = NULL;
    }
    if (tb->w.menu) {
        tb->sprite = MenuManager_FindFont(tb->w.menu, tb->font);
        if (tb->sprite) {
            tb->text = Text_New(tb->sprite, 0, 0, gEmptyString);
            tb->text->unk20 = 0;
        }
    }
    CALL_BASE(Param_GetChangedEvent(), tb, arg);
}
HOOK(0x41AD70, TextBox_Changed);

void TextBox_Run(TextBox *tb)
{
    if (VarString_HasExpr(&tb->select)) {
        int id = round_int(VarString_EvalReal(&tb->select, tb));
        int i;

        for (i = 0; i < tb->count; i++)
            if (id == tb->items[i].id) {
                tb->index = i;
                break;
            }
    }
    CALL_BASE(1, tb);
}
HOOK(0x41AE20, TextBox_Run);

int TextBox_DrawWrapped(int x, int y, float scale, int width, int height, const char *s, Sprite *font, Text *t,
                        float alpha);

void TextBox_Draw(TextBox *tb)
{
    float alpha = ui_alpha(tb->w.menu);
    int x = (Display_GetMode()->width >> 1) - 316 + tb->w.x;
    int y = (Display_GetMode()->height >> 1) - 236 + tb->w.y;
    const char *s;

    if (tb->count && tb->sprite && tb->text && (s = tb->items[tb->index].str) != NULL)
        TextBox_DrawWrapped(x, y, 1.0f, tb->w.w - 8, tb->w.h - 8, s, tb->sprite, tb->text, alpha);
    MenuWindow_DrawEnd(tb, 0.5f, alpha, 0);
    {
        uint16_t base = MenuWindow_GetType();

        ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(tb);
    }
}
HOOK(0x41AEA0, TextBox_Draw);

/* draws s word by word, wrapping at width; returns the y below the text */
int TextBox_DrawWrapped(int x, int y, float scale, int width, int height, const char *s, Sprite *font, Text *t,
                        float alpha)
{
    char word[256];
    char line[256];
    int cx = x, cy = y;
    int room = width;
    int last = 0;

    line[0] = 0;
    t->flags |= 0x80000;
    t->alpha = alpha;
    for (;;) {
        int n = 0, w;
        char c;

        word[0] = 0;
        for (;;) {
            c = *s++;
            if (!c) {
                last = 1;
                word[n] = 0;
                break;
            }
            if (c == ' ') {
                word[n] = ' ';
                word[n + 1] = 0;
                break;
            }
            word[n++] = c;
        }
        w = Sprite_StringWidth(font, word);
        if (room > w) {
            char *d = line;

            room -= w;
            while (*d)
                d++;
            for (n = 0; (d[n] = word[n]) != 0; n++)
                ;
        } else {
            cx = x;
            cy += Sprite_GetHeight(font, line);
            for (n = 0; (line[n] = word[n]) != 0; n++)
                ;
            room = width - w;
        }
        Text_SetString(t, word);
        t->x = cx;
        t->y = cy;
        Text_DrawAlt(t);
        cx += w;
        if (last)
            return Sprite_GetHeight(font, line) + cy;
    }
}
HOOK(0x41AFB0, TextBox_DrawWrapped);

void TextBox_GetParams(void *task, void **out)
{
    *out = gTextBoxParams;
}
HOOK(0x41B160, TextBox_GetParams);

void TextBox_Layout(TextBox *tb)
{
    int i;

    tb->index = 0;
    gTextBoxFilling = tb;
    if (VarString_HasExpr(&tb->fill))
        VarString_EvalReal(&tb->fill, tb);
    gTextBoxFilling = NULL;
    for (i = 0; i < tb->count; i++)
        tb->items[i].str = MenuManager_GetString(tb->w.menu, tb->items[i].text);
    CALL_CHILDREN(&tb->w.task, MenuWindow_GetEvent914);
}
HOOK(0x41B170, TextBox_Layout);

void TextBox_Shutdown(void)
{
    QTaskList_Free(gTextBoxParams);
    TaskType_Free(gTextBoxType);
    gTextBoxType = 0x181;
}
HOOK(0x41B260, TextBox_Shutdown);

/* called by script functions while a TextBox runs its fill expression */
void TextBox_AddItem(const char *text, int id)
{
    TextBox *tb = gTextBoxFilling;
    TextBoxItem *it = &tb->items[tb->count];
    int i;

    for (i = 0; i < tb->count; i++)
        if (tb->items[i].id == id)
            return;
    it->id = id;
    for (i = 0; (it->text[i] = text[i]) != 0; i++)
        ;
    tb->count++;
}
HOOK(0x41B290, TextBox_AddItem);
