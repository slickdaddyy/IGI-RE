#ifndef UI_UITYPES_H
#define UI_UITYPES_H

#include "widget/widgettypes.h"
#include "res/restypes.h"
#include "param/paramtypes.h"

/* one page of text, picked by id */
typedef struct TextBoxItem {
    char text[0x100];
    int id;
    const char *str;
} TextBoxItem;

/* shows one of up to 32 texts, word wrapped */
typedef struct TextBox {
    MenuWindow w;
    char font[0x20];
    char unk74[0x20];
    Text *text;
    Sprite *sprite;
    TextBoxItem items[32];
    int index;
    int count;
    VarString fill;
    VarString select;
} TextBox;

typedef struct ToggleBox {
    MenuWindow w;
    char unk54[0x40];
    MenuWindow *left;
    MenuWindow *right;
    Text *yes;
    Text *no;
    int isOn;
    char registered;
    char padA9[3];
    VarString value;
    VarString forward;
} ToggleBox;

#endif
