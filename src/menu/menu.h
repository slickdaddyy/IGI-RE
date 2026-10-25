#ifndef MENU_MENU_H
#define MENU_MENU_H

#include <stddef.h>
#include <stdint.h>

#include "igi.h"
#include "qvertex/qvertextypes.h"
#include "menu/menutypes.h"
#include "widget/widgettypes.h"
#include "core/coretypes.h"
#include "task/taskvars.h"

typedef void (*TaskFn)(void *task, void *arg);

extern Task *gTaskStack[];
extern int gTaskStackTop;
extern RenderMode gRenderModeCurrent;
extern struct Texture *gQVertexTexture;
extern int gQVertexStrip;
extern struct Palette *gQVertexPalette;

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
ORIG(0x424870, int, MenuWindow_GetLoadEvent, (void));
ORIG(0x424880, int, MenuWindow_GetSaveEvent, (void));
ORIG(0x424890, int, MenuWindow_GetUpdateTextEvent, (void));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4D3710, void, Task_Nothing, (void *task));
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
void MenuWindow_ForwardSave(Task *t);

#endif
