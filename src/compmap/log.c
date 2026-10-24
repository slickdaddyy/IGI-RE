#pragma GCC optimize("excess-precision=standard")
#include "hook.h"
#include "compmap/compmap.h"

ORIG(0x4B5730, void, QVertex_DrawLine, (float x0, float y0, float z0, float x1, float y1, float z1, float r, float g,
                                        float b, float unk10, float unk11, float unk12, int unk13,
                                        unsigned int flags, int unk15));

static const float kTabX = 0x1.99999ap-4f;
static const float kMapTabY = 0x1.9ddddep-1f;
static const float kTabY = 0x1.8cccccp-1f;
static const float kScrollX = 0x1.733334p-4f;
static const float kScrollY = 0x1.99999ap-4f;
static const float kScrollGap = 36.0f;
static const double kTextX = 0.15;
static const double kTextY = 0.1;
static const float kBackZ = 0x1.0f3d70p+9f;
static const float kTextZ = 0x1.053d70p+9f;

void ComputerLog_OnTab(ComputerItem *item, int *id);
void ComputerLog_OnScroll(ComputerButtonItem *b, int *id);

void ComputerLog_Create(ComputerLog *log)
{
    ComputerRes *r = ComputerRes_Get();
    int w = Display_GetMode()->width;
    int h = Display_GetMode()->height;
    Task *panel;
    const char *s;
    Sprite *font;
    float fw, fh, x, y;

    gLog = log;
    panel = MenuItemTask_New(log);
    ComputerMenu_SetPanel(panel);
    log->scroll = 0;
    if (Computer_IsMapEnabled()) {
        s = Resource_Load("MAP", 0);
        font = MenuManager_FindFont(MENU_MANAGER(), "font3.fnt");
        ComputerTextItem_New(panel, 1, 0, 0, 0, 0, 0, w * kTabX, h * kMapTabY, font, s, 1, ComputerLog_OnTab);
    }
    fh = h;
    fw = w;
    s = Resource_Load("OBJECTIVES", 0);
    font = MenuManager_FindFont(MENU_MANAGER(), "font3.fnt");
    ComputerTextItem_New(panel, 2, 0, 0, 0, 0, 0, fw * kTabX, fh * kTabY, font, s, 1, ComputerLog_OnTab);
    x = fw * kScrollX;
    y = fh * kScrollY;
    ComputerButtonItem_New(panel, 3, 0, 0, 0, 0, 0, x, y, 0, NULL, gEmptyString, (Sprite *)&r->arrowUp[0],
                           (Sprite *)&r->arrowUp[1], (Sprite *)&r->arrowUp[2], ComputerLog_OnScroll);
    ComputerButtonItem_New(panel, 4, 0, 0, 0, 0, 0, x, y + kScrollGap, 0, NULL, gEmptyString,
                           (Sprite *)&r->arrowDown[0], (Sprite *)&r->arrowDown[1], (Sprite *)&r->arrowDown[2],
                           ComputerLog_OnScroll);
}
HOOK(0x468920, ComputerLog_Create);

void ComputerLog_OnTab(ComputerItem *item, int *id)
{
    if (!(ComputerMenu_GetPressed() & 0x10))
        return;
    if (*id == 1)
        ComputerMenu_SetScreen(1);
    else if (*id == 2)
        ComputerMenu_SetScreen(2);
}
HOOK(0x468B00, ComputerLog_OnTab);

void ComputerLog_OnScroll(ComputerButtonItem *b, int *id)
{
    ComputerLog *log = gLog;
    unsigned int buttons;
    char was;

    if (b->base.id != *id)
        return;
    buttons = ComputerMenu_GetButtons();
    was = b->pressed;
    b->wasPressed = was;
    b->pressed = buttons & 0x10;
    if (!(buttons & 0x10))
        return;
    if (b->base.id == 3) {
        if (!b->wasPressed && log->scroll > 0)
            log->scroll--;
    } else if (b->base.id == 4) {
        if (!was && log->scroll < gLogCount - 15)
            log->scroll++;
    }
}
HOOK(0x468B30, ComputerLog_OnScroll);

void ComputerLog_Delete(void)
{
    gLog = NULL;
}
HOOK(0x468BB0, ComputerLog_Delete);

void ComputerLog_Draw(ComputerLog *log)
{
    int w = Display_GetMode()->width;
    int h = Display_GetMode()->height;
    int i, x, y;

    Draw_RectZ(0, 0, w, h, 0, 0, 0, 0x40040000, -1, kBackZ);
    ComputerRes_Get();
    Mem_PushMode(1);
    x = round_int(w * kTextX);
    y = round_int(h * kTextY);
    for (i = 0; i < gLogCount && i < 15; i++) {
        Sprite *font = MenuManager_FindFont(MENU_MANAGER(), "font1.fnt");

        y = Computer_DrawWrapped(x, y, kTextZ, 400, 0x78, gLogLines[gLogCount - log->scroll - i - 1], font, 1) + 2;
        QVertex_DrawLine(x, y, kTextZ, x + 100, y, kTextZ, 48.0f, 96.0f, 48.0f, 0, 16.0f, 0, 0, 0x40040000, -1);
        y += 2;
    }
    Mem_PopMode();
    CALL_CHILDREN(&log->task, Screen_GetDrawEvent);
}
HOOK(0x468BC0, ComputerLog_Draw);

void ComputerLog_Shutdown(void)
{
    TaskType_Free(gLogType);
}
HOOK(0x468DA0, ComputerLog_Shutdown);

Task *ComputerLog_New(void *parent)
{
    return Task_New(parent, gLogType, 0);
}
HOOK(0x468DB0, ComputerLog_New);

void ComputerLog_Clear(void)
{
    gLogCount = 0;
}
HOOK(0x468DD0, ComputerLog_Clear);

void ComputerLog_Add(const char *line)
{
    int i;

    if (gLogCount < 50) {
        gLogLines[gLogCount++] = line;
        return;
    }
    for (i = 0; i < 49; i++)
        gLogLines[i] = gLogLines[i + 1];
    gLogLines[gLogCount - 1] = line;
}
HOOK(0x468DE0, ComputerLog_Add);
