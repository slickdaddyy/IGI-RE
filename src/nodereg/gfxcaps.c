/* GFXCapsViewer: an editor task that shows the capabilities of the
   current renderer. */
#include "hook.h"
#include "nodereg/nodereg.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x491DE0, int, Display_GetGFXCapsMethod, (void));
ORIG(0x4F11F0, uint8_t, Level_GetEventDA4, (void));

#define gRenderFuncs ((void (**)(void *))0xA94E84)
#define gGFXCaps ((void *)0xA81028)
extern uint16_t gGFXCapsViewerType;

void GFXCapsViewer_GetCaps(Task *t, void **out)
{
    gRenderFuncs[Display_GetGFXCapsMethod()](gGFXCaps);
    *out = gGFXCaps;
}
HOOK(0x5196F0, GFXCapsViewer_GetCaps);

void GFXCapsViewer_Open(void)
{
    gGFXCapsViewerType = TaskType_New("GFXCapsViewer", sizeof(Task), TASK_NO_TYPE, 0, 0);
    TaskType_SetHandler(gGFXCapsViewerType, Level_GetEventDA4(), GFXCapsViewer_GetCaps);
}
HOOK(0x519710, GFXCapsViewer_Open);

void GFXCapsViewer_Close(void)
{
    TaskType_Free(gGFXCapsViewerType);
}
HOOK(0x519750, GFXCapsViewer_Close);
