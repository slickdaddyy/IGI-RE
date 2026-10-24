#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "vehicle/vehicle.h"

ORIG(0x48F130, int, AppContext_IsFullscreen, (void));
ORIG(0x491CF0, Display *, Display_GetMode, (void));

/* the mouse pointer of the menus */
typedef struct Cursor {
    Task task;
    void *owner;
    int x, y;
    int hotX, hotY;
    void *sprite;
    int prevButtons;
    int buttons;
} Cursor;

_Static_assert(sizeof(Cursor) == 0x40, "Cursor");

extern uint16_t gCursorType;
extern int gCursorWindowX;
extern int gCursorWindowY;
extern float gMouseDX;
extern float gMouseDY;
extern unsigned int gMouseButtons;

static const float kSpeedX = 320.0f;
static const float kSpeedY = 240.0f;

/* window messages: in a window the pointer follows the system mouse */
int Cursor_WinProc(void *hwnd, int msg, int wparam, int lparam)
{
    if (msg == 0x200 && !AppContext_IsFullscreen()) {
        gCursorWindowX = (short)lparam;
        gCursorWindowY = (short)((unsigned int)lparam >> 16);
    }
    return 1;
}
HOOK(0x424CA0, Cursor_WinProc);

void Cursor_RunHandler(Cursor *c)
{
    int v;

    if (!AppContext_IsFullscreen()) {
        c->x = gCursorWindowX;
        c->y = gCursorWindowY;
    } else {
        float dy = gMouseDY;

        c->x += round_int(gMouseDX * kSpeedX);
        c->y += round_int(dy * kSpeedY);
    }
    if (c->x < 0)
        v = 0;
    else if (Display_GetMode()->width - 1 < c->x)
        v = Display_GetMode()->width - 1;
    else
        v = c->x;
    c->x = v;
    if (c->y < 0)
        v = 0;
    else if (Display_GetMode()->height - 1 < c->y)
        v = Display_GetMode()->height - 1;
    else
        v = c->y;
    c->y = v;
    c->prevButtons = c->buttons;
    c->buttons = gMouseButtons & 1;
}
HOOK(0x424CE0, Cursor_RunHandler);

void Cursor_Close(void)
{
    TaskType_Free(gCursorType);
}
HOOK(0x424E10, Cursor_Close);

Task *Cursor_New(void *arg)
{
    return Task_New(NULL, gCursorType, arg);
}
HOOK(0x424E20, Cursor_New);

void Cursor_SetSprite(Cursor *c, void *sprite)
{
    c->sprite = sprite;
}
HOOK(0x424E40, Cursor_SetSprite);

void Cursor_GetState(Cursor *c, int *x, int *y, int *buttons, int *prev)
{
    *x = c->x;
    *y = c->y;
    *buttons = c->buttons;
    *prev = c->prevButtons;
}
HOOK(0x424E50, Cursor_GetState);
