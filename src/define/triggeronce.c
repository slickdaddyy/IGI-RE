#include <string.h>

#include "hook.h"
#include "igi.h"
#include "define/define.h"

extern uint16_t gSoundDefTriggerOnceType;
extern uint16_t gSoundDefTriggerOnceType2;

ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4D14D0, uint8_t, QCube_GetNearClipEvent, (void));

void SoundDef_SetPos(void *t, const Vec3d *pos);
void SoundDef_SetGlobal(void *t, const char *on);
void SoundDefGraph_SetVolume(void *t, const float *v);
void SoundDefTriggerOnce_Play(SoundDefTriggerOnce *s, SoundPlayArg *a);
void SoundDefTriggerOnce_Create(SoundDefTriggerOnce *s, SoundCreateArg *arg);
void SoundDefTriggerOnce_Reset(SoundDefTriggerOnce *s);
void SoundDefTriggerOnce_Run(SoundDefTriggerOnce *s);
void SoundDefTriggerOnce_Delete(SoundDefTriggerOnce *s);
void SoundDefTriggerOnce_Write(SoundDefTriggerOnce *s, SoundWriteArg *a);
void SoundDefTriggerOnce_Find(SoundDefTriggerOnce *s, SoundFindArg *a);
void SoundDefTriggerOnce_GetName(SoundDefTriggerOnce *s, const char **out);
void SoundDefTriggerOnce_SetPitch(SoundDefTriggerOnce *s, const float *v);
void SoundDefTriggerOnce_GetRange(SoundDefTriggerOnce *s, float *out);
void SoundDefTriggerOnce_GetDuration(SoundDefTriggerOnce *s, int *out);
void SoundDefTriggerOnce_Stop(SoundDefTriggerOnce *s);
void SoundDefTriggerOnce_Update(SoundDefTriggerOnce *s);

/* fistp with the current rounding mode, like the original */
static inline int fist(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

void SoundDefTriggerOnce_Init(void)
{
    gSoundDefTriggerOnceType =
        TaskType_New("SoundDefTriggerOnce", sizeof(SoundDefTriggerOnce), Task_GetRootType(), 0x200);
    gSoundDefTriggerOnceType2 = gSoundDefTriggerOnceType;
    TaskType_SetHandler(gSoundDefTriggerOnceType, 0, SoundDefTriggerOnce_Create);
    TaskType_SetHandler2(gSoundDefTriggerOnceType, 1, SoundDefTriggerOnce_Run);
    TaskType_SetHandler2(gSoundDefTriggerOnceType, 2, SoundDefTriggerOnce_Delete);
    TaskType_SetHandler(gSoundDefTriggerOnceType, SoundSysMicrophone_GetPlayEvent(), SoundDefTriggerOnce_Play);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetListEvent(), SoundDefTriggerOnce_Write);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetFindEvent(), SoundDefTriggerOnce_Find);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetNameEvent(), SoundDefTriggerOnce_GetName);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetSetPosEvent(), SoundDef_SetPos);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetSetVolumeEvent(), SoundDefGraph_SetVolume);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetSetPitchEvent(), SoundDefTriggerOnce_SetPitch);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetSetGlobalEvent(), SoundDef_SetGlobal);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetRangeEvent(), SoundDefTriggerOnce_GetRange);
    TaskType_SetHandler(gSoundDefTriggerOnceType, Sound_GetDurationEvent(), SoundDefTriggerOnce_GetDuration);
    TaskType_SetHandler2(gSoundDefTriggerOnceType, Sound_GetStopEvent(), SoundDefTriggerOnce_Stop);
    TaskType_SetHandler(gSoundDefTriggerOnceType, QCube_GetNearClipEvent(), NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, 9, NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, 8, NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, 10, NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, 11, NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, 12, NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, 4, NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, World_GetEvent1(), NULL);
    TaskType_SetHandler(gSoundDefTriggerOnceType, World_GetEvent0(), NULL);
}
HOOK(0x4E9520, SoundDefTriggerOnce_Init);

/* only plays while triggered */
void SoundDefTriggerOnce_Play(SoundDefTriggerOnce *s, SoundPlayArg *a)
{
    if (s->isDef && a->defs)
        return;
    if (!s->active)
        return;
    if (s->h.flags & 2) {
        Vec3d *lp = SoundSysMicrophone_GetPos(a->listener);
        double r = rd(s->range * kDistScale);

        if (!(r * r >= Vec3d_DistanceSq(lp, &s->pos)))
            return;
    }
    if (s->hash != s->h.hash) {
        s->h.hash = s->hash;
        s->h.bucket = s->bucket;
        s->h.id = 0;
        s->time = 0;
    }
    s->h.flags = (s->h.flags & ~0x10) | (s->loop ? 0x10 : 0);
    SoundHandle_Play(&s->h);
}
HOOK(0x4E9760, SoundDefTriggerOnce_Play);

void SoundDefTriggerOnce_Create(SoundDefTriggerOnce *s, SoundCreateArg *arg)
{
    SoundDefTriggerOnce_Reset(s);
    if (!arg) {
        s->isDef = 1;
    } else {
        SoundDefTriggerOnce *d = arg->def;

        s->offset = d->offset;
        s->pos = d->pos;
        memcpy(&s->volume, &d->volume, 4);
        memcpy(&s->minDist, &d->minDist, 4);
        memcpy(&s->maxDist, &d->maxDist, 4);
        memcpy(&s->range, &d->range, 4);
        memcpy(&s->hold, &d->hold, 4);
        s->triggerIndex = d->triggerIndex;
        s->volumeIndex = d->volumeIndex;
        s->pitchIndex = d->pitchIndex;
        s->sampleIndex = d->sampleIndex;
        s->loop = d->loop;
        s->triggerOnZero = d->triggerOnZero;
        s->isDef = arg->isDef;
        strcpy(s->name, d->name);
        strcpy(s->sample, d->sample);
        /* hashes (and upper-cases) the definition's copy */
        s->bucket = Sound_HashBucket(d->sample);
        s->hash = Sound_HashName(d->sample);
    }
    s->time = 0;
    s->h.bucket = 0;
    s->h.hash = 0;
    s->h.id = 0;
    s->active = 0;
    s->stop = 0;
    s->h.flags = 2;
    SoundParams_Init(&s->h.params);
    if (!s->isDef)
        Sound_AddTask(s);
    SoundDefTriggerOnce_Update(s);
}
HOOK(0x4E9830, SoundDefTriggerOnce_Create);

void SoundDefTriggerOnce_Reset(SoundDefTriggerOnce *s)
{
    s->volume = 1.0f;
    s->volume2 = 1.0f;
    s->pitch = 1.0f;
    s->maxDist = 1.0f;
    s->range = 1.0f;
    s->offset.x = 0;
    s->offset.y = 0;
    s->offset.z = 0;
    s->pos.x = 0;
    s->pos.y = 0;
    s->pos.z = 0;
    s->minDist = 0;
    s->hold = -1.0f;
    s->triggerIndex = -1;
    s->volumeIndex = -1;
    s->pitchIndex = -1;
    s->sampleIndex = -1;
    s->loop = 0;
    s->triggerOnZero = 0;
    s->isDef = 0;
    s->name[0] = 0;
    s->sample[0] = 0;
    s->bucket = 0;
    s->hash = 0;
}
HOOK(0x4E99D0, SoundDefTriggerOnce_Reset);

void SoundDefTriggerOnce_Run(SoundDefTriggerOnce *s)
{
    Task *c;

    if (s->stop) {
        Task_Delete(s);
        return;
    }
    if (s->active) {
        SoundDefTriggerOnce_Update(s);
        s->time = kOne / Sound_GetRate() + s->time;
    } else {
        SoundDefTriggerOnce_Update(s);
    }
    c = s->task.head;
    if (!c->next || !c)
        return;
    do {
        TaskFn fn;

        gTaskStack[gTaskStackTop++] = c->next && c->next->next ? c->next : NULL;
        fn = gTaskHandlers[1][c->type];
        if (fn)
            ((void (*)(void *))fn)(c);
        c = gTaskStack[--gTaskStackTop];
    } while (c);
}
HOOK(0x4E9AA0, SoundDefTriggerOnce_Run);

void SoundDefTriggerOnce_Delete(SoundDefTriggerOnce *s)
{
    if (!s->isDef)
        Sound_RemoveTask(s);
}
HOOK(0x4E9B60, SoundDefTriggerOnce_Delete);

/* appends the DefineTriggerOnce() call that makes this definition */
void SoundDefTriggerOnce_Write(SoundDefTriggerOnce *s, SoundWriteArg *a)
{
    char line[0x404];

    SoundDef_WriteIndent(a);
    crt_sprintf(line,
                "%s(\"%s\", \"%s\", %8.08f, %8.08f, %8.08f, %8.08f, %8.08f, %8.08f, %8.08f, %d, %d, %d, %d, %s, "
                "%s",
                "DefineTriggerOnce", s->name, s->sample, s->offset.x, s->offset.y, s->offset.z, (double)s->volume,
                (double)s->minDist, (double)s->maxDist, (double)s->hold, s->triggerIndex, s->volumeIndex,
                s->pitchIndex, s->sampleIndex, s->loop ? "TRUE" : "FALSE", s->triggerOnZero ? "TRUE" : "FALSE");
    SoundDef_Append(a->buf, line);
    SoundDef_WriteEnd(&s->task, a);
}
HOOK(0x4E9B80, SoundDefTriggerOnce_Write);

void SoundDefTriggerOnce_Find(SoundDefTriggerOnce *s, SoundFindArg *a)
{
    if (!a->found && strcmp(a->name, s->name) == 0)
        a->found = &s->task;
}
HOOK(0x4E9D30, SoundDefTriggerOnce_Find);

void SoundDefTriggerOnce_GetName(SoundDefTriggerOnce *s, const char **out)
{
    if (s->name[0])
        *out = s->name;
}
HOOK(0x4E9D90, SoundDefTriggerOnce_GetName);

void SoundDefTriggerOnce_SetPitch(SoundDefTriggerOnce *s, const float *v)
{
    memcpy(&s->pitch, v, 4);
}
HOOK(0x4E9DB0, SoundDefTriggerOnce_SetPitch);

void SoundDefTriggerOnce_GetRange(SoundDefTriggerOnce *s, float *out)
{
    Task *p = s->task.parent;
    long double r;

    if (p && p->type == SoundDefGroup_GetType())
        r = Vec3d_Length(&s->offset) + s->range;
    else
        r = s->range;
    if (r > *out)
        *out = r;
}
HOOK(0x4E9DD0, SoundDefTriggerOnce_GetRange);

void SoundDefTriggerOnce_GetDuration(SoundDefTriggerOnce *s, int *out)
{
    float d = rf(Sound_GetDuration(s->bucket, s->hash));
    int n = fist((long double)Sound_GetRate() * d);

    if (n > *out)
        *out = n;
}
HOOK(0x4E9E20, SoundDefTriggerOnce_GetDuration);

void SoundDefTriggerOnce_Stop(SoundDefTriggerOnce *s)
{
    s->stop = 1;
}
HOOK(0x4E9E70, SoundDefTriggerOnce_Stop);

void SoundDefTriggerOnce_Shutdown(void)
{
    TaskType_Free(gSoundDefTriggerOnceType);
    gSoundDefTriggerOnceType = TASK_NO_TYPE;
    gSoundDefTriggerOnceType2 = TASK_NO_TYPE;
}
HOOK(0x4E9E80, SoundDefTriggerOnce_Shutdown);

uint16_t SoundDefTriggerOnce_GetType(void)
{
    return gSoundDefTriggerOnceType2;
}
HOOK(0x4E9EB0, SoundDefTriggerOnce_GetType);

/* DefineTriggerOnce(name, sample, x, y, z, volume, minDist, maxDist, hold,
   triggerIndex, volumeIndex, pitchIndex, sampleIndex, loop, triggerOnZero) */
ScriptValue *DefineTriggerOnce(ScriptValue *out, void *data, void *args)
{
    SoundDefTriggerOnce def;
    SoundCreateArg arg;

    arg.def = &def;
    arg.isDef = 1;
    SoundDefTriggerOnce_Reset(&def);
    Script_GetArgString(args, 0, def.name, sizeof(def.name));
    Script_GetArgString(args, 1, def.sample, sizeof(def.sample));
    def.offset.x = Script_GetArgReal(args, 2);
    def.offset.y = Script_GetArgReal(args, 3);
    def.offset.z = Script_GetArgReal(args, 4);
    def.volume = Script_GetArgReal(args, 5);
    def.minDist = Script_GetArgReal(args, 6);
    def.maxDist = Script_GetArgReal(args, 7);
    def.hold = Script_GetArgReal(args, 8);
    def.triggerIndex = Script_GetArgInt(args, 9);
    def.volumeIndex = Script_GetArgInt(args, 10);
    def.pitchIndex = Script_GetArgInt(args, 11);
    def.sampleIndex = Script_GetArgInt(args, 12);
    def.loop = Script_GetArgInt(args, 13);
    def.triggerOnZero = Script_GetArgInt(args, 14);
    def.isDef = 1;
    Task_New(data, gSoundDefTriggerOnceType2, &arg);
    out->i = 15;
    out->d = 15.0;
    out->s = gScriptEmptyString;
    return out;
}
HOOK(0x4E9EC0, DefineTriggerOnce);

/* the trigger value starts the sound once; it can start again after the
   hold time (or, without one, once it stopped) when the trigger went off */
void SoundDefTriggerOnce_Update(SoundDefTriggerOnce *s)
{
    SoundInfo info;
    Task *p = s->task.parent;
    TaskFn fn;
    char trigger = 0;

    info.pos = NULL;
    info.matrix = NULL;
    info.values = NULL;
    fn = gTaskHandlers[Sound_GetInfoEvent()][p->type];
    if (fn)
        fn(p, &info);
    SoundParams_Init(&s->h.params);
    s->pos = s->offset;
    if (info.matrix) {
        const float *m = info.matrix;
        Vec3d r;

        r.x = m[1] * s->pos.y + m[2] * s->pos.z + m[0] * s->pos.x;
        r.y = m[4] * s->pos.y + m[5] * s->pos.z + m[3] * s->pos.x;
        r.z = m[7] * s->pos.y + m[8] * s->pos.z + m[6] * s->pos.x;
        s->pos = r;
    }
    if (info.pos) {
        s->pos.x = info.pos->x + s->pos.x;
        s->pos.y = info.pos->y + s->pos.y;
        s->pos.z = info.pos->z + s->pos.z;
    }
    if (info.values) {
        int *v = info.values + 1;

        if (s->volumeIndex != -1)
            memcpy(&s->h.params.volume, &v[s->volumeIndex], 4);
        if (s->pitchIndex != -1)
            memcpy(&s->h.params.pitch, &v[s->pitchIndex], 4);
        if (s->triggerIndex != -1) {
            float t;

            memcpy(&t, &v[s->triggerIndex], 4);
            if (s->triggerOnZero)
                trigger = !(t < kZero || t > kZero);
            else
                trigger = fist(t);
        }
        if (s->sampleIndex != -1) {
            const char *name = (const char *)v[s->sampleIndex];

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
    s->h.params.pos = s->pos;
    s->h.params.volume = s->volume * s->volume2 * s->h.params.volume;
    s->h.params.minDist = s->minDist;
    s->h.params.maxDist = s->maxDist;
    s->h.params.pitch = s->pitch * s->h.params.pitch;
    s->h.params.vel.x = 0;
    s->h.params.vel.y = 0;
    s->h.params.vel.z = 0;
    s->h.params.start = s->time;
    s->range = SoundParams_ClampVolume(&s->h.params);
    if (!s->active) {
        if (trigger) {
            s->time = 0;
            s->active = 1;
        }
        return;
    }
    if (!(s->hold >= kZero)) {
        if (!s->loop && SoundHandle_IsPlaying(&s->h))
            return;
    } else if (!(s->time >= s->hold)) {
        return;
    }
    if (!trigger)
        s->active = 0;
}
HOOK(0x4EA040, SoundDefTriggerOnce_Update);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DA60) uint16_t gSoundDefTriggerOnceType INIT(385);
DATA(54DA60) uint16_t gSoundDefTriggerOnceType2 INIT(385);
