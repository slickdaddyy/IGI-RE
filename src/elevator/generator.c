#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "elevator/elevator.h"

/* hums while on; with backup set it switches itself back on after a delay */
typedef struct Generator {
    Task task;
    Vec3d pos;
    char unk38[0x190 - 0x38];
    char unk190;
    char unk191[0x238 - 0x191];
    Task *sound;
    char isOn;
    char registered;
    char pad23E[2];
    VarString cond;
    char onSound[16];
    char offSound[16];
    float backupTime;
    char backup;
    char pad2B5[3];
    int backupTimer;
    char pad2BC[4];
} Generator;

_Static_assert(offsetof(Generator, cond) == 0x240, "Generator");
_Static_assert(offsetof(Generator, backupTimer) == 0x2B8, "Generator");
_Static_assert(sizeof(Generator) == 0x2C0, "Generator");

extern uint16_t gGeneratorType;
extern void *gGeneratorParams;

static const float kFrames = 30.0f;

void Generator_Create(Generator *g, void *arg);
void Generator_Run(Generator *g);
void Generator_Delete(Generator *g);
void Generator_GetParams(void *task, void **out);
void Generator_Changed(Generator *g, void *arg);
void Generator_Activate(Generator *g);
void Generator_Deactivate(Generator *g);

void Generator_Init(void)
{
    Generator g;

    gGeneratorType = TaskType_New("Generator", sizeof(Generator), ExplodeObject_GetType(), 0);
    TaskType_SetHandler(gGeneratorType, 0, Generator_Create);
    TaskType_SetHandler2(gGeneratorType, 1, Generator_Run);
    TaskType_SetHandler2(gGeneratorType, 2, Generator_Delete);
    TaskType_SetHandler(gGeneratorType, Param_GetTableEvent(), Generator_GetParams);
    TaskType_SetHandler(gGeneratorType, Param_GetChangedEvent(), Generator_Changed);
    TaskType_SetHandler2(gGeneratorType, Level_GetActivateEvent(), Generator_Activate);
    TaskType_SetHandler2(gGeneratorType, Level_GetDeactivateEvent(), Generator_Deactivate);
    gGeneratorParams = ParamTable_Extend(ExplodeObject_GetParams(), 5);
    ParamTable_Add(gGeneratorParams, &g, g.onSound, Param_String16(), 7, NULL);
    ParamTable_Add(gGeneratorParams, &g, g.offSound, Param_String16(), 7, NULL);
    ParamTable_Add(gGeneratorParams, &g, &g.cond, Param_VarString(), 7, NULL);
    ParamTable_Add(gGeneratorParams, &g, &g.backup, Param_Bool8(), 7, NULL);
    ParamTable_Add(gGeneratorParams, &g, &g.backupTime, Param_Real32(), 7, NULL);
}
HOOK(0x43B2F0, Generator_Init);

void Generator_Create(Generator *g, void *arg)
{
    HANDLER(0, ExplodeObject_GetType())(g, arg);
    g->sound = NULL;
    g->isOn = 0;
    g->registered = 0;
    VarString_Init(&g->cond);
    g->onSound[0] = 0;
    g->offSound[0] = 0;
    g->backupTime = 10.0f;
    g->backup = 1;
    g->backupTimer = -1;
}
HOOK(0x43B490, Generator_Create);

void Generator_Run(Generator *g)
{
    int wasOn = g->isOn;
    Task *s;

    ((void (*)(void *))HANDLER(1, ExplodeObject_GetType()))(g);
    if (g->unk190)
        g->isOn = 0;
    else if (VarString_HasExpr(&g->cond))
        g->isOn = (char)round_ll(VarString_EvalReal(&g->cond, g));
    if (g->backup) {
        if (!g->isOn && g->backupTimer == -1)
            g->backupTimer = (int)round_ll(g->backupTime * kFrames);
        if (g->backupTimer > 0)
            g->backupTimer--;
        else
            g->isOn = 1;
    }
    if (g->isOn) {
        if (g->sound || !g->onSound[0])
            return;
        g->sound = Sound_Play(g, g->onSound);
        if (g->sound)
            Sound_SetPos(g->sound, &g->pos);
        return;
    }
    if (g->sound) {
        Sound_Stop(g->sound);
        g->sound = NULL;
    }
    if (wasOn) {
        s = Sound_Play(g, g->offSound);
        if (s)
            Sound_SetPos(s, &g->pos);
    }
}
HOOK(0x43B500, Generator_Run);

void Generator_Delete(Generator *g)
{
    ((void (*)(void *))HANDLER(2, ExplodeObject_GetType()))(g);
    VarString_Free(&g->cond);
}
HOOK(0x43B650, Generator_Delete);

void Generator_GetParams(void *task, void **out)
{
    *out = gGeneratorParams;
}
HOOK(0x43B680, Generator_GetParams);

void Generator_Changed(Generator *g, void *arg)
{
    int slot = Param_GetChangedEvent();

    HANDLER(slot, ExplodeObject_GetType())(g, arg);
    VarString_Bind(&g->cond, g);
}
HOOK(0x43B690, Generator_Changed);

void Generator_Activate(Generator *g)
{
    char name[64];
    char buf[128];
    uint16_t base;

    if (g->registered)
        return;
    g->registered = 1;
    base = ExplodeObject_GetType();
    ((void (*)(void *))HANDLER(Level_GetActivateEvent(), base))(g);
    Task_GetName(name, g);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RegisterBool8(buf, &g->isOn);
    crt_sprintf(buf, "%s.nBackupTimer", name);
    Script_RegisterInt(buf, &g->backupTimer);
}
HOOK(0x43B6E0, Generator_Activate);

void Generator_Deactivate(Generator *g)
{
    char name[64];
    char buf[128];
    uint16_t base;

    if (!g->registered)
        return;
    base = ExplodeObject_GetType();
    ((void (*)(void *))HANDLER(Level_GetDeactivateEvent(), base))(g);
    Task_GetName(name, g);
    crt_sprintf(buf, "%s.isOn", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nBackupTimer", name);
    Script_RemoveSymbol(buf);
    g->registered = 0;
}
HOOK(0x43B790, Generator_Deactivate);

void Generator_Close(void)
{
    QTaskList_Free(gGeneratorParams);
    TaskType_Free(gGeneratorType);
}
HOOK(0x43B830, Generator_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53C2FC) uint16_t gGeneratorType INIT(385);
PAD(igd, 53C2FC, 53C2FE, 0x2);
BSS(57BF3C) void *gGeneratorParams;
