/* MenuItemTask: a panel that collects its MenuItem children each frame and
   sends them the pointer: the item under it gets the select event. */
#include "hook.h"
#include "editobj/editobj.h"

FN_EXTERN(MenuItem_Commit);
FN_EXTERN(MenuItem_ForwardPointer);
FN_EXTERN(MenuItem_Identify);
FN_EXTERN(MenuItem_Select);
FN_EXTERN(MenuWindow_Draw);
FN_EXTERN(SubLevel_Run);

/* the list node of a MenuItem (at +0x54 of the item) */
typedef struct ItemNode {
    struct ItemNode *next;
    struct ItemNode *prev;
    float x, y, w, h;
    int unk18[2];
    Task *item;
} ItemNode;

typedef struct ItemList {
    ItemNode *head;
    ItemNode *tail;
    ItemNode *tailpred;
} ItemList;

/* "MenuItemTask" (0x44) */
typedef struct MenuItemTask {
    Task task;
    ItemList items;
    int x, y, buttons;
    int oldX, oldY, oldButtons;
} MenuItemTask;

_Static_assert(offsetof(ItemNode, item) == 0x20, "ItemNode");
_Static_assert(sizeof(MenuItemTask) == 0x44, "MenuItemTask");

/* the id of a MenuItem */
#define ITEM_ID(t) (*(int *)((char *)(t) + 0x38))

ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4AF8F0, void, List_AddTail, (ItemList *list, ItemNode *node));
ORIG(0x4E7530, int, Screen_GetDrawEvent, (void));
ORIG(0x505A30, Task *, MenuItem_FindSelected, (MenuItemTask *t));
ORIG(0x505C50, uint8_t, MenuItem_GetFindEvent, (void));
ORIG(0x505C60, uint8_t, MenuItem_GetSelectEvent, (void));
ORIG(0x505C70, uint8_t, MenuItem_GetCommitEvent, (void));
ORIG(0x508F60, uint8_t, Sync_GetEvent0, (void));
ORIG(0x508F70, uint8_t, Sync_GetEvent1, (void));

#define MENUWINDOW_DRAW (FN(0x424280, MenuWindow_Draw))
#define TASK_RUNCHILDREN (FN(0x488700, SubLevel_Run))
#define MENUITEM_COMMIT (FN(0x505710, MenuItem_Commit))
#define MENUITEM_SELECT (FN(0x5057C0, MenuItem_Select))
#define MENUITEM_IDENTIFY (FN(0x505840, MenuItem_Identify))
#define MENUITEM_FORWARDPOINTER (FN(0x505850, MenuItem_ForwardPointer))

extern uint16_t gMenuItemTaskType;
extern uint8_t gMenuItemTaskPointerEvent;
extern uint8_t gMenuItemTaskListEvent;
extern uint8_t gMenuItemCommitEvent;
extern uint8_t gMenuItemSelectEvent;
extern uint8_t gMenuItemFindEvent;
extern uint16_t gMenuItemType;

/* list event handler of the items: adds node to the panel's list */
void MenuItemTask_AddToList(ItemList **list, ItemNode *node)
{
    node->prev = NULL;
    node->next = NULL;
    List_AddTail(*list, node);
}
HOOK(0x504DC0, MenuItemTask_AddToList);

uint8_t MenuItemTask_GetListEvent(void)
{
    return gMenuItemTaskListEvent;
}
HOOK(0x504DF0, MenuItemTask_GetListEvent);

uint8_t MenuItemTask_GetPointerEvent(void)
{
    return gMenuItemTaskPointerEvent;
}
HOOK(0x504E00, MenuItemTask_GetPointerEvent);

void MenuItemTask_Run(MenuItemTask *t);
void MenuItemTask_SetPointer(MenuItemTask *t, int *pos);
void MenuItemTask_Forward508F70(MenuItemTask *t, void *arg);
void MenuItemTask_Forward508F60(MenuItemTask *t, void *arg);
void MenuItemTask_ForwardList(MenuItemTask *t, void *arg);

void MenuItemTask_Open(void)
{
    gMenuItemTaskType = TaskType_New("MenuItemTask", sizeof(MenuItemTask), Task_GetRootType(), 0, 0);
    gMenuItemTaskListEvent = TaskEvent_Alloc(0);
    gMenuItemTaskPointerEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler2(gMenuItemTaskType, 1, MenuItemTask_Run);
    TaskType_SetHandler2(gMenuItemTaskType, Screen_GetDrawEvent(), MENUWINDOW_DRAW);
    TaskType_SetHandler(gMenuItemTaskType, Sync_GetEvent1(), MenuItemTask_Forward508F70);
    TaskType_SetHandler(gMenuItemTaskType, Sync_GetEvent0(), MenuItemTask_Forward508F60);
    TaskType_SetHandler(gMenuItemTaskType, MenuItemTask_GetListEvent(), MenuItemTask_ForwardList);
    TaskType_SetHandler(gMenuItemTaskType, MenuItemTask_GetPointerEvent(), MenuItemTask_SetPointer);
}
HOOK(0x504E10, MenuItemTask_Open);

/* Collects the items, then sends the select event with the id of the item
   under the pointer (or 0 to everyone when there is none) */
void MenuItemTask_Run(MenuItemTask *t)
{
    ItemList *list;
    ItemNode *n, *next;
    Task *item;
    int x, y, dx, dy, id;

    CALL_CHILDREN(&t->task, MenuItem_GetCommitEvent(), NULL);
    t->items.tailpred = (ItemNode *)&t->items;
    t->items.tail = NULL;
    t->items.head = (ItemNode *)&t->items.tail;
    CALL_CHILDREN(&t->task, 1);
    list = &t->items;
    CALL_CHILDREN(&t->task, MenuItemTask_GetListEvent(), &list);
    x = t->x;
    y = t->y;
    dx = x - t->oldX;
    dy = y - t->oldY;
    id = 0;
    if (!dx && !dy && !(t->buttons & 0x70)) {
        item = MenuItem_FindSelected(t);
        if (item) {
            id = ITEM_ID(item);
            CALL_CHILDREN(item->parent, MenuItem_GetSelectEvent(), &id);
        }
        return;
    }
    item = MenuItem_FindSelected(t);
    if (!dx && !dy && item) {
        id = ITEM_ID(item);
        CALL_CHILDREN(item->parent, MenuItem_GetSelectEvent(), &id);
        return;
    }
    for (n = t->items.head; (next = n->next); n = next) {
        int cx = (int)round_ll(n->x);
        int cy;

        if (cx <= x) {
            int r = (int)round_ll((long double)n->w + n->x);

            cx = r < x ? r : x;
        }
        cy = (int)round_ll(n->y);
        if (cy <= y) {
            int r = (int)round_ll((long double)n->h + n->y);

            cy = r < y ? r : y;
        }
        if (cx == x && cy == y) {
            id = ITEM_ID(n->item);
            CALL_CHILDREN(n->item->parent, MenuItem_GetSelectEvent(), &id);
        }
    }
    if (!id)
        CALL_CHILDREN(&t->task, MenuItem_GetSelectEvent(), &id);
}
HOOK(0x504EF0, MenuItemTask_Run);

void MenuItemTask_SetPointer(MenuItemTask *t, int *pos)
{
    t->oldX = t->x;
    t->oldY = t->y;
    t->oldButtons = t->buttons;
    t->x = pos[0];
    t->y = pos[1];
    t->buttons = pos[2];
}
HOOK(0x5053B0, MenuItemTask_SetPointer);

void MenuItemTask_Forward508F70(MenuItemTask *t, void *arg)
{
    CALL_CHILDREN(&t->task, Sync_GetEvent1(), arg);
}
HOOK(0x5053F0, MenuItemTask_Forward508F70);

void MenuItemTask_Forward508F60(MenuItemTask *t, void *arg)
{
    CALL_CHILDREN(&t->task, Sync_GetEvent0(), arg);
}
HOOK(0x505480, MenuItemTask_Forward508F60);

void MenuItemTask_ForwardList(MenuItemTask *t, void *arg)
{
    CALL_CHILDREN(&t->task, MenuItemTask_GetListEvent(), arg);
}
HOOK(0x505510, MenuItemTask_ForwardList);

void MenuItemTask_Close(void)
{
    TaskEvent_Free(gMenuItemTaskPointerEvent);
    TaskEvent_Free(gMenuItemTaskListEvent);
    TaskType_Free(gMenuItemTaskType);
}
HOOK(0x5055A0, MenuItemTask_Close);

void MenuItemTask_SendPointer(MenuItemTask *t, int *pos)
{
    gTaskHandlers[MenuItemTask_GetPointerEvent()][t->task.type](t, pos);
}
HOOK(0x5055D0, MenuItemTask_SendPointer);

MenuItemTask *MenuItemTask_New(void *parent)
{
    MenuItemTask *t = (MenuItemTask *)Task_New(parent, gMenuItemTaskType, NULL);

    t->x = 0;
    t->y = 0;
    t->buttons = 0;
    t->oldX = 0;
    t->oldY = 0;
    t->oldButtons = 0;
    return t;
}
HOOK(0x505600, MenuItemTask_New);

void MenuItem_Open(void)
{
    gMenuItemFindEvent = TaskEvent_Alloc(0);
    gMenuItemSelectEvent = TaskEvent_Alloc(0);
    gMenuItemCommitEvent = TaskEvent_Alloc(0);
    gMenuItemType = TaskType_New("MenuItem", 0x54, Task_GetRootType(), 0, 0);
    TaskType_SetHandler2(gMenuItemType, 1, TASK_RUNCHILDREN);
    TaskType_SetHandler2(gMenuItemType, Screen_GetDrawEvent(), MENUWINDOW_DRAW);
    TaskType_SetHandler(gMenuItemType, MenuItem_GetFindEvent(), MENUITEM_IDENTIFY);
    TaskType_SetHandler(gMenuItemType, MenuItem_GetSelectEvent(), MENUITEM_SELECT);
    TaskType_SetHandler(gMenuItemType, MenuItemTask_GetPointerEvent(), MENUITEM_FORWARDPOINTER);
    TaskType_SetHandler(gMenuItemType, MenuItem_GetCommitEvent(), MENUITEM_COMMIT);
}
HOOK(0x505630, MenuItem_Open);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E708) uint16_t gMenuItemTaskType INIT(385);
DATA(54E708) uint8_t gMenuItemTaskPointerEvent INIT(255);
DATA(54E708) uint8_t gMenuItemTaskListEvent INIT(255);
DATA(54E720) uint16_t gMenuItemType INIT(385);
PAD(igd, 54E720, 54E722, 0x2);
