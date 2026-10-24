#ifndef MISSION_MISSION_H
#define MISSION_MISSION_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "script/scripttypes.h"
#include "qvertex/qvertextypes.h"
#include "win/wintypes.h"
#include "res/restypes.h"
#include "levelflow/levelflowtypes.h"

typedef void (*TaskFn)(void *task, void *arg);

/* a "StatusMessage" task (0x1E0 bytes): a text shown on the status screen when cond becomes true */
typedef struct StatusMessage {
    Task task;
    Vec3d pos;
    Matrix matrix;
    VarString cond;
    char *text;
    char portrait[0x100];
    char sound[0x10];
    char once;
    char shown;
    char plain;
    char pad1C7;
    int tickSent;
    int tickFinished;
    int ticksSinceFinished;
    char isSent;
    char registered;
    char isFinished;
    char pad1D7;
    float duration;
    int pad1DC;
} StatusMessage;

/* one of the 24 lines of the message screen */
typedef struct StatusLine {
    Task **msg;
    int ticks;
    int shownTicks;
    char *cursor;
    Text *texts[3];
    Sprite *portrait;
    char used;
    char pad21[3];
    float width;
    float height;
    char text[0x1000];
} StatusLine;

#define STATUS_LINES 24

/* "StatusMessageScreen" task (0x18440 bytes) */
typedef struct StatusMessageScreen {
    Task task;
    StatusLine lines[STATUS_LINES];
} StatusMessageScreen;

/* "StatusScreen" task (0x4F0 bytes): the in-game HUD */
typedef struct StatusScreen {
    Task task;
    char scene[0x50];
    void *res;
    char sprites[0x38C - 0x74];
    Sprite portraits[4];
    Sprite actions[10];
    Text *timeText;
    Text *testText;
    StatusMessageScreen *messages;
    float blink;
    int unk4EC;
} StatusScreen;

_Static_assert(sizeof(StatusMessage) == 0x1E0, "StatusMessage");
_Static_assert(offsetof(StatusMessage, cond) == 0x60, "StatusMessage.cond");
_Static_assert(offsetof(StatusMessage, once) == 0x1C4, "StatusMessage.once");
_Static_assert(offsetof(StatusMessage, duration) == 0x1D8, "StatusMessage.duration");
_Static_assert(sizeof(StatusLine) == 0x102C, "StatusLine");
_Static_assert(sizeof(StatusMessageScreen) == 0x18440, "StatusMessageScreen");
_Static_assert(offsetof(StatusScreen, portraits) == 0x38C, "StatusScreen.portraits");
_Static_assert(offsetof(StatusScreen, timeText) == 0x4DC, "StatusScreen.timeText");
_Static_assert(sizeof(StatusScreen) == 0x4F0, "StatusScreen");

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern const char gEmptyString[];
extern LevelFlow *gLevelFlow;

#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

/* the next child of a broadcast, NULL after the last one */
#define NEXT_CHILD(c) ((c)->next && (c)->next->next ? (c)->next : NULL)

/* calls handler slot of each child of t (not deeper); slot is evaluated
   for each child after the push, like the original */
#define CALL_CHILDREN(t, slot, ...)                                     \
    do {                                                                \
        Task *c_ = ((Task *)(t))->head;                                 \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            TaskFn fn_;                                                 \
            gTaskStack[gTaskStackTop++] = NEXT_CHILD(c_);               \
            fn_ = gTaskHandlers[slot][c_->type];                        \
            if (fn_)                                                    \
                ((void (*)())fn_)(c_, ##__VA_ARGS__);                   \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode */
static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* fcomp then "test ah, 0x40": equal, or unordered */
static inline int x87_eq(long double a, long double b)
{
    return !(a < b || a > b);
}

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (int unk));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (void *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x416D40, char *, Game_Get, (void));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B0D60, void, Mem_PushMode, (char mode));
ORIG(0x4B0D90, void, Mem_PopMode, (void));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int *size));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4B6E90, void, Text_SetString, (Text *t, const char *s));
ORIG(0x4B6EC0, Text *, Text_New, (void *font, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (Text *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));
ORIG(0x4B6CE0, int, Sprite_Width, (void *s));
ORIG(0x4B6D00, int, Sprite_Height, (void *s));
ORIG(0x4B7E00, int, Text_GetHeight, (Text *t));
ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

StatusScreen *StatusScreen_Get(void);
const char **StatusScreen_GetPortraitNames(void);
Task *StatusScreen_New(void *parent);
StatusMessageScreen *StatusMessageScreen_New(void *parent);
void StatusMessageScreen_AddMessage(StatusMessageScreen *s, StatusMessage *m);
void StatusMessageScreen_Clear(StatusMessageScreen *s);

#endif
