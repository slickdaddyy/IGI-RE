#include "hook.h"
#include "mission/mission.h"

FN_EXTERN(MenuWindow_Draw);
FN_EXTERN(SubLevel_Run);
FN_EXTERN(Task_Nothing);

/* "GameScreen" task (0x28 bytes): holds the 3D view of the level and the status screen */
typedef struct GameScreen {
    Task task;
    int unk20;
    Task *view;
} GameScreen;

_Static_assert(sizeof(GameScreen) == 0x28, "GameScreen");

ORIG(0x4E7AC0, void, DebugText_New, (void *task, int unk));
ORIG(0x4E8110, Task *, Viewport_New, (void *parent, float x, float y, float w, float h, void *world, void *cam,
                                  int unk8, int unk9, int unk10, int unk11, float unk12));
ORIG(0x4E8370, void, TerraDrawTask_New, (void *view, void *world));
ORIG(0x4E9430, void, FadeOutStart_New, (Task *view));
ORIG(0x4E9460, void, FadeOutEnd_New, (Task *view));
ORIG(0x4E9500, void, GlowTask_New, (Task *view));

#define Task_RunChildren (FN(0x488700, SubLevel_Run))
#define Task_Nothing (FN(0x4D3710, Task_Nothing))
#define MenuWindow_Draw (FN(0x424280, MenuWindow_Draw))

extern uint16_t gGameScreenType;
extern Task *gGameStatusScreen;
extern char gUnk5C8E00;

void GameScreen_Create(GameScreen *s);

void GameScreen_Init(void)
{
    gGameScreenType = TaskType_New("GameScreen", sizeof(GameScreen), Task_GetRootType(), 0, 0);
    TaskType_SetHandler(gGameScreenType, 0, GameScreen_Create);
    TaskType_SetHandler2(gGameScreenType, 1, Task_RunChildren);
    TaskType_SetHandler2(gGameScreenType, 2, Task_Nothing);
    TaskType_SetHandler2(gGameScreenType, Screen_GetDrawEvent(), MenuWindow_Draw);
}
HOOK(0x4851B0, GameScreen_Init);

void GameScreen_Create(GameScreen *s)
{
    float x, y, w, h;
    char *game;
    Task *v;

    if (gUnk5C8E00) {
        h = 0.7f;
        w = 0.7f;
        y = 0.3f;
        x = 0.3f;
    } else {
        h = 1.0f;
        w = 1.0f;
        y = 0.0f;
        x = 0.0f;
    }
    game = Game_Get();
    v = Viewport_New(s, x, y, w, h, Game_GetWorld(), game + 0x60, 0, 0, 0, 0, 1.0f);
    s->view = v;
    FadeOutStart_New(v);
    TerraDrawTask_New(v, Game_GetWorld());
    GlowTask_New(v);
    FadeOutEnd_New(v);
    gGameStatusScreen = StatusScreen_New(s);
    DebugText_New(s, 1);
}
HOOK(0x485220, GameScreen_Create);

void GameScreen_Close(void)
{
    TaskType_Free(gGameScreenType);
}
HOOK(0x4852F0, GameScreen_Close);

Task *GameScreen_New(void *parent)
{
    return Task_New(parent, gGameScreenType, NULL);
}
HOOK(0x485300, GameScreen_New);

Task *GameScreen_GetView(GameScreen *s)
{
    return s->view;
}
HOOK(0x485320, GameScreen_GetView);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(5C8A20) Task *gGameStatusScreen;
PAD(igb, 5C8A20, 5C8A24, 0x4);
BSS(5C8A20) uint16_t gGameScreenType;
PAD(igb, 5C8A20, 5C8A2A, 0x2);
BSS(5C8E00) char gUnk5C8E00;
PAD(igb, 5C8E00, 5C8E01, 0x3);
