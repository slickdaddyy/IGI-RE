#include <string.h>

#include "hook.h"
#include "viewport/viewport.h"

/* "Screen": the task everything is drawn under. Its draw handler wraps a
   display frame and sends the draw event to its children. */

#define MODE_SIZE 0x11C

typedef struct ScreenOverlay {
    Task *task;
    int unk4;
} ScreenOverlay;

extern int gScreenDrawEvent;
extern uint16_t gScreenType;
extern char gScreenNextMode[];
extern ScreenOverlay gScreenOverlays[];
extern char gScreenMode[];
extern int gScreenFrame;
extern char gScreenModeFlag;
extern char gScreenModePending;
extern char gScreenReady;
extern int gScreenOverlayCount;

ORIG(0x491A90, void, Display_SetMode, (void *mode));
ORIG(0x491D00, void, Display_NextFrame, (void));
ORIG(0x491D20, void, Display_EndFrame, (void));

void Screen_Draw(Task *t);

void Screen_Init(void)
{
    gScreenDrawEvent = TaskEvent_Alloc2(0);
    gScreenType = TaskType_New("Screen", 0x20, Task_GetRootType(), 0);
    TaskType_SetHandler(gScreenType, 0, TASK_NOTHING);
    TaskType_SetHandler2(gScreenType, 1, SUBLEVEL_RUN);
    TaskType_SetHandler2(gScreenType, Screen_GetDrawEvent(), Screen_Draw);
    memset(gScreenMode, 0, MODE_SIZE);
    gScreenModePending = 0;
    gScreenReady = 1;
}
HOOK(0x4E7370, Screen_Init);

void Screen_Draw(Task *t)
{
    int i;

    if (gScreenModePending) {
        Display_SetMode(gScreenNextMode);
        memcpy(gScreenMode, gScreenNextMode, MODE_SIZE);
        gScreenModePending = 0;
    }
    Display_NextFrame();
    CALL_CHILDREN(t, Screen_GetDrawEvent(), 1);
    for (i = 0; i < gScreenOverlayCount; i++) {
        Task *o = gScreenOverlays[i].task;
        gTaskHandlers[Screen_GetDrawEvent()][o->type](o);
    }
    Display_EndFrame();
    gScreenFrame++;
}
HOOK(0x4E7400, Screen_Draw);

void Screen_Shutdown(void)
{
    TaskEvent_Free2(gScreenDrawEvent);
    gScreenDrawEvent = 0xFF;
    TaskType_Free(gScreenType);
    gScreenReady = 0;
}
HOOK(0x4E7500, Screen_Shutdown);

int Screen_GetDrawEvent(void)
{
    return gScreenDrawEvent;
}
HOOK(0x4E7530, Screen_GetDrawEvent);

/* The new display mode is set at the start of the next frame. */
int Screen_RequestMode(const void *mode, char flag)
{
    if (gScreenModePending)
        return 0;
    memcpy(gScreenNextMode, mode, MODE_SIZE);
    gScreenModeFlag = flag;
    gScreenModePending = 1;
    return 1;
}
HOOK(0x4E7540, Screen_RequestMode);

Task *Screen_New(void *parent)
{
    return Task_New(parent, gScreenType, NULL);
}
HOOK(0x4E7580, Screen_New);

int Screen_GetFrame(void)
{
    return gScreenFrame;
}
HOOK(0x4E75A0, Screen_GetFrame);

uint16_t Screen_GetType(void)
{
    return gScreenType;
}
HOOK(0x4E75B0, Screen_GetType);
