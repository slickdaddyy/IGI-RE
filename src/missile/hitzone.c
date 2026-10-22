#include "hook.h"
#include "igi.h"

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));

#define gHitZoneType VAR(uint16_t, 0x53BB6C)

/* the last function of the HitZone module (0x427D90 - 0x427FB0) */
void HitZone_Close(void)
{
    TaskType_Free(gHitZoneType);
}
HOOK(0x427FB0, HitZone_Close);
