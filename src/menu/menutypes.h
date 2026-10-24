#ifndef MENU_MENUTYPES_H
#define MENU_MENUTYPES_H

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

#endif
