#include "hook.h"
#include "igi.h"
#include "widget/widget.h"

/* a whole menu page; the type and its first handlers are made at 0x421750 */
typedef struct MenuScreen {
    MenuWindow w;
    char unk54[0xAC - 0x54];
    VarString onBack;  /* run when Esc / the back button is pressed */
    VarString onOpen;  /* run on MenuWindow event 914 (layout) */
    VarString onApply; /* run on MenuWindow event 918 */
    int userData;      /* the script variable <name>.nUserData */
    int registered;
} MenuScreen;

_Static_assert(sizeof(MenuScreen) == 0x1A4, "MenuScreen");

extern uint16_t gMenuScreenType;
extern void *gMenuScreenParams;

void MenuScreen_Run(MenuScreen *ms)
{
    MenuInput *in;

    if (ms->w.menu && (in = MenuManager_GetInput(ms->w.menu)) != NULL) {
        if (in->back && VarString_HasExpr(&ms->onBack))
            VarString_EvalReal(&ms->onBack, ms);
        in->back = 0;
    }
    CALL_BASE(1, ms);
}
HOOK(0x422110, MenuScreen_Run);

void MenuScreen_GetParams(void *task, void **out)
{
    *out = gMenuScreenParams;
}
HOOK(0x422180, MenuScreen_GetParams);

void MenuScreen_Activate(MenuScreen *ms)
{
    char name[128], buf[128];

    if (!ms->registered) {
        ms->registered = 1;
        Task_GetName(name, ms);
        crt_sprintf(buf, "%s.nUserData", name);
        Script_RegisterInt2(buf, &ms->userData);
    }
}
HOOK(0x422190, MenuScreen_Activate);

void MenuScreen_Deactivate(MenuScreen *ms)
{
    char name[128], buf[128];

    if (ms->registered) {
        Task_GetName(name, ms);
        crt_sprintf(buf, "%s.nUserData", name);
        Script_RemoveSymbol(buf);
        ms->registered = 0;
    }
}
HOOK(0x422200, MenuScreen_Deactivate);

void MenuScreen_Open(MenuScreen *ms)
{
    if (VarString_HasExpr(&ms->onOpen))
        VarString_EvalReal(&ms->onOpen, ms);
    CALL_CHILDREN(ms, MenuWindow_GetEvent914());
}
HOOK(0x422260, MenuScreen_Open);

void MenuScreen_Apply(MenuScreen *ms)
{
    if (VarString_HasExpr(&ms->onApply))
        VarString_EvalReal(&ms->onApply, ms);
    CALL_CHILDREN(ms, MenuWindow_GetEvent918());
}
HOOK(0x422300, MenuScreen_Apply);

void MenuScreen_Shutdown(void)
{
    QTaskList_Free(gMenuScreenParams);
    TaskType_Free(gMenuScreenType);
    gMenuScreenType = 0x181;
}
HOOK(0x4223A0, MenuScreen_Shutdown);
