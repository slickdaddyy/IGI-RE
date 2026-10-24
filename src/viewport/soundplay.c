#include <string.h>

#include "hook.h"
#include "viewport/viewport.h"
#include "qasm/qasmtypes.h"
#include "sound/soundtypes.h"
#include "viewport/viewporttypes.h"

/* Playing sounds by name or through a handle, on top of the sound system of src/sound. */

typedef void (*TaskFn2)(Task *task, void *arg);

extern int gSoundUnusedOff;
extern SoundInst *gSoundFree;
extern SoundInst *gSoundActive;
extern int gSoundNextId;
extern char gSoundDisabled;
extern char gSoundSilent;
extern QTaskList *gSoundTasks;
extern QTaskList *gSoundDefs;

ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x496650, void, Voice_Free, (void *voice));
ORIG(0x4966E0, char, Voice_IsPlaying, (void *voice));
ORIG(0x496770, void *, Voice_Play, (int bucket, int hash, uint32_t flags, SoundParams *p));
ORIG(0x496E40, void, Voice_Stop, (void *voice));
ORIG(0x496E60, void, Voice_SetParams, (void *voice, SoundParams *p));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497450, int, Sound_HashBucket, (const char *name));
ORIG(0x4974A0, void, SoundParams_Init, (SoundParams *p));
ORIG(0x4E6290, void, SoundInst_Push, (SoundInst *s, SoundInst **list));
ORIG(0x4E63B0, void, SoundInst_Unlink, (SoundInst *s, SoundInst **list));
ORIG(0x4E6630, uint8_t, Sound_GetFindEvent, (void));
ORIG(0x4E9EB0, uint16_t, SoundDefTriggerOnce_GetType, (void));
ORIG(0x4EAD10, uint16_t, SoundDefGraph_GetType, (void));
ORIG(0x4EBA80, uint16_t, SoundDefGroup_GetType, (void));

void SoundInst_Request(int id, int bucket, int hash, uint32_t flags, const SoundParams *params);

/* fcomp against 0 sets C3 for equal and for NaN */
static inline int IsZero(float v)
{
    return !(v < 0.0f || v > 0.0f);
}

void Sound_RemoveTask(void *task)
{
    QTaskList_Remove(gSoundTasks, task);
}
HOOK(0x4E6D40, Sound_RemoveTask);

void SoundHandle_Play(SoundHandle *h)
{
    if (gSoundDisabled)
        return;
    if (!h->id)
        h->id = gSoundNextId++;
    SoundInst_Request(h->id, h->bucket, h->hash, h->flags, &h->params);
}
HOOK(0x4E6D60, SoundHandle_Play);

static void SoundInst_Release(SoundInst *s)
{
    SoundInst_Unlink(s, &gSoundActive);
    SoundInst_Push(s, &gSoundFree);
}

/* Starts or refreshes sound id. A sound with volume 0 is stopped. While
   gSoundSilent is set no voices are made, the sounds are only tracked. */
void SoundInst_Request(int id, int bucket, int hash, uint32_t flags, const SoundParams *params)
{
    SoundInst *s;
    void *v;

    for (s = gSoundActive; s; s = s->next) {
        if (s->hash == hash && s->id == id)
            break;
    }
    if (s) {
        s->age = 0;
        if (params)
            memcpy(&s->params, params, sizeof(SoundParams));
        else
            SoundParams_Init(&s->params);
        if (s->voice) {
            Voice_SetParams(s->voice, &s->params);
        } else if (!gSoundSilent) {
            v = Voice_Play(bucket, hash, flags, &s->params);
            if (!v) {
                SoundInst_Release(s);
                return;
            }
            if (IsZero(s->params.volume)) {
                Voice_Stop(v);
                Voice_Free(v);
                SoundInst_Release(s);
                return;
            }
            s->voice = v;
        }
        if (!IsZero(s->params.volume))
            return;
        if (s->voice) {
            Voice_Stop(s->voice);
            Voice_Free(s->voice);
        }
        SoundInst_Release(s);
        return;
    }

    s = gSoundFree;
    if (!s)
        return;
    if (params)
        memcpy(&s->params, params, sizeof(SoundParams));
    else
        SoundParams_Init(&s->params);
    v = NULL;
    if (!gSoundSilent) {
        v = Voice_Play(bucket, hash, flags, &s->params);
        if (v) {
            if (IsZero(s->params.volume)) {
                Voice_Stop(v);
                Voice_Free(v);
                return;
            }
        } else if (!gSoundSilent) {
            return;
        }
    }
    s->voice = v;
    s->bucket = bucket;
    s->hash = hash;
    s->flags = flags;
    s->id = id;
    s->age = 0;
    SoundInst_Unlink(s, &gSoundFree);
    SoundInst_Push(s, &gSoundActive);
}
HOOK(0x4E6DB0, SoundInst_Request);

char SoundHandle_IsPlaying(const SoundHandle *h)
{
    SoundInst *s;

    for (s = gSoundActive; s; s = s->next) {
        if (s->hash == h->hash && s->id == h->id) {
            if (!s->voice)
                return 1;
            return Voice_IsPlaying(s->voice);
        }
    }
    return 0;
}
HOOK(0x4E6FA0, SoundHandle_IsPlaying);

double SoundParams_ClampVolume(SoundParams *p)
{
    if (!(p->volume >= 0.0f)) {
        p->volume = 0.0f;
        return p->maxDist;
    }
    if (p->volume > 1.0f)
        p->volume = 1.0f;
    return p->maxDist;
}
HOOK(0x4E6FF0, SoundParams_ClampVolume);

static const float kTen = 10.0f;
static const float kTenth = 0x1.99999ap-4f;

/* Linear lookup in an 11 point graph over 0..1. */
void SoundGraph_Eval(float *out, const float *graph, float x)
{
    const uint32_t *bits = (const uint32_t *)graph;
    volatile float a, b; /* rounded to float like the original */
    float ya, yb;
    int i;

    if (x >= 1.0f) {
        *(uint32_t *)out = bits[10];
        return;
    }
    if (!(x > 0.0f)) {
        *(uint32_t *)out = bits[0];
        return;
    }
    i = x87_fistp(x * kTen);
    a = (float)i * kTenth;
    b = (float)(i + 1) * kTenth;
    ya = graph[i];
    yb = graph[i + 1];
    if (a == x) {
        *(uint32_t *)out = bits[i];
        return;
    }
    if (b == x) {
        *(uint32_t *)out = bits[i + 1];
        return;
    }
    *out = (yb - ya) / (b - a) * (x - a) + ya;
}
HOOK(0x4E7030, SoundGraph_Eval);

void Sound_StopAll(void)
{
    SoundInst *s, *next;

    for (s = gSoundActive; s; s = next) {
        next = s->next;
        if (s->voice) {
            Voice_Stop(s->voice);
            Voice_Free(s->voice);
        }
        SoundInst_Release(s);
    }
}
HOOK(0x4E7130, Sound_StopAll);

/* Only the first 0x20 bytes of params are used, the rest is left unset. */
void Sound_PlayName(const char *name, uint32_t flags, const SoundParams *params)
{
    SoundParams p;
    int bucket, hash;

    if (gSoundDisabled)
        return;
    bucket = Sound_HashBucket(name);
    hash = Sound_HashName(name);
    if (params)
        memcpy(&p, params, 0x20);
    else
        SoundParams_Init(&p);
    SoundInst_Request(gSoundNextId++, bucket, hash, flags | 1, &p);
}
HOOK(0x4E7180, Sound_PlayName);

/* Plays the sample of the sound definition called name (not a group, graph or trigger). */
void Sound_PlayDefined(const char *name, uint32_t flags, const SoundParams *params)
{
    SoundFindArg arg;
    SoundParams p;
    const char *sample;
    int i, bucket, hash;

    if (gSoundDisabled || !gSoundDefs->count || !*name)
        return;
    arg.name = name;
    arg.found = NULL;
    for (i = 0; i < gSoundDefs->count; i++) {
        Task *d = gSoundDefs->items[i];
        ((TaskFn2)gTaskHandlers[Sound_GetFindEvent()][d->type])(d, &arg);
    }
    if (!arg.found || arg.found->type == SoundDefGroup_GetType() || arg.found->type == SoundDefGraph_GetType() ||
        arg.found->type == SoundDefTriggerOnce_GetType())
        return;
    sample = (const char *)arg.found + 0xFF;
    bucket = Sound_HashBucket(sample);
    hash = Sound_HashName(sample);
    if (params)
        memcpy(&p, params, 0x20);
    else
        SoundParams_Init(&p);
    SoundInst_Request(gSoundNextId++, bucket, hash, flags | 1, &p);
}
HOOK(0x4E7200, Sound_PlayDefined);

QTaskList *Sound_GetTasks(void)
{
    return gSoundTasks;
}
HOOK(0x4E7330, Sound_GetTasks);

void Sound_SetSilent(char on)
{
    gSoundSilent = on;
}
HOOK(0x4E7340, Sound_SetSilent);

void Sound_SetUnusedOff(int v)
{
    gSoundUnusedOff = v == 0;
}
HOOK(0x4E7350, Sound_SetUnusedOff);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A5E5E8) int gSoundUnusedOff;
BSS(A5E5E8) SoundInst *gSoundFree;
BSS(A5E5F4) SoundInst *gSoundActive;
BSS(A5E5F4) int gSoundNextId;
BSS(A5E608) char gSoundUnusedByte;
PAD(igb, A5E608, A5E609, 0x1);
BSS(A5E608) char gSoundDisabled;
BSS(A5E608) char gSoundSilent;
