#include <math.h>
#include <stdio.h>
#include <windows.h>

#include "igi.h"
#include "hook.h"
#include "flow/flow.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A5350, FILE *, crt_fopen, (const char *path, const char *mode));
ORIG(0x4A52A4, int, crt_fseek, (FILE *f, long offset, int origin));
ORIG(0x4A528A, int, crt_fgetc, (FILE *f));
ORIG(0x4A5234, int, crt_fclose, (FILE *f));
ORIG(0x4010A0, void, Game_Quit, (void));
ORIG(0x405850, void, Config_Read, (const char *path));
ORIG(0x405BD0, void, Config_Write, (const char *path));
ORIG(0x406220, char *, Profile_GetActive, (void));
ORIG(0x406DF0, void, HumanAIConfig_Load, (Flow *flow));
ORIG(0x4061C0, void *, Profile_GetKeys, (int mode, float a0, float a1, float a2, float b0, float b1, float b2,
                                         float c0, float c1, float c2, float d0, float d1, float d2));
ORIG(0x4061E0, int, Profile_GetKeyCount, (void *keys));
ORIG(0x4071E0, void, WeaponConfig_Read, (Flow *flow, const char *path));
ORIG(0x408350, void, GameMaterial_LoadParameters, (void));
ORIG(0x4137A0, void, HumanPlayer_LoadParameters, (void));
ORIG(0x413B70, void, WeaponType_LoadAll, (void));
ORIG(0x415020, int, Game_CheatKeyPressed, (int key));
ORIG(0x4154B0, void, IntroScreen_New, (void *unk20, int a, int b));
ORIG(0x416900, void, Game_New, (Flow *flow));
ORIG(0x416D40, Task *, Game_Get, (void));
ORIG(0x416FE0, void, LevelRestart, (void));
ORIG(0x418B00, void, MenuManager_New, (void *unk20, const char *path, int a, int b, int c));
ORIG(0x48F180, void, AppContext_SetActiveCallback, (void (*fn)(int active), int unk));
ORIG(0x490370, int, Timer_GetMs, (void));
ORIG(0x491220, char, Movie_IsPlaying, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int index));
ORIG(0x4C4930, void, MagicObjLoad, (const char *path));
ORIG(0x4E6030, void, Sound_SetRate, (int fps));
ORIG(0x4E6530, void, SoundSys_NewController, (Flow *flow));
ORIG(0x4E7340, void, Sound_SetSilent, (int first_frames));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4E7580, void *, Screen_New, (Flow *flow));
ORIG(0x4E7B80, void, DebugText_SetCursor, (int a, int b));
ORIG(0x4E7BB0, void, DebugText_Clear, (void));
ORIG(0x4EC0C0, void, AnimTriggerLoad, (const char *path));
ORIG(0x4ED5C0, void *, InputPort_New, (Flow *flow));
ORIG(0x4ED5E0, void, InputPort_Setup, (int port, int count, void *bindings, int analog, float a0, float a1, float a2,
                                        float b0, float b1, float b2, float c0, float c1, float c2, float d0,
                                        float d1, float d2));
ORIG(0x4EDCB0, uint16_t, ModelLODSettingsContainer_GetType, (int unk));
ORIG(0x4EE030, void, PhysicsObjLoad, (const char *path));
ORIG(0x4EE3B0, void, PhysicsObjType_FreeAll, (void));
ORIG(0x4EE940, char, ScreenGrab_IsActive, (void));

#define gFlow VAR(Flow *, 0x567C8C)
#define gFlowType VAR(uint16_t, 0x567C7C)
#define gFlowPauseStart VAR(int, 0x567C78)
#define gFlowPauseDepth VAR(int, 0x567C80)
#define gTaskHandlers ((void (*(*)[TASK_MAX_TYPES])(Task *))0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define gUnk5C8E00 VAR(char, 0x5C8E00)
#define gUnkA4438C ((int *)0xA4438C)
#define gUnkBC20A0 VAR(int, 0xBC20A0)
#define gUnkC28C5C VAR(int, 0xC28C5C)
#define gUnkC28C61 VAR(char, 0xC28C61)
#define gUnkC28F80 VAR(char, 0xC28F80)
#define gScriptNumberType ((void *)0x567C74)

void Flow_Open(void)
{
    gFlowType = TaskType_New("Flow", sizeof(Flow), Task_GetRootType(), 0);
    TaskType_SetHandler(gFlowType, TASK_SLOT_CREATE, (TaskHandler)Flow_CreateHandler);
    TaskType_SetHandler2(gFlowType, TASK_SLOT_RUN, (TaskHandler)Flow_RunHandler);
    TaskType_SetHandler2(gFlowType, TASK_SLOT_DELETE, (TaskHandler)Flow_DeleteHandler);
    Script_RegisterConstant("FLOW_EVENT_QUIT", FLOW_EVENT_QUIT);
    Script_RegisterConstant("FLOW_EVENT_INTRO", FLOW_EVENT_INTRO);
    Script_RegisterConstant("FLOW_EVENT_MAINMENU", FLOW_EVENT_MAINMENU);
    Script_RegisterConstant("FLOW_EVENT_GAME", FLOW_EVENT_GAME);
    Script_RegisterConstant("FLOW_EVENT_RESTART_GAME", FLOW_EVENT_RESTART_GAME);
    Script_RegisterFunction("Flow_RequestEvent", Flow_RequestEvent, NULL, 1);
}
HOOK(0x401FC0, Flow_Open);

void Flow_CreateHandler(Flow *flow)
{
    FILE *f;
    int ok;

    gFlow = flow;
    gFlow->root = Task_New(NULL, Task_GetRootType(), NULL);
    Task_SetUnk567C60((int)gFlow->root);
    FramesSet(30);
    gFlow->ticks_since_render = 0;
    gFlow->time_adjust = 0;
    gFlow->paused_time = 0;
    Config_Read("LOCAL:config.qsc");
    WeaponType_LoadAll();
    HumanPlayer_LoadParameters();
    GameMaterial_LoadParameters();
    MagicObjLoad("LOCAL:magicobj");
    AnimTriggerLoad("LOCAL:animtrigger");
    PhysicsObjLoad("LOCAL:physicsobj");
    WeaponConfig_Read(flow, "LOCAL:weapons/weaponconfig.qsc");
    Task_New(&flow->task, ModelLODSettingsContainer_GetType(0), NULL);
    HumanAIConfig_Load(flow);
    gFlow->event = 0;
    gFlow->request = 0;
    gFlow->unk20 = Screen_New(gFlow);
    gFlow->unk24 = InputPort_New(gFlow);
    SoundSys_NewController(gFlow);
    if (gUnkC28C61) {
        int mode = gUnkC28F80 ? 3 : 0;
        void *keys = Profile_GetKeys(mode, 0.03f, 0.001f, 0.001f, 0.03f, 0.001f, 0.001f, 0.03f, 0.001f, 0.001f, 0.03f,
                                     0.001f, 0.001f);

        /* the original reuses the pushes above as the arguments after keys */
        InputPort_Setup(0, Profile_GetKeyCount(keys), keys, mode, 0.03f, 0.001f, 0.001f, 0.03f, 0.001f, 0.001f, 0.03f,
                        0.001f, 0.001f, 0.03f, 0.001f, 0.001f);
        gUnkBC20A0 = 1;
    }
    AppContext_SetActiveCallback(Flow_FocusChanged, 0);
    /* disc check: first byte of anys.afp must be ':' */
    f = crt_fopen(".\\anys.afp", "rb");
    if (f) {
        crt_fseek(f, 0, SEEK_SET);
        ok = crt_fgetc(f) == 0x3A;
        crt_fclose(f);
        if (ok)
            return;
    }
    Error_Show("EXCEPTION FAULT: 0x000002");
    for (;;)
        ;
}
HOOK(0x402070, Flow_CreateHandler);

void Flow_FocusChanged(int active)
{
    if (active)
        Flow_PauseEnd();
    else
        Flow_PauseBegin();
}
HOOK(0x402240, Flow_FocusChanged);

/* run one handler slot on each direct child of task */
static void run_children(Task *task, int render)
{
    Node *n = task->children.head;
    Task *t;
    int slot;

    if (!n->next)
        return;
    while (n) {
        t = (Task *)n;
        gTaskStack[gTaskStackTop++] = n->next && n->next->next ? (Task *)n->next : NULL;
        slot = render ? Screen_GetDrawEvent() : TASK_SLOT_RUN;
        if (gTaskHandlers[slot][t->type])
            gTaskHandlers[slot][t->type](t);
        n = (Node *)gTaskStack[--gTaskStackTop];
    }
}

void Flow_RunHandler(Flow *flow)
{
    int now = Timer_GetMs();
    int frames, due;

    if (!gUnk5C8E00 && !ScreenGrab_IsActive() && !Movie_IsPlaying()) {
        frames = gFlow->frames;
        due = (int)llrint((double)(gFlow->paused_time + gFlow->start_time) + 1000.0 / gFlow->fps * frames);
        if (due < now && gFlow->ticks_since_render < 11 && (gFlow->need_render || frames > 2)) {
            gUnkA4438C[0] = 0;
            gUnkA4438C[1] = 0;
            gUnkA4438C[2] = 0;
            DebugText_Clear();
            DebugText_SetCursor(0, 0);
            if (frames < 2)
                Flow_PauseBegin();
            Sound_SetSilent(frames < 2);
            run_children(&flow->task, 0);
            run_children(gFlow->root, 0);
            if (frames < 2)
                Flow_PauseEnd();
            gFlow->frames++;
            gFlow->need_render = 0;
            gFlow->ticks_since_render++;
        } else if (!gFlow->need_render) {
            if (gFlow->renders < 2)
                Flow_PauseBegin();
            run_children(&flow->task, 1);
            if (gFlow->renders < 2)
                Flow_PauseEnd();
            gFlow->renders++;
            gFlow->need_render = 1;
            gFlow->ticks_since_render = 0;
        }
    } else {
        run_children(&flow->task, 0);
        run_children(gFlow->root, 0);
        gFlow->frames++;
        if (!Movie_IsPlaying())
            run_children(&flow->task, 1);
        gFlow->renders++;
        DebugText_Clear();
        DebugText_SetCursor(0, 0);
    }

    if (gFlow->request) {
        int event = gFlow->request;

        gFlow->event = event;
        gFlow->request = 0;
        switch (event) {
        case FLOW_EVENT_QUIT:
            Game_Quit();
            return;
        case FLOW_EVENT_INTRO:
            IntroScreen_New(Flow_GetUnk20(), 0, 0);
            break;
        case FLOW_EVENT_LEAVE_GAME:
            Task_Delete(Game_Get());
            gFlow->event = FLOW_EVENT_MAINMENU;
            /* fall through */
        case FLOW_EVENT_MAINMENU:
            MenuManager_New(Flow_GetUnk20(), "LOCAL:menusystem\\mainmenu.qsc", 1, 1, 1);
            break;
        case FLOW_EVENT_GAME:
            Game_New(flow);
            FramesSet(30);
            break;
        case FLOW_EVENT_RESTART_GAME:
            LevelRestart();
            FramesSet(30);
            gFlow->event = FLOW_EVENT_GAME;
            break;
        case FLOW_EVENT_MOVIE_A:
            IntroScreen_New(Flow_GetUnk20(), 1, 0);
            break;
        case FLOW_EVENT_MOVIE_B:
            IntroScreen_New(Flow_GetUnk20(), 0, 1);
            break;
        }
    }
    gUnkC28C5C = gFlow->frames;
    gFlow->start_time += gFlow->time_adjust;
    if (Game_CheatKeyPressed(0x43))
        *(int *)(Profile_GetActive() + 0x560) = 14;
}
HOOK(0x402260, Flow_RunHandler);

void Flow_DeleteHandler(Flow *flow)
{
    (void)flow;
    AppContext_SetActiveCallback(NULL, 0);
    Config_Write("LOCAL:config.qsc");
    PhysicsObjType_FreeAll();
    Task_Delete(gFlow->root);
    gFlow = NULL;
}
HOOK(0x402740, Flow_DeleteHandler);

ScriptValue *Flow_RequestEvent(ScriptValue *ret, void *unused, void *args)
{
    (void)unused;
    Flow_SetRequest(Script_GetArgInt(args, 0));
    ret->i = 1;
    ret->d = 1.0;
    ret->type = gScriptNumberType;
    return ret;
}
HOOK(0x402780, Flow_RequestEvent);

void Flow_Close(void)
{
    TaskType_Free(gFlowType);
}
HOOK(0x4027E0, Flow_Close);

uint16_t Flow_GetType(void)
{
    return gFlowType;
}
HOOK(0x4027F0, Flow_GetType);

void Flow_New(Task *parent)
{
    Task_New(parent, gFlowType, NULL);
}
HOOK(0x402800, Flow_New);

void FramesSet(int fps)
{
    gFlow->start_time = Timer_GetMs();
    gFlow->paused_time = 0;
    gFlow->frames = 0;
    gFlow->renders = 0;
    gFlow->fps = fps;
    gFlow->need_render = 1;
    Sound_SetRate(fps);
}
HOOK(0x402820, FramesSet);

int Flow_GetFps(void)
{
    return gFlow->fps;
}
HOOK(0x402870, Flow_GetFps);

Flow *Flow_Get(void)
{
    return gFlow;
}
HOOK(0x402880, Flow_Get);

void *Flow_GetUnk20(void)
{
    return gFlow->unk20;
}
HOOK(0x402890, Flow_GetUnk20);

void Flow_SetRequest(int event)
{
    gFlow->request = event;
}
HOOK(0x4028A0, Flow_SetRequest);

int Flow_GetFrames(void)
{
    return gFlow->frames;
}
HOOK(0x4028B0, Flow_GetFrames);

int Flow_GetRenders(void)
{
    return gFlow->renders;
}
HOOK(0x4028C0, Flow_GetRenders);

void Flow_PauseBegin(void)
{
    if (!gFlowPauseDepth)
        gFlowPauseStart = Timer_GetMs();
    gFlowPauseDepth++;
}
HOOK(0x4028D0, Flow_PauseBegin);

void Flow_PauseEnd(void)
{
    gFlowPauseDepth--;
    if (!gFlowPauseDepth) {
        gFlow->paused_time += Timer_GetMs() - gFlowPauseStart;
        gFlowPauseStart = 0;
    }
}
HOOK(0x4028F0, Flow_PauseEnd);

/* The supported exe carries a no-CD patch here: the drive letter, the
   CD-ROM type test and the path setup are NOPed out, so every pass checks
   the game folder. */
void Flow_CheckCD(void)
{
    char root[4];
    FILE *f;
    int ok;
    char drive;

    *(uint32_t *)root = VAR(uint32_t, 0x536408);
    for (drive = 'A'; drive <= 'Z'; drive++) {
        GetDriveTypeA(root);
        f = crt_fopen(".\\ymbe.afp", "rb");
        if (f) {
            crt_fseek(f, 0, SEEK_SET);
            ok = crt_fgetc(f) == 0x40;
            crt_fclose(f);
            if (ok)
                return;
        }
    }
    Error_Show("IGI CD not found. Please insert CD in drive.");
    for (;;)
        ;
}
HOOK(0x402F50, Flow_CheckCD);
