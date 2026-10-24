#include <string.h>

#include "hook.h"
#include "editcam/editcam.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x504E00, uint8_t, MenuItemTask_GetPointerEvent, (void));

extern uint8_t gMenuItemCommitEvent;
extern uint8_t gMenuItemSelectEvent;
extern uint8_t gMenuItemFindEvent;
extern uint16_t gMenuItemType;

typedef void (*MenuItemFn)(MenuItem *item, void *ctx);

typedef struct MenuItemSearch {
    int id;
    MenuItem *found;
    int unk8;
} MenuItemSearch;

uint8_t MenuItem_GetFindEvent(void);
uint8_t MenuItem_GetSelectEvent(void);
uint8_t MenuItem_GetCommitEvent(void);
int MenuItem_Move(Task *panel, int dir);

/* selected follows pressed, then the children get the event */
void MenuItem_Commit(MenuItem *item, void *arg)
{
    item->selected = item->pressed ? 1 : item->selected;
    CALL_CHILDREN(item, MenuItem_GetCommitEvent(), arg);
    item->pressed = 0;
}
HOOK(0x505710, MenuItem_Commit);

/* the item with the event's id is pressed, all others lose the selection;
   direction keys move the selection instead */
void MenuItem_Select(MenuItem *item, MenuItemEvent *ev)
{
    if (ev->id == item->id) {
        if (ev->keys & 1) {
            MenuItem_Move(item->task.parent, -3);
            return;
        }
        if (ev->keys & 2) {
            MenuItem_Move(item->task.parent, -4);
            return;
        }
        if (ev->keys & 4) {
            MenuItem_Move(item->task.parent, -1);
            return;
        }
        if (ev->keys & 8) {
            MenuItem_Move(item->task.parent, -2);
            return;
        }
        item->pressed = 1;
    } else {
        item->selected = 0;
    }
    if (item->onEvent)
        item->onEvent(item, ev);
}
HOOK(0x5057C0, MenuItem_Select);

void MenuItem_Identify(MenuItem *item, MenuItem **out)
{
    *out = item;
}
HOOK(0x505840, MenuItem_Identify);

void MenuItem_ForwardPointer(MenuItem *item, void *arg)
{
    CALL_CHILDREN(item, MenuItemTask_GetPointerEvent(), arg);
}
HOOK(0x505850, MenuItem_ForwardPointer);

void MenuItem_Shutdown(void)
{
    TaskEvent_Free(gMenuItemFindEvent);
    TaskEvent_Free(gMenuItemSelectEvent);
    TaskEvent_Free(gMenuItemCommitEvent);
    TaskType_Free(gMenuItemType);
    gMenuItemType = 0x181;
}
HOOK(0x5058E0, MenuItem_Shutdown);

/* calls fn for every child of the panel that answers the find event */
void MenuItem_ForEach(Task *panel, MenuItemFn fn, void *ctx)
{
    MenuItem *found = NULL;
    Task *c = panel->head;
    Task *next;

    if (!c->next || !c)
        return;
    do {
        next = next_sibling(c);
        if (HANDLER(MenuItem_GetFindEvent(), c->type)) {
            HANDLER(MenuItem_GetFindEvent(), c->type)(c, &found);
            if (found) {
                fn(found, ctx);
                found = NULL;
            }
        }
        c = next;
    } while (c);
}
HOOK(0x505920, MenuItem_ForEach);

void MenuItem_MatchId(MenuItem *item, MenuItemSearch *s);
void MenuItem_MatchSelected(MenuItem *item, MenuItemSearch *s);
void MenuItem_MatchFirst(MenuItem *item, MenuItemSearch *s);

MenuItem *MenuItem_FindById(Task *panel, int id)
{
    MenuItemSearch s = {id, NULL, 0};

    MenuItem_ForEach(panel, (MenuItemFn)MenuItem_MatchId, &s);
    return s.found;
}
HOOK(0x5059D0, MenuItem_FindById);

void MenuItem_MatchId(MenuItem *item, MenuItemSearch *s)
{
    if (item->id == s->id)
        s->found = item;
}
HOOK(0x505A10, MenuItem_MatchId);

MenuItem *MenuItem_FindSelected(Task *panel)
{
    MenuItemSearch s = {0, NULL, 0};

    MenuItem_ForEach(panel, (MenuItemFn)MenuItem_MatchSelected, &s);
    return s.found;
}
HOOK(0x505A30, MenuItem_FindSelected);

void MenuItem_MatchSelected(MenuItem *item, MenuItemSearch *s)
{
    if (item->selected)
        s->found = item;
}
HOOK(0x505A60, MenuItem_MatchSelected);

MenuItem *MenuItem_FindFirst(Task *panel)
{
    MenuItemSearch s = {0, NULL, 0};

    MenuItem_ForEach(panel, (MenuItemFn)MenuItem_MatchFirst, &s);
    return s.found;
}
HOOK(0x505A80, MenuItem_FindFirst);

void MenuItem_MatchFirst(MenuItem *item, MenuItemSearch *s)
{
    if (!s->found)
        s->found = item;
}
HOOK(0x505AB0, MenuItem_MatchFirst);

/* dir > 0 selects the item with that id, -1 .. -4 follow a link from the
   selected item, skipping disabled ones */
int MenuItem_Move(Task *panel, int dir)
{
    MenuItem *cur = MenuItem_FindSelected(panel);
    MenuItem *it;
    MenuItemEvent ev;

    if (dir >= 1) {
        it = MenuItem_FindById(panel, dir);
        if (it->disabled)
            return 0;
    } else {
        it = MenuItem_FindSelected(panel);
        if (!it) {
            it = MenuItem_FindFirst(panel);
        } else {
            for (;;) {
                int id;

                switch (dir) {
                case -4:
                    id = it->link[1];
                    break;
                case -3:
                    id = it->link[0];
                    break;
                case -2:
                    id = it->link[3];
                    break;
                case -1:
                    id = it->link[2];
                    break;
                default:
                    Error_Show("MenuItem_Move() called with illegal direction parameter.");
                    for (;;)
                        ;
                }
                if (!id)
                    return 0;
                it = MenuItem_FindById(panel, id);
                if (!it->disabled)
                    break;
            }
        }
    }
    if (!it)
        return 0;
    memset(&ev, 0, sizeof(ev));
    ev.id = it->id;
    HANDLER(MenuItem_GetSelectEvent(), it->task.type)(it, &ev);
    if (cur)
        HANDLER(MenuItem_GetSelectEvent(), cur->task.type)(cur, &ev);
    return 1;
}
HOOK(0x505AD0, MenuItem_Move);

void MenuItem_Setup(MenuItem *item, int id, int link0, int link1, int link2, int link3, char disabled, TaskFn onEvent)
{
    item->id = id;
    item->link[1] = link1;
    item->link[0] = link0;
    item->link[3] = link3;
    item->link[2] = link2;
    item->selected = 0;
    item->pressed = 0;
    item->disabled = disabled;
    item->onEvent = onEvent;
}
HOOK(0x505C10, MenuItem_Setup);

uint8_t MenuItem_GetFindEvent(void)
{
    return gMenuItemFindEvent;
}
HOOK(0x505C50, MenuItem_GetFindEvent);

uint8_t MenuItem_GetSelectEvent(void)
{
    return gMenuItemSelectEvent;
}
HOOK(0x505C60, MenuItem_GetSelectEvent);

uint8_t MenuItem_GetCommitEvent(void)
{
    return gMenuItemCommitEvent;
}
HOOK(0x505C70, MenuItem_GetCommitEvent);

uint16_t MenuItem_GetType(void)
{
    return gMenuItemType;
}
HOOK(0x505C80, MenuItem_GetType);
