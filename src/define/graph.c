#include <string.h>

#include "hook.h"
#include "igi.h"
#include "define/define.h"

extern uint16_t gSoundDefGraphType;
extern uint16_t gSoundDefGraphType2;

ORIG(0x4EAEB0, void, SoundDefGraph_Update, (SoundDefGraph *s));

void SoundDef_SetPos(void *t, const Vec3d *pos);
void SoundDef_SetGlobal(void *t, const char *on);
void SoundDefGraph_SetVolume(void *t, const float *v);
void SoundDefGraph_Play(SoundDefGraph *s, SoundPlayArg *a);
void SoundDefGraph_Create(SoundDefGraph *s, SoundCreateArg *arg);
void SoundDefGraph_Reset(SoundDefGraph *s);
void SoundDefGraph_Run(SoundDefGraph *s);
void SoundDefGraph_Delete(SoundDefGraph *s);
void SoundDefGraph_Stop(SoundDefGraph *s);
void SoundDefGraph_Write(SoundDefGraph *s, SoundWriteArg *a);
void SoundDefGraph_Find(SoundDefGraph *s, SoundFindArg *a);
void SoundDefGraph_GetName(SoundDefGraph *s, const char **out);
void SoundDefGraph_SetPitch(SoundDefGraph *s, const float *v);
void SoundDefGraph_GetRange(SoundDefGraph *s, float *out);
void SoundDefGraph_GetDuration(SoundDefGraph *s, int *out);

/* fistp with the current rounding mode, like the original */
static inline int fist(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return (int)r;
}

void SoundDefGraph_Init(void)
{
    gSoundDefGraphType = TaskType_New("SoundDefGraph", sizeof(SoundDefGraph), Task_GetRootType(), 0x200);
    gSoundDefGraphType2 = gSoundDefGraphType;
    TaskType_SetHandler(gSoundDefGraphType, 0, SoundDefGraph_Create);
    TaskType_SetHandler2(gSoundDefGraphType, 1, SoundDefGraph_Run);
    TaskType_SetHandler2(gSoundDefGraphType, 2, SoundDefGraph_Delete);
    TaskType_SetHandler(gSoundDefGraphType, SoundSysMicrophone_GetPlayEvent(), SoundDefGraph_Play);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetListEvent(), SoundDefGraph_Write);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetFindEvent(), SoundDefGraph_Find);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetNameEvent(), SoundDefGraph_GetName);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetSetPosEvent(), SoundDef_SetPos);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetSetVolumeEvent(), SoundDefGraph_SetVolume);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetSetPitchEvent(), SoundDefGraph_SetPitch);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetSetGlobalEvent(), SoundDef_SetGlobal);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetRangeEvent(), SoundDefGraph_GetRange);
    TaskType_SetHandler(gSoundDefGraphType, Sound_GetDurationEvent(), SoundDefGraph_GetDuration);
    TaskType_SetHandler2(gSoundDefGraphType, Sound_GetStopEvent(), SoundDefGraph_Stop);
}
HOOK(0x4EA3D0, SoundDefGraph_Init);

/* graphs always loop */
void SoundDefGraph_Play(SoundDefGraph *s, SoundPlayArg *a)
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
    }
    s->h.flags |= 0x10;
    SoundHandle_Play(&s->h);
}
HOOK(0x4EA560, SoundDefGraph_Play);

void SoundDefGraph_Create(SoundDefGraph *s, SoundCreateArg *arg)
{
    SoundDefGraph_Reset(s);
    if (!arg) {
        s->isDef = 1;
    } else {
        SoundDefGraph *d = arg->def;

        s->offset = d->offset;
        s->pos = d->offset;
        memcpy(&s->volume, &d->volume, 12);
        memcpy(&s->minDist, &d->minDist, 12);
        memcpy(&s->volumeIndex, &d->volumeIndex, 16 + sizeof(d->volumeGraph) + sizeof(d->pitchGraph));
        strcpy(s->name, d->name);
        strcpy(s->sample, d->sample);
        s->isDef = arg->isDef;
        s->bucket = Sound_HashBucket(s->sample);
        s->hash = Sound_HashName(s->sample);
    }
    s->stop = 0;
    s->h.bucket = 0;
    s->h.hash = 0;
    s->h.id = 0;
    s->h.flags = 0x12;
    SoundParams_Init(&s->h.params);
    if (!s->isDef)
        Sound_AddTask(s);
    SoundDefGraph_Update(s);
}
HOOK(0x4EA600, SoundDefGraph_Create);

void SoundDefGraph_Reset(SoundDefGraph *s)
{
    int i;

    s->offset.x = 0;
    s->offset.y = 0;
    s->offset.z = 0;
    s->pos.x = 0;
    s->pos.y = 0;
    s->pos.z = 0;
    s->volumeIndex = -1;
    s->pitchIndex = -1;
    s->pitchAddIndex = -1;
    s->sampleIndex = -1;
    s->volume = 1.0f;
    s->volume2 = 1.0f;
    s->pitch = 1.0f;
    s->minDist = 0;
    s->maxDist = 1.0f;
    s->range = 1.0f;
    for (i = 0; i < 11; i++) {
        s->volumeGraph[i] = 1.0f;
        s->pitchGraph[i] = 1.0f;
    }
    s->name[0] = 0;
    s->sample[0] = 0;
    s->isDef = 0;
    s->bucket = 0;
    s->hash = 0;
}
HOOK(0x4EA7B0, SoundDefGraph_Reset);

void SoundDefGraph_Run(SoundDefGraph *s)
{
    if (s->stop) {
        Task_Delete(s);
        return;
    }
    SoundDefGraph_Update(s);
}
HOOK(0x4EA880, SoundDefGraph_Run);

void SoundDefGraph_Delete(SoundDefGraph *s)
{
    if (!s->isDef)
        Sound_RemoveTask(s);
}
HOOK(0x4EA8A0, SoundDefGraph_Delete);

void SoundDefGraph_Stop(SoundDefGraph *s)
{
    s->stop = 1;
}
HOOK(0x4EA8C0, SoundDefGraph_Stop);

/* appends the DefineGraph() call that makes this definition */
void SoundDefGraph_Write(SoundDefGraph *s, SoundWriteArg *a)
{
    char line[0x404];
    int i;

    SoundDef_WriteIndent(a);
    crt_sprintf(line, "%s(\"%s\", \"%s\", %8.08f, %8.08f, %8.08f, %8.08f, %8.08f, %8.08f, %d, %d, %d, %d",
                "DefineGraph", s->name, s->sample, s->offset.x, s->offset.y, s->offset.z, (double)s->volume,
                (double)s->minDist, (double)s->maxDist, s->volumeIndex, s->pitchIndex, s->sampleIndex,
                s->pitchAddIndex);
    SoundDef_Append(a->buf, line);
    for (i = 0; i < 11; i++) {
        crt_sprintf(line, ", %8.08f", (double)s->volumeGraph[i]);
        SoundDef_Append(a->buf, line);
    }
    for (i = 0; i < 11; i++) {
        crt_sprintf(line, ", %8.08f", (double)s->pitchGraph[i]);
        SoundDef_Append(a->buf, line);
    }
    SoundDef_WriteEnd(&s->task, a);
}
HOOK(0x4EA8D0, SoundDefGraph_Write);

void SoundDefGraph_Find(SoundDefGraph *s, SoundFindArg *a)
{
    if (!a->found && strcmp(a->name, s->name) == 0)
        a->found = &s->task;
}
HOOK(0x4EAB30, SoundDefGraph_Find);

void SoundDefGraph_GetName(SoundDefGraph *s, const char **out)
{
    if (s->name[0])
        *out = s->name;
}
HOOK(0x4EAB90, SoundDefGraph_GetName);

/* all three definition types keep their offset at 0xA0 */
void SoundDef_SetPos(void *t, const Vec3d *pos)
{
    ((SoundDefGraph *)t)->offset = *pos;
}
HOOK(0x4EABB0, SoundDef_SetPos);

/* also used by SoundDefTriggerOnce, same fields */
void SoundDefGraph_SetVolume(void *t, const float *v)
{
    SoundDefGraph *s = t;

    if (v[0] >= kZero)
        s->volume = v[0];
    if (v[1] >= kZero)
        s->volume2 = v[1];
}
HOOK(0x4EABD0, SoundDefGraph_SetVolume);

/* straight into the sound parameters */
void SoundDefGraph_SetPitch(SoundDefGraph *s, const float *v)
{
    memcpy(&s->h.params.pitch, v, 4);
}
HOOK(0x4EAC10, SoundDefGraph_SetPitch);

/* on: heard at any distance */
void SoundDef_SetGlobal(void *t, const char *on)
{
    SoundDefGraph *s = t;

    if (*on)
        s->h.flags &= ~2;
    else
        s->h.flags |= 2;
}
HOOK(0x4EAC20, SoundDef_SetGlobal);

void SoundDefGraph_GetRange(SoundDefGraph *s, float *out)
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
HOOK(0x4EAC40, SoundDefGraph_GetRange);

void SoundDefGraph_GetDuration(SoundDefGraph *s, int *out)
{
    float d = rf(Sound_GetDuration(s->bucket, s->hash));
    int n = fist((long double)Sound_GetRate() * d);

    if (n > *out)
        *out = n;
}
HOOK(0x4EAC90, SoundDefGraph_GetDuration);

void SoundDefGraph_Shutdown(void)
{
    TaskType_Free(gSoundDefGraphType);
    gSoundDefGraphType = TASK_NO_TYPE;
    gSoundDefGraphType2 = TASK_NO_TYPE;
}
HOOK(0x4EACE0, SoundDefGraph_Shutdown);

uint16_t SoundDefGraph_GetType(void)
{
    return gSoundDefGraphType2;
}
HOOK(0x4EAD10, SoundDefGraph_GetType);

/* DefineGraph(name, sample, x, y, z, volume, minDist, maxDist, volumeIndex,
   pitchIndex, sampleIndex, pitchAddIndex, 11 volume points, 11 pitch points) */
ScriptValue *DefineGraph(ScriptValue *out, void *data, void *args)
{
    SoundDefGraph def;
    SoundCreateArg arg;
    int i, n;

    arg.def = &def;
    arg.isDef = 1;
    SoundDefGraph_Reset(&def);
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
    def.pitchAddIndex = Script_GetArgInt(args, 11);
    n = 12;
    for (i = 0; i < 11; i++)
        def.volumeGraph[i] = Script_GetArgReal(args, n++);
    for (i = 0; i < 11; i++)
        def.pitchGraph[i] = Script_GetArgReal(args, n++);
    def.isDef = 1;
    Task_New(data, gSoundDefGraphType2, &arg);
    out->i = n;
    out->d = n;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4EAD20, DefineGraph);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DAD8) uint16_t gSoundDefGraphType INIT(385);
DATA(54DAD8) uint16_t gSoundDefGraphType2 INIT(385);
