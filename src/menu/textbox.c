#include "hook.h"
#include "igi.h"
#include "menu/menu.h"

ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));

typedef struct TextBox {
    MenuWindow w;
    char font[0x20];
    char unk74[0x2130];
    char text[0x50];
    char text2[0x50];
} TextBox;

_Static_assert(sizeof(TextBox) == 0x2244, "TextBox");

#define gTextBoxType VAR(uint16_t, 0x53B580)
#define gTextBoxParams VAR(void *, 0x57BB98)

/* the handlers are not rewritten yet */
#define TextBox_Create ((void *)0x41ACA0)
#define TextBox_Delete ((void *)0x41AD20)
#define TextBox_Changed ((void *)0x41AD70)
#define TextBox_Run ((void *)0x41AE20)
#define TextBox_Draw ((void *)0x41AEA0)
#define TextBox_GetParams ((void *)0x41B160)
#define TextBox_Layout ((void *)0x41B170)

void TextBox_Init(void)
{
    TextBox tb;

    gTextBoxType = TaskType_New("TextBox", sizeof(TextBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gTextBoxType, 1, TextBox_Run);
    TaskType_SetHandler2(gTextBoxType, Screen_GetDrawEvent(), TextBox_Draw);
    TaskType_SetHandler2(gTextBoxType, 2, TextBox_Delete);
    TaskType_SetHandler2(gTextBoxType, MenuWindow_GetEvent914(), TextBox_Layout);
    TaskType_SetHandler2(gTextBoxType, MenuWindow_GetEvent918(), MenuWindow_ForwardEvent918);
    TaskType_SetHandler(gTextBoxType, 0, TextBox_Create);
    TaskType_SetHandler(gTextBoxType, Param_GetTableEvent(), TextBox_GetParams);
    TaskType_SetHandler(gTextBoxType, Param_GetChangedEvent(), TextBox_Changed);
    gTextBoxParams = ParamTable_New(5);
    ParamTable_Add(gTextBoxParams, &tb, tb.font, Param_String32(), 7, NULL);
    ParamTable_Add(gTextBoxParams, &tb, &tb.w.w, Param_Int32(), 7, NULL);
    ParamTable_Add(gTextBoxParams, &tb, &tb.w.h, Param_Int32(), 7, NULL);
    ParamTable_Add(gTextBoxParams, &tb, tb.text, Param_VarString(), 7, NULL);
    ParamTable_Add(gTextBoxParams, &tb, tb.text2, Param_VarString(), 7, NULL);
}
HOOK(0x41AAF0, TextBox_Init);
