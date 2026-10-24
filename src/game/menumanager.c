/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "game/game.h"

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x4061C0, void *, Profile_GetKeys, (int mode, float a0, float a1, float a2, float b0, float b1, float b2,
                                         float c0, float c1, float c2, float d0, float d1, float d2));
ORIG(0x4061E0, int, Profile_GetKeyCount, (void *keys));
ORIG(0x4062E0, int, Profile_GetLanguage, (void));
ORIG(0x4062F0, void, Profile_SetLanguage, (int language));
ORIG(0x417A20, void, MenuManager_Delete, (void *mm));
ORIG(0x417CF0, void, MenuManager_Update, (void *mm));
ORIG(0x418070, void, MenuManager_GetParams, (void *mm));
ORIG(0x418080, void, MenuManager_Run, (void *mm));
ORIG(0x418970, void, MenuManager_Draw, (void *mm));
ORIG(0x418A80, void, MenuManager_Activate, (void *mm));
ORIG(0x418AA0, void, MenuManager_Deactivate, (void *mm));
ORIG(0x424860, int, MenuWindow_GetEvent910, (void));
ORIG(0x424870, int, MenuWindow_GetEvent914, (void));
ORIG(0x424880, int, MenuWindow_GetEvent918, (void));
ORIG(0x424890, int, MenuWindow_GetEvent91C, (void));
ORIG(0x424E20, Task *, Cursor_New, (void *mm));
ORIG(0x489460, char, LevelFlow_IsFailed, (void));
ORIG(0x48A710, void, Keys_Install, (void));
ORIG(0x491CF0, int *, Display_GetMode, (void));
ORIG(0x497570, void, Scene_Init, (void *scene, int on));
ORIG(0x497680, void, Scene_SetRect, (void *scene, float *rect));
ORIG(0x4B12F0, void, Alias_Set, (const char *name, const char *path));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4ED5E0, void, InputPort_Setup, (int a, int b));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

/* the fields of the menu system task used here, see the menu module */
typedef struct MenuManager {
    Task task;
    int screen;
    int popup;
    int state;
    int nextState;
    Task *screenTask;
    Task *popupTask;
    Task *focus;
    char unk3C[0x98];
    char resPath[0x100];
    void *menuRes;
    void *missionRes;
    char unk1DC[0x80];
    int langResCount;
    void *strings;
    char unk264[0x2400];
    short spriteCount;
    short fontCount;
    short picCount;
    short pad266A;
    char unk266C[0x50];
    char unk26BC;
    char active;
    char unk26BE;
    char soundsLoaded;
    char unk26C0;
    char fastFade;
    char unk26C2[3];
    char scriptName[0x100];
    char pad27C5[3];
    Task *pointer;
    char input;
    char unk27CD[0x13];
    Task *backgroundFX;
    int language;
    int depth;
    int screenStack[16];
    char unk282C[0x6C];
} MenuManager;

_Static_assert(offsetof(MenuManager, resPath) == 0xD4, "MenuManager.resPath");
_Static_assert(offsetof(MenuManager, langResCount) == 0x25C, "MenuManager.langResCount");
_Static_assert(offsetof(MenuManager, spriteCount) == 0x2664, "MenuManager.spriteCount");
_Static_assert(offsetof(MenuManager, scriptName) == 0x26C5, "MenuManager.scriptName");
_Static_assert(offsetof(MenuManager, pointer) == 0x27C8, "MenuManager.pointer");
_Static_assert(offsetof(MenuManager, backgroundFX) == 0x27E0, "MenuManager.backgroundFX");
_Static_assert(offsetof(MenuManager, screenStack) == 0x27EC, "MenuManager.screenStack");
_Static_assert(sizeof(MenuManager) == 0x2898, "MenuManager");

#define gMenuManagerType VAR(uint16_t, 0x539824)
#define gMenuManagerParams VAR(void *, 0x57BAE0)
#define gUnkBC20A0 VAR(int, 0xBC20A0)
#define gUnkC28C61 VAR(char, 0xC28C61)
#define gUnkC28F80 VAR(char, 0xC28F80)

#define FIELD(name) ((void *)offsetof(MenuManager, name))

void MenuManager_Create(MenuManager *mm);
void MenuManager_Refresh(MenuManager *mm, int all);
void MenuManager_SendEvent910(MenuManager *mm, Task *w);
ScriptValue *MenuManager_RequestScreen(ScriptValue *out, MenuManager *mm, void *args);
ScriptValue *MenuManager_LeaveMenus(ScriptValue *out, MenuManager *mm, void *args);
ScriptValue *MenuManager_SetLanguage(ScriptValue *out, MenuManager *mm, void *args);
ScriptValue *MenuManager_PushScreen(ScriptValue *out, MenuManager *mm, void *args);
ScriptValue *MenuManager_PopScreen(ScriptValue *out, MenuManager *mm, void *args);
ScriptValue *MenuManager_ForceUpdateWindow(ScriptValue *out, MenuManager *mm, void *args);
ScriptValue *MenuManager_ActivatePopupScreen(ScriptValue *out, MenuManager *mm, void *args);
ScriptValue *MenuManager_DeactivatePopuScreen(ScriptValue *out, MenuManager *mm);
ScriptValue *MenuManager_SetEnabled(ScriptValue *out, MenuManager *mm, void *args);

void MenuManager_Init(void)
{
    Alias_Set("MENUSYS:", "LOCAL:menusystem\\");
    gMenuManagerType = TaskType_New("MenuManager", sizeof(MenuManager), Task_GetRootType(), 0);
    TaskType_SetHandler(gMenuManagerType, 0, MenuManager_Create);
    TaskType_SetHandler2(gMenuManagerType, 2, MenuManager_Delete);
    TaskType_SetHandler(gMenuManagerType, Param_GetTableEvent(), MenuManager_GetParams);
    TaskType_SetHandler(gMenuManagerType, Param_GetChangedEvent(), MenuManager_Update);
    TaskType_SetHandler2(gMenuManagerType, 1, MenuManager_Run);
    TaskType_SetHandler2(gMenuManagerType, Screen_GetDrawEvent(), MenuManager_Draw);
    TaskType_SetHandler2(gMenuManagerType, Level_GetEventDA8(), MenuManager_Activate);
    TaskType_SetHandler2(gMenuManagerType, Level_GetEventDAC(), MenuManager_Deactivate);
    gMenuManagerParams = ParamTable_New(5);
    /* the original passes a dummy MenuManager on its stack, only the
       difference of the two pointers is kept */
    ParamTable_Add(gMenuManagerParams, NULL, FIELD(unk26BC), Param_PushButton(), 6, NULL);
    ParamTable_Add(gMenuManagerParams, NULL, FIELD(screen), Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuManagerParams, NULL, FIELD(resPath), Param_String256(), 7, NULL);
    ParamTable_Add(gMenuManagerParams, NULL, FIELD(scriptName), Param_String256(), 6, NULL);
    ParamTable_Add(gMenuManagerParams, NULL, FIELD(fastFade), Param_Bool8(), 7, NULL);
}
HOOK(0x4170A0, MenuManager_Init);

void MenuManager_Create(MenuManager *mm)
{
    float rect[4];

    Script_RegisterFunction("MenuManager_RequestScreen", MenuManager_RequestScreen, mm, 2);
    Script_RegisterFunction("MenuManager_LeaveMenus", MenuManager_LeaveMenus, mm, 2);
    Script_RegisterFunction("MenuManager_SetLanguage", MenuManager_SetLanguage, mm, 1);
    Script_RegisterFunction("MenuManager_PushScreen", MenuManager_PushScreen, mm, 1);
    Script_RegisterFunction("MenuManager_PopScreen", MenuManager_PopScreen, mm, 1);
    Script_RegisterFunction("MenuManager_ForceUpdateWindow", MenuManager_ForceUpdateWindow, mm, 1);
    Script_RegisterFunction("MenuManager_ActivatePopupScreen", MenuManager_ActivatePopupScreen, mm, 1);
    Script_RegisterFunction("MenuManager_DeactivatePopuScreen", MenuManager_DeactivatePopuScreen, mm, 0);
    Script_RegisterFunction("MenuManager_SetEnabled", MenuManager_SetEnabled, mm, 1);
    Script_RegisterConstant("MENUMANAGER_QUIT", 10);
    Script_RegisterConstant("MENUMANAGER_QUITTOMAINMENU", 11);
    Script_RegisterConstant("MENUMANAGER_STARTGAME", 13);
    Script_RegisterConstant("MENUMANAGER_IDLE", 9);
    Script_RegisterConstant("MENUMANAGER_RESTARTLEVEL", 12);
    mm->unk26BE = 0;
    if (gUnkC28C61) {
        void *keys = Profile_GetKeys(gUnkC28F80 ? 3 : 0, 0.05f, 0x1.b4e81cp-9f, 0x1.b4e81cp-9f, 0.25f,
                                     0x1.b4e81cp-9f, 0x1.b4e81cp-9f, 0.05f, 0x1.b4e81cp-9f, 0x1.b4e81cp-9f, 0.25f,
                                     0x1.b4e81cp-9f, 0x1.b4e81cp-9f);

        InputPort_Setup(0, Profile_GetKeyCount(keys));
        gUnkBC20A0 = 1;
    }
    Keys_Install();
    mm->unk26BE = 1;
    rect[0] = 0.0f;
    rect[1] = 0.0f;
    rect[2] = Display_GetMode()[1];
    rect[3] = Display_GetMode()[2];
    Scene_Init(mm->unk266C, 1);
    Scene_SetRect(mm->unk266C, rect);
    mm->screen = -1;
    mm->popup = -1;
    mm->screenTask = NULL;
    mm->popupTask = NULL;
    mm->unk26BC = 0;
    mm->active = 0;
    mm->input = 0;
    mm->resPath[0] = 0;
    mm->menuRes = NULL;
    mm->missionRes = NULL;
    mm->langResCount = 0;
    mm->spriteCount = 0;
    mm->strings = NULL;
    mm->fontCount = 0;
    mm->soundsLoaded = 0;
    mm->focus = NULL;
    mm->picCount = 0;
    mm->pointer = Cursor_New(mm);
    mm->backgroundFX = NULL;
    mm->depth = 0;
}
HOOK(0x417260, MenuManager_Create);

/* sends the screen its language changed event and reloads it when the
   language really changed */
void MenuManager_ApplyLanguage(MenuManager *mm)
{
    int old = Profile_GetLanguage();
    Task *w = mm->screenTask;
    TaskFn1 fn;

    if (w) {
        fn = gTaskHandlers1[MenuWindow_GetEvent918()][w->type];
        if (fn)
            fn(w);
    }
    Profile_SetLanguage(mm->language);
    if (mm->language != old)
        MenuManager_Refresh(mm, 1);
}
HOOK(0x417520, MenuManager_ApplyLanguage);

void MenuManager_Refresh(MenuManager *mm, int all)
{
    GameNode *n;
    Task *w;
    TaskFn1 fn;

    if (!all) {
        w = mm->screenTask;
        fn = gTaskHandlers1[MenuWindow_GetEvent91C()][w->type];
        if (fn)
            fn(w);
    } else {
        n = mm->task.head;
        if (n->next && n) {
            do {
                gTaskStack[gTaskStackTop++] = n->next && n->next->next ? n->next : NULL;
                fn = gTaskHandlers1[MenuWindow_GetEvent91C()][((Task *)n)->type];
                if (fn)
                    fn(n);
                n = gTaskStack[--gTaskStackTop];
            } while (n);
        }
    }
    MenuManager_SendEvent910(mm, mm->screenTask);
}
HOOK(0x417590, MenuManager_Refresh);

void MenuManager_SendEvent910(MenuManager *mm, Task *w)
{
    if (w)
        gTaskHandlers1[MenuWindow_GetEvent910()][w->type](w);
}
HOOK(0x417660, MenuManager_SendEvent910);

ScriptValue *MenuManager_RequestScreen(ScriptValue *out, MenuManager *mm, void *args)
{
    if (Script_GetArgInt(args, 1))
        MenuManager_ApplyLanguage(mm);
    mm->screen = Script_GetArgInt(args, 0);
    mm->nextState = 0;
    mm->state = 6;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x4174A0, MenuManager_RequestScreen);

ScriptValue *MenuManager_PushScreen(ScriptValue *out, MenuManager *mm, void *args)
{
    mm->screenStack[mm->depth] = mm->screen;
    mm->depth++;
    mm->screen = Script_GetArgInt(args, 0);
    mm->nextState = 0;
    mm->state = 6;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x417690, MenuManager_PushScreen);

ScriptValue *MenuManager_PopScreen(ScriptValue *out, MenuManager *mm, void *args)
{
    if (Script_GetArgInt(args, 0))
        MenuManager_ApplyLanguage(mm);
    mm->depth--;
    mm->screen = mm->screenStack[mm->depth];
    mm->nextState = 1;
    mm->state = 6;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x417710, MenuManager_PopScreen);

ScriptValue *MenuManager_LeaveMenus(ScriptValue *out, MenuManager *mm, void *args)
{
    int screen = Script_GetArgInt(args, 0);
    int next = Script_GetArgInt(args, 1);

    if (!LevelFlow_IsFailed() || next != 9) {
        mm->screen = screen;
        mm->state = 6;
        mm->nextState = next;
    }
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x417790, MenuManager_LeaveMenus);

ScriptValue *MenuManager_SetLanguage(ScriptValue *out, MenuManager *mm, void *args)
{
    mm->state = 6;
    mm->nextState = 4;
    mm->language = Script_GetArgInt(args, 0);
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x417810, MenuManager_SetLanguage);

ScriptValue *MenuManager_ForceUpdateWindow(ScriptValue *out, MenuManager *mm, void *args)
{
    Task *w = Task_FindById(Script_GetArgInt(args, 0));
    TaskFn1 fn;

    if (w) {
        fn = gTaskHandlers1[MenuWindow_GetEvent914()][w->type];
        if (fn)
            fn(w);
    }
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x417880, MenuManager_ForceUpdateWindow);

ScriptValue *MenuManager_ActivatePopupScreen(ScriptValue *out, MenuManager *mm, void *args)
{
    mm->popup = Script_GetArgInt(args, 0);
    mm->state = 14;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x417900, MenuManager_ActivatePopupScreen);

ScriptValue *MenuManager_DeactivatePopuScreen(ScriptValue *out, MenuManager *mm)
{
    mm->state = 8;
    mm->nextState = 15;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x417960, MenuManager_DeactivatePopuScreen);

ScriptValue *MenuManager_SetEnabled(ScriptValue *out, MenuManager *mm, void *args)
{
    mm->state = Script_GetArgInt(args, 0) == 1 ? 17 : 16;
    return Game_ReturnOne(out, 1, 1.0);
}
HOOK(0x4179B0, MenuManager_SetEnabled);
