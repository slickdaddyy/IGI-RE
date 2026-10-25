#include "hook.h"
#include "igi.h"
#include "ui/ui.h"

ORIG(0x4B53B0, void, QVertex_DrawQuadEx, (Sprite *s, float x0, float y0, float x1, float y1, float x2, float y2,
                                          float x3, float y3, float r, float g, float b, float z, float a, int frame,
                                          unsigned int flags, int unk20));

typedef struct PictureBoxItem {
    char name[0x40];
    int id;
    Sprite *sprite;
} PictureBoxItem;

/* shows one of up to 32 sprites, picked by id */
typedef struct PictureBox {
    MenuWindow w;
    PictureBoxItem items[32];
    int index;
    int count;
    VarString fill;
    VarString select;
} PictureBox;

_Static_assert(sizeof(PictureBox) == 0x9FC, "PictureBox");

extern uint16_t gPictureBoxType;
extern void *gPictureBoxParams;
extern PictureBox *gPictureBoxFilling;

void PictureBox_Create(PictureBox *pb, void *arg);
void PictureBox_Delete(PictureBox *pb);
void PictureBox_Changed(PictureBox *pb, void *arg);
void PictureBox_Run(PictureBox *pb);
void PictureBox_Draw(PictureBox *pb);
void PictureBox_Layout(PictureBox *pb);
void PictureBox_GetParams(void *task, void **out);

void PictureBox_Init(void)
{
    PictureBox pb;

    gPictureBoxType = TaskType_New("PictureBox", sizeof(PictureBox), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gPictureBoxType, 1, PictureBox_Run);
    TaskType_SetHandler2(gPictureBoxType, Screen_GetDrawEvent(), PictureBox_Draw);
    TaskType_SetHandler2(gPictureBoxType, 2, PictureBox_Delete);
    TaskType_SetHandler(gPictureBoxType, 0, PictureBox_Create);
    TaskType_SetHandler(gPictureBoxType, Param_GetTableEvent(), PictureBox_GetParams);
    TaskType_SetHandler(gPictureBoxType, Param_GetChangedEvent(), PictureBox_Changed);
    TaskType_SetHandler2(gPictureBoxType, MenuWindow_GetLoadEvent(), PictureBox_Layout);
    TaskType_SetHandler2(gPictureBoxType, MenuWindow_GetSaveEvent(), MenuWindow_ForwardSave);
    gPictureBoxParams = ParamTable_New(4);
    ParamTable_Add(gPictureBoxParams, &pb, &pb.w.w, Param_Int32(), 7, NULL);
    ParamTable_Add(gPictureBoxParams, &pb, &pb.w.h, Param_Int32(), 7, NULL);
    ParamTable_Add(gPictureBoxParams, &pb, &pb.fill, Param_VarString(), 7, NULL);
    ParamTable_Add(gPictureBoxParams, &pb, &pb.select, Param_VarString(), 7, NULL);
}
HOOK(0x41B310, PictureBox_Init);

void PictureBox_Create(PictureBox *pb, void *arg)
{
    CALL_BASE(0, pb, arg);
    pb->w.centreX = 1;
    pb->w.centreY = 1;
    pb->w.x = 320;
    pb->w.y = 240;
    pb->w.w = 256;
    pb->w.h = 192;
    pb->index = 0;
    pb->count = 0;
    VarString_Init(&pb->fill);
    VarString_Init(&pb->select);
}
HOOK(0x41B490, PictureBox_Create);

void PictureBox_Delete(PictureBox *pb)
{
    VarString_Free(&pb->fill);
    VarString_Free(&pb->select);
}
HOOK(0x41B500, PictureBox_Delete);

void PictureBox_Changed(PictureBox *pb, void *arg)
{
    VarString_Bind(&pb->fill, pb);
    VarString_Bind(&pb->select, pb);
    CALL_BASE(Param_GetChangedEvent(), pb, arg);
}
HOOK(0x41B530, PictureBox_Changed);

/* unlike the other boxes this does not call the MenuWindow run handler */
void PictureBox_Run(PictureBox *pb)
{
    if (VarString_HasExpr(&pb->select)) {
        int id = round_int(VarString_EvalReal(&pb->select, pb));
        int i;

        for (i = 0; i < pb->count; i++)
            if (id == pb->items[i].id) {
                pb->index = i;
                return;
            }
    }
}
HOOK(0x41B590, PictureBox_Run);

void PictureBox_Draw(PictureBox *pb)
{
    float alpha = ui_alpha(pb->w.menu);
    Sprite *s;
    uint16_t base = MenuWindow_GetType();

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(pb);
    if (pb->count && (s = pb->items[pb->index].sprite) != NULL) {
        int x = (Display_GetMode()->width >> 1) - 318 + pb->w.x;
        int y = (Display_GetMode()->height >> 1) - 238 + pb->w.y;
        float x0 = x, y0 = y;
        float x1 = pb->w.w - 4 + x, y1 = pb->w.h - 4 + y;

        QVertex_DrawQuadEx(s, x0, y0, x1, y0, x0, y1, x1, y1, 128.0f, 128.0f, 128.0f, 1.0f, alpha, 0, 0x80000, 0);
    }
    MenuWindow_DrawEnd(pb, 0.5f, alpha, 0);
}
HOOK(0x41B5F0, PictureBox_Draw);

void PictureBox_Layout(PictureBox *pb)
{
    int i;

    pb->index = 0;
    gPictureBoxFilling = pb;
    if (VarString_HasExpr(&pb->fill))
        VarString_EvalReal(&pb->fill, pb);
    gPictureBoxFilling = NULL;
    for (i = 0; i < pb->count; i++)
        pb->items[i].sprite = MenuManager_FindSprite(pb->w.menu, pb->items[i].name);
    CALL_CHILDREN(&pb->w.task, MenuWindow_GetLoadEvent);
}
HOOK(0x41B750, PictureBox_Layout);

void PictureBox_GetParams(void *task, void **out)
{
    *out = gPictureBoxParams;
}
HOOK(0x41B830, PictureBox_GetParams);

void PictureBox_Shutdown(void)
{
    QTaskList_Free(gPictureBoxParams);
    TaskType_Free(gPictureBoxType);
    gPictureBoxType = 0x181;
}
HOOK(0x41B840, PictureBox_Shutdown);

/* called by script functions while a PictureBox runs its fill expression */
void PictureBox_AddItem(const char *name, int id)
{
    PictureBox *pb = gPictureBoxFilling;
    PictureBoxItem *it = &pb->items[pb->count];
    int i;

    for (i = 0; i < pb->count; i++)
        if (pb->items[i].id == id)
            return;
    it->id = id;
    for (i = 0; (it->name[i] = name[i]) != 0; i++)
        ;
    pb->count++;
}
HOOK(0x41B870, PictureBox_AddItem);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53B58C) uint16_t gPictureBoxType INIT(385);
PAD(igd, 53B58C, 53B58E, 0x2);
BSS(57BBA4) PictureBox *gPictureBoxFilling;
BSS(57BBA4) void *gPictureBoxParams;
