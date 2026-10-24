#include "hook.h"
#include "win/win.h"

extern DWORD gTimerStart;

void Timer_Open(void)
{
    gTimerStart = GetTickCount();
}
HOOK(0x490360, Timer_Open);

int Timer_GetMs(void)
{
    return GetTickCount() - gTimerStart;
}
HOOK(0x490370, Timer_GetMs);
