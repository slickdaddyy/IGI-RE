#include "hook.h"
#include "igi.h"
#include "menuscreen/menuscreen.h"

ORIG(0x4B4AF0, void, Draw_RectAZ, (float x, float y, float w, float h, float r, float g, float b, float a, float z,
                                   int flags, int unk));
ORIG(0x418D80, Text *, MenuManager_FindPic, (void *mm, const char *name));
ORIG(0x419040, Task *, MenuManager_GetBackgroundFX, (void *mm));
ORIG(0x48F2D0, int, AppContext_IsUSA, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4B6E60, void, Text_Draw, (Text *t));
ORIG(0x4B6E70, int, Text_Width, (Text *t));
ORIG(0x4B6E80, int, Text_Height, (Text *t));

/* handlers that follow this range */
#define MenuScreen_Run ((void *)0x422110)
#define MenuScreen_GetParams ((void *)0x422180)
#define MenuScreen_Activate ((void *)0x422190)
#define MenuScreen_Deactivate ((void *)0x422200)
#define MenuScreen_Layout ((void *)0x422260)
#define MenuScreen_Forward918 ((void *)0x422300)

/* one page of the front end: a background picture, the game logo and a
   framed box, all other elements are its children */
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

_Static_assert(sizeof(MenuScreen) == 0x1A4, "MenuScreen");

#define gMenuScreenType VAR(uint16_t, 0x53B800)
#define gMenuScreenParams VAR(void *, 0x57BC04)
#define gLogoOffset VAR(int, 0x57BC0C)

void MenuScreen_DrawHandler(MenuScreen *ms);
void MenuScreen_Create(MenuScreen *ms, void *arg);
void MenuScreen_Delete(MenuScreen *ms);
void MenuScreen_UpdateInternalDataHandler(MenuScreen *ms, void *arg);
void MenuScreen_DrawFrame(MenuScreen *ms);

void MenuScreen_Init(void)
{
    MenuScreen ms;

    gMenuScreenType = TaskType_New("MenuScreen", sizeof(MenuScreen), MenuWindow_GetType(), 0);
    TaskType_SetHandler2(gMenuScreenType, 1, MenuScreen_Run);
    TaskType_SetHandler2(gMenuScreenType, Screen_GetDrawEvent(), MenuScreen_DrawHandler);
    TaskType_SetHandler(gMenuScreenType, 0, MenuScreen_Create);
    TaskType_SetHandler2(gMenuScreenType, 2, MenuScreen_Delete);
    TaskType_SetHandler(gMenuScreenType, Param_GetTableEvent(), MenuScreen_GetParams);
    TaskType_SetHandler(gMenuScreenType, Param_GetChangedEvent(), MenuScreen_UpdateInternalDataHandler);
    TaskType_SetHandler2(gMenuScreenType, Level_GetEventDA8(), MenuScreen_Activate);
    TaskType_SetHandler2(gMenuScreenType, Level_GetEventDAC(), MenuScreen_Deactivate);
    TaskType_SetHandler2(gMenuScreenType, MenuWindow_GetEvent914(), MenuScreen_Layout);
    TaskType_SetHandler2(gMenuScreenType, MenuWindow_GetEvent918(), MenuScreen_Forward918);
    gMenuScreenParams = ParamTable_New(8);
    ParamTable_Add(gMenuScreenParams, &ms, ms.pic, Param_String32(), 7, NULL);
    ParamTable_Add(gMenuScreenParams, &ms, &ms.frameX0, Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuScreenParams, &ms, &ms.frameY0, Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuScreenParams, &ms, &ms.frameX1, Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuScreenParams, &ms, &ms.frameY1, Param_Int32(), 7, NULL);
    ParamTable_Add(gMenuScreenParams, &ms, &ms.onBack, Param_VarString(), 7, NULL);
    ParamTable_Add(gMenuScreenParams, &ms, &ms.onLayout, Param_VarString(), 7, NULL);
    ParamTable_Add(gMenuScreenParams, &ms, &ms.onForward, Param_VarString(), 7, NULL);
}
HOOK(0x421750, MenuScreen_Init);

void MenuScreen_Create(MenuScreen *ms, void *arg)
{
    CALL_BASE(0, ms, arg);
    ms->picText = NULL;
    ms->logo = NULL;
    ms->pic[0] = 0;
    ms->registered = 0;
    VarString_Init(&ms->onBack);
    VarString_Init(&ms->onLayout);
    VarString_Init(&ms->onForward);
}
HOOK(0x4219B0, MenuScreen_Create);

void MenuScreen_Delete(MenuScreen *ms)
{
    VarString_Free(&ms->onBack);
    VarString_Free(&ms->onLayout);
    VarString_Free(&ms->onForward);
    if (Task_GetId(ms) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), ms->w.task.type);

        if (fn)
            ((void (*)(void *))fn)(ms);
    }
}
HOOK(0x421A10, MenuScreen_Delete);

/* the US build has its own logo, drawn 12 pixels further right */
void MenuScreen_UpdateInternalDataHandler(MenuScreen *ms, void *arg)
{
    if (ms->w.menu) {
        ms->picText = MenuManager_FindPic(ms->w.menu, ms->pic);
        if (!AppContext_IsUSA()) {
            ms->logo = MenuManager_FindPic(ms->w.menu, "logo.pic");
            gLogoOffset = 0;
        } else {
            ms->logo = MenuManager_FindPic(ms->w.menu, "logo_us.pic");
            gLogoOffset = 12;
        }
        if (ms->logo) {
            int x = round_int(320.0f - (long double)Text_Width(ms->logo) * 0.5f);

            ms->logo->x = gLogoOffset + x;
            ms->logo->y = (Display_GetMode()->height >> 1) - 220;
            ms->logo->unk20 = 0;
        }
        if (ms->picText) {
            ms->picText->unk20 = 0;
            ms->picText->x = (Display_GetMode()->width >> 1) - 320;
            ms->picText->y = (Display_GetMode()->height >> 1) - 240;
            ms->w.w = Text_Width(ms->picText);
            ms->w.h = Text_Height(ms->picText);
        }
    }
    VarString_Bind(&ms->onBack, ms);
    VarString_Bind(&ms->onLayout, ms);
    VarString_Bind(&ms->onForward, ms);
    CALL_BASE(Param_GetChangedEvent(), ms, arg);
}
HOOK(0x421A70, MenuScreen_UpdateInternalDataHandler);

void MenuScreen_DrawHandler(MenuScreen *ms)
{
    uint16_t base = MenuWindow_GetType();
    Task *fx;

    ((void (*)(void *))HANDLER(Screen_GetDrawEvent(), base))(ms);
    MenuScreen_DrawFrame(ms);
    if (ms->logo)
        Text_Draw(ms->logo);
    if (MenuManager_GetBackgroundFX(ms->w.menu)) {
        uint16_t type = MenuManager_GetBackgroundFX(ms->w.menu)->type;
        TaskFn fn = HANDLER(Screen_GetDrawEvent(), type);

        fx = MenuManager_GetBackgroundFX(ms->w.menu);
        ((void (*)(void *))fn)(fx);
    }
    if (ms->picText)
        Text_Draw(ms->picText);
}
HOOK(0x421C20, MenuScreen_DrawHandler);

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

/* a green frame with a title line, a green to black gradient and, with no
   background picture, a dark fill; it grows from the centre as the menu
   fades in */
void MenuScreen_DrawFrame(MenuScreen *ms)
{
    QVertex v[5];
    QVertexStrip s;
    float x0, y0, x1, y1, line, a, a1, a2;
    int i;

    x0 = rf((long double)ms->frameX0 + 1.0f);
    y0 = rf((long double)SCREEN_Y(0) + ms->frameY0 + 1.0f);
    x1 = rf((long double)ms->frameX1 + 1.0f);
    y1 = rf((long double)SCREEN_Y(0) + ms->frameY1 + 1.0f);
    line = rf((long double)y0 + 20.0f);
    if ((long double)MenuManager_GetFade(ms->w.menu) * 0.125f < 1.0f)
        a = rf((long double)MenuManager_GetFade(ms->w.menu) * 0.125f);
    else
        a = 1.0f;
    a1 = rf((long double)a * 0.2f);
    a2 = rf((long double)a * 0.05f);
    x0 = rf(((long double)x0 - 320.0f) * a + SCREEN_X(0) + 320.0f);
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    x1 = rf(((long double)x1 - 320.0f) * a + SCREEN_X(0) + 320.0f);
    for (i = 0; i < 4; i++)
        set_vertex(&v[i], 0, 0, a, 0.75f);
    v[0].x = x0;
    v[0].y = y0;
    v[1].x = x1;
    v[1].y = y0;
    v[2].x = x1;
    v[2].y = y1;
    v[3].x = x0;
    v[3].y = y1;
    v[4] = v[0];
    s.untextured = 1;
    s.flags = 0x80000;
    s.unk8 = 0;
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
    s.untextured = 0;
    s.unk8 = 0;
    s.count = 4;
    s.verts = v;
    gRenderFuncs[gQVertexStrip](&s);

    if (!ms->picText)
        Draw_RectAZ(x0, y0, rf((long double)x1 - x0), rf((long double)y1 - y0), 0, 0, 0, rf((long double)a * 0.66f),
                    1.0f, 0x80000, 0);
}
HOOK(0x421CD0, MenuScreen_DrawFrame);
