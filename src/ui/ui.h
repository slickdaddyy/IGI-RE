#ifndef UI_UI_H
#define UI_UI_H

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

typedef struct Text {
    void *grid;
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
    char unk48[2];
    char held;
    char clicked;
    char selectable;
    char focused;
    char unk4E[2];
    int key;
} MenuWindow;

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

/* a script string parameter that may hold an expression ("$...") */
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

_Static_assert(sizeof(MenuWindow) == 0x54, "MenuWindow");
_Static_assert(sizeof(VarString) == 0x50, "VarString");

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define gEmptyString ((char *)0x567C74)

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
ORIG(0x419070, int, MenuExpr_Eval, (VarString *v, void *task));
ORIG(0x424850, uint16_t, MenuWindow_GetType, (void));
ORIG(0x424870, int, MenuWindow_GetEvent914, (void));
ORIG(0x424880, int, MenuWindow_GetEvent918, (void));
ORIG(0x424890, int, MenuWindow_GetEvent91C, (void));
ORIG(0x4248A0, void, MenuWindow_HitTest, (void *w, int x, int y, int width, int height, int *mx, int *my));
ORIG(0x424980, void, MenuWindow_DrawEnd, (void *w, float unk, float alpha, int focused));
ORIG(0x424B60, MenuWindow *, MenuWindow_New, (void *parent, void *menu));
ORIG(0x41A120, void, MenuWindow_ForwardEvent918, (void *task));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4E7180, void, Sound_PlayName, (const char *name, int flags, void *params));
ORIG(0x491CF0, Display *, Display_Get, (void));
ORIG(0x4028B0, int, Timer_GetTicks, (void));
ORIG(0x402870, int, Timer_GetFrequency, (void));
ORIG(0x48A910, int, Keys_GetCount, (void));
ORIG(0x48A8D0, int, Keys_Pop, (void));
ORIG(0x418D70, void, MenuManager_Free, (void *task));
ORIG(0x418DE0, Sprite *, MenuManager_FindSprite, (void *mm, const char *name));
ORIG(0x418E40, Sprite *, MenuManager_FindFont, (void *mm, const char *name));
ORIG(0x418EA0, const char *, MenuManager_GetString, (void *mm, const char *name));
ORIG(0x418FB0, MenuInput *, MenuManager_GetInput, (void *mm));
ORIG(0x418FC0, void, MenuManager_Pause, (void *mm));
ORIG(0x418FD0, void, MenuManager_Resume, (void *mm));
ORIG(0x419030, int, MenuManager_GetFade, (void *mm));
ORIG(0x4190B0, void, MenuManager_DisableInput, (void *mm));
ORIG(0x4190C0, void, MenuManager_EnableInput, (void *mm));
ORIG(0x4201E0, MenuWindow *, SlideBar_NewVertical, (void *parent, void *menu));
ORIG(0x420240, void, ScrollBar_SetRange, (void *bar, int x, int y, int unk, int h, int count));
ORIG(0x420290, int, ScrollBar_GetPos, (void *bar));
ORIG(0x4202D0, void, ScrollBar_SetPos, (void *bar, int pos));
ORIG(0x4B6E90, void, Text_SetString, (Text *t, const char *s));
ORIG(0x4B6EC0, Text *, Text_New, (Sprite *s, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (Text *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));
ORIG(0x4B7DF0, int, Text_GetWidth, (Text *t));
ORIG(0x4B7E00, int, Text_GetHeight, (Text *t));
ORIG(0x4B7050, int, Sprite_StringWidth, (Sprite *s, const char *str));
ORIG(0x4B70F0, int, Sprite_GetHeight, (Sprite *s, const char *str));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));

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

/* the alpha all elements fade with: (fade - 8) / 8, not below 0 */
static inline float ui_alpha(void *menu)
{
    float a = (MenuManager_GetFade(menu) - 8) * 0.125f;

    return a > 0.0f ? a : 0.0f;
}

/* fistp with the current rounding mode, like the original */
static inline int round_int(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

#endif
