#include <string.h>

#include "hook.h"
#include "igi.h"
#include "define/define.h"

#define gSoundDefSoundType VAR(uint16_t, 0x54D9B4)
#define gSoundDefSoundType2 VAR(uint16_t, 0x54D9B6)

void SoundDef_SetPos(void *t, const Vec3d *pos);
void SoundDef_SetGlobal(void *t, const char *on);
void SoundDefSound_Play(SoundDefSound *s, SoundPlayArg *a);
void SoundDefSound_Create(SoundDefSound *s, SoundCreateArg *arg);
void SoundDefSound_Reset(SoundDefSound *s);
void SoundDefSound_Run(SoundDefSound *s);
void SoundDefSound_Delete(SoundDefSound *s);
void SoundDefSound_Stop(SoundDefSound *s);
void SoundDefSound_Write(SoundDefSound *s, SoundWriteArg *a);
void SoundDefSound_Find(SoundDefSound *s, SoundFindArg *a);
void SoundDefSound_GetName(SoundDefSound *s, const char **out);
void SoundDefSound_SetVolume(SoundDefSound *s, const float *v);
void SoundDefSound_SetPitch(SoundDefSound *s, const float *v);
void SoundDefSound_GetRange(SoundDefSound *s, float *out);
void SoundDefSound_GetDuration(SoundDefSound *s, int *out);
void SoundDefSound_Update(SoundDefSound *s);

/* fistp with the current rounding mode, like the original */
static inline int fist(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

void SoundDefSound_Init(void)
{
    gSoundDefSoundType = TaskType_New("SoundDefSound", sizeof(SoundDefSound), Task_GetRootType(), 0x200);
    gSoundDefSoundType2 = gSoundDefSoundType;
    TaskType_SetHandler(gSoundDefSoundType, 0, SoundDefSound_Create);
    TaskType_SetHandler2(gSoundDefSoundType, 1, SoundDefSound_Run);
    TaskType_SetHandler2(gSoundDefSoundType, 2, SoundDefSound_Delete);
    TaskType_SetHandler(gSoundDefSoundType, SoundSysMicrophone_GetPlayEvent(), SoundDefSound_Play);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetListEvent(), SoundDefSound_Write);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetFindEvent(), SoundDefSound_Find);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetNameEvent(), SoundDefSound_GetName);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetSetPosEvent(), SoundDef_SetPos);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetSetVolumeEvent(), SoundDefSound_SetVolume);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetSetPitchEvent(), SoundDefSound_SetPitch);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetSetGlobalEvent(), SoundDef_SetGlobal);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetRangeEvent(), SoundDefSound_GetRange);
    TaskType_SetHandler(gSoundDefSoundType, Sound_GetDurationEvent(), SoundDefSound_GetDuration);
    TaskType_SetHandler2(gSoundDefSoundType, Sound_GetStopEvent(), SoundDefSound_Stop);
}
HOOK(0x4E8390, SoundDefSound_Init);

/* the listener is in range: (re)start the sound */
void SoundDefSound_Play(SoundDefSound *s, SoundPlayArg *a)
{
    if (s->isDef && a->defs)
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
HOOK(0x4E8520, SoundDefSound_Play);

void SoundDefSound_Create(SoundDefSound *s, SoundCreateArg *arg)
{
    SoundDefSound_Reset(s);
    if (!arg) {
        s->isDef = 1;
    } else {
        SoundDefSound *d = arg->def;

        s->offset = d->offset;
        s->pos = d->offset;
        memcpy(&s->volumeIndex, &d->volumeIndex, offsetof(SoundDefSound, loop) - offsetof(SoundDefSound, volumeIndex));
        memcpy(&s->unk124, &d->unk124, 4);
        s->loop = d->loop;
        s->isDef = arg->isDef;
        strcpy(s->name, d->name);
        strcpy(s->sample, d->sample);
        s->bucket = Sound_HashBucket(s->sample);
        s->hash = Sound_HashName(s->sample);
    }
    s->time = 0;
    s->h.bucket = 0;
    s->h.hash = 0;
    s->h.id = 0;
    s->stop = 0;
    s->h.flags = 2;
    SoundParams_Init(&s->h.params);
    if (!s->isDef)
        Sound_AddTask(s);
    SoundDefSound_Update(s);
}
HOOK(0x4E85E0, SoundDefSound_Create);

void SoundDefSound_Reset(SoundDefSound *s)
{
    s->volumeIndex = -1;
    s->pitchIndex = -1;
    s->sampleIndex = -1;
    s->offset.x = 0;
    s->offset.y = 0;
    s->offset.z = 0;
    s->pos.x = 0;
    s->pos.y = 0;
    s->pos.z = 0;
    s->volume = 1.0f;
    s->volume2 = 1.0f;
    s->pitch = 1.0f;
    s->minDist = 0;
    s->maxDist = 1.0f;
    s->range = 1.0f;
    s->unk124 = -1.0f;
    s->loop = 0;
    s->isDef = 0;
    s->name[0] = 0;
    s->sample[0] = 0;
    s->bucket = 0;
    s->hash = 0;
}
HOOK(0x4E8780, SoundDefSound_Reset);

/* plays once unless looped, deleted when done */
void SoundDefSound_Run(SoundDefSound *s)
{
    if (s->stop) {
        Task_Delete(s);
        return;
    }
    if (s->loop) {
        SoundDefSound_Update(s);
        return;
    }
    if (!(Sound_GetDuration(s->bucket, s->hash) > s->time) && !SoundHandle_IsPlaying(&s->h)) {
        Task_Delete(s);
        return;
    }
    SoundDefSound_Update(s);
    s->time = kOne / Sound_GetRate() + s->time;
}
HOOK(0x4E8840, SoundDefSound_Run);

void SoundDefSound_Delete(SoundDefSound *s)
{
    if (!s->isDef)
        Sound_RemoveTask(s);
}
HOOK(0x4E88E0, SoundDefSound_Delete);

void SoundDefSound_Stop(SoundDefSound *s)
{
    s->stop = 1;
}
HOOK(0x4E8900, SoundDefSound_Stop);

void SoundDef_Append(char *buf, const char *s)
{
    buf += strlen(buf);
    while ((*buf++ = *s++))
        ;
}

void SoundDef_WriteIndent(SoundWriteArg *a)
{
    int i;

    for (i = 0; i < a->indent; i++)
        SoundDef_Append(a->buf, "  ");
}

/* the last child of a group gets no comma */
void SoundDef_WriteEnd(Task *t, SoundWriteArg *a)
{
    const char *end;

    if (!a->indent) {
        end = ");\r\n";
    } else {
        Task *last = t->parent->tailpred;

        end = t == (last->prev ? last : NULL) ? ")" : "),\r\n";
    }
    SoundDef_Append(a->buf, end);
}

/* appends the DefineSound() call that makes this definition */
void SoundDefSound_Write(SoundDefSound *s, SoundWriteArg *a)
{
    char line[0x404];

    SoundDef_WriteIndent(a);
    crt_sprintf(line,
                "%s(\"%s\", \"%s\", %8.08f, %8.08f, %8.08f, %8.08f, %8.08f, %8.08f, %d, %d, %d, %8.08f, %s",
                "DefineSound", s->name, s->sample, s->offset.x, s->offset.y, s->offset.z, (double)s->volume,
                (double)s->minDist, (double)s->maxDist, s->volumeIndex, s->pitchIndex, s->sampleIndex,
                (double)s->unk124, s->loop ? "TRUE" : "FALSE");
    SoundDef_Append(a->buf, line);
    SoundDef_WriteEnd(&s->task, a);
}
HOOK(0x4E8910, SoundDefSound_Write);

void SoundDefSound_Find(SoundDefSound *s, SoundFindArg *a)
{
    if (!a->found && strcmp(a->name, s->name) == 0)
        a->found = &s->task;
}
HOOK(0x4E8AA0, SoundDefSound_Find);

void SoundDefSound_GetName(SoundDefSound *s, const char **out)
{
    if (s->name[0])
        *out = s->name;
}
HOOK(0x4E8B00, SoundDefSound_GetName);

/* a negative volume leaves that one unchanged */
void SoundDefSound_SetVolume(SoundDefSound *s, const float *v)
{
    if (v[0] >= kZero)
        s->volume = v[0];
    if (v[1] >= kZero)
        s->volume2 = v[1];
}
HOOK(0x4E8B20, SoundDefSound_SetVolume);

void SoundDefSound_SetPitch(SoundDefSound *s, const float *v)
{
    memcpy(&s->pitch, v, 4);
}
HOOK(0x4E8B60, SoundDefSound_SetPitch);

/* inside a group the offset adds to the range */
void SoundDefSound_GetRange(SoundDefSound *s, float *out)
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
HOOK(0x4E8B80, SoundDefSound_GetRange);

/* in ticks */
void SoundDefSound_GetDuration(SoundDefSound *s, int *out)
{
    float d = rf(Sound_GetDuration(s->bucket, s->hash));
    int n = fist((long double)Sound_GetRate() * d);

    if (n > *out)
        *out = n;
}
HOOK(0x4E8BD0, SoundDefSound_GetDuration);

void SoundDefSound_Shutdown(void)
{
    TaskType_Free(gSoundDefSoundType);
    gSoundDefSoundType = TASK_NO_TYPE;
    gSoundDefSoundType2 = TASK_NO_TYPE;
}
HOOK(0x4E8C20, SoundDefSound_Shutdown);

uint16_t SoundDefSound_GetType(void)
{
    return gSoundDefSoundType2;
}
HOOK(0x4E8C50, SoundDefSound_GetType);

/* DefineSound(name, sample, x, y, z, volume, minDist, maxDist, volumeIndex,
   pitchIndex, sampleIndex, unk, loop) */
ScriptValue *DefineSound(ScriptValue *out, void *data, void *args)
{
    SoundDefSound def;
    SoundCreateArg arg;

    arg.def = &def;
    arg.isDef = 1;
    SoundDefSound_Reset(&def);
    Script_GetArgString(args, 0, def.name, sizeof(def.name));
    Script_GetArgString(args, 1, def.sample, sizeof(def.sample));
    def.offset.x = Script_GetArgReal(args, 2);
    def.offset.y = Script_GetArgReal(args, 3);
    def.offset.z = Script_GetArgReal(args, 4);
    def.volume = Script_GetArgReal(args, 5);
    def.minDist = Script_GetArgReal(args, 6);
    def.maxDist = Script_GetArgReal(args, 7);
    def.volumeIndex = Script_GetArgInt(args, 8);
    def.pitchIndex = Script_GetArgInt(args, 9);
    def.sampleIndex = Script_GetArgInt(args, 10);
    def.unk124 = Script_GetArgReal(args, 11);
    def.loop = Script_GetArgInt(args, 12);
    def.isDef = 1;
    Task_New(data, gSoundDefSoundType2, &arg);
    out->i = 13;
    out->d = 13.0;
    out->s = gScriptEmptyString;
    return out;
}
HOOK(0x4E8C60, DefineSound);

/* asks the owner for position, rotation and script values, then sets up the
   sound parameters */
void SoundDefSound_Update(SoundDefSound *s)
{
    SoundInfo info;
    Task *p = s->task.parent;
    TaskFn fn;

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

        r.x = m[2] * s->pos.z + m[1] * s->pos.y + m[0] * s->pos.x;
        r.y = m[5] * s->pos.z + m[3] * s->pos.x + m[4] * s->pos.y;
        r.z = m[8] * s->pos.z + m[6] * s->pos.x + m[7] * s->pos.y;
        s->pos = r;
    }
    if (info.pos) {
        s->pos.x = s->pos.x + info.pos->x;
        s->pos.y = info.pos->y + s->pos.y;
        s->pos.z = info.pos->z + s->pos.z;
    }
    if (info.values) {
        int *v = info.values + 1;

        if (s->volumeIndex != -1)
            memcpy(&s->h.params.volume, &v[s->volumeIndex], 4);
        if (s->pitchIndex != -1)
            memcpy(&s->h.params.pitch, &v[s->pitchIndex], 4);
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
}
HOOK(0x4E8DC0, SoundDefSound_Update);
