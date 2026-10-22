#include "hook.h"
#include "igi.h"
#include "menuscreen/menuscreen.h"

ORIG(0x4B50B0, void, QVertex_DrawFrame, (Sprite *s, float x, float y, float r, float g, float b, float alpha, float z,
                                         int frame, unsigned int flags, int unk20));

#define gMenuButtonType VAR(uint16_t, 0x53B7F0)
#define gMenuButtonParams VAR(void *, 0x57BBFC)

void MenuButton_Run(MenuButton *mb);
void MenuButton_Draw(MenuButton *mb);
void MenuButton_Create(MenuButton *mb, void *arg);
void MenuButton_GetParams(void *task, void **out);
void MenuButton_Changed(MenuButton *mb, void *arg);

void MenuButton_Init(void)
{
    MenuButton mb;

    gMenuButtonType = TaskType_New("MenuButton", sizeof(MenuButton), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gMenuButtonType, 1, MenuButton_Run);
    TaskType_SetHandler2(gMenuButtonType, Screen_GetDrawEvent(), MenuButton_Draw);
    TaskType_SetHandler2(gMenuButtonType, 2, Task_Nothing);
    TaskType_SetHandler(gMenuButtonType, 0, MenuButton_Create);
    TaskType_SetHandler(gMenuButtonType, Param_GetTableEvent(), MenuButton_GetParams);
    TaskType_SetHandler(gMenuButtonType, Param_GetChangedEvent(), MenuButton_Changed);
    gMenuButtonParams = ParamTable_New(1);
    ParamTable_Add(gMenuButtonParams, &mb, mb.sprite, Param_String32(), 7, NULL);
}
HOOK(0x421430, MenuButton_Init);

void MenuButton_Create(MenuButton *mb, void *arg)
{
    CALL_BASE(0, mb, arg);
    mb->image = NULL;
    mb->sprite[0] = 0;
    mb->w.x = 320;
    mb->w.y = 240;
}
HOOK(0x421510, MenuButton_Create);

void MenuButton_Changed(MenuButton *mb, void *arg)
{
    if (mb->w.menu) {
        Sprite *s = MenuManager_FindSprite(mb->w.menu, mb->sprite);

        mb->image = s;
        if (s) {
            mb->w.w = Sprite_Width(s);
            mb->w.h = Sprite_Height(s);
        } else {
            mb->w.w = 0;
            mb->w.h = 0;
        }
    }
    CALL_BASE(Param_GetChangedEvent(), mb, arg);
}
HOOK(0x421550, MenuButton_Changed);

/* only tracks the mouse; the owner reads w.clicked */
void MenuButton_Run(MenuButton *mb)
{
    MenuWindow_HitTest(mb, mb->w.x, mb->w.y, mb->w.w, mb->w.h, NULL, NULL);
}
HOOK(0x4215D0, MenuButton_Run);

void MenuButton_Draw(MenuButton *mb)
{
    float alpha = FADE_ALPHA(mb->w.menu);
    uint16_t base = MenuWindow_GetType();

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(mb);
    if (mb->image) {
        int x = SCREEN_X(mb->w.x);
        int y = SCREEN_Y(mb->w.y);

        QVertex_DrawFrame(mb->image, x, y, 128.0f, 128.0f, 128.0f, alpha, 1.0f, 0, 0x80000, 0);
    }
}
HOOK(0x421600, MenuButton_Draw);

void MenuButton_GetParams(void *task, void **out)
{
    *out = gMenuButtonParams;
}
HOOK(0x4216F0, MenuButton_GetParams);

void MenuButton_Shutdown(void)
{
    QTaskList_Free(gMenuButtonParams);
    TaskType_Free(gMenuButtonType);
    gMenuButtonType = 0x181;
}
HOOK(0x421700, MenuButton_Shutdown);

/* the create arg is the parent too */
MenuButton *MenuButton_New(void *parent)
{
    return (MenuButton *)Task_New(parent, gMenuButtonType, parent);
}
HOOK(0x421730, MenuButton_New);
