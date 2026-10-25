#include "hook.h"
#include "vehbase/vehbase.h"
#include "param/paramvars.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));

/* deletes every child of owner that answers the weapon state event */
void Weapon_DeleteAll(Task *owner)
{
    Task *t = owner->head;

    if (!t->next)
        return;
    while (t) {
        void *state = NULL;
        TaskFn fn = HANDLER(gWeaponStateEvent, t->type);

        if (fn) {
            fn(t, &state);
            if (state) {
                Task_Delete(t);
                t = owner->head;
                if (!t->next)
                    return;
                continue;
            }
        }
        t = t->next;
        if (!t || !t->next)
            return;
    }
}
HOOK(0x4F2290, Weapon_DeleteAll);

void Weapon_Move(Task *w, Vec3d *pos, Matrix *m)
{
    MoveArg msg = {pos, m};

    HANDLER(gWeaponMoveEvent, w->type)(w, &msg);
}
HOOK(0x4F2310, Weapon_Move);

void Weapon_UseAmmo(Task *w, char full)
{
    HANDLER(gWeaponUseAmmoEvent, w->type)(w, &full);
}
HOOK(0x4F2350, Weapon_UseAmmo);

/* returns the pickup left behind (w itself if the handler does not answer) */
Task *Weapon_Drop(Task *w)
{
    Task *t = w;

    HANDLER(gWeaponDropEvent, t->type)(t, &w);
    return w;
}
HOOK(0x4F2390, Weapon_Drop);

void Weapon_GetAmmo(Task *w, int *out)
{
    HANDLER(gWeaponAmmoEvent, w->type)(w, out);
}
HOOK(0x4F23D0, Weapon_GetAmmo);
