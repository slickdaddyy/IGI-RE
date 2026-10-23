/* SoundManager task type; its handlers live after this range. */
#include "hook.h"
#include "nodereg/nodereg.h"

/* "SoundManager" (0x44) */
typedef struct SoundManager {
    Task task;
    char name[32];
    int unk40;
} SoundManager;

_Static_assert(sizeof(SoundManager) == 0x44, "SoundManager");

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

#define SOUNDMANAGER_CREATE ((void *)0x51C090)
#define SOUNDMANAGER_RUN ((void *)0x488700)
#define SOUNDMANAGER_DELETE ((void *)0x51C0A0)
#define SOUNDMANAGER_INFO ((void *)0x51C0C0)
#define SOUNDMANAGER_CHANGED ((void *)0x51C100)
#define SOUNDMANAGER_TABLE ((void *)0x51C280)

#define gSoundManagerType VAR(uint16_t, 0x54EE3C)
#define gSoundManagerParams VAR(void *, 0xA83780)

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
