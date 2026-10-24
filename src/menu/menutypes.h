#ifndef MENU_MENUTYPES_H
#define MENU_MENUTYPES_H

#include "res/restypes.h"
#include "task/tasktypes.h"

/* mouse and keys, as seen by the menu elements */
typedef struct MenuInput {
    char button;
    char pressed;
    char released;
    char entered;
    char back;
    char select;
    char pad[2];
    int wheel;
    int x, y;
} MenuInput;

typedef struct Color {
    float r, g, b;
} Color;

typedef struct MenuSprite {
    char name[0x40];
    Sprite sprite;
} MenuSprite;

typedef struct MenuPic {
    char name[0x40];
    Text *text;
} MenuPic;

/* a string looked up by MenuManager_GetString that has no resource */
typedef struct MenuString {
    char name[0x40];
    char text[0x44];
    struct MenuString *next;
} MenuString;

typedef struct MenuManager {
    Task task;
    int screen;
    int popup;
    int state;
    int nextState;
    struct MenuWindow *screenTask;
    struct MenuWindow *popupTask;
    struct MenuWindow *focus;
    struct MenuWindow *items[32];
    int itemCount;
    int itemIndex;
    int fadeShown;
    int fadeShownPrev;
    int fade;
    int drawFade;
    char resPath[0x100];
    void *menuRes;
    void *missionRes;
    void *langRes[32];
    int langResCount;
    MenuString *strings;
    MenuSprite fonts[16];
    MenuSprite sprites[64];
    MenuPic pics[32];
    short spriteCount;
    short fontCount;
    short picCount;
    short pad266A;
    char unk266C[0x4C];
    int savedFrames;
    char unk26BC;
    char active;
    char unk26BE;
    char soundsLoaded;
    char unk26C0;
    char fastFade;
    char inputEnabled;
    char idleTimeout;
    char busy;
    char scriptName[0x100];
    char pad27C5[3];
    Task *pointer;
    MenuInput input;
    Task *backgroundFX;
    int language;
    int depth;
    int screenStack[16];
    char ownsDisplay;
    char pad282D[3];
    int music[4];
    char unk2840[0x58];
} MenuManager;
_Static_assert(sizeof(MenuManager) == 0x2898, "MenuManager");

#endif
