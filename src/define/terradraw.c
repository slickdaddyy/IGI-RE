#include "hook.h"
#include "igi.h"
#include "define/define.h"

#define gTerraDrawType VAR(uint16_t, 0x54D9A0)

/* a "TerraDrawTask" (0x24 bytes) that draws world */
Task *TerraDrawTask_New(void *parent, void *world)
{
    Task *t = Task_New(parent, gTerraDrawType, NULL);

    *(void **)(t + 1) = world;
    return t;
}
HOOK(0x4E8370, TerraDrawTask_New);
