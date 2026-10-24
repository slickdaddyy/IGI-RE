#include "hook.h"
#include "igi.h"
#include "widget/widget.h"

ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FB0, void *, Param_Int16, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x418DE0, Sprite *, MenuManager_FindSprite, (void *mm, const char *name));
ORIG(0x418FE0, void, MenuManager_FocusItem, (void *mm, void *item));
ORIG(0x4B6CE0, int, Sprite_Width, (Sprite *s));
ORIG(0x4B6D00, int, Sprite_Height, (Sprite *s));

/* a box drawn from eight sprites "<name>_1.spr" .. "<name>_8.spr":
   top left, top, top right, right, bottom right, bottom, bottom left, left */
typedef struct MenuFrame {
    MenuWindow w;
    char name[0x40];
    Sprite *sprites[8];
} MenuFrame;

_Static_assert(sizeof(MenuFrame) == 0xB4, "MenuFrame");

extern uint16_t gMenuFrameType;
extern void *gMenuFrameParams;
extern IntRange gMenuFrameRange;

void MenuFrame_Run(MenuFrame *mf);
void MenuFrame_Draw(MenuFrame *mf);
void MenuFrame_Create(MenuFrame *mf, void *arg);
void MenuFrame_GetParams(void *task, void **out);
void MenuFrame_Changed(MenuFrame *mf, void *arg);

void MenuFrame_Init(void)
{
    MenuFrame mf;

    gMenuFrameRange.min = 0;
    gMenuFrameRange.max = 640;
    gMenuFrameType = TaskType_New("MenuFrame", sizeof(MenuFrame), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gMenuFrameType, 1, MenuFrame_Run);
    TaskType_SetHandler2(gMenuFrameType, Screen_GetDrawEvent(), MenuFrame_Draw);
    TaskType_SetHandler2(gMenuFrameType, 2, Task_Nothing);
    TaskType_SetHandler(gMenuFrameType, 0, MenuFrame_Create);
    TaskType_SetHandler(gMenuFrameType, Param_GetTableEvent(), MenuFrame_GetParams);
    TaskType_SetHandler(gMenuFrameType, Param_GetChangedEvent(), MenuFrame_Changed);
    gMenuFrameParams = ParamTable_New(9);
    ParamTable_Add(gMenuFrameParams, &mf, mf.name, Param_String32(), 7, NULL);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.x, Param_Int32(), 7, &gMenuFrameRange);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.y, Param_Int32(), 7, &gMenuFrameRange);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.w, Param_Int32(), 7, &gMenuFrameRange);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.h, Param_Int32(), 7, &gMenuFrameRange);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.lineup, Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.spacingX, Param_Int16(), 7, NULL);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.spacingY, Param_Int16(), 7, NULL);
    ParamTable_Add(gMenuFrameParams, &mf, &mf.w.selectable, Param_Bool8(), 7, NULL);
}
HOOK(0x4223D0, MenuFrame_Init);

void MenuFrame_Create(MenuFrame *mf, void *arg)
{
    int i;

    CALL_BASE(0, mf, arg);
    for (i = 0; i < 8; i++)
        mf->sprites[i] = NULL;
    mf->name[0] = 0;
    mf->w.x = 220;
    mf->w.y = 220;
    mf->w.w = 200;
    mf->w.h = 40;
}
HOOK(0x4225F0, MenuFrame_Create);

void MenuFrame_Changed(MenuFrame *mf, void *arg)
{
    if (mf->w.menu) {
        int i = 0;

        do {
            char buf[64];

            i++;
            crt_sprintf(buf, "%s_%d.spr", mf->name, i);
            mf->sprites[i - 1] = MenuManager_FindSprite(mf->w.menu, buf);
        } while (i < 8);
    }
    CALL_BASE(Param_GetChangedEvent(), mf, arg);
}
HOOK(0x422640, MenuFrame_Changed);

int MenuFrame_DrawRow(MenuFrame *mf, int x, int y, int w, int left, int mid, int right, int bottom);
void MenuFrame_DrawColumn(MenuFrame *mf, int x, int y, int h, int index, int right);

void MenuFrame_Draw(MenuFrame *mf)
{
    int x = SCREEN_X(&mf->w);
    int y = SCREEN_Y(&mf->w);
    int w = mf->w.w;
    int h = mf->w.h;
    int top, bottom;

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), MenuWindow_GetType()))(mf);
    top = MenuFrame_DrawRow(mf, x, y, w, 0, 1, 2, 0);
    bottom = MenuFrame_DrawRow(mf, x, y + h, w, 6, 5, 4, 1);
    h -= bottom + top;
    MenuFrame_DrawColumn(mf, x, y + top, h, 7, 0);
    MenuFrame_DrawColumn(mf, w + x, y + top, h, 3, 1);
}
HOOK(0x4226D0, MenuFrame_Draw);

/* a corner, the edge repeated, the other corner; returns the row height.
   Which corners exist is checked on sprites 0-2 whatever row it is. */
int MenuFrame_DrawRow(MenuFrame *mf, int x, int y, int w, int left, int mid, int right, int bottom)
{
    int lw = 0, mw = 0, rw = 0, height = 0;

    if (mf->sprites[0]) {
        lw = Sprite_Width(mf->sprites[left]);
        height = Sprite_Height(mf->sprites[left]);
    }
    if (mf->sprites[1]) {
        mw = Sprite_Width(mf->sprites[mid]);
        if (height <= Sprite_Height(mf->sprites[mid]))
            height = Sprite_Height(mf->sprites[mid]);
    }
    if (mf->sprites[2]) {
        rw = Sprite_Width(mf->sprites[right]);
        if (height <= Sprite_Height(mf->sprites[right]))
            height = Sprite_Height(mf->sprites[right]);
    }
    if (bottom)
        y -= height;
    if (lw) {
        Draw_Sprite(mf->sprites[left], x, y, 128.0f, 128.0f, 128.0f, 0, 0, 0);
        x += lw;
        w -= lw;
    }
    if (mw) {
        int n = (w - rw) / mw;

        for (; n > 0; n--) {
            Draw_Sprite(mf->sprites[mid], x, y, 128.0f, 128.0f, 128.0f, 0, 0, 0);
            x += mw;
        }
    } else {
        x += w - rw;
    }
    if (rw)
        Draw_Sprite(mf->sprites[right], x, y, 128.0f, 128.0f, 128.0f, 0, 0, 0);
    return height;
}
HOOK(0x4227A0, MenuFrame_DrawRow);

void MenuFrame_DrawColumn(MenuFrame *mf, int x, int y, int h, int index, int right)
{
    if (mf->sprites[index]) {
        int sh = Sprite_Height(mf->sprites[index]);
        int n = h / sh;

        if (right)
            x -= Sprite_Width(mf->sprites[index]);
        for (; n > 0; n--) {
            Draw_Sprite(mf->sprites[index], x, y, 128.0f, 128.0f, 128.0f, 0, 0, -1);
            y += sh;
        }
    }
}
HOOK(0x4229A0, MenuFrame_DrawColumn);

/* focuses itself when the pointer moves onto it and passes its focus and
   key on to its MenuWindow children */
void MenuFrame_Run(MenuFrame *mf)
{
    if (mf->w.selectable) {
        Task *c;

        MenuWindow_HitTest(&mf->w, mf->w.x, mf->w.y, mf->w.w, mf->w.h, NULL, NULL);
        if (mf->w.hover && !mf->w.wasHover)
            MenuManager_FocusItem(mf->w.menu, mf);
        c = mf->w.task.head;
        if (c->next && c) {
            do {
                if (TaskType_IsA(c->type, MenuWindow_GetType())) {
                    ((MenuWindow *)c)->key = mf->w.key;
                    ((MenuWindow *)c)->focused = mf->w.focused;
                }
                c = c->next;
            } while (c && c->next);
        }
    }
    mf->w.key = 0;
    CALL_BASE(1, mf);
}
HOOK(0x422A50, MenuFrame_Run);

void MenuFrame_GetParams(void *task, void **out)
{
    *out = gMenuFrameParams;
}
HOOK(0x422B00, MenuFrame_GetParams);

void MenuFrame_Shutdown(void)
{
    QTaskList_Free(gMenuFrameParams);
    TaskType_Free(gMenuFrameType);
    gMenuFrameType = 0x181;
}
HOOK(0x422B10, MenuFrame_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B838) uint16_t gMenuFrameType INIT(385);
PAD(igd, 53B838, 53B83A, 0x2);
BSS(57BC10) void *gMenuFrameParams;
PAD(igb, 57BC10, 57BC14, 0x4);
BSS(57BC10) IntRange gMenuFrameRange;
