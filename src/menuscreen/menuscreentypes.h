#ifndef MENUSCREEN_MENUSCREENTYPES_H
#define MENUSCREEN_MENUSCREENTYPES_H

#include "widget/widgettypes.h"
#include "res/restypes.h"
#include "param/paramtypes.h"

typedef struct MenuScreen {
    MenuWindow w;
    char pic[0x40];
    Text *picText;
    Text *logo;
    int frameX0, frameX1, frameY0, frameY1;
    VarString onBack;
    VarString onLayout;
    VarString onForward;
    int userData; /* "<name>.nUserData" */
    int registered;
} MenuScreen;

#endif
