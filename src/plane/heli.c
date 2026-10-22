#include "hook.h"
#include "igi.h"
#include "plane/plane.h"

ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));
ORIG(0x4F45F0, uint16_t, Vehicle_GetType, (void));
ORIG(0x4F46E0, void, Vehicle_SaveState, (void *obj, void **msg));
ORIG(0x4F4780, void, Vehicle_LoadState, (void *obj, void **msg));
ORIG(0x5031F0, void, Vehicle_ParseConfig, (VehicleConfig *cfg, int *next, void *args));

#define gHeliType VAR(uint16_t, 0x53BE58)
#define gHeliParams VAR(void *, 0x57BD40)

void Heli_LoadState(Heli *h, void **msg)
{
    Vehicle_LoadState(h, msg);
}
HOOK(0x432F20, Heli_LoadState);

void Heli_SaveState(Heli *h, void **msg)
{
    char *state = msg[0];

    Vehicle_SaveState(h, msg);
    *(int *)(state + 0x70) = 0;
}
HOOK(0x432F40, Heli_SaveState);

void Heli_ParseConfig(HeliConfig *cfg, int *next, void *args);

void Heli_ParseArgs(Task *t, void **msg)
{
    (void)t;
    Heli_ParseConfig(msg[0], (int *)&msg[2], msg[1]);
}
HOOK(0x432F60, Heli_ParseArgs);

void Heli_GetConfigSize(Task *t, int *out)
{
    (void)t;
    *out = sizeof(HeliConfig);
}
HOOK(0x432F80, Heli_GetConfigSize);

/* the vehicle's own handler first, then the rotor sound goes */
void Heli_RemoveFromWorld(Heli *h, void *arg)
{
    uint8_t slot = World_GetEvent1();

    ((TaskFn2)TASK_HANDLER(slot, Vehicle_GetType()))(h, arg);
    if (h->sound) {
        if (*h->sound)
            Sound_Stop(*h->sound);
        TaskRef_Remove(h->sound);
        h->sound = NULL;
    }
}
HOOK(0x432F90, Heli_RemoveFromWorld);

void Heli_GetSoundInfo(Heli *h, void **out)
{
    *out = h->unk8C0;
}
HOOK(0x433000, Heli_GetSoundInfo);

void Heli_Close(void)
{
    QTaskList_Free(gHeliParams);
    TaskType_Free(gHeliType);
}
HOOK(0x433010, Heli_Close);

/* the second list overlaps the first when that has more than three entries */
void Heli_ParseConfig(HeliConfig *cfg, int *next, void *args)
{
    int i;

    Vehicle_ParseConfig(&cfg->v, next, args);
    cfg->count1 = Script_GetArgInt(args, (*next)++);
    for (i = 0; i < cfg->count1; i++)
        cfg->list[i] = Script_GetArgInt(args, (*next)++);
    cfg->count2 = Script_GetArgInt(args, (*next)++);
    for (i = 0; i < cfg->count2; i++)
        cfg->list[3 + i] = Script_GetArgInt(args, (*next)++);
}
HOOK(0x433030, Heli_ParseConfig);
