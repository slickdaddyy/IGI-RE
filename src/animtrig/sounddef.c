#pragma GCC optimize("excess-precision=standard")
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "animtrig/animtrig.h"
#include "qasm/qasmtypes.h"
#include "sound/soundtypes.h"
#include "define/definetypes.h"
#include "task/taskvars.h"
#include "config/configvars.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4974A0, void, SoundParams_Init, (void *params));
ORIG(0x497410, int, Sound_HashName, (char *name));
ORIG(0x497450, int, Sound_HashBucket, (char *name));
ORIG(0x496E00, void, Sound_SetListener, (float *matrix, double *pos, int *vel));
ORIG(0x4E65F0, uint8_t, Sound_GetListEvent, (void));
ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6630, uint8_t, Sound_GetFindEvent, (void));
ORIG(0x4E6650, uint8_t, Sound_GetNameEvent, (void));
ORIG(0x4E6670, uint8_t, Sound_GetSetPosEvent, (void));
ORIG(0x4E6690, uint8_t, Sound_GetSetVolumeEvent, (void));
ORIG(0x4E66B0, uint8_t, Sound_GetSetPitchEvent, (void));
ORIG(0x4E66D0, uint8_t, Sound_GetSetGlobalEvent, (void));
ORIG(0x4E66F0, uint8_t, Sound_GetRangeEvent, (void));
ORIG(0x4E6710, uint8_t, Sound_GetDurationEvent, (void));
ORIG(0x4E6730, int, Sound_GetStopEvent, (void));
ORIG(0x4E6FF0, long double, SoundParams_ClampVolume, (void *params));
ORIG(0x4E7030, void, SoundGraph_Eval, (float *out, float *graph, float x));

/* "DefineGroup": a named group of sound definitions */
typedef struct SoundDefGroup {
    Task task;
    char keep;
    char stopped;
    char name[0x26];
    double pos[3];
} SoundDefGroup;

typedef struct SoundSysMicrophone {
    Task task;
    float matrix[10];
    double pos[3];
    int unk60;
    int unk64;
} SoundSysMicrophone;

ORIG(0x4E7330, QTaskList *, Sound_GetTasks, (void));

_Static_assert(sizeof(SoundDefGroup) == 0x60, "SoundDefGroup");
_Static_assert(sizeof(SoundSysMicrophone) == 0x68, "SoundSysMicrophone");

extern uint16_t gSoundDefGroupType;
extern uint16_t gSoundDefGroupType2;
extern uint16_t gSoundSysMicrophoneType;
extern uint8_t gSoundSysMicrophonePlayEvent;

static const float kZero = 0.0f;
static const float kOne = 1.0f;

/* recomputes the voice parameters from the owner's info and the graphs */
void SoundDefGraph_Update(SoundDefGraph *s)
{
    SoundInfo info;
    Task *owner;
    TaskFn fn;

    info.pos = NULL;
    info.matrix = NULL;
    info.values = NULL;
    owner = s->task.parent;
    fn = gTaskHandlers[Sound_GetInfoEvent()][owner->type];
    if (fn)
        fn(owner, &info);
    SoundParams_Init(&s->h.params);
    memcpy(&s->pos, &s->offset, sizeof(s->pos));
    if (info.matrix) {
        const float *m = info.matrix;
        double r[3];

        r[0] = ((long double)m[1] * s->pos.y + (long double)m[2] * s->pos.z) + (long double)m[0] * s->pos.x;
        r[1] = ((long double)m[4] * s->pos.y + (long double)m[5] * s->pos.z) + (long double)m[3] * s->pos.x;
        r[2] = ((long double)m[7] * s->pos.y + (long double)m[8] * s->pos.z) + (long double)m[6] * s->pos.x;
        memcpy(&s->pos, r, sizeof(r));
    }
    if (info.pos) {
        s->pos.x = info.pos->x + s->pos.x;
        s->pos.y = info.pos->y + s->pos.y;
        s->pos.z = info.pos->z + s->pos.z;
    }
    if (info.values) {
        int *vals = info.values + 1;

        if (s->volumeIndex != -1) {
            float v = ((float *)vals)[s->volumeIndex];

            if (!(v >= kZero))
                s->h.params.volume = 0;
            else
                SoundGraph_Eval(&s->h.params.volume, s->volumeGraph, v);
        }
        if (s->pitchIndex != -1)
            SoundGraph_Eval(&s->h.params.pitch, s->pitchGraph, ((float *)vals)[s->pitchIndex]);
        if (s->pitchAddIndex != -1)
            s->h.params.pitch = ((float *)vals)[s->pitchAddIndex] + s->h.params.pitch;
        if (s->sampleIndex != -1) {
            char *name = ((char **)vals)[s->sampleIndex];

            if (name && strcmp(name, s->sample) != 0) {
                strcpy(s->sample, name);
                s->bucket = Sound_HashBucket(s->sample);
                s->hash = Sound_HashName(s->sample);
                s->h.id = 0;
            }
        }
    }
    if (!(s->volume >= kZero))
        s->volume = 0;
    else if (s->volume > kOne)
        s->volume = 1.0f;
    if (!(s->volume2 >= kZero))
        s->volume2 = 0;
    else if (s->volume2 > kOne)
        s->volume2 = 1.0f;
    memset(&s->h.params.vel, 0, sizeof(s->h.params.vel));
    s->h.params.volume = (long double)s->volume2 * s->volume * s->h.params.volume;
    memcpy(&s->h.params.pos, &s->pos, sizeof(s->pos));
    s->h.params.pitch = (long double)s->pitch * s->h.params.pitch;
    s->h.params.minDist = s->minDist;
    s->h.params.maxDist = s->maxDist;
    s->range = SoundParams_ClampVolume(&s->h.params);
}
HOOK(0x4EAEB0, SoundDefGraph_Update);

void SoundDefGroup_Create(SoundDefGroup *g, SoundCreateArg *arg);
void SoundDefGroup_Run(SoundDefGroup *g);
void SoundDefGroup_Stop(SoundDefGroup *g);
void SoundDefGroup_Write(SoundDefGroup *g, SoundWriteArg *a);
void SoundDefGroup_Info(SoundDefGroup *g, SoundInfo *info);
void SoundDefGroup_Find(SoundDefGroup *g, SoundFindArg *a);
void SoundDefGroup_GetName(SoundDefGroup *g, const char **out);
void SoundDefGroup_SetPos(SoundDefGroup *g, double *pos);
void SoundDefGroup_SetVolume(SoundDefGroup *g, void *arg);
void SoundDefGroup_SetPitch(SoundDefGroup *g, void *arg);
void SoundDefGroup_SetGlobal(SoundDefGroup *g, void *arg);
void SoundDefGroup_GetRange(SoundDefGroup *g, void *arg);
void SoundDefGroup_GetDuration(SoundDefGroup *g, void *arg);

void SoundDefGroup_Init(void)
{
    gSoundDefGroupType = TaskType_New("SoundDefGroup", 0x60, Task_GetRootType(), 0x200);
    gSoundDefGroupType2 = gSoundDefGroupType;
    TaskType_SetHandler(gSoundDefGroupType, 0, SoundDefGroup_Create);
    TaskType_SetHandler2(gSoundDefGroupType, 1, SoundDefGroup_Run);
    TaskType_SetHandler2(gSoundDefGroupType, 2, TASK_NOTHING);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetListEvent(), SoundDefGroup_Write);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetInfoEvent(), SoundDefGroup_Info);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetFindEvent(), SoundDefGroup_Find);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetNameEvent(), SoundDefGroup_GetName);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetSetPosEvent(), SoundDefGroup_SetPos);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetSetVolumeEvent(), SoundDefGroup_SetVolume);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetSetPitchEvent(), SoundDefGroup_SetPitch);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetSetGlobalEvent(), SoundDefGroup_SetGlobal);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetRangeEvent(), SoundDefGroup_GetRange);
    TaskType_SetHandler(gSoundDefGroupType, Sound_GetDurationEvent(), SoundDefGroup_GetDuration);
    TaskType_SetHandler2(gSoundDefGroupType, Sound_GetStopEvent(), SoundDefGroup_Stop);
}
HOOK(0x4EB1D0, SoundDefGroup_Init);

/* arg is NULL or {definition, keep} */
void SoundDefGroup_Create(SoundDefGroup *g, SoundCreateArg *arg)
{
    memset(g->pos, 0, sizeof(g->pos));
    if (!arg) {
        g->name[0] = 0;
        g->stopped = 0;
        g->keep = 1;
        return;
    }
    g->keep = arg->isDef;
    strcpy(g->name, ((SoundDefGroup *)arg->def)->name);
    g->stopped = 0;
}
HOOK(0x4EB360, SoundDefGroup_Create);

/* deletes itself once all its sounds are gone, unless it is a definition */
void SoundDefGroup_Run(SoundDefGroup *g)
{
    RUN_CHILDREN(&g->task);
    if ((!g->task.head->next || !g->task.head) && !g->keep)
        Task_Delete(g);
}
HOOK(0x4EB3D0, SoundDefGroup_Run);

void SoundDefGroup_Stop(SoundDefGroup *g)
{
    g->stopped = 1;
}
HOOK(0x4EB460, SoundDefGroup_Stop);

/* writes the group back as script text */
void SoundDefGroup_Write(SoundDefGroup *g, SoundWriteArg *a)
{
    char line[0x400];
    const char *end;
    Task *last;
    int i;

    for (i = 0; i < a->indent; i++)
        strcat(a->buf, "  ");
    crt_sprintf(line, "%s(\"%s\"", "DefineGroup", g->name);
    strcat(a->buf, line);
    a->indent++;
    if (g->task.head->next && g->task.head) {
        strcat(a->buf, ",\r\n");
        CALL_CHILDREN(&g->task, Sound_GetListEvent(), a);
    }
    if (--a->indent == 0) {
        end = ");\r\n";
    } else {
        last = g->task.parent->tailpred;
        if (&g->task == (last->prev ? last : NULL))
            end = ")";
        else
            end = "),\r\n";
    }
    strcat(a->buf, end);
}
HOOK(0x4EB470, SoundDefGroup_Write);

void SoundDefGroup_Info(SoundDefGroup *g, SoundInfo *info)
{
    Task *owner = g->task.parent;
    TaskFn fn = gTaskHandlers[Sound_GetInfoEvent()][owner->type];

    if (fn)
        fn(owner, info);
    if (!info->pos)
        info->pos = (Vec3d *)g->pos;
}
HOOK(0x4EB620, SoundDefGroup_Info);

void SoundDefGroup_Find(SoundDefGroup *g, SoundFindArg *a)
{
    if (a->found)
        return;
    if (strcmp(a->name, g->name) == 0) {
        a->found = &g->task;
        return;
    }
    CALL_CHILDREN(&g->task, Sound_GetFindEvent(), a);
}
HOOK(0x4EB670, SoundDefGroup_Find);

void SoundDefGroup_GetName(SoundDefGroup *g, const char **out)
{
    if (g->name[0])
        *out = g->name;
}
HOOK(0x4EB740, SoundDefGroup_GetName);

void SoundDefGroup_SetPos(SoundDefGroup *g, double *pos)
{
    memcpy(g->pos, pos, sizeof(g->pos));
}
HOOK(0x4EB760, SoundDefGroup_SetPos);

void SoundDefGroup_SetVolume(SoundDefGroup *g, void *arg)
{
    CALL_CHILDREN(&g->task, Sound_GetSetVolumeEvent(), arg);
}
HOOK(0x4EB780, SoundDefGroup_SetVolume);

void SoundDefGroup_SetPitch(SoundDefGroup *g, void *arg)
{
    CALL_CHILDREN(&g->task, Sound_GetSetPitchEvent(), arg);
}
HOOK(0x4EB810, SoundDefGroup_SetPitch);

void SoundDefGroup_SetGlobal(SoundDefGroup *g, void *arg)
{
    CALL_CHILDREN(&g->task, Sound_GetSetGlobalEvent(), arg);
}
HOOK(0x4EB8A0, SoundDefGroup_SetGlobal);

void SoundDefGroup_GetRange(SoundDefGroup *g, void *arg)
{
    CALL_CHILDREN(&g->task, Sound_GetRangeEvent(), arg);
}
HOOK(0x4EB930, SoundDefGroup_GetRange);

void SoundDefGroup_GetDuration(SoundDefGroup *g, void *arg)
{
    CALL_CHILDREN(&g->task, Sound_GetDurationEvent(), arg);
}
HOOK(0x4EB9C0, SoundDefGroup_GetDuration);

void SoundDefGroup_Shutdown(void)
{
    TaskType_Free(gSoundDefGroupType);
    gSoundDefGroupType = 0x181;
    gSoundDefGroupType2 = 0x181;
}
HOOK(0x4EBA50, SoundDefGroup_Shutdown);

uint16_t SoundDefGroup_GetType(void)
{
    return gSoundDefGroupType2;
}
HOOK(0x4EBA80, SoundDefGroup_GetType);

static void set_context(void *t)
{
    Script_SetSymbolContext("DefineGroup", t);
    Script_SetSymbolContext("DefineSound", t);
    Script_SetSymbolContext("DefineGraph", t);
    Script_SetSymbolContext("DefineTriggerOnce", t);
}

/* DefineGroup(name, definitions...): the definitions are made inside the group */
ScriptValue *DefineGroup(ScriptValue *out, Task *parent, ScriptArgs *args)
{
    SoundDefGroup def;
    SoundCreateArg arg;
    Task *t;
    int i = 1;

    arg.def = &def;
    arg.isDef = 1;
    Script_GetArgString(args, 0, def.name, 0x20);
    def.keep = 1;
    t = Task_New(parent, gSoundDefGroupType2, &arg);
    set_context(t);
    while (Script_HasArg(args, i)) {
        Script_GetArgInt(args, i);
        i++;
    }
    set_context(parent);
    out->i = i;
    out->d = i;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4EBA90, DefineGroup);

SoundSysMicrophone *SoundSysMicrophone_New(Task *parent, int unk)
{
    SoundSysMicrophone *m = (SoundSysMicrophone *)Task_New(parent, gSoundSysMicrophoneType, NULL);

    if (m) {
        memset(m->matrix, 0, sizeof(m->matrix));
        m->matrix[0] = 1.0f;
        m->matrix[4] = 1.0f;
        m->matrix[8] = 1.0f;
        m->unk60 = unk;
    }
    return m;
}
HOOK(0x4EBBA0, SoundSysMicrophone_New);

void SoundSysMicrophone_Set(SoundSysMicrophone *m, float *matrix, double *pos)
{
    int vel[6] = {0};

    memcpy(m->matrix, matrix, sizeof(m->matrix));
    memcpy(m->pos, pos, sizeof(m->pos));
    Sound_SetListener(matrix, pos, vel);
}
HOOK(0x4EBBF0, SoundSysMicrophone_Set);

void SoundSysMicrophone_Run(SoundSysMicrophone *m);

void SoundSysMicrophone_Init(void)
{
    gSoundSysMicrophoneType = TaskType_New("SoundSysMicrophone", 0x68, Task_GetRootType(), 0);
    gSoundSysMicrophonePlayEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gSoundSysMicrophoneType, 0, TASK_NOTHING);
    TaskType_SetHandler2(gSoundSysMicrophoneType, 1, SoundSysMicrophone_Run);
    TaskType_SetHandler2(gSoundSysMicrophoneType, 2, TASK_NOTHING);
}
HOOK(0x4EBC60, SoundSysMicrophone_Init);

/* sounds within range of this microphone play */
void SoundSysMicrophone_Run(SoundSysMicrophone *m)
{
    SoundPlayArg arg;
    int i;

    arg.listener = m;
    arg.defs = 1;
    for (i = 0; i < Sound_GetTasks()->count; i++) {
        Task *t = Sound_GetTasks()->items[i];
        TaskFn fn = gTaskHandlers[gSoundSysMicrophonePlayEvent][t->type];

        fn(Sound_GetTasks()->items[i], &arg);
    }
    RUN_CHILDREN(&m->task);
}
HOOK(0x4EBCD0, SoundSysMicrophone_Run);

void SoundSysMicrophone_Shutdown(void)
{
    TaskEvent_Free(gSoundSysMicrophonePlayEvent);
    TaskType_Free(gSoundSysMicrophoneType);
}
HOOK(0x4EBDB0, SoundSysMicrophone_Shutdown);

uint8_t SoundSysMicrophone_GetPlayEvent(void)
{
    return gSoundSysMicrophonePlayEvent;
}
HOOK(0x4EBDD0, SoundSysMicrophone_GetPlayEvent);

double *SoundSysMicrophone_GetPos(SoundSysMicrophone *m)
{
    return m->pos;
}
HOOK(0x4EBDE0, SoundSysMicrophone_GetPos);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DB48) uint16_t gSoundDefGroupType INIT(385);
DATA(54DB48) uint16_t gSoundDefGroupType2 INIT(385);
PAD(igd, 54DB48, 54DB4C, 0x1C) INIT({0x53, 0x6F, 0x75, 0x6E, 0x64, 0x44, 0x65, 0x66, 0x47, 0x72, 0x6F, 0x75, 0x70, 0x00, 0x00, 0x00, 0x2C, 0x0D, 0x0A, 0x00, 0x25, 0x73, 0x28, 0x22, 0x25, 0x73, 0x22});
DATA(54DB48) uint16_t gSoundSysMicrophoneType INIT(385);
DATA(54DB48) uint8_t gSoundSysMicrophonePlayEvent INIT(255);
PAD(igd, 54DB48, 54DB6B, 0x1);
