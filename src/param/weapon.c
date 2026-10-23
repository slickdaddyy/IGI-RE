#include "hook.h"
#include "param/param.h"

ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x401810, int, TaskEvent_Alloc2, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018E0, void, TaskEvent_Free2, (int index));

#define gWeaponSelectEvent VAR(int, 0xA76BFC)
#define gWeaponTriggerEvent VAR(uint8_t, 0xA76C00)
#define gWeaponEvent3 VAR(uint8_t, 0xA76C01)
#define gWeaponSlotsEvent VAR(uint8_t, 0xA76C02)
#define gWeaponStateEvent VAR(uint8_t, 0xA76C03)
#define gWeaponDeselectEvent VAR(int, 0xA76C04)
#define gWeaponEvent7 VAR(uint8_t, 0xA76C08)
#define gWeaponMoveEvent VAR(uint8_t, 0xA76C09)
#define gWeaponEvent4 VAR(uint8_t, 0xA76C10)

void Weapon_InitEvents(void)
{
    gWeaponSlotsEvent = TaskEvent_Alloc(0);
    gWeaponStateEvent = TaskEvent_Alloc(0);
    gWeaponMoveEvent = TaskEvent_Alloc(0);
    gWeaponTriggerEvent = TaskEvent_Alloc(0);
    gWeaponEvent3 = TaskEvent_Alloc(0);
    gWeaponEvent4 = TaskEvent_Alloc(0);
    gWeaponSelectEvent = TaskEvent_Alloc2(0);
    gWeaponDeselectEvent = TaskEvent_Alloc2(0);
    gWeaponEvent7 = TaskEvent_Alloc(0);
}
HOOK(0x4F1EE0, Weapon_InitEvents);

void Weapon_FreeEvents(void)
{
    TaskEvent_Free(gWeaponSlotsEvent);
    TaskEvent_Free(gWeaponStateEvent);
    TaskEvent_Free(gWeaponEvent4);
    TaskEvent_Free(gWeaponEvent3);
    TaskEvent_Free(gWeaponTriggerEvent);
    TaskEvent_Free(gWeaponMoveEvent);
    TaskEvent_Free2(gWeaponSelectEvent);
    TaskEvent_Free2(gWeaponDeselectEvent);
    TaskEvent_Free(gWeaponEvent7);
}
HOOK(0x4F1F50, Weapon_FreeEvents);

char Weapon_GetSlotsEvent(void)
{
    return gWeaponSlotsEvent;
}
HOOK(0x4F1FC0, Weapon_GetSlotsEvent);

char Weapon_GetStateEvent(void)
{
    return gWeaponStateEvent;
}
HOOK(0x4F1FD0, Weapon_GetStateEvent);

char Weapon_GetMoveEvent(void)
{
    return gWeaponMoveEvent;
}
HOOK(0x4F1FE0, Weapon_GetMoveEvent);

char Weapon_GetTriggerEvent(void)
{
    return gWeaponTriggerEvent;
}
HOOK(0x4F1FF0, Weapon_GetTriggerEvent);

char Weapon_GetEvent3(void)
{
    return gWeaponEvent3;
}
HOOK(0x4F2000, Weapon_GetEvent3);

char Weapon_GetEvent4(void)
{
    return gWeaponEvent4;
}
HOOK(0x4F2010, Weapon_GetEvent4);

int Weapon_GetSelectEvent(void)
{
    return gWeaponSelectEvent;
}
HOOK(0x4F2020, Weapon_GetSelectEvent);

int Weapon_GetDeselectEvent(void)
{
    return gWeaponDeselectEvent;
}
HOOK(0x4F2030, Weapon_GetDeselectEvent);

char Weapon_GetEvent7(void)
{
    return gWeaponEvent7;
}
HOOK(0x4F2040, Weapon_GetEvent7);

void WeaponSlots_Init(Task **slots)
{
    slots[0] = NULL;
    slots[1] = NULL;
    slots[2] = NULL;
}
HOOK(0x4F2050, WeaponSlots_Init);

Task *WeaponSlots_Get(Task **slots, int i)
{
    return slots[i];
}
HOOK(0x4F2060, WeaponSlots_Get);

/* deselects the weapon in slot i and selects w in its place */
void WeaponSlots_Set(Task **slots, int i, Task *w)
{
    Task *old = WeaponSlots_Get(slots, i);
    WeaponState *state;

    if (old) {
        ((TaskArgFn)HANDLER(gWeaponStateEvent, old))(old, &state);
        state->selected = 0;
        if (HANDLER(gWeaponDeselectEvent, old))
            ((TaskFn)HANDLER(gWeaponDeselectEvent, old))(old);
    }
    if (w) {
        ((TaskArgFn)HANDLER(gWeaponStateEvent, w))(w, &state);
        state->selected = 1;
        ((TaskFn)HANDLER(gWeaponSelectEvent, w))(w);
    }
    slots[i] = w;
}
HOOK(0x4F2070, WeaponSlots_Set);

static char fits_slot(Task *t, int i)
{
    WeaponState *state = NULL;

    if (!HANDLER(gWeaponStateEvent, t))
        return 0;
    ((TaskArgFn)HANDLER(gWeaponStateEvent, t))(t, &state);
    return state && state->slot == i && !state->unk08;
}

static Task *first_child(Task *owner)
{
    Task *t = (Task *)owner->children.head;

    return t->node.next ? t : NULL;
}

/* Puts the owner's next child weapon for slot i (after the current one,
   wrapping around) into the slot. Returns 1 when one was found. */
char WeaponSlots_SelectNext(Task *owner, Task **slots, int i)
{
    Task *cur = WeaponSlots_Get(slots, i);
    Task *t;

    if (!cur) {
        t = (Task *)owner->children.head;
        if (!t->node.next || !t)
            return 0;
        for (;;) {
            if (fits_slot(t, i)) {
                WeaponSlots_Set(slots, i, t);
                return 1;
            }
            t = (Task *)t->node.next;
            if (!t || !t->node.next || !t)
                return 0;
        }
    }
    t = (Task *)cur->node.next;
    if (!t || !t->node.next || !t)
        t = first_child(owner);
    while (t != cur) {
        if (fits_slot(t, i)) {
            WeaponSlots_Set(slots, i, t);
            return 1;
        }
        t = (Task *)t->node.next;
        if (!t || !t->node.next || !t)
            t = first_child(owner);
    }
    return 0;
}
HOOK(0x4F2140, WeaponSlots_SelectNext);
