#include "hook.h"
#include "igi.h"
#include "widget/widget.h"

ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x401810, int, TaskEvent_Alloc2, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x4018E0, void, TaskEvent_Free2, (int slot));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x418AF0, uint16_t, MenuManager_GetType, (void));
ORIG(0x419090, void, MenuManager_RefreshScreen, (void *mm));
ORIG(0x488700, void, Task_RunChildren, (void *task));

/* the state MenuWindow_Arrange hands to each child's place handler */
typedef struct LineUp {
    char horizontal;
    char vertical;
    char placed;
    int x, y;
    int spacingX, spacingY;
    MenuWindow *parent;
} LineUp;

/* what a child's size handler reports */
typedef struct ChildSize {
    char valid;
    int w, h;
} ChildSize;

#define gMenuWindowType VAR(uint16_t, 0x53B90C)
#define gEvent910 VAR(int, 0x53B910)
#define gEvent914 VAR(int, 0x53B914)
#define gEvent918 VAR(int, 0x53B918)
#define gEvent91C VAR(int, 0x53B91C)
#define gPlaceEvent VAR(uint8_t, 0x53B920)
#define gSizeEvent VAR(uint8_t, 0x53B921)

enum {
    LINEUPMODE_NONE,
    LINEUPMODE_VERT_TOP,
    LINEUPMODE_VERT_CENTRE,
    LINEUPMODE_VERT_BOTTOM,
    LINEUPMODE_HOR_LEFT,
    LINEUPMODE_HOR_CENTRE,
    LINEUPMODE_HOR_RIGHT,
    LINEUPMODE_VERT_FILLOUT,
    LINEUPMODE_HOR_FILLOUT,
};

void MenuWindow_Create(MenuWindow *w, void *menu);
void MenuWindow_Changed(MenuWindow *w, const char *name);
void MenuWindow_Place(MenuWindow *w, LineUp *l);
void MenuWindow_GetSize(MenuWindow *w, ChildSize *out);
void MenuWindow_Arrange(MenuWindow *w);
void MenuWindow_Draw(MenuWindow *w);
void MenuWindow_ForwardEvent914(MenuWindow *w);
void MenuWindow_ForwardEvent91C(MenuWindow *w);

void MenuWindow_Init(void)
{
    gMenuWindowType = TaskType_New("MenuWindow", sizeof(MenuWindow), Task_GetRootType(), 0);
    gEvent910 = TaskEvent_Alloc2(gMenuWindowType);
    gPlaceEvent = TaskEvent_Alloc(gMenuWindowType);
    gSizeEvent = TaskEvent_Alloc(gMenuWindowType);
    gEvent914 = TaskEvent_Alloc2(gMenuWindowType);
    gEvent918 = TaskEvent_Alloc2(gMenuWindowType);
    gEvent91C = TaskEvent_Alloc2(gMenuWindowType);
    TaskType_SetHandler(gMenuWindowType, 0, MenuWindow_Create);
    TaskType_SetHandler(gMenuWindowType, Param_GetChangedEvent(), MenuWindow_Changed);
    TaskType_SetHandler(gMenuWindowType, gPlaceEvent, MenuWindow_Place);
    TaskType_SetHandler(gMenuWindowType, gSizeEvent, MenuWindow_GetSize);
    TaskType_SetHandler2(gMenuWindowType, 2, Task_Nothing);
    TaskType_SetHandler2(gMenuWindowType, gEvent910, MenuWindow_Arrange);
    TaskType_SetHandler2(gMenuWindowType, Screen_GetDrawEvent(), MenuWindow_Draw);
    TaskType_SetHandler2(gMenuWindowType, 1, Task_RunChildren);
    TaskType_SetHandler2(gMenuWindowType, gEvent914, MenuWindow_ForwardEvent914);
    TaskType_SetHandler2(gMenuWindowType, gEvent918, MenuWindow_ForwardEvent918);
    TaskType_SetHandler2(gMenuWindowType, gEvent91C, MenuWindow_ForwardEvent91C);
    Script_RegisterConstant("LINEUPMODE_NONE", LINEUPMODE_NONE);
    Script_RegisterConstant("LINEUPMODE_VERT_TOP", LINEUPMODE_VERT_TOP);
    Script_RegisterConstant("LINEUPMODE_VERT_CENTRE", LINEUPMODE_VERT_CENTRE);
    Script_RegisterConstant("LINEUPMODE_VERT_BOTTOM", LINEUPMODE_VERT_BOTTOM);
    Script_RegisterConstant("LINEUPMODE_VERT_FILLOUT", LINEUPMODE_VERT_FILLOUT);
    Script_RegisterConstant("LINEUPMODE_HOR_LEFT", LINEUPMODE_HOR_LEFT);
    Script_RegisterConstant("LINEUPMODE_HOR_CENTRE", LINEUPMODE_HOR_CENTRE);
    Script_RegisterConstant("LINEUPMODE_HOR_RIGHT", LINEUPMODE_HOR_RIGHT);
    Script_RegisterConstant("LINEUPMODE_HOR_FILLOUT", LINEUPMODE_HOR_FILLOUT);
}
HOOK(0x423FC0, MenuWindow_Init);

void MenuWindow_FindMenu(MenuWindow *w, Task *t);

void MenuWindow_Create(MenuWindow *w, void *menu)
{
    w->spacingX = 4;
    w->spacingY = 4;
    w->x = 0;
    w->y = 0;
    w->w = 0;
    w->h = 0;
    w->lineup = 0;
    w->centreX = 0;
    w->centreY = 0;
    w->held = 0;
    w->hover = 0;
    w->clicked = 0;
    w->wasHover = 0;
    w->key = 0;
    w->focused = 0;
    w->selectable = 0;
    if (menu) {
        w->menu = menu;
        return;
    }
    MenuWindow_FindMenu(w, &w->task);
}
HOOK(0x4241C0, MenuWindow_Create);

/* the MenuManager is the nearest ancestor of that type */
void MenuWindow_FindMenu(MenuWindow *w, Task *t)
{
    while (t->type != MenuManager_GetType()) {
        t = t->parent;
        if (!t) {
            w->menu = NULL;
            return;
        }
    }
    w->menu = t;
}
HOOK(0x424220, MenuWindow_FindMenu);

void MenuWindow_Changed(MenuWindow *w, const char *name)
{
    if ((!name || !*name) && w->menu)
        MenuManager_RefreshScreen(w->menu);
}
HOOK(0x424260, MenuWindow_Changed);

void MenuWindow_Draw(MenuWindow *w)
{
    CALL_CHILDREN(w, Screen_GetDrawEvent());
}
HOOK(0x424280, MenuWindow_Draw);

void MenuWindow_ForwardEvent914(MenuWindow *w)
{
    CALL_CHILDREN(w, MenuWindow_GetEvent914());
}
HOOK(0x424300, MenuWindow_ForwardEvent914);

void MenuWindow_ForwardEvent91C(MenuWindow *w)
{
    CALL_CHILDREN(w, MenuWindow_GetEvent91C());
}
HOOK(0x424380, MenuWindow_ForwardEvent91C);

void MenuWindow_GetSize(MenuWindow *w, ChildSize *out)
{
    out->valid = 1;
    out->w = w->w;
    out->h = w->h;
}
HOOK(0x424400, MenuWindow_GetSize);

void MenuWindow_Place(MenuWindow *w, LineUp *l)
{
    MenuWindow *p = l->parent;

    if (l->horizontal) {
        w->x = l->x;
        l->x += w->w + l->spacingX;
    } else if (w->centreX == 1) {
        w->x = ((p->w - w->w) >> 1) + p->x;
    }
    if (l->vertical) {
        w->y = l->y;
        l->placed = 1;
        l->y += w->h + l->spacingY;
        return;
    }
    if (w->centreY == 1)
        w->y = ((p->h - w->h) >> 1) + p->y;
    l->placed = 1;
}
HOOK(0x424420, MenuWindow_Place);

/* lines the children up by the window's lineup mode, then arranges them */
void MenuWindow_Arrange(MenuWindow *w)
{
    LineUp l;

    l.horizontal = 0;
    l.vertical = 0;
    l.parent = w;
    if (w->lineup) {
        Task *c = w->task.head->next ? w->task.head : NULL;
        int totalW = 0, totalH = 0, count = 0, n;

        while (c) {
            ChildSize size;
            TaskFn fn;

            size.valid = 0;
            fn = HANDLER(gSizeEvent, c->type);
            if (fn) {
                fn(c, &size);
                if (size.valid) {
                    totalH += size.h;
                    totalW += size.w;
                    count++;
                }
            }
            c = c->next;
            if (!c || !c->next)
                break;
        }
        switch (w->lineup) {
        case LINEUPMODE_VERT_TOP:
            l.spacingY = w->spacingY;
            l.y = w->y + l.spacingY;
            l.vertical = 1;
            break;
        case LINEUPMODE_VERT_CENTRE:
            l.vertical = 1;
            l.spacingY = (w->h - totalH) / (count + 1);
            l.y = w->y + l.spacingY;
            break;
        case LINEUPMODE_VERT_BOTTOM:
            l.spacingY = w->spacingY;
            l.vertical = 1;
            l.y = w->h - l.spacingY * count + w->y - totalH;
            break;
        case LINEUPMODE_VERT_FILLOUT:
            l.y = w->y + w->spacingY;
            n = count - 1 < 1 ? 1 : count - 1;
            l.vertical = 1;
            l.spacingY = (w->h - w->spacingY * 2 - totalH) / n;
            break;
        case LINEUPMODE_HOR_LEFT:
            l.spacingX = w->spacingX;
            l.x = w->x + l.spacingX;
            l.horizontal = 1;
            break;
        case LINEUPMODE_HOR_CENTRE:
            l.spacingX = (w->w - totalW) / (count + 1);
            l.x = w->x + l.spacingX;
            l.horizontal = 1;
            break;
        case LINEUPMODE_HOR_RIGHT:
            l.spacingX = w->spacingX;
            l.x = w->w - l.spacingX * count + w->x - totalW;
            l.horizontal = 1;
            break;
        case LINEUPMODE_HOR_FILLOUT:
            n = count - 1 < 1 ? 1 : count - 1;
            l.x = w->x + w->spacingX;
            l.spacingX = (w->w - w->spacingX * 2 - totalW) / n;
            l.horizontal = 1;
            break;
        }
    }
    {
        Task *c_ = w->task.head;

        if (c_->next && c_) {
            do {
                TaskFn fn_;

                gTaskStack[gTaskStackTop++] = next_sibling(c_);
                fn_ = HANDLER(gPlaceEvent, c_->type);
                if (fn_)
                    fn_(c_, &l);
                c_ = gTaskStack[--gTaskStackTop];
            } while (c_);
        }
    }
    CALL_CHILDREN(w, gEvent910);
}
HOOK(0x4244B0, MenuWindow_Arrange);

void MenuWindow_Shutdown(void)
{
    Script_UnregisterSymbol("LINEUPMODE_NONE");
    Script_UnregisterSymbol("LINEUPMODE_VERT_TOP");
    Script_UnregisterSymbol("LINEUPMODE_VERT_CENTRE");
    Script_UnregisterSymbol("LINEUPMODE_VERT_BOTTOM");
    Script_UnregisterSymbol("LINEUPMODE_VERT_FILLOUT");
    Script_UnregisterSymbol("LINEUPMODE_HOR_LEFT");
    Script_UnregisterSymbol("LINEUPMODE_HOR_CENTRE");
    Script_UnregisterSymbol("LINEUPMODE_HOR_RIGHT");
    Script_UnregisterSymbol("LINEUPMODE_HOR_FILLOUT");
    TaskEvent_Free2(gEvent910);
    TaskEvent_Free2(gEvent914);
    TaskEvent_Free2(gEvent918);
    TaskEvent_Free2(gEvent91C);
    TaskEvent_Free(gPlaceEvent);
    TaskEvent_Free(gSizeEvent);
    TaskType_Free(gMenuWindowType);
    gMenuWindowType = 0x181;
}
HOOK(0x424790, MenuWindow_Shutdown);

uint16_t MenuWindow_GetType(void)
{
    return gMenuWindowType;
}
HOOK(0x424850, MenuWindow_GetType);

int MenuWindow_GetEvent910(void)
{
    return gEvent910;
}
HOOK(0x424860, MenuWindow_GetEvent910);

int MenuWindow_GetEvent914(void)
{
    return gEvent914;
}
HOOK(0x424870, MenuWindow_GetEvent914);

int MenuWindow_GetEvent918(void)
{
    return gEvent918;
}
HOOK(0x424880, MenuWindow_GetEvent918);

int MenuWindow_GetEvent91C(void)
{
    return gEvent91C;
}
HOOK(0x424890, MenuWindow_GetEvent91C);

/* updates the pointer state flags for the rectangle and returns the pointer */
void MenuWindow_HitTest(MenuWindow *w, int x, int y, int width, int height, int *mx, int *my)
{
    MenuInput *in = NULL;
    char inside;
    char held, clicked, hover;

    /* without a menu the original reads through NULL; keep that a plain load */
    __asm__("" : "+r"(in));
    if (w->menu)
        in = MenuManager_GetInput(w->menu);
    inside = x < in->x && y < in->y && in->x < x + width && in->y < y + height;
    held = (in->button && w->held) || (in->pressed && inside);
    clicked = w->held && in->released && inside;
    hover = (!in->button || held) && inside;

    w->held = held;
    w->wasHover = w->hover || in->entered;
    w->hover = hover;
    w->clicked = clicked;
    if (mx)
        *mx = in->x;
    if (my)
        *my = in->y;
}
HOOK(0x4248A0, MenuWindow_HitTest);

/* the window's outline (green, or white when focused) and a black fill */
void MenuWindow_DrawEnd(MenuWindow *w, float fill, float alpha, int focused)
{
    static const float half = 0.5f, opaque = 0.99f;
    int sx = SCREEN_X(w);
    int width = w->w;
    int sy = SCREEN_Y(w);
    int height = w->h;
    QVertex v[5];
    QVertexStrip strip;
    int i;

    for (i = 0; i < 4; i++) {
        v[i].r = focused ? half : 0.0f;
        v[i].g = 1.0f;
        v[i].b = focused ? half : 0.0f;
        v[i].a = alpha;
        v[i].u = 0.0f;
        v[i].v = 0.0f;
        v[i].z = 1.0f;
    }
    v[0].x = sx + 1;
    v[0].y = sy + 1;
    v[1].x = width + sx;
    v[1].y = v[0].y;
    v[2].x = v[1].x;
    v[2].y = height + sy;
    v[3].x = v[0].x;
    v[3].y = v[2].y;
    v[4] = v[0];
    strip.flags = 0;
    if (!(alpha > opaque))
        strip.flags = 0x80000;
    strip.untextured = 1;
    strip.unk8 = 0;
    strip.count = 5;
    strip.verts = v;
    gQVertexTexture = NULL;
    gQVertexPalette = NULL;
    gRenderFuncs[gQVertexStrip](&strip);
    Draw_RectA(sx, sy, width, height, 0.0f, 0.0f, 0.0f, alpha * fill, 0x80000, 0);
}
HOOK(0x424980, MenuWindow_DrawEnd);

MenuWindow *MenuWindow_New(void *parent, void *menu)
{
    return (MenuWindow *)Task_New(parent, MenuWindow_GetType(), menu);
}
HOOK(0x424B60, MenuWindow_New);

uint8_t MenuWindow_GetPlaceEvent(void)
{
    return gPlaceEvent;
}
HOOK(0x424B80, MenuWindow_GetPlaceEvent);
