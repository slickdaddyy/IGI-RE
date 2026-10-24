#include "hook.h"
#include "statgun/statgun.h"

/* the reload message (Human event 1) */
typedef struct ReloadMsg {
    void *h;
    char unk4[0x1C];
    char done;
} ReloadMsg;

#define gSpas12Type VAR(uint16_t, 0x5BE350)

ORIG(0x489F20, void, HumanPlayer_PlayAnim, (void *h, int anim, int loop, float blend));
ORIG(0x489F50, char, HumanPlayer_AnimDone, (void *h));

void Spas12_Reload(Gun *g, ReloadMsg *msg);
void Spas12_Create(Gun *g);

void Spas12_Open(void)
{
    gSpas12Type = TaskType_New("GunSPAS12", sizeof(Gun), Gun_GetType(), 0);
    TaskType_SetHandler(gSpas12Type, Human_GetEvent1(), Spas12_Reload);
    TaskType_SetHandler(gSpas12Type, 0, Spas12_Create);
    Script_RegisterConstant("TASKTYPE_GUNSPAS12", gSpas12Type);
}
HOOK(0x472A70, Spas12_Open);

/* loads one shell per animation until the clip is full or the ammo runs out */
void Spas12_Reload(Gun *g, ReloadMsg *msg)
{
    void *h = msg->h;
    WeaponInfo *info = g->info;
    int *ammoId = g->ammoId;
    Task *owner = g->obj.task.parent;
    void *list = NULL;
    AmmoEntry *ammo;
    TaskFn fn;
    int need;

    fn = HANDLER(AmmoList_GetEvent(), owner->type);
    if (fn)
        fn(owner, &list);
    ammo = AmmoList_Find(list, *ammoId);
    msg->done = 0;
    if ((!ammo || !ammo->count) && !g->reload) {
        msg->done = 1;
        return;
    }
    switch (g->reload) {
    case 0:
        HumanPlayer_PlayAnim(h, 0x3D, 0, 0x1.99999ap-2f);
        g->reload = 1;
        break;
    case 1:
        if (HumanPlayer_AnimDone(h)) {
            HumanPlayer_PlayAnim(h, 0x3E, 0, 0.0f);
            g->reload = 2;
        }
        break;
    case 2:
        if (HumanPlayer_AnimDone(h)) {
            need = info->clipSize - g->rounds;
            if (need && ammo->count) {
                ammo->count--;
                g->rounds++;
                if (need - 1) {
                    HumanPlayer_PlayAnim(h, 0x3E, 0, 0.0f);
                    break;
                }
            }
            HumanPlayer_PlayAnim(h, 0x3F, 0, 0.0f);
            g->reload = 3;
        }
        break;
    case 3:
        if (HumanPlayer_AnimDone(h)) {
            msg->done = 1;
            g->reload = 0;
        }
        break;
    }
}
HOOK(0x472AE0, Spas12_Reload);

void Spas12_Create(Gun *g)
{
    HANDLER(0, Gun_GetType())(g, 0);
    memcpy(g->id, "WEAPON_ID_SPAS12", 17);
}
HOOK(0x472CA0, Spas12_Create);

void Spas12_Close(void)
{
    TaskType_Free(gSpas12Type);
}
HOOK(0x472CF0, Spas12_Close);
