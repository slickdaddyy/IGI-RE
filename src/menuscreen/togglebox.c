#include "hook.h"
#include "igi.h"
#include "menuscreen/menuscreen.h"
#include "ui/uitypes.h"

ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));

extern uint16_t gToggleBoxType;
extern void *gToggleBoxParams;

/* the script sees the state as "<name>.isOn" while the box is active */
void ToggleBox_Activate(ToggleBox *tb)
{
    char name[64];
    char buf[128];

    if (!tb->registered) {
        tb->registered = 1;
        Task_GetName(name, tb);
        crt_sprintf(buf, "%s.isOn", name);
        Script_RegisterInt(buf, &tb->isOn);
    }
}
HOOK(0x41E5A0, ToggleBox_Activate);

void ToggleBox_Deactivate(ToggleBox *tb)
{
    char name[64];
    char buf[128];

    if (tb->registered) {
        Task_GetName(name, tb);
        crt_sprintf(buf, "%s.isOn", name);
        Script_RemoveSymbol(buf);
        tb->registered = 0;
    }
}
HOOK(0x41E600, ToggleBox_Deactivate);

void ToggleBox_Shutdown(void)
{
    QTaskList_Free(gToggleBoxParams);
    TaskType_Free(gToggleBoxType);
    gToggleBoxType = 0x181;
}
HOOK(0x41E660, ToggleBox_Shutdown);
