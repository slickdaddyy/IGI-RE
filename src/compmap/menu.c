#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "orig.h"
#include "compmap/compmap.h"

ORIG(0x401780, void, Task_DeleteChildren, (void *task));
ORIG(0x4B12F0, void, Alias_Set, (const char *name, const char *path));
ORIG(0x4B13A0, void, Alias_Remove, (const char *name));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x416D50, char, Game_IsPaused, (void));
ORIG(0x485320, void *, GameScreen_GetView, (void *task));
ORIG(0x5055D0, void, MenuItemTask_SendPointer, (Task *panel, int *pos));

static const float kLeft = 0x1.8cccccp-5f;
static const float kRight = 0x1.733334p-5f;
static const float kTop = 0x1.bbbbbcp-5f;
static const float kBottom = 0x1.888888p-4f;
static const float kPointerZ = 0x1.003d70p+9f;
static const float kBackZ = 0x1.0f3d70p+9f;
static volatile const float kFlip = -1.0f;

void ComputerMenu_Create(ComputerMenu *m);
void ComputerMenu_Run(ComputerMenu *m);
void ComputerMenu_Delete(void);
void ComputerMenu_Draw(ComputerMenu *m);

void ComputerMenu_Init(void)
{
    uint16_t type = TaskType_New("ComputerMenu", sizeof(ComputerMenu), Task_GetRootType(), 0);

    gMenuType = type;
    TaskType_SetHandler(type, 0, ComputerMenu_Create);
    TaskType_SetHandler2(gMenuType, 1, ComputerMenu_Run);
    TaskType_SetHandler2(gMenuType, 2, ComputerMenu_Delete);
    TaskType_SetHandler2(gMenuType, Screen_GetDrawEvent(), ComputerMenu_Draw);
    Alias_Set("COMPUTER:", "LOCAL:computer/");
}
HOOK(0x46AB60, ComputerMenu_Init);

void ComputerMenu_Create(ComputerMenu *m)
{
    float fw = Display_GetMode()->width;
    long double fh = Display_GetMode()->height;

    gMenu = m;
    m->panel = NULL;
    m->dragging = 0;
    m->x = fw * 0.5f;
    m->buttons = 0;
    m->pressed = 0;
    m->nextScreen = 0;
    m->y = fh * 0.5f;
}
HOOK(0x46ABE0, ComputerMenu_Create);

void ComputerMenu_Run(ComputerMenu *m)
{
    char *in = Computer_Get()->input;
    unsigned int buttons = 0, old;
    float fw = Display_GetMode()->width;
    float fh = Display_GetMode()->height;
    int pos[3];

    if (Game_IsPaused())
        return;
    if (m->nextScreen) {
        Task_DeleteChildren(m);
        ComputerMenu_OpenScreen(m->nextScreen);
        m->nextScreen = 0;
    }
    if (!m->dragging) {
        char *p = Profile_GetActive();
        long double my = *(float *)(in + 0x1B4);
        float dx, x, y;
        long double dy, lo, hi;

        if (*(int *)(p + 0x538))
            my = my * kFlip;
        dx = *(float *)(in + 0x1B0) * fw * 0.5f;
        dy = my * fh * 0.5f;
        x = dx + m->x;
        m->x = x;
        y = dy + m->y;
        m->y = y;
        lo = fw * kLeft;
        if (lo > x) {
            m->x = lo;
        } else {
            hi = fw - fw * kRight;
            m->x = !(hi >= x) ? hi : x;
        }
        lo = fh * kTop;
        if (lo > y) {
            m->y = lo;
        } else {
            hi = fh - fh * kBottom;
            m->y = !(hi >= y) ? hi : y;
        }
    }
    if (gMouseButtons & 1)
        buttons = 0x10;
    if (gMouseButtons & 2)
        buttons |= 0x20;
    pos[0] = round_int(m->x);
    pos[1] = round_int(m->y);
    pos[2] = 0;
    MenuItemTask_SendPointer(m->panel, pos);
    old = m->buttons;
    m->buttons = buttons;
    m->pressed = ~old & buttons;
    CALL_CHILDREN(&m->task, RUN_SLOT);
}
HOOK(0x46AC30, ComputerMenu_Run);

void ComputerMenu_Delete(void)
{
    gMenu = NULL;
}
HOOK(0x46AE20, ComputerMenu_Delete);

void ComputerMenu_Draw(ComputerMenu *m)
{
    int w, h;

    Computer_Get();
    w = Display_GetMode()->width;
    h = Display_GetMode()->height;
    Draw_RectZ(0, 0, w, h, 0, 127.0f, 0, 0xC0040000, -1, kBackZ);
    CALL_CHILDREN(&m->task, Screen_GetDrawEvent);
    if (!m->dragging) {
        float x = m->x, y = m->y;
        Sprite *s = (Sprite *)&gComputerRes.pointer;

        x = x - Sprite_Width(s) * 0.5f;
        QVertex_DrawFrame(s, x, y - Sprite_Height(s) * 0.5f, 128.0f, 128.0f, 128.0f, 1.0f, kPointerZ, 0, 0x400C0000,
                          -1);
    }
}
HOOK(0x46AE30, ComputerMenu_Draw);

void ComputerMenu_Shutdown(void)
{
    TaskType_Free(gMenuType);
    Alias_Remove("COMPUTER:");
}
HOOK(0x46AF90, ComputerMenu_Shutdown);

void ComputerMenu_OpenScreen(int screen)
{
    Mem_PushMode(1);
    if (!gMenu)
        Task_New(GameScreen_GetView(*(void **)(Game_Get() + 0x44)), gMenuType, 0);
    switch (screen) {
    case 1:
        ComputerMap_New(gMenu);
        break;
    case 2:
        ComputerObjectives_New(gMenu);
        break;
    case 3:
        ComputerLog_New(gMenu);
        break;
    default:
        Error_Show("No New function for menu!!");
        for (;;)
            ;
    }
    Mem_PopMode();
}
HOOK(0x46AFB0, ComputerMenu_OpenScreen);

ComputerMenu *ComputerMenu_Get(void)
{
    return gMenu;
}
HOOK(0x46B030, ComputerMenu_Get);

ComputerMenu *ComputerMenu_Get2(void)
{
    return gMenu;
}
HOOK(0x46B040, ComputerMenu_Get2);

Task *ComputerMenu_GetPanel(void)
{
    return ComputerMenu_Get2()->panel;
}
HOOK(0x46B050, ComputerMenu_GetPanel);

void ComputerMenu_SetPanel(Task *panel)
{
    ComputerMenu_Get2()->panel = panel;
}
HOOK(0x46B060, ComputerMenu_SetPanel);

void ComputerMenu_SetScreen(int screen)
{
    ComputerMenu_Get2()->nextScreen = screen;
}
HOOK(0x46B4E0, ComputerMenu_SetScreen);

unsigned int ComputerMenu_GetButtons(void)
{
    return ComputerMenu_Get2()->buttons;
}
HOOK(0x46B4F0, ComputerMenu_GetButtons);

unsigned int ComputerMenu_GetPressed(void)
{
    return ComputerMenu_Get2()->pressed;
}
HOOK(0x46B500, ComputerMenu_GetPressed);
