#ifndef WIDGET_WIDGETTYPES_H
#define WIDGET_WIDGETTYPES_H

#include "task/tasktypes.h"

typedef struct IntRange {
    int min, max;
} IntRange;

typedef struct MenuWindow {
    Task task;
    int x, y, w, h;
    int spacingX, spacingY;
    int lineup;     /* LINEUPMODE_* */
    int centreX;    /* 1 = centred in the parent when not lined up */
    int centreY;
    struct MenuManager *menu;   /* the MenuManager above it */
    char wasHover;  /* hover last frame, or the pointer just entered */
    char hover;
    char held;
    char clicked;
    char selectable;
    char focused;
    char pad4E[2];
    int key;
} MenuWindow;
_Static_assert(sizeof(MenuWindow) == 0x54, "MenuWindow");

typedef struct Cursor {
    Task task;
    void *owner;
    int x, y;
    int hotX, hotY;
    struct Sprite *sprite;
    int prevButtons;
    int buttons;
} Cursor;

#endif
