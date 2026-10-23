#include "hook.h"
#include "mission/mission.h"

/* "GameScreen" task (0x28 bytes): holds the 3D view of the level and the status screen */
typedef struct GameScreen {
    Task task;
    int unk20;
    Task *view;
} GameScreen;

_Static_assert(sizeof(GameScreen) == 0x28, "GameScreen");

ORIG(0x4E7AC0, void, sub_4E7AC0, (void *task, int unk));
ORIG(0x4E8110, Task *, View_New, (void *parent, float x, float y, float w, float h, void *world, void *cam,
                                  int unk8, int unk9, int unk10, int unk11, float unk12));
ORIG(0x4E8370, void, View_SetWorld, (void *view, void *world));
ORIG(0x4E9430, void, sub_4E9430, (Task *view));
ORIG(0x4E9460, void, sub_4E9460, (Task *view));
ORIG(0x4E9500, void, sub_4E9500, (Task *view));

#define Task_RunChildren ((void *)0x488700)
#define Task_Nothing ((void *)0x4D3710)
#define MenuWindow_Draw ((void *)0x424280)

#define gGameScreenType VAR(uint16_t, 0x5C8A28)
#define gGameStatusScreen VAR(Task *, 0x5C8A20)
#define gUnk5C8E00 VAR(char, 0x5C8E00)

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
    v = View_New(s, x, y, w, h, Game_GetWorld(), game + 0x60, 0, 0, 0, 0, 1.0f);
    s->view = v;
    sub_4E9430(v);
    View_SetWorld(v, Game_GetWorld());
    sub_4E9500(v);
    sub_4E9460(v);
    gGameStatusScreen = StatusScreen_New(s);
    sub_4E7AC0(s, 1);
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
