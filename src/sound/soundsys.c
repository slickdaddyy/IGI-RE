#include <string.h>

#include "hook.h"
#include "igi.h"
#include "sound/sound.h"
#include "qasm/qasmtypes.h"
#include "sound/soundtypes.h"
#include "define/definetypes.h"

FN_EXTERN(DefineGraph);
FN_EXTERN(DefineGroup);
FN_EXTERN(DefineSound);
FN_EXTERN(DefineTriggerOnce);

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x517580, char *, Str_ToUpper, (char *s));
ORIG(0x4B1420, void, File_ScanDir, (const char *dir, void *fn, void *user));
ORIG(0x4B1E90, void, Path_GetExtension, (char *out, int size, const char *path));
ORIG(0x4B1EE0, void, Path_GetDir, (char *out, int size, const char *path));
ORIG(0x4B5B60, int, Resource_Exists, (const char *name));
ORIG(0x4B5D80, void, Resource_ForEach, (const char *dir, void *fn, void *user));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int flags));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, int unk, int nargs));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4B8930, void, Script_SetSymbolContext, (const char *name, void *context));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1800, void *, QTaskList_New, (int capacity));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x495CB0, char, Sound_IsReady, (void));
ORIG(0x495CC0, void, Sound_SetUpdateRate, (int rate));
ORIG(0x495DF0, void, Stream_Update, (void));
ORIG(0x495F70, void *, Sound_Load, (const char *path, void *data));
ORIG(0x4965A0, void, Sound_Unload, (const char *path));
ORIG(0x496650, void, Voice_Free, (void *voice));
ORIG(0x4966E0, char, Voice_IsPlaying, (void *voice));
ORIG(0x496E40, void, Voice_Stop, (void *voice));
ORIG(0x496E60, void, Voice_SetParams, (void *voice, void *params));
ORIG(0x497390, void, Sound_GetName, (char *out, const char *path));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497510, int, Sound_GetVoiceCount, (void));
ORIG(0x4E7350, void, sub_4E7350, (int unk));

/* script functions of the sound definitions, not rewritten yet */
#define DefineGroup (FN(0x4EBA90, DefineGroup))
#define DefineSound (FN(0x4E8C60, DefineSound))
#define DefineGraph (FN(0x4EAD20, DefineGraph))
#define DefineTriggerOnce (FN(0x4E9EC0, DefineTriggerOnce))

/* one playing (or wanted) sound, there is one per DirectSound voice */
typedef struct SoundInst {
    struct SoundInst *prev;
    struct SoundInst *next;
    uint32_t flags;
    int bucket;
    int hash;
    int id;
    uint32_t age;
    void *voice;
    float volume;           /* first field of the 0x58 byte SoundParams */
    char params[0x54];
} SoundInst;

/* a "sounds.qsc" script, its tasks are the sound definitions */
typedef struct SoundSysDef {
    Task task;
    char path[0x80];
} SoundSysDef;

typedef struct ResourceEntry {
    const char *name;
    void *data;
} ResourceEntry;

_Static_assert(sizeof(SoundInst) == 0x78, "SoundInst");
_Static_assert(sizeof(SoundSysDef) == 0xA0, "SoundSysDef");

extern SoundInst *gSoundFree;
extern int gSoundInstCount;
extern SoundInst *gSoundActive;
extern int gSoundNextId;
extern int gSoundUpdateRate;
extern SoundInst *gSoundInsts;
extern char gUnkA5E608;
extern char gSoundDisabled;
extern QTaskList *gSoundTasks;
extern QTaskList *gSoundDefs;

extern uint16_t gSoundSysControllerType;
extern uint16_t gSoundSysDefType;
extern uint16_t gSoundSysDefType2;
extern uint16_t gSoundSysDefParentType;
extern uint16_t gSoundSysDefParentType2;

extern uint8_t gSoundListEvent;
extern uint8_t gSoundInfoEvent;
extern uint8_t gSoundFindEvent;
extern uint8_t gSoundNameEvent;
extern uint8_t gSoundSetPosEvent;
extern uint8_t gSoundSetVolumeEvent;
extern uint8_t gSoundSetPitchEvent;
extern uint8_t gSoundRangeEvent;
extern uint8_t gSoundDurationEvent;
extern uint8_t gSoundSetGlobalEvent;
extern int gSoundStopEvent;

static const float kZero = 0.0f;

uint8_t Sound_GetFindEvent(void);
void SoundInst_Push(SoundInst *s, SoundInst **list);
void SoundInst_Unlink(SoundInst *s, SoundInst **list);

void SoundSysDef_Create(SoundSysDef *d, const char *path)
{
    void *script;

    strcpy(d->path, path);
    QTaskList_Add(gSoundDefs, d);
    Script_SetSymbolContext("DefineGroup", d);
    Script_SetSymbolContext("DefineSound", d);
    Script_SetSymbolContext("DefineGraph", d);
    Script_SetSymbolContext("DefineTriggerOnce", d);
    script = Script_Load(path);
    if (script) {
        Script_Run(script);
        Script_Free(script);
    }
}
HOOK(0x4E5EC0, SoundSysDef_Create);

void SoundSysDef_Delete(SoundSysDef *d)
{
    QTaskList_Remove(gSoundDefs, d);
}
HOOK(0x4E5F50, SoundSysDef_Delete);

void SoundSysDef_Find(Task *t, SoundFindArg *arg)
{
    CALL_CHILDREN(t, Sound_GetFindEvent(), arg);
}
HOOK(0x4E5F70, SoundSysDef_Find);

void SoundSysDefParent_Create(SoundSysDef *d, const char *path)
{
    strcpy(d->path, path);
}
HOOK(0x4E6000, SoundSysDefParent_Create);

void Sound_SetRate(int rate)
{
    gSoundUpdateRate = rate;
    Sound_SetUpdateRate(rate);
}
HOOK(0x4E6030, Sound_SetRate);

int Sound_GetRate(void)
{
    return gSoundUpdateRate;
}
HOOK(0x4E6050, Sound_GetRate);

void sub_4E6060(char v)
{
    gUnkA5E608 = v;
}
HOOK(0x4E6060, sub_4E6060);

void SoundSysController_Run(void);
void SoundSysDefParent_Create(SoundSysDef *d, const char *path);

void SoundSys_Init(void)
{
    SoundInst *s;
    int i;

    sub_4E7350(0);
    gSoundTasks = QTaskList_New(0x100);
    gSoundDefs = QTaskList_New(8);
    gSoundSysControllerType = TaskType_New("SoundSysController", 0x20, Task_GetRootType(), 0);
    gSoundSysDefType = TaskType_New("SoundSysDef", 0xA0, Task_GetRootType(), 0);
    gSoundSysDefParentType = TaskType_New("SoundSysDefParent", 0xA0, Task_GetRootType(), 0);
    gSoundSysDefType2 = gSoundSysDefType;
    gSoundSysDefParentType2 = gSoundSysDefParentType;
    TaskType_SetHandler(gSoundSysControllerType, 0, TASK_NOTHING);
    TaskType_SetHandler2(gSoundSysControllerType, 1, SoundSysController_Run);
    TaskType_SetHandler2(gSoundSysControllerType, 2, TASK_NOTHING);
    TaskType_SetHandler(gSoundSysDefType, 0, SoundSysDef_Create);
    TaskType_SetHandler2(gSoundSysDefType, 2, SoundSysDef_Delete);
    TaskType_SetHandler(gSoundSysDefType, Sound_GetFindEvent(), SoundSysDef_Find);
    TaskType_SetHandler(gSoundSysDefParentType, 0, SoundSysDefParent_Create);
    gSoundNextId = 1;
    gSoundUpdateRate = 0;
    gSoundInstCount = Sound_GetVoiceCount();
    gSoundInsts = Mem_Alloc(gSoundInstCount * sizeof(SoundInst), 4);
    memset(gSoundInsts, 0, gSoundInstCount * sizeof(SoundInst));
    for (i = 0, s = gSoundInsts; i < gSoundInstCount; i++, s++)
        SoundInst_Push(s, &gSoundFree);
    Script_RegisterFunction("DefineGroup", DefineGroup, 0, -1);
    Script_RegisterFunction("DefineSound", DefineSound, 0, -1);
    Script_RegisterFunction("DefineGraph", DefineGraph, 0, -1);
    Script_RegisterFunction("DefineTriggerOnce", DefineTriggerOnce, 0, -1);
    gSoundDisabled = Sound_IsReady() == 0;
}
HOOK(0x4E6070, SoundSys_Init);

void SoundInst_Push(SoundInst *s, SoundInst **list)
{
    s->prev = NULL;
    s->next = *list;
    if (*list)
        (*list)->prev = s;
    *list = s;
}
HOOK(0x4E6290, SoundInst_Push);

void SoundSys_Update(void);

void SoundSysController_Run(void)
{
    SoundSys_Update();
    Stream_Update();
}
HOOK(0x4E62B0, SoundSysController_Run);

static void SoundInst_Release(SoundInst *s)
{
    SoundInst_Unlink(s, &gSoundActive);
    SoundInst_Push(s, &gSoundFree);
}

/* Sounds without flag 1 last one frame unless they are played again. */
void SoundSys_Update(void)
{
    SoundInst *s, *next;

    for (s = gSoundActive; s; s = next) {
        next = s->next;
        if (s->flags & 1) {
            if (s->voice && !Voice_IsPlaying(s->voice)) {
                Voice_Stop(s->voice);
                Voice_Free(s->voice);
                SoundInst_Release(s);
                continue;
            }
            if (s->voice)
                Voice_SetParams(s->voice, &s->volume);
            if (s->volume != kZero)
                continue;
        } else if (s->age == 0) {
            s->age++;
            continue;
        }
        if (s->voice) {
            Voice_Stop(s->voice);
            Voice_Free(s->voice);
        }
        SoundInst_Release(s);
    }
}
HOOK(0x4E62C0, SoundSys_Update);

void SoundInst_Unlink(SoundInst *s, SoundInst **list)
{
    if (s->next)
        s->next->prev = s->prev;
    if (s->prev)
        s->prev->next = s->next;
    if (*list == s)
        *list = s->next;
}
HOOK(0x4E63B0, SoundInst_Unlink);

void SoundSys_Shutdown(void)
{
    QTaskList_Free(gSoundTasks);
    QTaskList_Free(gSoundDefs);
    Mem_Free(gSoundInsts);
    gSoundInsts = NULL;
    if (gSoundListEvent != 0xFF)
        TaskEvent_Free(gSoundListEvent);
    if (gSoundInfoEvent != 0xFF)
        TaskEvent_Free(gSoundInfoEvent);
    if (gSoundFindEvent != 0xFF)
        TaskEvent_Free(gSoundFindEvent);
    if (gSoundNameEvent != 0xFF)
        TaskEvent_Free(gSoundNameEvent);
    if (gSoundSetPosEvent != 0xFF)
        TaskEvent_Free(gSoundSetPosEvent);
    if (gSoundSetVolumeEvent != 0xFF)
        TaskEvent_Free(gSoundSetVolumeEvent);
    if (gSoundSetPitchEvent != 0xFF)
        TaskEvent_Free(gSoundSetPitchEvent);
    if (gSoundSetGlobalEvent != 0xFF)
        TaskEvent_Free(gSoundSetGlobalEvent);
    if (gSoundRangeEvent != 0xFF)
        TaskEvent_Free(gSoundRangeEvent);
    if (gSoundDurationEvent != 0xFF)
        TaskEvent_Free(gSoundDurationEvent);
    if (gSoundStopEvent != 0xFF)
        TaskEvent_Free2(gSoundStopEvent);
    TaskType_Free(gSoundSysControllerType);
    TaskType_Free(gSoundSysDefParentType);
    TaskType_Free(gSoundSysDefType);
    Script_UnregisterSymbol("DefineGroup");
    Script_UnregisterSymbol("DefineSound");
    Script_UnregisterSymbol("DefineGraph");
    Script_UnregisterSymbol("DefineTriggerOnce");
}
HOOK(0x4E63E0, SoundSys_Shutdown);

Task *SoundSys_NewController(Task *parent)
{
    return Task_New(parent, gSoundSysControllerType, NULL);
}
HOOK(0x4E6530, SoundSys_NewController);

Task *SoundSys_NewDef(Task *parent, const char *path);

/* dir is "LOCAL:common/sounds/" style; runs <dir>sounds.qsc */
Task *SoundSys_LoadDefs(Task *parent, const char *dir)
{
    char path[0x400];
    Task *t = Task_New(parent, gSoundSysDefParentType, (void *)dir);
    char last = dir[strlen(dir) - 1];

    if (last == '/' || last == '\\')
        crt_sprintf(path, "%ssounds.qsc", dir);
    else
        crt_sprintf(path, "%s/sounds.qsc", dir);
    SoundSys_NewDef(t, path);
    return t;
}
HOOK(0x4E6550, SoundSys_LoadDefs);

Task *SoundSys_NewDef(Task *parent, const char *path)
{
    return Task_New(parent, gSoundSysDefType2, (void *)path);
}
HOOK(0x4E65D0, SoundSys_NewDef);

uint8_t Sound_GetListEvent(void)
{
    if (gSoundListEvent == 0xFF)
        gSoundListEvent = TaskEvent_Alloc(0);
    return gSoundListEvent;
}
HOOK(0x4E65F0, Sound_GetListEvent);

uint8_t Sound_GetInfoEvent(void)
{
    if (gSoundInfoEvent == 0xFF)
        gSoundInfoEvent = TaskEvent_Alloc(0);
    return gSoundInfoEvent;
}
HOOK(0x4E6610, Sound_GetInfoEvent);

uint8_t Sound_GetFindEvent(void)
{
    if (gSoundFindEvent == 0xFF)
        gSoundFindEvent = TaskEvent_Alloc(0);
    return gSoundFindEvent;
}
HOOK(0x4E6630, Sound_GetFindEvent);

uint8_t Sound_GetNameEvent(void)
{
    if (gSoundNameEvent == 0xFF)
        gSoundNameEvent = TaskEvent_Alloc(0);
    return gSoundNameEvent;
}
HOOK(0x4E6650, Sound_GetNameEvent);

uint8_t Sound_GetSetPosEvent(void)
{
    if (gSoundSetPosEvent == 0xFF)
        gSoundSetPosEvent = TaskEvent_Alloc(0);
    return gSoundSetPosEvent;
}
HOOK(0x4E6670, Sound_GetSetPosEvent);

uint8_t Sound_GetSetVolumeEvent(void)
{
    if (gSoundSetVolumeEvent == 0xFF)
        gSoundSetVolumeEvent = TaskEvent_Alloc(0);
    return gSoundSetVolumeEvent;
}
HOOK(0x4E6690, Sound_GetSetVolumeEvent);

uint8_t Sound_GetSetPitchEvent(void)
{
    if (gSoundSetPitchEvent == 0xFF)
        gSoundSetPitchEvent = TaskEvent_Alloc(0);
    return gSoundSetPitchEvent;
}
HOOK(0x4E66B0, Sound_GetSetPitchEvent);

uint8_t Sound_GetSetGlobalEvent(void)
{
    if (gSoundSetGlobalEvent == 0xFF)
        gSoundSetGlobalEvent = TaskEvent_Alloc(0);
    return gSoundSetGlobalEvent;
}
HOOK(0x4E66D0, Sound_GetSetGlobalEvent);

uint8_t Sound_GetRangeEvent(void)
{
    if (gSoundRangeEvent == 0xFF)
        gSoundRangeEvent = TaskEvent_Alloc(0);
    return gSoundRangeEvent;
}
HOOK(0x4E66F0, Sound_GetRangeEvent);

uint8_t Sound_GetDurationEvent(void)
{
    if (gSoundDurationEvent == 0xFF)
        gSoundDurationEvent = TaskEvent_Alloc(0);
    return gSoundDurationEvent;
}
HOOK(0x4E6710, Sound_GetDurationEvent);

int Sound_GetStopEvent(void)
{
    if (gSoundStopEvent == 0xFF)
        gSoundStopEvent = TaskEvent_Alloc2(0);
    return gSoundStopEvent;
}
HOOK(0x4E6730, Sound_GetStopEvent);

void Sound_LoadResourceEntry(ResourceEntry *e);

/* loads every sound packed in a .res file */
void Sound_LoadResource(const char *res)
{
    char dir[0x80];

    Path_GetDir(dir, sizeof(dir), res);
    if (Resource_Exists(res)) {
        Resource_Load(res, 0);
        Resource_ForEach(dir, Sound_LoadResourceEntry, NULL);
        Resource_Unload(res);
    }
}
HOOK(0x4E6750, Sound_LoadResource);

void Sound_LoadResourceEntry(ResourceEntry *e)
{
    Sound_Load(e->name, e->data);
}
HOOK(0x4E67B0, Sound_LoadResourceEntry);

void Sound_FreeResourceEntry(ResourceEntry *e);

void Sound_FreeResource(const char *res)
{
    char dir[0x80];

    Path_GetDir(dir, sizeof(dir), res);
    if (Resource_Exists(res)) {
        Resource_Load(res, 0);
        Resource_ForEach(dir, Sound_FreeResourceEntry, NULL);
        Resource_Unload(res);
    }
}
HOOK(0x4E67D0, Sound_FreeResource);

/* stops every instance of the sound before unloading it */
void Sound_FreeResourceEntry(ResourceEntry *e)
{
    char name[0x400];
    SoundInst *s = gSoundActive, *next;
    int hash;

    Sound_GetName(name, e->name);
    hash = Sound_HashName(name);
    for (; s; s = next) {
        next = s->next;
        if (s->hash != hash)
            continue;
        if (s->voice) {
            Voice_Stop(s->voice);
            Voice_Free(s->voice);
        }
        SoundInst_Release(s);
    }
    Sound_Unload(e->name);
}
HOOK(0x4E6830, Sound_FreeResourceEntry);

void SoundLoad_File(FileInfo *f);

void SoundLoad(const char *dir)
{
    File_ScanDir(dir, SoundLoad_File, NULL);
}
HOOK(0x4E68D0, SoundLoad);

void SoundLoad_File(FileInfo *f)
{
    char ext[12];

    if (f->isDir)
        return;
    Path_GetExtension(ext, 10, f->path);
    Str_ToUpper(ext);
    if (strcmp(ext, ".RES") == 0) {
        Sound_LoadResource(f->path);
        return;
    }
    if (strcmp(ext, ".WAV") == 0)
        Sound_Load(f->path, f->data);
}
HOOK(0x4E68F0, SoundLoad_File);

void SoundFree_File(FileInfo *f);

void SoundFree(const char *dir)
{
    File_ScanDir(dir, SoundFree_File, NULL);
}
HOOK(0x4E69C0, SoundFree);

void SoundFree_File(FileInfo *f)
{
    char ext[12];

    if (f->isDir)
        return;
    Path_GetExtension(ext, 10, f->path);
    Str_ToUpper(ext);
    if (strcmp(ext, ".RES") == 0) {
        Sound_FreeResource(f->path);
        return;
    }
    if (strcmp(ext, ".WAV") == 0)
        Sound_Unload(f->path);
}
HOOK(0x4E69E0, SoundFree_File);

/* a count followed by that many zeroed values */
int *SoundParams_New(int count)
{
    int *p;
    int i;

    Mem_PushMode(1);
    p = Mem_Alloc(count * 4 + 4, 4);
    Mem_PopMode();
    if (p) {
        p[0] = count;
        for (i = 1; i <= count; i++)
            p[i] = 0;
    }
    return p;
}
HOOK(0x4E6AB0, SoundParams_New);

/* finds the definition named name and makes a sound task under owner with
   one child per child of the definition */
Task *Sound_Play(Task *owner, const char *name)
{
    SoundFindArg find;
    SoundCreateArg arg;
    Task *t, *c;
    int i;

    if (gSoundDisabled || gSoundDefs->count == 0 || !*name)
        return NULL;
    find.name = name;
    find.found = NULL;
    if (gSoundDefs->count <= 0)
        return NULL;
    for (i = 0; i < gSoundDefs->count; i++) {
        Task *d = gSoundDefs->items[i];

        gTaskHandlers[Sound_GetFindEvent()][d->type](d, &find);
    }
    if (!find.found)
        return NULL;
    arg.def = find.found;
    arg.isDef = 0;
    t = Task_New(owner, find.found->type, &arg);
    c = find.found->head;
    if (c->next && c) {
        do {
            arg.def = c;
            Task_New(t, c->type, &arg);
            c = c->next;
        } while (c && c->next);
    }
    return t;
}
HOOK(0x4E6B00, Sound_Play);

void Sound_Stop(Task *t)
{
    TaskFn fn = gTaskHandlers[Sound_GetStopEvent()][t->type];

    if (fn)
        ((void (*)(void *))fn)(t);
}
HOOK(0x4E6C00, Sound_Stop);

void Sound_SetPos(Task *t, Vec3d *pos)
{
    gTaskHandlers[gSoundSetPosEvent][t->type](t, pos);
}
HOOK(0x4E6C30, Sound_SetPos);

/* a negative value leaves that one unchanged */
void Sound_SetVolume(Task *t, float volume, float volume2)
{
    float v[2];

    v[0] = volume;
    v[1] = volume2;
    gTaskHandlers[gSoundSetVolumeEvent][t->type](t, v);
}
HOOK(0x4E6C60, Sound_SetVolume);

void Sound_SetPitch(Task *t, float pitch)
{
    gTaskHandlers[gSoundSetPitchEvent][t->type](t, &pitch);
}
HOOK(0x4E6CA0, Sound_SetPitch);

/* on: heard at any distance */
void Sound_SetGlobal(Task *t, char on)
{
    gTaskHandlers[gSoundSetGlobalEvent][t->type](t, &on);
}
HOOK(0x4E6CE0, Sound_SetGlobal);

void Sound_AddTask(Task *t)
{
    QTaskList_Add(gSoundTasks, t);
}
HOOK(0x4E6D20, Sound_AddTask);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(548808) uint16_t gSoundSysControllerType INIT(385);
DATA(548808) uint16_t gSoundSysDefType INIT(385);
DATA(548808) uint16_t gSoundSysDefType2 INIT(385);
DATA(548808) uint16_t gSoundSysDefParentType INIT(385);
DATA(548808) uint16_t gSoundSysDefParentType2 INIT(385);
DATA(548808) uint8_t gSoundListEvent INIT(255);
DATA(548808) uint8_t gSoundInfoEvent INIT(255);
DATA(548808) uint8_t gSoundFindEvent INIT(255);
DATA(548808) uint8_t gSoundNameEvent INIT(255);
DATA(548808) uint8_t gSoundSetPosEvent INIT(255);
DATA(548808) uint8_t gSoundSetVolumeEvent INIT(255);
DATA(548808) uint8_t gSoundSetPitchEvent INIT(255);
DATA(548808) uint8_t gSoundRangeEvent INIT(255);
DATA(548808) uint8_t gSoundDurationEvent INIT(255);
DATA(548808) uint8_t gSoundSetGlobalEvent INIT(255);
DATA(548808) int gSoundStopEvent INIT(255);
BSS(A5E5F0) int gSoundInstCount;
BSS(A5E5FC) int gSoundUpdateRate;
BSS(A5E5FC) SoundInst *gSoundInsts;
BSS(A5E60C) QTaskList *gSoundTasks;
BSS(A5E60C) QTaskList *gSoundDefs;
