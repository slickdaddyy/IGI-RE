#include "hook.h"
#include "igi.h"
#include "menu/menu.h"

FN_EXTERN(TextBox_Changed);
FN_EXTERN(TextBox_Create);
FN_EXTERN(TextBox_Delete);
FN_EXTERN(TextBox_Draw);
FN_EXTERN(TextBox_GetParams);
FN_EXTERN(TextBox_Layout);
FN_EXTERN(TextBox_Run);

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

extern uint16_t gTextBoxType;
extern void *gTextBoxParams;

/* the handlers are not rewritten yet */
#define TextBox_Create (FN(0x41ACA0, TextBox_Create))
#define TextBox_Delete (FN(0x41AD20, TextBox_Delete))
#define TextBox_Changed (FN(0x41AD70, TextBox_Changed))
#define TextBox_Run (FN(0x41AE20, TextBox_Run))
#define TextBox_Draw (FN(0x41AEA0, TextBox_Draw))
#define TextBox_GetParams (FN(0x41B160, TextBox_GetParams))
#define TextBox_Layout (FN(0x41B170, TextBox_Layout))

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

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B580) uint16_t gTextBoxType INIT(385);
PAD(igd, 53B580, 53B582, 0x2);
BSS(57BB98) void *gTextBoxParams;
