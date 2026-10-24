#include "hook.h"
#include "computer/computer.h"

FN_EXTERN(ComputerLog_Create);
FN_EXTERN(ComputerLog_Delete);
FN_EXTERN(ComputerLog_Draw);
FN_EXTERN(SubLevel_Run);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));

/* the handlers of "ComputerLog" come right after this range */
#define LOG_CREATE (FN(0x468920, ComputerLog_Create))
#define LOG_RUN (FN(0x488700, SubLevel_Run))
#define LOG_DELETE (FN(0x468BB0, ComputerLog_Delete))
#define LOG_DRAW (FN(0x468BC0, ComputerLog_Draw))

#define gComputerLogType VAR(uint16_t, 0x5BDC98)

void ComputerLog_Open(void)
{
    gComputerLogType = TaskType_New("ComputerLog", 0x24, Task_GetRootType(), 0);
    TaskType_SetHandler(gComputerLogType, 0, LOG_CREATE);
    TaskType_SetHandler2(gComputerLogType, 1, LOG_RUN);
    TaskType_SetHandler2(gComputerLogType, 2, LOG_DELETE);
    TaskType_SetHandler2(gComputerLogType, Screen_GetDrawEvent(), LOG_DRAW);
}
HOOK(0x4688B0, ComputerLog_Open);
