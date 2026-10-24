#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "elevator/elevator.h"

FN_EXTERN(Terminal_MapHandler);

#define HANDLER_443DC0 (FN(0x443DC0, Terminal_MapHandler))

ORIG(0x467D90, uint8_t, ComputerObject_GetEvent0, (void));

/* plays a random one of four sounds every few seconds while on */
typedef struct Radio {
    Task task;
    Vec3d pos;
    char unk38[0x190 - 0x38];
    char unk190;
    char unk191[0x238 - 0x191];
    Task **sound;
    int timer;
    char isOn;
    char registered;
    char pad242[2];
    VarString cond;
    char sounds[4][16];
    char offSound[16];
    char pad2E4[4];
} Radio;

_Static_assert(offsetof(Radio, cond) == 0x244, "Radio");
_Static_assert(offsetof(Radio, offSound) == 0x2D4, "Radio");
_Static_assert(sizeof(Radio) == 0x2E8, "Radio");

#define gRadioType VAR(uint16_t, 0x53C2E0)
#define gRadioParams VAR(void *, 0x57BEF8)
#define gRadioSoundNames ((char (*)[15])0x57BEFC)

void Radio_Create(Radio *r, void *arg);
void Radio_Run(Radio *r);
void Radio_Delete(Radio *r);
void Radio_GetParams(void *task, void **out);
void Radio_Changed(Radio *r, void *arg);
void Radio_Activate(Radio *r);
void Radio_Deactivate(Radio *r);

void Radio_Init(void)
{
    Radio r;
    int i;

    gRadioType = TaskType_New("Radio", sizeof(Radio), ExplodeObject_GetType(), 0);
    TaskType_SetHandler(gRadioType, 0, Radio_Create);
    TaskType_SetHandler2(gRadioType, 1, Radio_Run);
    TaskType_SetHandler2(gRadioType, 2, Radio_Delete);
    TaskType_SetHandler(gRadioType, Param_GetTableEvent(), Radio_GetParams);
    TaskType_SetHandler(gRadioType, Param_GetChangedEvent(), Radio_Changed);
    TaskType_SetHandler2(gRadioType, Level_GetEventDA8(), Radio_Activate);
    TaskType_SetHandler2(gRadioType, Level_GetEventDAC(), Radio_Deactivate);
    TaskType_SetHandler(gRadioType, ComputerObject_GetEvent0(), HANDLER_443DC0);
    gRadioParams = ParamTable_Extend(ExplodeObject_GetParams(), 6);
    for (i = 0; i < 4; i++) {
        crt_sprintf(gRadioSoundNames[i], "Radio sound %d", i);
        ParamTable_Add(gRadioParams, &r, r.sounds[i], Param_String16(), 7, NULL);
    }
    ParamTable_Add(gRadioParams, &r, r.offSound, Param_String16(), 7, NULL);
    ParamTable_Add(gRadioParams, &r, &r.cond, Param_VarString(), 7, NULL);
}
HOOK(0x43AD80, Radio_Init);

void Radio_Create(Radio *r, void *arg)
{
    int i;

    HANDLER(0, ExplodeObject_GetType())(r, arg);
    r->sound = NULL;
    r->timer = 0;
    r->isOn = 0;
    r->registered = 0;
    VarString_Init(&r->cond);
    for (i = 0; i < 4; i++)
        r->sounds[i][0] = 0;
    r->offSound[0] = 0;
}
HOOK(0x43AF20, Radio_Create);

void Radio_Run(Radio *r)
{
    int wasOn = r->isOn;
    Task *s;
    int i;

    ((void (*)(void *))HANDLER(1, ExplodeObject_GetType()))(r);
    if (r->unk190)
        r->isOn = 0;
    else if (VarString_HasExpr(&r->cond))
        r->isOn = (char)round_ll(VarString_EvalReal(&r->cond, r));
    if (!r->isOn) {
        if (r->sound) {
            if (*r->sound)
                Sound_Stop(*r->sound);
            TaskRef_Remove(r->sound);
            r->sound = NULL;
        }
        if (wasOn) {
            s = Sound_Play(r, r->offSound);
            if (s)
                Sound_SetPos(s, &r->pos);
        }
        return;
    }
    if (r->sound) {
        if (*r->sound)
            return;
        TaskRef_Remove(r->sound);
        r->sound = NULL;
    }
    if (r->timer) {
        r->timer--;
        return;
    }
    i = Random_Int(Game_GetRandom(), 4);
    s = Sound_Play(r, r->sounds[i]);
    r->timer = Random_Int(Game_GetRandom(), 150) + 150;
    if (s) {
        r->sound = TaskRef_Add(s);
        Sound_SetPos(s, &r->pos);
    }
}
HOOK(0x43AF90, Radio_Run);

void Radio_Delete(Radio *r)
{
    ((void (*)(void *))HANDLER(2, ExplodeObject_GetType()))(r);
    if (r->sound) {
        TaskRef_Remove(r->sound);
        r->sound = NULL;
    }
    VarString_Free(&r->cond);
}
HOOK(0x43B110, Radio_Delete);

void Radio_GetParams(void *task, void **out)
{
    *out = gRadioParams;
}
HOOK(0x43B160, Radio_GetParams);

void Radio_Changed(Radio *r, void *arg)
{
    int slot = Param_GetChangedEvent();

    HANDLER(slot, ExplodeObject_GetType())(r, arg);
    r->isOn = 0;
    r->timer = 0;
    VarString_Bind(&r->cond, r);
}
HOOK(0x43B170, Radio_Changed);

void Radio_Activate(Radio *r)
{
    char name[64];
    char buf[128];
    uint16_t base;

    if (r->registered)
        return;
    r->registered = 1;
    base = ExplodeObject_GetType();
    ((void (*)(void *))HANDLER(Level_GetEventDA8(), base))(r);
    Task_GetName(name, r);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &r->isOn);
}
HOOK(0x43B1C0, Radio_Activate);

void Radio_Deactivate(Radio *r)
{
    char name[64];
    char buf[128];
    uint16_t base;

    if (!r->registered)
        return;
    base = ExplodeObject_GetType();
    ((void (*)(void *))HANDLER(Level_GetEventDAC(), base))(r);
    Task_GetName(name, r);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    r->registered = 0;
}
HOOK(0x43B250, Radio_Deactivate);

void Radio_Close(void)
{
    QTaskList_Free(gRadioParams);
    TaskType_Free(gRadioType);
}
HOOK(0x43B2D0, Radio_Close);
