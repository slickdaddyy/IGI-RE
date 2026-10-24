#ifndef MENUSCREEN_MENUSCREEN_H
#define MENUSCREEN_MENUSCREEN_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef void (*TaskFn)(void *task, void *arg);

typedef struct Sprite Sprite;

typedef struct Grid {
    int proportional;
    short cols, rows;
    Sprite *sprite;
} Grid;

typedef struct Text {
    Grid *grid;
    float x, y, z;
    float alpha;
    float color[3];
    int unk20;
    unsigned int flags;
} Text;

/* the "MenuWindow" task (0x54 bytes), base of every menu element */
typedef struct MenuWindow {
    Task task;
    int x, y, w, h;
    char unk30[0xC];
    int unk3C;
    int unk40;
    void *menu;
    char wasOver;
    char over;
    char held;
    char clicked;
    char selectable;
    char focused;
    char unk4E[2];
    int key;
} MenuWindow;

/* a script string parameter that may hold an expression */
typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

typedef struct Display {
    int unk0;
    int width;
    int height;
} Display;

typedef struct Color {
    float r, g, b;
} Color;

typedef struct QVertex {
    float x, y, z;
    float u, v;
    float a, r, g, b;
} QVertex;

typedef struct QVertexStrip {
    int untextured;
    unsigned int flags;
    int unk8;
    int count;
    QVertex *verts;
} QVertexStrip;

_Static_assert(sizeof(MenuWindow) == 0x54, "MenuWindow");
_Static_assert(sizeof(VarString) == 0x50, "VarString");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
extern Task *gTaskStack[];
extern int gTaskStackTop;
#define gRenderFuncs ((void (**)(QVertexStrip *))0xA94E84)
extern void *gQVertexTexture;
extern int gQVertexStrip;
extern void *gQVertexPalette;
extern char gEmptyString[];

#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x424850, uint16_t, MenuWindow_GetType, (void));
ORIG(0x424870, int, MenuWindow_GetEvent914, (void));
ORIG(0x424880, int, MenuWindow_GetEvent918, (void));
ORIG(0x424890, int, MenuWindow_GetEvent91C, (void));
ORIG(0x424B80, uint8_t, MenuWindow_GetPlaceEvent, (void));
ORIG(0x4248A0, void, MenuWindow_HitTest, (void *w, int x, int y, int width, int height, int *mx, int *my));
ORIG(0x424980, void, MenuWindow_DrawEnd, (void *w, float unk, float alpha, int focused));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4E7180, void, Sound_PlayName, (const char *name, int unk1, int unk2));
ORIG(0x491CF0, Display *, Display_GetMode, (void));
ORIG(0x418D70, void, MenuManager_Free, (void *task));
ORIG(0x418DE0, Sprite *, MenuManager_FindSprite, (void *mm, const char *name));
ORIG(0x418E40, Sprite *, MenuManager_FindFont, (void *mm, const char *name));
ORIG(0x418EA0, const char *, MenuManager_GetString, (void *mm, const char *name));
ORIG(0x418FE0, void, MenuManager_FocusItem, (void *mm, void *item));
ORIG(0x419030, int, MenuManager_GetFade, (void *mm));
ORIG(0x4190D0, void, MenuManager_MarkBusy, (void *mm));
ORIG(0x4B6CE0, int, Sprite_Width, (Sprite *s));
ORIG(0x4B6D00, int, Sprite_Height, (Sprite *s));
ORIG(0x4B6E90, void, Text_SetString, (Text *t, const char *s));
ORIG(0x4B6EC0, Text *, Text_New, (Sprite *s, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (Text *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));
ORIG(0x4B7DF0, int, Text_GetWidth, (Text *t));
ORIG(0x4B7E00, int, Text_GetHeight, (Text *t));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));

/* next sibling, or NULL at the end of the list */
static inline Task *next_sibling(Task *t)
{
    return t->next && t->next->next ? t->next : NULL;
}

/* calls the handler for the slot slot_fn() returns on each child of t (not
   deeper), keeping the next child on gTaskStack so handlers may delete tasks.
   A macro: gcc drops calls through ORIG pointers passed to inline functions. */
#define CALL_CHILDREN(t, slot_fn)                                       \
    do {                                                                \
        Task *c_ = (t)->head;                                           \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = HANDLER(slot_fn(), c_->type);                         \
            if (fn_)                                                    \
                ((void (*)(void *))fn_)(c_);                            \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* calls the MenuWindow handler for a slot, like a base class method */
#define CALL_BASE(slot, ...) ((void (*)())HANDLER(slot, MenuWindow_GetType()))(__VA_ARGS__)

/* calls a task's own handler for a slot */
#define CALL_OWN(slot, t) ((void (*)(void *))HANDLER(slot, ((Task *)(t))->type))(t)

/* runs an expression if the string holds one, dropping the result */
#define EVAL(v, t)                              \
    do {                                        \
        if (VarString_HasExpr(v))               \
            VarString_EvalReal(v, t);           \
    } while (0)

/* round to float, even when the compiler keeps extended precision */
static inline float rf(long double x)
{
    volatile float f = x;
    return f;
}

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

static inline void str_copy(char *d, const char *s)
{
    while ((*d++ = *s++) != 0)
        ;
}

/* the alpha elements fade with: (fade - 8) / 8, not below 0 */
#define FADE_ALPHA(menu) fade_alpha(MenuManager_GetFade(menu))

static inline float fade_alpha(int fade)
{
    float a = (fade - 8) * 0.125f;

    return a > 0.0f ? a : 0.0f;
}

/* screen position of a point in 640x480 menu space */
#define SCREEN_X(x) ((Display_GetMode()->width >> 1) - 320 + (x))
#define SCREEN_Y(y) ((Display_GetMode()->height >> 1) - 240 + (y))

/* an image that can be clicked, used for the arrows of ListBox and SlideBar */
typedef struct MenuButton {
    MenuWindow w;
    char sprite[0x40];
    Sprite *image;
} MenuButton;

_Static_assert(sizeof(MenuButton) == 0x98, "MenuButton");

MenuButton *MenuButton_New(void *parent);

#endif
