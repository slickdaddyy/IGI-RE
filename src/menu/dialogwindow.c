#include "hook.h"
#include "igi.h"
#include "menu/menu.h"

FN_EXTERN(MenuWindow_ForwardEvent914);

ORIG(0x4B4AF0, void, Draw_RectAZ, (float x, float y, float w, float h, float r, float g, float b, float a, float z,
                                   int flags, int unk));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));

/* a framed box with a title line, the base of the popups */
typedef struct DialogWindow {
    MenuWindow w;
    int unk54;
    char unk58;
    char horizontal;
    char pad[2];
} DialogWindow;

_Static_assert(sizeof(DialogWindow) == 0x5C, "DialogWindow");

extern uint16_t gDialogWindowType;
extern IntRange gDialogRange;
extern void *gDialogWindowParams;
#define MenuWindow_SetLayout (FN(0x424300, MenuWindow_ForwardEvent914))

void DialogWindow_Run(DialogWindow *dw);
void DialogWindow_Draw(DialogWindow *dw);
void DialogWindow_Delete(DialogWindow *dw);
void DialogWindow_Create(DialogWindow *dw, void *arg);
void DialogWindow_GetParams(void *task, void **out);
void DialogWindow_Changed(DialogWindow *dw, void *arg);
void DialogWindow_DrawFrame(DialogWindow *dw, float alpha);

void DialogWindow_Init(void)
{
    DialogWindow dw;

    gDialogRange.min = 0;
    gDialogRange.max = 640;
    gDialogWindowType = TaskType_New("DialogWindow", sizeof(DialogWindow), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gDialogWindowType, 1, DialogWindow_Run);
    TaskType_SetHandler2(gDialogWindowType, Screen_GetDrawEvent(), DialogWindow_Draw);
    TaskType_SetHandler2(gDialogWindowType, 2, DialogWindow_Delete);
    TaskType_SetHandler(gDialogWindowType, 0, DialogWindow_Create);
    TaskType_SetHandler(gDialogWindowType, Param_GetTableEvent(), DialogWindow_GetParams);
    TaskType_SetHandler(gDialogWindowType, Param_GetChangedEvent(), DialogWindow_Changed);
    TaskType_SetHandler2(gDialogWindowType, MenuWindow_GetEvent914(), MenuWindow_SetLayout);
    TaskType_SetHandler2(gDialogWindowType, MenuWindow_GetEvent918(), MenuWindow_ForwardEvent918);
    TaskType_SetHandler2(gDialogWindowType, Level_GetEventDA8(), Task_Nothing);
    TaskType_SetHandler2(gDialogWindowType, Level_GetEventDAC(), Task_Nothing);
    gDialogWindowParams = ParamTable_New(5);
    ParamTable_Add(gDialogWindowParams, &dw, &dw.w.x, Param_Int32(), 7, &gDialogRange);
    ParamTable_Add(gDialogWindowParams, &dw, &dw.w.y, Param_Int32(), 7, &gDialogRange);
    ParamTable_Add(gDialogWindowParams, &dw, &dw.w.w, Param_Int32(), 7, &gDialogRange);
    ParamTable_Add(gDialogWindowParams, &dw, &dw.w.h, Param_Int32(), 7, &gDialogRange);
    ParamTable_Add(gDialogWindowParams, &dw, &dw.horizontal, Param_Bool8(), 7, &gDialogRange);
}
HOOK(0x4199F0, DialogWindow_Init);

void DialogWindow_Create(DialogWindow *dw, void *arg)
{
    HANDLER(0, MenuWindow_GetType())(dw, arg);
    dw->w.centreX = 1;
    dw->w.centreY = 1;
    dw->unk58 = 0;
    dw->unk54 = 0;
    dw->horizontal = 1;
}
HOOK(0x419BE0, DialogWindow_Create);

void DialogWindow_Delete(DialogWindow *dw)
{
    ((void (*)(void *))HANDLER(Level_GetEventDAC(), dw->w.task.type))(dw);
}
HOOK(0x419C20, DialogWindow_Delete);

void DialogWindow_Changed(DialogWindow *dw, void *arg)
{
    uint8_t ev = Param_GetChangedEvent();

    HANDLER(ev, MenuWindow_GetType())(dw, arg);
}
HOOK(0x419C50, DialogWindow_Changed);

void DialogWindow_Run(DialogWindow *dw)
{
    ((void (*)(void *))HANDLER(1, MenuWindow_GetType()))(dw);
}
HOOK(0x419C90, DialogWindow_Run);

void DialogWindow_Draw(DialogWindow *dw)
{
    long double a = (long double)MenuManager_GetFade(dw->w.menu) * 0.125f;
    float alpha = a < 1.0f ? rf(a) : 1.0f;
    uint16_t base = MenuWindow_GetType();

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(dw);
    DialogWindow_DrawFrame(dw, alpha);
}
HOOK(0x419CB0, DialogWindow_Draw);

static void set_vertex(QVertex *v, float x, float y, float a, float g)
{
    v->x = x;
    v->y = y;
    v->z = 1.0f;
    v->u = 0;
    v->v = 0;
    v->a = a;
    v->r = 0;
    v->g = g;
    v->b = 0;
}

/* the box shrinks to its centre as alpha goes to 0 */
void DialogWindow_DrawFrame(DialogWindow *dw, float alpha)
{
    QVertex v[5];
    QVertexStrip s;
    long double top, right, cx, y0e;
    float left, bottom, line, cy, a1, a2, x0, x1, y0, y1;

    left = rf((long double)((Display_GetMode()[1] >> 1) - 320) + dw->w.x);
    top = (long double)((Display_GetMode()[2] >> 1) - 240) + dw->w.y;
    right = (long double)dw->w.w + left;
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    bottom = rf((long double)dw->w.h + top);
    line = rf(top + 20.0f);
    cx = (right + left) * 0.5f;
    cy = rf(((long double)bottom + top) * 0.5f);
    a1 = rf((long double)alpha * 0.2f);
    a2 = rf((long double)alpha * 0.05f);
    x0 = rf((left - cx) * alpha + cx);
    x1 = rf((right - cx) * alpha + cx);
    y0e = (top - cy) * alpha + cy;
    y0 = rf(y0e);
    y1 = rf(((long double)bottom - cy) * alpha + cy);
    line = rf(((long double)line - cy) * alpha + cy);

    set_vertex(&v[0], x0, y0, alpha, 0.75f);
    set_vertex(&v[1], x1, y0, alpha, 0.75f);
    set_vertex(&v[2], x1, y1, alpha, 0.75f);
    set_vertex(&v[3], x0, y1, alpha, 0.75f);
    v[4] = v[0];
    s.lines = 1;
    s.flags = 0x80000;
    s.queue = 0;
    s.count = 5;
    s.verts = v;
    gRenderFuncs[gQVertexStrip](&s);

    v[0].x = x0;
    v[0].y = line;
    v[1].x = x1;
    v[1].y = line;
    s.count = 2;
    gRenderFuncs[gQVertexStrip](&s);

    set_vertex(&v[0], x0, y0, a1, 1.0f);
    set_vertex(&v[1], x1, y0, a1, 1.0f);
    set_vertex(&v[2], x0, y1, a2, 0);
    set_vertex(&v[3], x1, y1, a2, 0);
    s.lines = 0;
    s.queue = 0;
    s.count = 4;
    gRenderFuncs[gQVertexStrip](&s);

    Draw_RectAZ(x0, y0, rf((long double)x1 - x0), rf((long double)y1 - y0), 0, 0, 0, rf((long double)alpha * 0.9f),
                1.0f, 0x80000, 0);
}
HOOK(0x419D30, DialogWindow_DrawFrame);

void DialogWindow_GetParams(void *task, void **out)
{
    *out = gDialogWindowParams;
}
HOOK(0x41A110, DialogWindow_GetParams);

/* used by several menu window types for this event */
void MenuWindow_ForwardEvent918(Task *t)
{
    CALL_CHILDREN(t, MenuWindow_GetEvent918);
}
HOOK(0x41A120, MenuWindow_ForwardEvent918);

void DialogWindow_Shutdown(void)
{
    QTaskList_Free(gDialogWindowParams);
    TaskType_Free(gDialogWindowType);
    gDialogWindowType = 0x181;
}
HOOK(0x41A1A0, DialogWindow_Shutdown);

int DialogWindow_IsHorizontal(MenuWindow *w)
{
    return ((DialogWindow *)w)->horizontal;
}
HOOK(0x41A1D0, DialogWindow_IsHorizontal);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(539C28) uint16_t gDialogWindowType INIT(385);
PAD(igd, 539C28, 539C2A, 0x2);
BSS(57BB80) IntRange gDialogRange;
BSS(57BB80) void *gDialogWindowParams;
