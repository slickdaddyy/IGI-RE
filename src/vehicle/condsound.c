#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "vehicle/vehicle.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497450, int, Sound_HashBucket, (const char *name));
ORIG(0x4974A0, void, SoundParams_Init, (void *p));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C30, void, Sound_SetPos, (Task *sound, int *params));
ORIG(0x4E6CE0, void, Sound_SetGlobal, (Task *sound, int on));
ORIG(0x4E6D60, void, SoundHandle_Play, (void *stream));
ORIG(0x4E6FA0, char, SoundHandle_IsPlaying, (void *stream));

/* plays a sound while a script condition is true */
typedef struct ConditionalSound {
    Task task;
    VarString cond;
    char name[0x20];
    int params[6];
    char stream;
    char once;
    char pad[2];
    int nPlayTick;
    char isRun;
    char isLastRun;
    char isPlaying;
    char loop;
    char registered;
    char padB5[3];
    int streamA;
    int streamB;
    int streamC;
    int streamD;
    char streamData[0x58];
    Task **sound;
    char played;
} ConditionalSound;

_Static_assert(offsetof(ConditionalSound, streamA) == 0xB8, "ConditionalSound");
_Static_assert(sizeof(ConditionalSound) == 0x128, "ConditionalSound");

extern uint16_t gConditionalSoundType;
extern void *gConditionalSoundParams;

void ConditionalSound_Create(ConditionalSound *cs);
void ConditionalSound_Run(ConditionalSound *cs);
void ConditionalSound_Delete(ConditionalSound *cs);
void ConditionalSound_GetParams(void *task, void **out);
void ConditionalSound_Changed(ConditionalSound *cs);
void ConditionalSound_Activate(ConditionalSound *cs);
void ConditionalSound_Deactivate(ConditionalSound *cs);
void ConditionalSound_Reset(ConditionalSound *cs);

void ConditionalSound_Init(void)
{
    ConditionalSound cs;

    gConditionalSoundType = TaskType_New("ConditionalSound", sizeof(ConditionalSound), Task_GetRootType(), 0);
    TaskType_SetHandler(gConditionalSoundType, 0, ConditionalSound_Create);
    TaskType_SetHandler2(gConditionalSoundType, 1, ConditionalSound_Run);
    TaskType_SetHandler2(gConditionalSoundType, 2, ConditionalSound_Delete);
    TaskType_SetHandler(gConditionalSoundType, Param_GetTableEvent(), ConditionalSound_GetParams);
    TaskType_SetHandler(gConditionalSoundType, Param_GetChangedEvent(), ConditionalSound_Changed);
    TaskType_SetHandler2(gConditionalSoundType, Level_GetActivateEvent(), ConditionalSound_Activate);
    TaskType_SetHandler2(gConditionalSoundType, Level_GetDeactivateEvent(), ConditionalSound_Deactivate);
    TaskType_SetHandler(gConditionalSoundType, World_GetEvent1(), ConditionalSound_Reset);
    gConditionalSoundParams = ParamTable_New(6);
    ParamTable_Add(gConditionalSoundParams, &cs, &cs.cond, Param_VarString(), 7, NULL);
    ParamTable_Add(gConditionalSoundParams, &cs, cs.name, Param_String32(), 7, NULL);
    ParamTable_Add(gConditionalSoundParams, &cs, cs.params, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gConditionalSoundParams, &cs, &cs.stream, Param_Bool8(), 7, NULL);
    ParamTable_Add(gConditionalSoundParams, &cs, &cs.once, Param_Bool8(), 7, NULL);
    ParamTable_Add(gConditionalSoundParams, &cs, &cs.loop, Param_Bool8(), 7, NULL);
}
HOOK(0x424E80, ConditionalSound_Init);

void ConditionalSound_Create(ConditionalSound *cs)
{
    int i;

    VarString_Init(&cs->cond);
    for (i = 0; i < 6; i++)
        cs->params[i] = 0;
    cs->name[0] = 0;
    cs->stream = 0;
    cs->once = 0;
    cs->isRun = 0;
    cs->isLastRun = 0;
    cs->isPlaying = 0;
    cs->registered = 0;
    cs->loop = 0;
    cs->nPlayTick = -1;
    cs->sound = NULL;
    cs->played = 0;
}
HOOK(0x425050, ConditionalSound_Create);

void ConditionalSound_UpdatePlaying(ConditionalSound *cs);
void ConditionalSound_Stop(ConditionalSound *cs);
void ConditionalSound_Play(ConditionalSound *cs);

void ConditionalSound_Run(ConditionalSound *cs)
{
    if (VarString_HasExpr(&cs->cond)) {
        cs->isLastRun = cs->isRun;
        cs->isRun = (char)round_int(VarString_EvalReal(&cs->cond, cs));
    }
    ConditionalSound_UpdatePlaying(cs);
    if (cs->once && cs->played)
        return;
    if (!cs->isRun)
        ConditionalSound_Stop(cs);
    else if (!cs->isLastRun)
        ConditionalSound_Play(cs);
    CALL_CHILDREN(&cs->task, 1);
}
HOOK(0x4250D0, ConditionalSound_Run);

void ConditionalSound_Stop(ConditionalSound *cs)
{
    if (!cs->sound)
        return;
    if (*cs->sound)
        Task_Delete(*cs->sound);
    TaskRef_Remove(cs->sound);
    cs->sound = NULL;
    cs->isPlaying = 0;
}
HOOK(0x4251C0, ConditionalSound_Stop);

void ConditionalSound_Play(ConditionalSound *cs)
{
    Task *sound;

    ConditionalSound_Stop(cs);
    if (cs->stream) {
        cs->streamA = Sound_HashBucket(cs->name);
        cs->streamB = Sound_HashName(cs->name);
        cs->streamC = 0;
        cs->streamD = 0;
        SoundParams_Init(cs->streamData);
        SoundHandle_Play(&cs->streamA);
        cs->played = 1;
        cs->isPlaying = 1;
        return;
    }
    sound = Sound_Play(cs, cs->name);
    if (!sound)
        return;
    cs->sound = TaskRef_Add(sound);
    if (cs->loop)
        Sound_SetGlobal(sound, 1);
    else
        Sound_SetPos(sound, cs->params);
    cs->played = 1;
    cs->isPlaying = 1;
}
HOOK(0x425200, ConditionalSound_Play);

void ConditionalSound_UpdatePlaying(ConditionalSound *cs)
{
    if (cs->stream) {
        cs->isPlaying = SoundHandle_IsPlaying(&cs->streamA);
        if (cs->isPlaying)
            SoundHandle_Play(&cs->streamA);
    } else if (cs->sound) {
        cs->isPlaying = *cs->sound != NULL;
    }
}
HOOK(0x4252E0, ConditionalSound_UpdatePlaying);

void ConditionalSound_Delete(ConditionalSound *cs)
{
    ConditionalSound_Stop(cs);
    if (Task_GetId(cs) >= 0) {
        TaskFn fn = HANDLER(Level_GetDeactivateEvent(), cs->task.type);

        if (fn)
            ((void (*)(void *))fn)(cs);
    }
    VarString_Free(&cs->cond);
}
HOOK(0x425340, ConditionalSound_Delete);

void ConditionalSound_GetParams(void *task, void **out)
{
    *out = gConditionalSoundParams;
}
HOOK(0x425390, ConditionalSound_GetParams);

void ConditionalSound_Changed(ConditionalSound *cs)
{
    VarString_Bind(&cs->cond, cs);
}
HOOK(0x4253A0, ConditionalSound_Changed);

/* makes the state visible to scripts as <name>.isRun and so on */
void ConditionalSound_Activate(ConditionalSound *cs)
{
    char name[64];
    char buf[128];

    if (cs->registered)
        return;
    cs->registered = 1;
    Task_GetName(name, cs);
    crt_sprintf(buf, "%s.isRun", name);
    Script_RegisterBool8(buf, &cs->isRun);
    crt_sprintf(buf, "%s.isLastRun", name);
    Script_RegisterBool8(buf, &cs->isLastRun);
    crt_sprintf(buf, "%s.isPlaying", name);
    Script_RegisterBool8(buf, &cs->isPlaying);
    crt_sprintf(buf, "%s.nPlayTick", name);
    Script_RegisterInt2(buf, &cs->nPlayTick);
}
HOOK(0x4253C0, ConditionalSound_Activate);

void ConditionalSound_Deactivate(ConditionalSound *cs)
{
    char name[64];
    char buf[128];

    if (!cs->registered)
        return;
    Task_GetName(name, cs);
    cs->registered = 0;
    crt_sprintf(buf, "%s.isRun", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastRun", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isPlaying", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nPlayTick", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x4254A0, ConditionalSound_Deactivate);

void ConditionalSound_Reset(ConditionalSound *cs)
{
    ConditionalSound_Stop(cs);
    cs->isRun = 0;
    cs->isLastRun = 0;
}
HOOK(0x425560, ConditionalSound_Reset);

void ConditionalSound_Close(void)
{
    TaskType_Free(gConditionalSoundType);
    QTaskList_Free(gConditionalSoundParams);
}
HOOK(0x425580, ConditionalSound_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BC68) uint16_t gConditionalSoundType;
PAD(igb, 57BC68, 57BC6A, 0x2);
BSS(57BC68) void *gConditionalSoundParams;
