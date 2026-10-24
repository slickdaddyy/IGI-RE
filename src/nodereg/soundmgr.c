/* SoundManager task type; its handlers live after this range. */
#include "hook.h"
#include "nodereg/nodereg.h"
#include "flatwater/flatwatertypes.h"

FN_EXTERN(SoundManager_Create);
FN_EXTERN(SoundManager_Delete);
FN_EXTERN(SoundManager_GetInfo);
FN_EXTERN(SoundManager_GetParams);
FN_EXTERN(SoundManager_ParamsChanged);
FN_EXTERN(SubLevel_Run);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4F0FE0, int, Param_String32, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, int type, int flags, void *extra));

#define SOUNDMANAGER_CREATE (FN(0x51C090, SoundManager_Create))
#define SOUNDMANAGER_RUN (FN(0x488700, SubLevel_Run))
#define SOUNDMANAGER_DELETE (FN(0x51C0A0, SoundManager_Delete))
#define SOUNDMANAGER_INFO (FN(0x51C0C0, SoundManager_GetInfo))
#define SOUNDMANAGER_CHANGED (FN(0x51C100, SoundManager_ParamsChanged))
#define SOUNDMANAGER_TABLE (FN(0x51C280, SoundManager_GetParams))

extern uint16_t gSoundManagerType;
extern void *gSoundManagerParams;

void SoundManager_Open(void)
{
    SoundManager p;
    uint16_t t = TaskType_New("SoundManager", sizeof(SoundManager), Task_GetRootType(), 0, 0);

    gSoundManagerType = t;
    TaskType_SetHandler(t, 0, SOUNDMANAGER_CREATE);
    TaskType_SetHandler2(gSoundManagerType, 1, SOUNDMANAGER_RUN);
    TaskType_SetHandler2(gSoundManagerType, 2, SOUNDMANAGER_DELETE);
    TaskType_SetHandler(gSoundManagerType, Sound_GetInfoEvent(), SOUNDMANAGER_INFO);
    TaskType_SetHandler(gSoundManagerType, Param_GetChangedEvent(), SOUNDMANAGER_CHANGED);
    TaskType_SetHandler(gSoundManagerType, Param_GetTableEvent(), SOUNDMANAGER_TABLE);
    gSoundManagerParams = ParamTable_New(1);
    ParamTable_Add(gSoundManagerParams, &p, &p.name, Param_String32(), 7, NULL);
}
HOOK(0x51BFB0, SoundManager_Open);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54EE3C) uint16_t gSoundManagerType INIT(385);
PAD(igd, 54EE3C, 54EE3E, 0x2);
BSS(A83780) void *gSoundManagerParams;
