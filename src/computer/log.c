#include "hook.h"
#include "computer/computer.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));

/* the handlers of "ComputerLog" come right after this range */
#define LOG_CREATE ((void *)0x468920)
#define LOG_RUN ((void *)0x488700)
#define LOG_DELETE ((void *)0x468BB0)
#define LOG_DRAW ((void *)0x468BC0)

#define gLogType VAR(uint16_t, 0x5BDC98)

void ComputerLog_Open(void)
{
    gLogType = TaskType_New("ComputerLog", 0x24, Task_GetRootType(), 0);
    TaskType_SetHandler(gLogType, 0, LOG_CREATE);
    TaskType_SetHandler2(gLogType, 1, LOG_RUN);
    TaskType_SetHandler2(gLogType, 2, LOG_DELETE);
    TaskType_SetHandler2(gLogType, Screen_GetDrawEvent(), LOG_DRAW);
}
HOOK(0x4688B0, ComputerLog_Open);
