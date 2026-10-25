#include "hook.h"
#include "igi.h"
#include "widget/widget.h"
#include "widget/widgetvars.h"

ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x48F490, void, AppMain_AddWinProcCallback, (void *fn));
ORIG(0x424CA0, void, Cursor_WinProc, (void));
ORIG(0x424CE0, void, Cursor_RunHandler, (void *task));

void Cursor_CreateHandler(Cursor *c, void *owner);
void Cursor_Draw(Cursor *c);

void Cursor_Open(void)
{
    gCursorType = TaskType_New("Cursor", sizeof(Cursor), Task_GetRootType(), 0);
    TaskType_SetHandler(gCursorType, 0, Cursor_CreateHandler);
    TaskType_SetHandler2(gCursorType, 1, Cursor_RunHandler);
    TaskType_SetHandler2(gCursorType, 2, Task_Nothing);
    TaskType_SetHandler2(gCursorType, Screen_GetDrawEvent(), Cursor_Draw);
    AppMain_AddWinProcCallback(Cursor_WinProc);
}
HOOK(0x424B90, Cursor_Open);

void Cursor_CreateHandler(Cursor *c, void *owner)
{
    c->owner = owner;
    c->sprite = NULL;
    c->x = 320;
    c->y = 240;
    c->hotX = -2;
    c->hotY = 0;
    c->prevButtons = 0;
    c->buttons = 0;
}
HOOK(0x424C10, Cursor_CreateHandler);

void Cursor_Draw(Cursor *c)
{
    if (c->sprite)
        Draw_Sprite(c->sprite, c->hotX + c->x, c->hotY + c->y, 128.0f, 128.0f, 128.0f, 0, 0, 0);
}
HOOK(0x424C40, Cursor_Draw);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BC60) uint16_t gCursorType;
PAD(igb, 57BC60, 57BC62, 0x2);
