#include "hook.h"
#include "editcam/editcam.h"

FN_EXTERN(ConditionalContainer_ForwardEvent4);
FN_EXTERN(GuardGenerator_WorldAdd);
FN_EXTERN(GuardGenerator_WorldRemove);
FN_EXTERN(MagicObjContainer_Event);
FN_EXTERN(SubLevel_Run);

ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));

#define SubLevel_Run (FN(0x488700, SubLevel_Run))
#define MagicObjContainer_Event (FN(0x4E0890, MagicObjContainer_Event))
#define ConditionalContainer_ForwardEvent4 (FN(0x4879E0, ConditionalContainer_ForwardEvent4))
#define GuardGenerator_WorldRemove (FN(0x487E20, GuardGenerator_WorldRemove))
#define GuardGenerator_WorldAdd (FN(0x487D90, GuardGenerator_WorldAdd))

extern uint16_t gContainerType;

/* a plain task that runs its children and passes world events on */
void Container_Init(void)
{
    uint16_t type = TaskType_New("Container", 0x20, Task_GetRootType(), 0, 0);

    gContainerType = type;
    TaskType_SetHandler2(type, 1, SubLevel_Run);
    TaskType_SetHandler(gContainerType, RigidDynCubeObj_GetEvent2(), MagicObjContainer_Event);
    TaskType_SetHandler(gContainerType, RigidDynCubeObj_GetEvent4(), ConditionalContainer_ForwardEvent4);
    TaskType_SetHandler(gContainerType, World_GetEvent1(), GuardGenerator_WorldRemove);
    TaskType_SetHandler(gContainerType, World_GetEvent0(), GuardGenerator_WorldAdd);
}
HOOK(0x505C90, Container_Init);

void Container_Shutdown(void)
{
    TaskType_Free(gContainerType);
}
HOOK(0x505D30, Container_Shutdown);

uint16_t Container_GetType(void)
{
    return gContainerType;
}
HOOK(0x505D40, Container_GetType);
