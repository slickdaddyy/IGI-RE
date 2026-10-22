#pragma GCC optimize("excess-precision=standard")

#include "glass/glass.h"
#include "hook.h"

ORIG(0x4F0FE0, void *, Param_String32, (void));

/* "Siren" (0x2D8), an ExplodeObject that loops a sound while its script is on */
typedef struct Siren {
    ExplodeObj e;
    char sound_name[0x20];  /* 0x238 */
    char broken_name[0x20]; /* 0x258, played after it exploded */
    VarString script;   /* 0x278 */
    Task *sound;        /* 0x2C8 */
    Task *broken;       /* 0x2CC */
    char on;            /* 0x2D0 */
    char last_on;       /* 0x2D1 */
    char registered;    /* 0x2D2 */
    char unk2D3[5];
} Siren;

_Static_assert(offsetof(Siren, sound) == 0x2C8, "Siren");
_Static_assert(sizeof(Siren) == 0x2D8, "Siren");

#define gSirenType VAR(uint16_t, 0x57C214)
#define gSirenParams VAR(void *, 0x57C21C)

void Siren_Create(Siren *s, void *arg);
void Siren_Run(Siren *s);
void Siren_Delete(Siren *s);
void Siren_GetParamsHandler(Task *t, void **out);
void Siren_ParamsChanged(Siren *s, void *arg);
void Siren_AddSymbols(Siren *s);
void Siren_RemoveSymbols(Siren *s);
void Siren_GetSoundSource(Siren *s, void **out);

#define FIELD(name) ((void *)offsetof(Siren, name))

void Siren_Create(Siren *s, void *arg)
{
    HANDLER(0, ExplodeObject_GetType())(s, arg);
    ExplodeObject_Setup(s, 1.0f, 0.5f, 0.5f, 1.0f, 0, 5, 4, "explosion_1");
    s->on = 0;
    s->last_on = 0;
    s->registered = 0;
    s->sound_name[0] = 0;
    s->broken_name[0] = 0;
    s->sound = NULL;
    s->broken = NULL;
    VarString_Init(&s->script);
}
HOOK(0x4492A0, Siren_Create);

void Siren_Delete(Siren *s)
{
    HANDLER(2, ExplodeObject_GetType())(s);
    VarString_Free(&s->script);
}
HOOK(0x449330, Siren_Delete);

void Siren_Open(void)
{
    gSirenType = TaskType_New("Siren", sizeof(Siren), ExplodeObject_GetType(), 0);
    TaskType_SetHandler(gSirenType, 0, Siren_Create);
    TaskType_SetHandler2(gSirenType, 1, Siren_Run);
    TaskType_SetHandler2(gSirenType, 2, Siren_Delete);
    TaskType_SetHandler(gSirenType, Param_GetTableEvent(), Siren_GetParamsHandler);
    TaskType_SetHandler(gSirenType, Param_GetChangedEvent(), Siren_ParamsChanged);
    TaskType_SetHandler2(gSirenType, Level_GetEventDA8(), Siren_AddSymbols);
    TaskType_SetHandler2(gSirenType, Level_GetEventDAC(), Siren_RemoveSymbols);
    TaskType_SetHandler(gSirenType, Sound_GetInfoEvent(), Siren_GetSoundSource);
    gSirenParams = ParamTable_Extend(ExplodeObject_GetParams(), 3);
    ParamTable_Add(gSirenParams, NULL, FIELD(sound_name), Param_String32(), 7, NULL);
    ParamTable_Add(gSirenParams, NULL, FIELD(broken_name), Param_String32(), 7, NULL);
    ParamTable_Add(gSirenParams, NULL, FIELD(script), Param_VarString(), 7, NULL);
}
HOOK(0x449360, Siren_Open);

static void stop(Task **sound)
{
    if (*sound) {
        Sound_Stop(*sound);
        *sound = NULL;
    }
}

/* plays the normal sound while on, the broken one once exploded */
void Siren_Run(Siren *s)
{
    HANDLER(1, ExplodeObject_GetType())(s);
    s->last_on = s->on;
    s->on = round_int(VarString_EvalReal(&s->script, s));
    if (!s->on && !s->e.exploded)
        stop(&s->sound);
    if (!s->on) {
        if (s->e.exploded)
            stop(&s->broken);
        return;
    }
    if (!s->e.exploded && !s->sound)
        s->sound = Sound_Play(s, s->sound_name);
    if (s->e.exploded) {
        stop(&s->sound);
        if (!s->broken)
            s->broken = Sound_Play(s, s->broken_name);
    }
}
HOOK(0x4494D0, Siren_Run);

void Siren_GetParamsHandler(Task *t, void **out)
{
    *out = gSirenParams;
}
HOOK(0x449600, Siren_GetParamsHandler);

void Siren_ParamsChanged(Siren *s, void *arg)
{
    uint8_t slot = Param_GetChangedEvent();

    HANDLER(slot, ExplodeObject_GetType())(s, arg);
    VarString_Bind(&s->script, s);
}
HOOK(0x449610, Siren_ParamsChanged);

void Siren_AddSymbols(Siren *s)
{
    char name[64], buf[128];
    uint16_t type;

    if (s->registered)
        return;
    s->registered = 1;
    type = ExplodeObject_GetType();
    gTaskHandlers[Level_GetEventDA8()][type](s);
    Task_GetName(name, s);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &s->on);
    crt_sprintf(buf, "%s.isLastOn", name);
    Script_RegisterBool8(buf, &s->last_on);
}
HOOK(0x449660, Siren_AddSymbols);

void Siren_RemoveSymbols(Siren *s)
{
    char name[64], buf[128];
    uint16_t type;

    if (!s->registered)
        return;
    type = ExplodeObject_GetType();
    gTaskHandlers[Level_GetEventDAC()][type](s);
    Task_GetName(name, s);
    s->registered = 0;
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastOn", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x449710, Siren_RemoveSymbols);

void Siren_GetSoundSource(Siren *s, void **out)
{
    out[0] = &s->e.o.pos;
    out[1] = &s->e.o.matrix;
    out[2] = NULL;
}
HOOK(0x4497B0, Siren_GetSoundSource);

void Siren_Close(void)
{
    TaskType_Free(gSirenType);
    QTaskList_Free(gSirenParams);
}
HOOK(0x4497D0, Siren_Close);
