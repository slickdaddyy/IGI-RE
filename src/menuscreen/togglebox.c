#include "hook.h"
#include "igi.h"
#include "menuscreen/menuscreen.h"

ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));

/* only the tail of the ToggleBox type lives here, the rest is just before */
typedef struct ToggleBox {
    MenuWindow w;
    char unk54[0x50];
    int isOn;
    char registered;
    char unkA9[0xA3];
} ToggleBox;

_Static_assert(sizeof(ToggleBox) == 0x14C, "ToggleBox");

#define gToggleBoxType VAR(uint16_t, 0x53B674)
#define gToggleBoxParams VAR(void *, 0x57BBD0)

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
