#ifndef MENU_MENU_H
#define MENU_MENU_H

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

typedef struct Sprite {
    void *palette;
    void *textures;
    void *frames;
    void *map;
    int ntextures;
    unsigned short nframes;
    unsigned char format;
} Sprite;

typedef struct Text {
    void *grid;
    float x, y, z;
    float alpha;
    float color[3];
    int unk20;
    unsigned int flags;
} Text;

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

/* the "MenuWindow" task (0x54 bytes), base of every menu element */
typedef struct MenuWindow {
    Task task;
    int x, y, w, h;
    char unk30[0xC];
    int unk3C;
    int unk40;
    struct MenuManager *menu;
    char unk48[4];
    char selectable;
    char focused;
    char unk4E[2];
    int key;
} MenuWindow;

typedef struct MenuSprite {
    char name[0x40];
    Sprite sprite;
} MenuSprite;

typedef struct MenuPic {
    char name[0x40];
    Text *text;
} MenuPic;

/* a string looked up by MenuManager_GetString that has no resource */
typedef struct MenuString {
    char name[0x40];
    char text[0x44];
    struct MenuString *next;
} MenuString;

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

typedef struct MenuManager {
    Task task;
    int screen;
    int popup;
    int state;
    int nextState;
    MenuWindow *screenTask;
    MenuWindow *popupTask;
    MenuWindow *focus;
    MenuWindow *items[32];
    int itemCount;
    int itemIndex;
    int fadeShown;
    int fadeShownPrev;
    int fade;
    int drawFade;
    char resPath[0x100];
    void *menuRes;
    void *missionRes;
    void *langRes[32];
    int langResCount;
    MenuString *strings;
    MenuSprite fonts[16];
    MenuSprite sprites[64];
    MenuPic pics[32];
    short spriteCount;
    short fontCount;
    short picCount;
    short pad266A;
    char unk266C[0x4C];
    int savedFrames;
    char unk26BC;
    char active;
    char unk26BE;
    char soundsLoaded;
    char unk26C0;
    char fastFade;
    char inputEnabled;
    char idleTimeout;
    char busy;
    char scriptName[0x100];
    char pad27C5[3];
    Task *pointer;
    MenuInput input;
    Task *backgroundFX;
    int language;
    int depth;
    int screenStack[16];
    char ownsDisplay;
    char pad282D[3];
    int music[4];
    char unk2840[0x58];
} MenuManager;

_Static_assert(sizeof(MenuWindow) == 0x54, "MenuWindow");
_Static_assert(offsetof(MenuManager, resPath) == 0xD4, "MenuManager.resPath");
_Static_assert(offsetof(MenuManager, fonts) == 0x264, "MenuManager.fonts");
_Static_assert(offsetof(MenuManager, pics) == 0x1DE4, "MenuManager.pics");
_Static_assert(offsetof(MenuManager, unk266C) == 0x266C, "MenuManager.unk266C");
_Static_assert(offsetof(MenuManager, scriptName) == 0x26C5, "MenuManager.scriptName");
_Static_assert(offsetof(MenuManager, input) == 0x27CC, "MenuManager.input");
_Static_assert(offsetof(MenuManager, screenStack) == 0x27EC, "MenuManager.screenStack");
_Static_assert(sizeof(MenuManager) == 0x2898, "MenuManager");

extern TaskFn gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern void (*gRenderFuncs[])(QVertexStrip *);
extern void *gQVertexTexture;
extern int gQVertexStrip;
extern void *gQVertexPalette;
extern char gEmptyString[];

/* the handler a task type has for a slot */
#define HANDLER(slot, type) gTaskHandlers[slot][(uint16_t)(type)]

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x424850, uint16_t, MenuWindow_GetType, (void));
ORIG(0x424870, int, MenuWindow_GetEvent914, (void));
ORIG(0x424880, int, MenuWindow_GetEvent918, (void));
ORIG(0x424890, int, MenuWindow_GetEvent91C, (void));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
ORIG(0x4C1780, void, sub_4C1780, (void));
ORIG(0x491CF0, int *, Display_GetMode, (void));

/* round to float, even when the compiler keeps extended precision */
static inline float rf(long double x)
{
    volatile float f = x;
    return f;
}

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

int MenuManager_GetFade(MenuManager *mm);
void MenuManager_MarkBusy(MenuManager *mm);
const char *MenuManager_GetString(MenuManager *mm, const char *name);
Sprite *MenuManager_FindFont(MenuManager *mm, const char *name);
uint16_t MenuManager_GetType(void);
MenuManager *MenuManager_New(Task *parent, const char *script, char ownsDisplay, char start, char idleTimeout);
Task *BackgroundFX_New(Task *parent);
int DialogWindow_IsHorizontal(MenuWindow *w);
void MenuWindow_ForwardEvent918(Task *t);

#endif
