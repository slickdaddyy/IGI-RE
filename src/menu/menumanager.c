#include <string.h>

#include "hook.h"
#include "igi.h"
#include "menu/menu.h"
#include "win/wintypes.h"
#include "config/configtypes.h"
#include "config/configvars.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5980, int, crt_stricmp, (const char *a, const char *b));
ORIG(0x4A54D0, char *, crt_strrchr, (const char *s, int c));
ORIG(0x4A5350, void *, crt_fopen, (const char *name, const char *mode));
ORIG(0x4A52A4, int, crt_fseek, (void *f, long offset, int whence));
ORIG(0x4A528A, int, crt_fgetc, (void *f));
ORIG(0x4A5234, int, crt_fclose, (void *f));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401BE0, Task *, Task_FindChild, (void *task, uint16_t type));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x401D80, void, Task_Broadcast, (void *task, uint8_t slot, void *arg));
ORIG(0x402820, void, FramesSet, (int frames));
ORIG(0x402870, int, Flow_GetFps, (void));
ORIG(0x4028A0, void, Flow_SetRequest, (int request));
ORIG(0x406220, void *, Profile_GetActive, (void));
ORIG(0x4062E0, int, Profile_GetLanguage, (void));
ORIG(0x406310, int, Config_GetLanguageCount, (void));
ORIG(0x406320, const char *, Config_GetLanguageName, (int i));
ORIG(0x416A70, int, Game_GetPlayer, (int i));
ORIG(0x416D50, char, Game_IsPaused, (void));
ORIG(0x416D60, void, Game_SetPaused, (char on));
ORIG(0x417590, void, MenuManager_Refresh, (MenuManager *mm, int all));
ORIG(0x417660, void, MenuManager_ArrangeWindow, (MenuManager *mm, void *window));
ORIG(0x424E40, void, Cursor_SetSprite, (Task *pointer, Sprite *sprite));
ORIG(0x424E50, void, Cursor_GetState, (Task *pointer, int *x, int *y, int *buttons, int *prev));
ORIG(0x463170, char, Human_IsDead, (int unk));
ORIG(0x489460, char, LevelFlow_IsFailed, (void));
ORIG(0x48A8A0, void, Keys_Uninstall, (void));
ORIG(0x48A8C0, void, Keys_Clear, (void));
ORIG(0x48A8D0, int, Keys_Pop, (void));
ORIG(0x48A910, int, Keys_GetCount, (void));
ORIG(0x491A30, void, Display_Clear, (void));
ORIG(0x491A90, void, Display_SetMode, (void *mode));
ORIG(0x491E70, void, Display_SetClearColor, (int a, int b, int c));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497450, int, Sound_HashBucket, (const char *name));
ORIG(0x4974A0, void, SoundParams_Init, (void *p));
ORIG(0x4975F0, void, Scene_Free, (void *p));
ORIG(0x497620, void, Scene_Select, (void *p, int on));
ORIG(0x4B5D80, void, Resource_ForEach, (const char *path, void *fn, void *arg));
ORIG(0x4B5F00, void *, Resource_Load, (const char *path, int *size));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));
ORIG(0x4B66A0, void, Sprite_Free, (Sprite *s));
ORIG(0x4B6720, int, Sprite_Load, (Sprite *s, const char *name));
ORIG(0x4B6DB0, Text *, Text_Load, (const char *name));
ORIG(0x4B6E30, void, Text_Delete, (Text *t));
ORIG(0x4B80B0, void *, Script_Load, (const char *name));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4B8930, void, Script_SetSymbolContext, (const char *name, void *value));
ORIG(0x4B9120, int, Expr_EvalInt, (void *e));
ORIG(0x4E6750, void, Sound_LoadResource, (const char *res));
ORIG(0x4E67D0, void, Sound_FreeResource, (const char *res));
ORIG(0x4E6D60, void, SoundHandle_Play, (int *music));
ORIG(0x4E7130, void, Sound_StopAll, (void));
ORIG(0x4E7180, void, Sound_PlayName, (const char *name, int unk1, int unk2));
ORIG(0x4E7540, void, Screen_RequestMode, (void *mode, int unk));
ORIG(0x4F0E50, void, ScriptInit, (const char *name, int unk1, int unk2, int unk3));
ORIG(0x4F1030, MenuWindow *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4C1780, void, Debug_Log, (void));

typedef struct ResourceInfo {
    const char *name;
} ResourceInfo;

extern uint16_t gMenuManagerType;
extern int gMenuKeepScreen;
extern MenuInput gMenuLastInput;
extern int gMenuDemoTimer;
extern void *gMenuManagerParams;
extern MenuWindow *gMenuLastScreen;
extern int gMenuIdleTimer;
extern MenuWindow *gMenuLastFocus;
extern int gDisplayClearColor[];
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];
extern Mouse gMouse;

#define SOUNDS_RES "LOCAL:menusystem/sound/sounds.res"
#define KEY_HIT(k) ((gKeys[k] & 0x80) && !(gKeysPrev[k] & 0x80))

enum {
    MENU_OPEN,
    MENU_OPEN_KEEP_LANGUAGE,
    MENU_RUN,
    MENU_PAUSED,
    MENU_RELOAD,
    MENU_FADE_IN,
    MENU_FADE_OUT,
    MENU_POPUP_FADE_IN,
    MENU_POPUP_FADE_OUT,
    MENU_IDLE,
    MENU_QUIT,
    MENU_QUIT_TO_MAIN,
    MENU_RESTART_LEVEL,
    MENU_START_GAME,
    MENU_OPEN_POPUP,
    MENU_SHOW,
    MENU_DISABLED,
    MENU_ENABLED,
};

void MenuManager_FreeResources(MenuManager *mm);
void MenuManager_LoadResources(MenuManager *mm);
void MenuManager_AddResource(ResourceInfo *info, MenuManager *mm);
void MenuManager_LoadLanguageRes(MenuManager *mm, const char *file);
void MenuManager_PlayMusic(MenuManager *mm);
void MenuManager_ReadInput(MenuManager *mm);
void MenuManager_SetFocus(MenuManager *mm, int i);
void MenuManager_OpenScreen(MenuManager *mm, int screen, int checkLanguage);
void MenuManager_CheckLanguage(MenuManager *mm);
void MenuManager_CollectItems(MenuManager *mm, MenuWindow *root);
void MenuManager_AddItems(MenuManager *mm, Task *t);
void MenuManager_OpenPopup(MenuManager *mm);
void MenuManager_ClosePopup(MenuManager *mm);
void MenuManager_Free(void *task);
Sprite *MenuManager_FindSprite(MenuManager *mm, const char *name);
void MenuManager_Reset(MenuManager *mm);

static void run(void *task)
{
    ((void (*)(void *))HANDLER(1, ((Task *)task)->type))(task);
}

void MenuManager_Delete(MenuManager *mm)
{
    TaskFn fn;

    if (mm->ownsDisplay)
        Sound_StopAll();
    if (mm->unk26BE) {
        Keys_Uninstall();
        mm->unk26BE = 0;
    }
    MenuManager_Free(mm->pointer);
    MenuManager_FreeResources(mm);
    Scene_Free(mm->unk266C);
    if (mm->savedFrames > -1) {
        int a = gDisplayClearColor[0], b = gDisplayClearColor[1], c = gDisplayClearColor[2];

        Display_SetClearColor(0, 0, 0);
        Display_Clear();
        Display_SetClearColor(a, b, c);
        FramesSet(mm->savedFrames);
    }
    if (Task_GetId(mm) >= 0) {
        fn = HANDLER(Level_GetDeactivateEvent(), mm->task.type);
        if (fn)
            ((void (*)(void *))fn)(mm);
    }
    Script_UnregisterSymbol("MenuManager_RequestScreen");
    Script_UnregisterSymbol("MenuManager_LeaveMenus");
    Script_UnregisterSymbol("MenuManager_SetLanguage");
    Script_UnregisterSymbol("MenuManager_PushScreen");
    Script_UnregisterSymbol("MenuManager_PopScreen");
    Script_UnregisterSymbol("MenuManager_ForceUpdateWindow");
    Script_UnregisterSymbol("MenuManager_ActivatePopupScreen");
    Script_UnregisterSymbol("MenuManager_DeactivatePopuScreen");
    Script_UnregisterSymbol("MenuManager_SetEnabled");
    Script_UnregisterSymbol("MENUMANAGER_QUIT");
    Script_UnregisterSymbol("MENUMANAGER_QUITTOMAINMENU");
    Script_UnregisterSymbol("MENUMANAGER_IDLE");
    Script_UnregisterSymbol("MENUMANAGER_RESTARTLEVEL");
    Script_UnregisterSymbol("MENUMANAGER_STARTGAME");
}
HOOK(0x417A20, MenuManager_Delete);

void MenuManager_FreeResources(MenuManager *mm)
{
    MenuString *s, *next;
    int i;

    for (i = 0; i < mm->spriteCount; i++)
        Sprite_Free(&mm->sprites[i].sprite);
    for (i = 0; i < mm->picCount; i++)
        Text_Delete(mm->pics[i].text);
    for (i = 0; i < mm->fontCount; i++)
        Sprite_Free(&mm->fonts[i].sprite);
    for (s = mm->strings; s; s = next) {
        next = s->next;
        Mem_PushMode(1);
        Mem_Free(s);
        Mem_PopMode();
    }
    mm->spriteCount = 0;
    mm->picCount = 0;
    mm->fontCount = 0;
    mm->strings = NULL;
    if (mm->soundsLoaded) {
        Mem_PushMode(1);
        Sound_FreeResource(SOUNDS_RES);
        mm->soundsLoaded = 0;
        Mem_PopMode();
    }
    if (mm->menuRes) {
        Resource_Flush(mm->menuRes);
        mm->menuRes = NULL;
    }
    if (mm->missionRes) {
        Resource_Flush(mm->missionRes);
        mm->missionRes = NULL;
    }
    for (i = 0; i < mm->langResCount; i++) {
        Resource_Flush(mm->langRes[i]);
        mm->langRes[i] = NULL;
    }
    mm->langResCount = 0;
}
HOOK(0x417B80, MenuManager_FreeResources);

/* params changed; *reload is set when the resources must be loaded again */
void MenuManager_Update(MenuManager *mm, const char *reload)
{
    mm->screenTask = Task_FindById(mm->screen);
    if (reload && *reload)
        MenuManager_LoadResources(mm);
    else
        MenuManager_ArrangeWindow(mm, mm->screenTask);
    Cursor_SetSprite(mm->pointer, MenuManager_FindSprite(mm, "pointer.spr"));
    mm->language = Profile_GetLanguage();
}
HOOK(0x417CF0, MenuManager_Update);

void MenuManager_LoadResources(MenuManager *mm)
{
    char dir[256];
    char *slash;

    strcpy(dir, mm->resPath);
    slash = crt_strrchr(dir, '/');
    if (slash)
        *slash = 0;
    MenuManager_FreeResources(mm);
    Sound_LoadResource(SOUNDS_RES);
    mm->soundsLoaded = 1;
    mm->menuRes = Resource_Load(mm->resPath, NULL);
    Resource_ForEach(dir, MenuManager_AddResource, mm);
    mm->missionRes = Resource_Load("LOCAL:menusystem/missionsprites.res", NULL);
    Resource_ForEach(dir, MenuManager_AddResource, mm);
    MenuManager_LoadLanguageRes(mm, "menusystem.res");
    MenuManager_LoadLanguageRes(mm, "missions.res");
}
HOOK(0x417D50, MenuManager_LoadResources);

void MenuManager_AddResource(ResourceInfo *info, MenuManager *mm)
{
    const char *name = info->name;
    const char *ext = name + strlen(name) - 3;
    MenuSprite *sprite = &mm->sprites[mm->spriteCount];
    MenuPic *pic = &mm->pics[mm->picCount];
    MenuSprite *font = &mm->fonts[mm->fontCount];
    int kind, i;

    if (!crt_stricmp(ext, "SPR"))
        kind = 0;
    else if (!crt_stricmp(ext, "PIC"))
        kind = 1;
    else if (!crt_stricmp(ext, "FNT"))
        kind = 2;
    else
        kind = crt_stricmp(ext, "SND") ? 4 : 3;
    for (i = strlen(name) - 1; i > 0; i--) {
        char c = name[i - 1];

        if (c == '/' || c == ':' || c == '\\')
            break;
    }
    if (kind == 0) {
        strcpy(sprite->name, info->name + i);
        Sprite_Load(&sprite->sprite, name);
        mm->spriteCount++;
    } else if (kind == 1) {
        strcpy(pic->name, info->name + i);
        pic->text = Text_Load(name);
        mm->picCount++;
    } else if (kind == 2) {
        strcpy(font->name, info->name + i);
        Sprite_Load(&font->sprite, name);
        mm->fontCount++;
    }
}
HOOK(0x417E20, MenuManager_AddResource);

void MenuManager_LoadLanguageRes(MenuManager *mm, const char *file)
{
    char path[256];
    int i;

    for (i = 0; i < Config_GetLanguageCount(); i++) {
        crt_sprintf(path, "LOCAL:language/%s/%s", Config_GetLanguageName(i), file);
        mm->langRes[mm->langResCount] = Resource_Load(path, NULL);
        mm->langResCount++;
    }
}
HOOK(0x417FF0, MenuManager_LoadLanguageRes);

void MenuManager_GetParams(void *task, void **out)
{
    *out = gMenuManagerParams;
}
HOOK(0x418070, MenuManager_GetParams);

static int input_same(const MenuInput *a, const MenuInput *b)
{
    const int *x = (const int *)a, *y = (const int *)b;
    int i;

    for (i = 0; i < 5; i++)
        if (x[i] != y[i])
            return 0;
    return 1;
}

static void idle(MenuManager *mm)
{
    int hit = KEY_HIT(1);

    if (LevelFlow_IsFailed() && !Human_IsDead(Game_GetPlayer(0))) {
        if (gMenuDemoTimer > 150) {
            hit = 1;
            gMenuDemoTimer = 0;
        } else {
            gMenuDemoTimer++;
        }
    }
    if (Human_IsDead(Game_GetPlayer(0))) {
        hit |= KEY_HIT(0x39);
        hit |= KEY_HIT(0x1C);
    }
    if (hit) {
        mm->screenTask = NULL;
        MenuManager_Reset(mm);
        if (!Game_IsPaused())
            Game_SetPaused(1);
    } else if (Game_IsPaused()) {
        Game_SetPaused(0);
    }
}

void MenuManager_Run(MenuManager *mm)
{
    MenuInput *in = &mm->input;
    int buttons, prev;

    if (mm->ownsDisplay && mm->soundsLoaded)
        MenuManager_PlayMusic(mm);
    if (mm->backgroundFX)
        run(mm->backgroundFX);
    run(mm->pointer);
    if (mm->idleTimeout) {
        if (input_same(in, &gMenuLastInput) && mm->focus == gMenuLastFocus && mm->screenTask == gMenuLastScreen &&
            !Keys_GetCount() && !mm->busy && !mm->depth) {
            if (gMenuIdleTimer > 3600) {
                MenuManager_Free(mm);
                Flow_SetRequest(8);
                gMenuIdleTimer = 0;
                return;
            }
            gMenuIdleTimer++;
        } else {
            gMenuIdleTimer = 0;
        }
        gMenuLastInput = *in;
        gMenuLastFocus = mm->focus;
        gMenuLastScreen = mm->screenTask;
        mm->busy = 0;
    }
    if (mm->state != MENU_IDLE)
        gMenuDemoTimer = 0;
    switch (mm->state) {
    case MENU_OPEN:
    case MENU_OPEN_KEEP_LANGUAGE:
        MenuManager_OpenScreen(mm, mm->screen, mm->state == MENU_OPEN);
        goto faded_out;
    case MENU_RUN:
        MenuManager_ReadInput(mm);
        Cursor_GetState(mm->pointer, &in->x, &in->y, &buttons, &prev);
        in->button = buttons;
        in->pressed = (buttons != prev) & (unsigned char)buttons;
        in->released = buttons == 0 && buttons != prev;
        in->x += 320 - (Display_GetMode()[1] >> 1);
        in->y += 240 - (Display_GetMode()[2] >> 1);
        break;
    case MENU_PAUSED:
        in->pressed = 0;
        in->released = 0;
        in->button = 0;
        break;
    case MENU_RELOAD:
        MenuManager_Refresh(mm, 0);
    faded_out:
        mm->fade = 0;
        mm->fadeShown = 0;
        mm->fadeShownPrev = 0;
        mm->state = MENU_FADE_IN;
        return;
    case MENU_FADE_IN:
    case MENU_POPUP_FADE_IN:
        if (mm->fade != 16)
            mm->fade += 2 - (mm->fastFade != 0);
        if (mm->fadeShownPrev == 16) {
            mm->state = MENU_RUN;
            Keys_Clear();
        }
        return;
    case MENU_FADE_OUT:
    case MENU_POPUP_FADE_OUT:
        if (mm->fade)
            mm->fade -= 2 - (mm->fastFade != 0);
        if (mm->fadeShownPrev == 0) {
            mm->state = mm->nextState;
            MenuManager_ClosePopup(mm);
        }
        return;
    case MENU_IDLE:
        idle(mm);
        return;
    case MENU_QUIT:
        MenuManager_Free(mm);
        Flow_SetRequest(1);
        return;
    case MENU_QUIT_TO_MAIN:
        Flow_SetRequest(6);
        mm->state = MENU_IDLE;
        return;
    case MENU_RESTART_LEVEL:
        Flow_SetRequest(5);
        mm->state = MENU_IDLE;
        return;
    case MENU_START_GAME:
        MenuManager_Free(mm);
        Flow_SetRequest(4);
        return;
    case MENU_OPEN_POPUP:
        MenuManager_OpenPopup(mm);
        mm->fade = 0;
        mm->fadeShownPrev = 0;
        mm->fadeShown = 0;
        mm->state = MENU_POPUP_FADE_IN;
        return;
    case MENU_SHOW:
        mm->state = MENU_RUN;
        mm->fade = 16;
        mm->fadeShownPrev = 16;
        mm->fadeShown = 16;
        return;
    case MENU_DISABLED:
        return;
    case MENU_ENABLED:
        mm->state = MENU_IDLE;
        return;
    }
    if (mm->popupTask) {
        run(mm->popupTask);
        return;
    }
    if (mm->screenTask)
        run(mm->screenTask);
}
HOOK(0x418080, MenuManager_Run);

void MenuManager_PlayMusic(MenuManager *mm)
{
    if (!mm->music[2]) {
        mm->music[0] = Sound_HashBucket("menu_music");
        mm->music[1] = Sound_HashName("menu_music");
        mm->music[3] = 0x30;
        SoundParams_Init(mm->unk2840);
    }
    SoundHandle_Play(mm->music);
}
HOOK(0x418520, MenuManager_PlayMusic);

void MenuManager_ReadInput(MenuManager *mm)
{
    MenuInput *in = &mm->input;
    int i = mm->itemIndex;

    in->back = 0;
    in->select = 0;
    if (gMouse.buttons & 0x100)
        in->wheel = -1;
    else if (gMouse.buttons & 0x200)
        in->wheel = 1;
    else
        in->wheel = 0;
    if (!mm->inputEnabled)
        return;
    while (Keys_GetCount()) {
        int key = Keys_Pop();

        /* horizontal popups turn left/right into up/down */
        if (mm->popupTask && DialogWindow_IsHorizontal(mm->popupTask)) {
            switch (key) {
            case -4:
                key = -2;
                break;
            case -3:
                key = -1;
                break;
            case -2:
                key = -4;
                break;
            case -1:
                key = -3;
                break;
            }
        }
        switch (key) {
        case -11:
            in->select = 1;
            break;
        case -8:
            in->back = 1;
            break;
        case -7:
        case -2:
        case -1:
            if (mm->focus)
                mm->focus->key = key;
            break;
        case -4:
            if (++i >= mm->itemCount)
                i = 0;
            MenuManager_SetFocus(mm, i);
            Sound_PlayName("menu_high", 0, 0);
            break;
        case -3:
            if (--i < 0)
                i = mm->itemCount - 1;
            MenuManager_SetFocus(mm, i);
            Sound_PlayName("menu_high", 0, 0);
            break;
        }
    }
}
HOOK(0x418580, MenuManager_ReadInput);

void MenuManager_SetFocus(MenuManager *mm, int i)
{
    if (mm->focus)
        mm->focus->focused = 0;
    mm->itemIndex = i;
    mm->focus = mm->items[i];
    if (mm->focus)
        mm->focus->focused = 1;
}
HOOK(0x4186E0, MenuManager_SetFocus);

void MenuManager_OpenScreen(MenuManager *mm, int screen, int checkLanguage)
{
    mm->screen = screen;
    mm->screenTask = Task_FindById(screen);
    if (!mm->screenTask)
        return;
    MenuManager_ArrangeWindow(mm, mm->screenTask);
    if (checkLanguage)
        MenuManager_CheckLanguage(mm);
    MenuManager_CollectItems(mm, mm->screenTask);
    mm->input.pressed = 0;
    mm->input.released = 0;
    mm->input.button = 0;
    mm->input.entered = 1;
    run(mm->screenTask);
    mm->input.entered = 0;
}
HOOK(0x418710, MenuManager_OpenScreen);

void MenuManager_CheckLanguage(MenuManager *mm)
{
    int language = mm->language;
    MenuWindow *w = mm->screenTask;
    TaskFn fn = HANDLER(MenuWindow_GetLoadEvent(), w->task.type);

    if (fn)
        ((void (*)(void *))fn)(w);
    mm->language = Profile_GetLanguage();
    if (mm->language != language)
        MenuManager_Refresh(mm, 1);
}
HOOK(0x418790, MenuManager_CheckLanguage);

void MenuManager_CollectItems(MenuManager *mm, MenuWindow *root)
{
    mm->itemIndex = 0;
    mm->items[0] = NULL;
    mm->itemCount = 0;
    mm->inputEnabled = 1;
    if (mm->screenTask)
        MenuManager_AddItems(mm, &root->task);
    MenuManager_SetFocus(mm, 0);
}
HOOK(0x4187F0, MenuManager_CollectItems);

void MenuManager_AddItems(MenuManager *mm, Task *t)
{
    Task *c;

    if (TaskType_IsA(t->type, MenuWindow_GetType()) && ((MenuWindow *)t)->selectable) {
        mm->items[mm->itemCount++] = (MenuWindow *)t;
        return;
    }
    for (c = t->head; c->next; c = c->next)
        MenuManager_AddItems(mm, c);
}
HOOK(0x418840, MenuManager_AddItems);

void MenuManager_OpenPopup(MenuManager *mm)
{
    MenuWindow *w;
    TaskFn fn;

    w = mm->popupTask = Task_FindById(mm->popup);
    if (!w)
        return;
    fn = HANDLER(MenuWindow_GetLoadEvent(), w->task.type);
    if (fn)
        ((void (*)(void *))fn)(w);
    MenuManager_ArrangeWindow(mm, mm->popupTask);
    MenuManager_CollectItems(mm, mm->popupTask);
    mm->input.pressed = 0;
    mm->input.released = 0;
    mm->input.button = 0;
    mm->input.entered = 1;
    run(mm->popupTask);
    mm->input.entered = 0;
}
HOOK(0x4188B0, MenuManager_OpenPopup);

void MenuManager_ClosePopup(MenuManager *mm)
{
    if (!mm->popupTask)
        return;
    mm->popupTask = NULL;
    if (mm->screenTask) {
        MenuManager_ArrangeWindow(mm, mm->screenTask);
        MenuManager_CollectItems(mm, mm->screenTask);
    }
}
HOOK(0x418940, MenuManager_ClosePopup);

static void draw(void *task)
{
    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), ((Task *)task)->type))(task);
}

void MenuManager_Draw(MenuManager *mm)
{
    if (mm->screenTask && mm->state != MENU_IDLE) {
        Scene_Select(mm->unk266C, 1);
        if (mm->state == MENU_RUN)
            draw(mm->pointer);
        if (mm->popupTask) {
            mm->drawFade = mm->fade;
            draw(mm->popupTask);
        }
        if (mm->screenTask) {
            int s = mm->state;

            if (s == MENU_POPUP_FADE_IN || s == MENU_POPUP_FADE_OUT || s == MENU_SHOW)
                mm->drawFade = 16;
            else
                mm->drawFade = mm->fade;
            draw(mm->screenTask);
        }
        Debug_Log();
    }
    mm->fadeShownPrev = mm->fadeShown;
    mm->fadeShown = mm->fade;
}
HOOK(0x418970, MenuManager_Draw);

void MenuManager_Activate(MenuManager *mm)
{
    if (!mm->active)
        mm->active = 1;
}
HOOK(0x418A80, MenuManager_Activate);

void MenuManager_Deactivate(MenuManager *mm)
{
    if (mm->active)
        mm->active = 0;
}
HOOK(0x418AA0, MenuManager_Deactivate);

void MenuManager_Shutdown(void)
{
    QTaskList_Free(gMenuManagerParams);
    TaskType_Free(gMenuManagerType);
    gMenuManagerType = 0x181;
}
HOOK(0x418AC0, MenuManager_Shutdown);

uint16_t MenuManager_GetType(void)
{
    return gMenuManagerType;
}
HOOK(0x418AF0, MenuManager_GetType);

MenuManager *MenuManager_New(Task *parent, const char *script, char ownsDisplay, char start, char idleTimeout)
{
    MenuManager *mm;
    void *s, *f;
    char reload;

    if (ownsDisplay) {
        PlayerProfile *p = Profile_GetActive();
        DisplayMode mode;
        const char *str;

        memset(&mode, 0, sizeof(mode));
        mode.renderMode = Direct3D_GetRenderMode();
        mode.width = 640;
        mode.height = 480;
        mode.depth = 16;
        mode.backBuffers = 1;
        mode.renderMode = Direct3D_GetRenderMode();
        mode.unk16 = 1;
        mode.unk14 = 1;
        str = p->gfx.device_desc;
        if (!str)
            str = gEmptyString;
        strcpy(mode.desc, str);
        str = p->gfx.device;
        if (!str)
            str = gEmptyString;
        strcpy(mode.device, str);
        Screen_RequestMode(&mode, 0);
        Display_SetMode(&mode);
    }
    s = Script_Load(script);
    if (!s) {
        Error_Show("Failed to load menumanager: '%s'", script);
        for (;;)
            ;
    }
    ScriptInit(script, 1, 0, 0);
    Script_SetSymbolContext("Task_New", parent);
    Script_Run(s);
    Script_Free(s);
    mm = (MenuManager *)Task_FindChild(parent, MenuManager_GetType());
    strcpy(mm->scriptName, script);
    reload = 1;
    Task_Broadcast(parent, Param_GetChangedEvent(), &reload);
    mm->state = start ? MENU_OPEN : MENU_IDLE;
    if (!gMenuKeepScreen)
        mm->screen = 900;
    gMenuKeepScreen = 0;
    if (ownsDisplay) {
        mm->backgroundFX = BackgroundFX_New(&mm->task);
        mm->savedFrames = Flow_GetFps();
        FramesSet(60);
        mm->ownsDisplay = 1;
    } else {
        mm->savedFrames = -1;
        mm->ownsDisplay = 0;
    }
    mm->music[2] = 0;
    mm->idleTimeout = idleTimeout;
    /* the first byte of this file must be '@' */
    f = crt_fopen(".\\ymbe.afp", "rb");
    if (f) {
        int ok;

        crt_fseek(f, 0, 0);
        ok = crt_fgetc(f) == '@';
        crt_fclose(f);
        if (ok)
            return mm;
    }
    Error_Show("EXCEPTION FAULT: 0x000004");
    for (;;)
        ;
}
HOOK(0x418B00, MenuManager_New);

void MenuManager_Free(void *task)
{
    Task_Delete(task);
}
HOOK(0x418D70, MenuManager_Free);

Text *MenuManager_FindPic(MenuManager *mm, const char *name)
{
    int i;

    for (i = 0; i < mm->picCount; i++)
        if (!crt_stricmp(name, mm->pics[i].name))
            return mm->pics[i].text;
    return NULL;
}
HOOK(0x418D80, MenuManager_FindPic);

Sprite *MenuManager_FindSprite(MenuManager *mm, const char *name)
{
    int i;

    for (i = 0; i < mm->spriteCount; i++)
        if (!crt_stricmp(name, mm->sprites[i].name))
            return &mm->sprites[i].sprite;
    return NULL;
}
HOOK(0x418DE0, MenuManager_FindSprite);

Sprite *MenuManager_FindFont(MenuManager *mm, const char *name)
{
    int i;

    for (i = 0; i < mm->fontCount; i++)
        if (!crt_stricmp(name, mm->fonts[i].name))
            return &mm->fonts[i].sprite;
    return NULL;
}
HOOK(0x418E40, MenuManager_FindFont);

/* text of a language resource, or "RM: name" when it is missing */
const char *MenuManager_GetString(MenuManager *mm, const char *name)
{
    char path[256];
    MenuString *s;

    crt_sprintf(path, "LOCAL:%s/%s", Config_GetLanguageName(mm->language), name);
    if (Resource_Find(path))
        return Resource_Load(path, NULL);
    for (s = mm->strings; s; s = s->next)
        if (!crt_stricmp(path, s->name))
            return s->text;
    Mem_PushMode(1);
    s = Mem_Alloc(sizeof(MenuString), 4);
    crt_sprintf(s->text, "RM: %s", name);
    strcpy(s->name, path);
    Mem_PopMode();
    s->next = mm->strings;
    mm->strings = s;
    return s->text;
}
HOOK(0x418EA0, MenuManager_GetString);

MenuInput *MenuManager_GetInput(MenuManager *mm)
{
    return &mm->input;
}
HOOK(0x418FB0, MenuManager_GetInput);

void MenuManager_Pause(MenuManager *mm)
{
    mm->state = MENU_PAUSED;
}
HOOK(0x418FC0, MenuManager_Pause);

void MenuManager_Resume(MenuManager *mm)
{
    mm->state = MENU_RUN;
}
HOOK(0x418FD0, MenuManager_Resume);

void MenuManager_FocusItem(MenuManager *mm, MenuWindow *item)
{
    int i, n = mm->itemCount;

    for (i = 0; i < n; i++) {
        if (mm->items[i] == item) {
            if (mm->itemIndex != i) {
                MenuManager_SetFocus(mm, i);
                Sound_PlayName("menu_high", 0, 0);
            }
            return;
        }
    }
}
HOOK(0x418FE0, MenuManager_FocusItem);

int MenuManager_GetFade(MenuManager *mm)
{
    return mm->drawFade;
}
HOOK(0x419030, MenuManager_GetFade);

Task *MenuManager_GetBackgroundFX(MenuManager *mm)
{
    return mm->backgroundFX;
}
HOOK(0x419040, MenuManager_GetBackgroundFX);

void MenuManager_Reset(MenuManager *mm)
{
    mm->screen = 900;
    mm->state = MENU_OPEN;
}
HOOK(0x419050, MenuManager_Reset);

typedef struct MenuExpr {
    char unk0[8];
    void *expr;
} MenuExpr;

int MenuExpr_Eval(MenuExpr *e)
{
    int r = 0;

    if (e->expr)
        r = Expr_EvalInt(e->expr);
    return r;
}
HOOK(0x419070, MenuExpr_Eval);

void MenuManager_RefreshScreen(MenuManager *mm)
{
    if (mm->screenTask)
        MenuManager_ArrangeWindow(mm, mm->screenTask);
}
HOOK(0x419090, MenuManager_RefreshScreen);

void MenuManager_DisableInput(MenuManager *mm)
{
    mm->inputEnabled = 0;
}
HOOK(0x4190B0, MenuManager_DisableInput);

void MenuManager_EnableInput(MenuManager *mm)
{
    mm->inputEnabled = 1;
}
HOOK(0x4190C0, MenuManager_EnableInput);

void MenuManager_MarkBusy(MenuManager *mm)
{
    mm->busy = 1;
}
HOOK(0x4190D0, MenuManager_MarkBusy);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(539828) int gMenuKeepScreen INIT(1);
PAD(igd, 544E44, 544E44, 0x2);
DATA(544E44) char str_544E46[1] INIT("");
PAD(igd, 544E44, 544E47, 0x1);
BSS(57BAC8) MenuInput gMenuLastInput;
BSS(57BAC8) int gMenuDemoTimer;
BSS(57BAE4) MenuWindow *gMenuLastScreen;
BSS(57BAE4) int gMenuIdleTimer;
BSS(57BAE4) MenuWindow *gMenuLastFocus;
