#include "hook.h"
#include "igi.h"
#include "menu/menu.h"
#include "game/gametypes.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401BE0, Task *, Task_FindChild, (void *task, uint16_t type));
ORIG(0x416D40, void *, Game_Get, (void));
ORIG(0x4B1020, int, File_ResolvePath, (char *out, const char *path, int index));
ORIG(0x4B11A0, int, Device_Find, (const char *path));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1110, void, Level_ClearTaskIdTree, (Task *task));

/* a script task that loads or unloads a menu script when its buttons are pushed */
typedef struct MenuLoader {
    Task task;
    char path[0x100];
    char load;
    char unload;
    char pad[2];
} MenuLoader;

_Static_assert(sizeof(MenuLoader) == 0x124, "MenuLoader");

extern uint16_t gMenuLoaderType;
extern void *gMenuLoaderParams;
extern char gMenuLoaderPath[];

void MenuLoader_Create(MenuLoader *ml);
void MenuLoader_GetParams(void *task, void **out);
void MenuLoader_Changed(MenuLoader *ml);

void MenuLoader_Init(void)
{
    MenuLoader ml;

    gMenuLoaderType = TaskType_New("MenuLoader", sizeof(MenuLoader), Task_GetRootType(), 0);
    TaskType_SetHandler(gMenuLoaderType, 0, MenuLoader_Create);
    TaskType_SetHandler2(gMenuLoaderType, 2, Task_Nothing);
    TaskType_SetHandler(gMenuLoaderType, Param_GetTableEvent(), MenuLoader_GetParams);
    TaskType_SetHandler(gMenuLoaderType, Param_GetChangedEvent(), MenuLoader_Changed);
    TaskType_SetHandler2(gMenuLoaderType, 1, Task_Nothing);
    gMenuLoaderParams = ParamTable_New(3);
    ParamTable_Add(gMenuLoaderParams, &ml, ml.path, Param_String256(), 7, NULL);
    ParamTable_Add(gMenuLoaderParams, &ml, &ml.load, Param_PushButton(), 6, NULL);
    ParamTable_Add(gMenuLoaderParams, &ml, &ml.unload, Param_PushButton(), 6, NULL);
}
HOOK(0x4190E0, MenuLoader_Init);

void MenuLoader_Create(MenuLoader *ml)
{
    ml->path[0] = 0;
    ml->unload = 0;
    ml->load = 0;
}
HOOK(0x4191F0, MenuLoader_Create);

void MenuLoader_Changed(MenuLoader *ml)
{
    Game *g = Game_Get();

    if (ml->load) {
        File_ResolvePath(gMenuLoaderPath, ml->path, 0);
        if (Device_Find(gMenuLoaderPath))
            MenuManager_New(g->root, ml->path, 0, 1, 0);
    }
    if (ml->unload) {
        Task *mm = Task_FindChild(g->root, MenuManager_GetType());

        if (mm) {
            Level_ClearTaskIdTree(mm);
            Task_Delete(mm);
        }
    }
}
HOOK(0x419210, MenuLoader_Changed);

void MenuLoader_GetParams(void *task, void **out)
{
    *out = gMenuLoaderParams;
}
HOOK(0x4192A0, MenuLoader_GetParams);

void MenuLoader_Shutdown(void)
{
    QTaskList_Free(gMenuLoaderParams);
    TaskType_Free(gMenuLoaderType);
    gMenuLoaderType = 0x181;
}
HOOK(0x4192B0, MenuLoader_Shutdown);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(539ADC) uint16_t gMenuLoaderType INIT(385);
PAD(igd, 539ADC, 539ADE, 0x2);
BSS(57BAF4) void *gMenuLoaderParams;
PAD(igb, 57BAF4, 57BAF8, 0x88);
ALIAS(gMenuLoaderPath, pad_57BAF8, 0);
