#include "hook.h"
#include "igi.h"
#include "define/define.h"
#include "viewport/viewportvars.h"

/* a "TerraDrawTask" (0x24 bytes) that draws world */
Task *TerraDrawTask_New(void *parent, void *world)
{
    Task *t = Task_New(parent, gTerraDrawType, NULL);

    *(void **)(t + 1) = world;
    return t;
}
HOOK(0x4E8370, TerraDrawTask_New);
