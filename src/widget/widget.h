#ifndef WIDGET_WIDGET_H
#define WIDGET_WIDGET_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "qvertex/qvertextypes.h"
#include "win/wintypes.h"
#include "menu/menutypes.h"
#include "widget/widgettypes.h"
#include "core/coretypes.h"

typedef void (*TaskFn)(void *task, void *arg);

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern RenderMode gRenderModeCurrent;
extern void *gQVertexTexture;
extern int gQVertexStrip;
extern void *gQVertexPalette;
extern char gEmptyString[];

/* the handler a task type has for a slot */
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x419030, int, MenuManager_GetFade, (void *mm));
ORIG(0x418FB0, MenuInput *, MenuManager_GetInput, (void *mm));
ORIG(0x41A120, void, MenuWindow_ForwardEvent918, (void *task));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4B4E00, void, Draw_Sprite, (Sprite *s, float x, float y, float r, float g, float b, int frame, int unk4,
                                   int unk8));
ORIG(0x4B4A00, void, Draw_RectA, (float x, float y, float w, float h, float r, float g, float b, float a, int flags,
                                  int unk));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));

uint16_t MenuWindow_GetType(void);
int MenuWindow_GetEvent910(void);
int MenuWindow_GetEvent914(void);
int MenuWindow_GetEvent918(void);
int MenuWindow_GetEvent91C(void);
void MenuWindow_HitTest(MenuWindow *w, int x, int y, int width, int height, int *mx, int *my);
void MenuWindow_DrawEnd(MenuWindow *w, float fill, float alpha, int focused);

/* next sibling, or NULL at the end of the list */
static inline Task *next_sibling(Task *t)
{
    return t->next && t->next->next ? t->next : NULL;
}

/* calls the handler for slot on each child of t (not deeper), keeping the
   next child on gTaskStack so handlers may delete tasks. A macro: gcc drops
   calls through ORIG pointers passed to inline functions. */
#define CALL_CHILDREN(t, slot)                                          \
    do {                                                                \
        Task *c_ = ((Task *)(t))->head;                                 \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = HANDLER(slot, c_->type);                              \
            if (fn_)                                                    \
                ((void (*)(void *))fn_)(c_);                            \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* calls the MenuWindow handler for a slot, like a base class method */
#define CALL_BASE(slot, ...) ((void (*)())HANDLER(slot, MenuWindow_GetType()))(__VA_ARGS__)

/* the alpha all elements fade with: (fade - 8) / 8, not below 0 */
#define UI_ALPHA(menu, out)                                             \
    do {                                                                \
        float a_ = (MenuManager_GetFade(menu) - 8) * 0.125f;            \
        (out) = a_ > 0.0f ? a_ : 0.0f;                                  \
    } while (0)

/* the window's top left corner on screen: the 640x480 layout is centred */
#define SCREEN_X(w) ((Display_GetMode()->width >> 1) - 320 + (w)->x)
#define SCREEN_Y(w) ((Display_GetMode()->height >> 1) - 240 + (w)->y)

#endif
