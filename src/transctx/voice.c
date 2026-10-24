#include <string.h>

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include "hook.h"
#include "igi.h"
#include "transctx/transctxtypes.h"

ORIG(0x495620, void, Sound_ListPush, (void *node, void *head));
ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail, (void *list, void *node));
ORIG(0x4AF960, void, List_Remove, (void *node));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B1510, void *, File_Open, (const char *name, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B16D0, int, File_Read, (void *f, void *buf, int size));
ORIG(0x4B1730, int, File_Seek, (void *f, int pos, int whence));
ORIG(0x4B17C0, void, File_Flush, (void *f));
ORIG(0x4B3130, long double, Vec3d_Length, (const double *v));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x4B64E0, void *, Resource_Find, (const char *name));

/* IMA ADPCM decoder state. */
typedef struct AdpcmState {
    short channels;
    short sample[2];
    short index[2];
    char odd; /* the low nibble of the last byte is still to be decoded */
} AdpcmState;

/* A playing sound, 32 of them at gSoundVoices. Positions are in sample frames. */
typedef struct Voice {
    struct Voice *next;
    struct Voice *prev;
    int flags; /* 1 update twice, 2 positional, 4 has 3D buffer, 0x10 loop, 0x20 music */
    Sound *sound;
    IDirectSoundBuffer *buffer;
    IUnknown *buffer3d;
    void *file;
    int startTime;
    int playPos;
    int readPos;
    int writePos;
    int endTime;
    int unk30;
    int halfFrames;
    char unk38;
    int state; /* 0 playing, 1 file ended, 2 stopped, -1 DirectSound error */
    AdpcmState adpcm;
} Voice;

_Static_assert(sizeof(AdpcmState) == 0xC, "AdpcmState");
_Static_assert(sizeof(Voice) == 0x4C, "Voice");

extern int gSoundVoiceCount;
extern double gListenerPos[];
extern float gListenerMatrix[];
extern SoundList gSoundLists[];
extern unsigned char gAdpcmBuffer[];
extern float gMusicVolume;
extern float gMasterVolume;
extern float gSfxVolume;
extern int gSoundSwapPan;
extern int gVoiceUpdateRate;
extern const signed char gAdpcmIndexTable[];
extern const short gAdpcmStepTable[];
extern double gListenerVel[];
extern IDirectSound *gDSound;
extern int gVolumeTable[];
extern char gSoundReady;
extern Voice *gActiveVoices;
extern Voice *gFreeVoices;
extern int gFlowFrames;

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

F32(kZero, 0x00000000);
F32(kOne, 0x3F800000);
F32(kMinusOne, 0xBF800000);
F32(kHalf, 0x3F000000);
F32(kTwo, 0x40000000);
F32(kVolumeSteps, 0x461C4000);
static const double kOneD = 1.0;
static const double kZeroD = 0.0;
static const double kDistScale = 4096.0;

static long long Round(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return r;
}

void SoundList_Add(SoundList *lists, int key1, int key2, Sound *s)
{
    List_AddTail(&lists[key1], s);
    s->key1 = key1;
    s->key2 = key2;
}
HOOK(0x496250, SoundList_Add);

/* Decodes frames samples per channel; the high nibble of each byte comes first. */
void Adpcm_Decode(const unsigned char *src, short *dst, int frames, AdpcmState *st)
{
    int sample[2], index[2], step[2];
    int byte = 0;
    int ch, nib, diff, s;

    sample[0] = st->sample[0];
    sample[1] = st->sample[1];
    index[0] = st->index[0];
    index[1] = st->index[1];
    step[0] = gAdpcmStepTable[index[0]];
    step[1] = gAdpcmStepTable[index[1]];
    if (st->odd)
        byte = *src++;
    for (; frames > 0; frames--) {
        for (ch = 0; ch < st->channels; ch++) {
            if (st->odd) {
                nib = byte & 0xF;
            } else {
                byte = *src++;
                nib = (byte >> 4) & 0xF;
            }
            st->odd = !st->odd;
            index[ch] += gAdpcmIndexTable[nib];
            if (index[ch] < 0)
                index[ch] = 0;
            else if (index[ch] > 88)
                index[ch] = 88;
            s = step[ch];
            diff = s >> 3;
            if (nib & 4)
                diff += s;
            if (nib & 2)
                diff += s >> 1;
            if (nib & 1)
                diff += s >> 2;
            sample[ch] = (nib & 8) ? sample[ch] - diff : sample[ch] + diff;
            if (sample[ch] < -0x8000)
                sample[ch] = -0x8000;
            else if (sample[ch] > 0x7FFF)
                sample[ch] = 0x7FFF;
            step[ch] = gAdpcmStepTable[index[ch]];
            *dst++ = (short)sample[ch];
        }
    }
    st->index[0] = index[0];
    st->sample[0] = sample[0];
    st->sample[1] = sample[1];
    st->index[1] = index[1];
}
HOOK(0x496280, Adpcm_Decode);

void Adpcm_Init(AdpcmState *st, short channels)
{
    st->channels = channels;
    st->index[0] = 0;
    st->sample[0] = 0;
    st->index[1] = 0;
    st->sample[1] = 0;
    st->odd = 0;
}
HOOK(0x496420, Adpcm_Init);

Sound *SoundList_Find(SoundList *lists, int key1, int key2);

Sound *Sound_Find(int key1, int key2)
{
    return SoundList_Find(gSoundLists, key1, key2);
}
HOOK(0x496450, Sound_Find);

Sound *SoundList_Find(SoundList *lists, int key1, int key2)
{
    SoundNode *n = lists[key1].head;

    for (; n->next; n = n->next)
        if (((Sound *)n)->key2 == key2)
            return (Sound *)n;
    return NULL;
}
HOOK(0x496470, SoundList_Find);

/* Copies the sample data into the buffer, or with restore set restores a lost buffer. */
char Sound_Upload(Voice *v, Sound *s, char restore)
{
    IDirectSoundBuffer *buf = s->buffer;
    int size = (unsigned)(s->channels * s->bits * s->samples) >> 3;
    void *p1, *p2;
    DWORD n1, n2;

    if (v)
        buf = v->buffer;
    if (restore)
        return IDirectSoundBuffer_Restore(buf) == 0;
    if (IDirectSoundBuffer_Lock(buf, 0, size, &p1, &n1, &p2, &n2, 0))
        return 0;
    if (size <= (int)n1) {
        memcpy(p1, s->data, n1);
        size -= n1;
    }
    /* the second part is copied from data + n1 even when the first one was not */
    if (size > 0 && size <= (int)n2)
        memcpy(p2, (char *)s->data + n1, n2);
    return IDirectSoundBuffer_Unlock(buf, p1, n1, p2, n2) == 0;
}
HOOK(0x4964A0, Sound_Upload);

void Sound_GetName(char *out, const char *name);
int Sound_HashName(char *key);
int Sound_HashBucket(char *key);
void SoundList_Remove(SoundList *lists, Sound *s);

void Sound_Unload(const char *name)
{
    char key[32];
    int key1, key2;
    Sound *s;

    Sound_GetName(key, name);
    key1 = Sound_HashBucket(key);
    key2 = Sound_HashName(key);
    s = SoundList_Find(gSoundLists, key1, key2);
    if (!s)
        return;
    SoundList_Remove(gSoundLists, s);
    if (s->buffer)
        IDirectSoundBuffer_Release(s->buffer);
    if (s->name) {
        Mem_Free(s->name);
        s->name = NULL;
    }
    Mem_Free(s);
    if (Resource_Find(name))
        Resource_Unload(name);
}
HOOK(0x4965A0, Sound_Unload);

void SoundList_Remove(SoundList *lists, Sound *s)
{
    List_Remove(s);
}
HOOK(0x496640, SoundList_Remove);

void Voice_Unlink(Voice *v, Voice **head);

void Voice_Free(Voice *v)
{
    if (!gSoundReady)
        return;
    IDirectSoundBuffer_Release(v->buffer);
    if (v->flags & 4)
        v->buffer3d->lpVtbl->Release(v->buffer3d);
    if (v->file) {
        File_Close(v->file);
        v->file = NULL;
    }
    Voice_Unlink(v, &gActiveVoices);
    Sound_ListPush(v, &gFreeVoices);
}
HOOK(0x496650, Voice_Free);

void Voice_Unlink(Voice *v, Voice **head)
{
    if (v->next)
        v->next->prev = v->prev;
    if (v->prev)
        v->prev->next = v->next;
    if (*head == v)
        *head = v->next;
}
HOOK(0x4966B0, Voice_Unlink);

/* A lost looping buffer is restored and restarted. */
char Voice_IsPlaying(Voice *v)
{
    DWORD status = 0;
    short format;

    if (!gSoundReady)
        return 0;
    format = v->sound->format;
    if (format == 2 || format == 0)
        return v->state != 2;
    if (IDirectSoundBuffer_GetStatus(v->buffer, &status))
        return 0;
    if (!(status & DSBSTATUS_BUFFERLOST))
        return (status & DSBSTATUS_PLAYING) != 0;
    if (!(status & DSBSTATUS_LOOPING))
        return 0;
    if (!Sound_Upload(v, v->sound, 1))
        return 0;
    if (IDirectSoundBuffer_Play(v->buffer, 0, 0, DSBPLAY_LOOPING))
        return 0;
    return 1;
}
HOOK(0x4966E0, Voice_IsPlaying);

void SoundParams_Init(SoundParams *p);
Voice *Voice_Create(int key1, int key2, int flags, SoundParams *p);
void Voice_SetParams(Voice *v, SoundParams *p);

Voice *Voice_Play(int key1, int key2, int flags, SoundParams *p)
{
    SoundParams def;
    Voice *v;
    Sound *s;

    if (!gSoundReady)
        return NULL;
    if (!p) {
        SoundParams_Init(&def);
        p = &def;
    }
    v = Voice_Create(key1, key2, flags, p);
    if (!v)
        return NULL;
    Voice_SetParams(v, p);
    s = v->sound;
    if (s->format == 2 || s->format == 0) {
        IDirectSoundBuffer_Play(v->buffer, 0, 0, DSBPLAY_LOOPING);
        return v;
    }
    IDirectSoundBuffer_SetCurrentPosition(v->buffer,
                                          (DWORD)Round((long double)(s->channels * s->bits * s->rate / 8) * p->start));
    IDirectSoundBuffer_Play(v->buffer, 0, 0, (flags >> 4) & 1);
    return v;
}
HOOK(0x496770, Voice_Play);

void Voice_Fill(Voice *v, int frames);

/* Takes a free voice for the sound. Streamed sounds get their own 4 second buffer and file. */
Voice *Voice_Create(int key1, int key2, int flags, SoundParams *p)
{
    WAVEFORMATEX wf;
    DSBUFFERDESC desc;
    Sound *s;
    Voice *v;
    int frames;

    if (!gSoundReady)
        return NULL;
    s = Sound_Find(key1, key2);
    if (!s)
        return NULL;
    v = gFreeVoices;
    if (!v)
        return NULL;
    Voice_Unlink(v, &gFreeVoices);
    v->sound = s;
    v->flags = flags;
    switch ((unsigned)s->format) {
    case 0:
    case 2:
        wf.nChannels = s->channels;
        wf.wBitsPerSample = s->bits;
        wf.nSamplesPerSec = s->rate;
        wf.nBlockAlign = s->bits * s->channels / 8;
        wf.nAvgBytesPerSec = wf.nBlockAlign * s->rate;
        memset(&desc, 0, sizeof(desc));
        wf.cbSize = sizeof(WAVEFORMATEX);
        wf.wFormatTag = WAVE_FORMAT_PCM;
        desc.dwSize = sizeof(desc);
        desc.dwFlags = 0x100E8;
        desc.dwBufferBytes = (int)(wf.nBlockAlign * s->rate * 4000) / 1000;
        desc.lpwfxFormat = &wf;
        if (IDirectSound_CreateSoundBuffer(gDSound, &desc, &v->buffer, NULL))
            break;
        Adpcm_Init(&v->adpcm, s->channels);
        v->file = File_Open(s->name, "r");
        if (!v->file) {
            IDirectSoundBuffer_Release(v->buffer);
            break;
        }
        File_Seek(v->file, 0x14, 0);
        File_Flush(v->file);
        v->state = 0;
        v->readPos = 0;
        v->playPos = 0;
        v->writePos = 0;
        v->endTime = 0;
        v->unk30 = 0;
        v->unk38 = 0;
        v->startTime = gFlowFrames;
        frames = s->rate * 4000 / 1000;
        v->halfFrames = frames / 2;
        frames = s->rate * 4000 / 1000;
        Voice_Fill(v, frames * 9 / 10);
        Sound_ListPush(v, &gActiveVoices);
        return v;
    case 1:
    case 3:
        if (IDirectSound_DuplicateSoundBuffer(gDSound, s->buffer, &v->buffer))
            break;
        /* fall through */
    default:
        Sound_ListPush(v, &gActiveVoices);
        return v;
    }
    Sound_ListPush(v, &gFreeVoices);
    return NULL;
}
HOOK(0x496860, Voice_Create);

void Voice_ReadAdpcm(short *dst, Voice *v, int frames, int frameBytes);
void Voice_ReadPcm(void *dst, Voice *v, int frames, int frameBytes);

/* Streams the next frames from the file into the ring buffer; silence once the file has ended. */
void Voice_Fill(Voice *v, int frames)
{
    int ending = v->state == 1;
    int bufFrames = v->sound->rate * 4000 / 1000;
    int frameBytes = v->sound->channels * v->sound->bits / 8;
    char *p;
    DWORD bytes;
    int n, avail, end, d;

    if (v->state == 2)
        return;
    if (IDirectSoundBuffer_Lock(v->buffer, 0, 0, (void **)&p, &bytes, NULL, NULL, DSBLOCK_ENTIREBUFFER)) {
        v->state = -1;
        return;
    }
    while (frames != 0) {
        if (frames < 0) {
            n = 0;
        } else {
            n = bufFrames - v->writePos;
            if (n >= frames)
                n = frames;
        }
        char *dst = p + v->writePos * frameBytes;
        if (ending) {
            memset(dst, 0, (unsigned)(n * frameBytes));
        } else {
            if (n < 0) {
                n = 0;
            } else {
                avail = v->sound->samples - v->readPos;
                if (avail < n)
                    n = avail;
                if (n) {
                    if (v->sound->format == 2)
                        Voice_ReadAdpcm((short *)dst, v, n, frameBytes);
                    else
                        Voice_ReadPcm(dst, v, n, frameBytes);
                }
            }
            v->readPos += n;
        }
        v->writePos += n;
        if (v->writePos >= bufFrames)
            v->writePos = 0;
        if (v->readPos >= v->sound->samples) {
            if (v->flags & 0x10) {
                File_Seek(v->file, 0x14, 0);
                File_Flush(v->file);
                v->readPos = 0;
                Adpcm_Init(&v->adpcm, v->sound->channels);
            } else if (!ending) {
                ending = 1;
                end = gVoiceUpdateRate * 4000 / 1000 + gFlowFrames;
                v->endTime = end;
                d = (int)((unsigned)v->playPos - (unsigned)end);
                v->unk30 = d < 0 ? end - v->playPos : d;
            }
        }
        frames -= n;
    }
    if (IDirectSoundBuffer_Unlock(v->buffer, p, bytes, NULL, 0)) {
        v->state = -1;
        return;
    }
    v->state = ending != 0;
}
HOOK(0x496AB0, Voice_Fill);

/* Reads whole bytes; a frame count ending in the middle of a byte rereads that byte next time. */
void Voice_ReadAdpcm(short *dst, Voice *v, int frames, int frameBytes)
{
    short ch = v->sound->channels;
    int back = 0;
    int bytes = ch * frames / 2;

    if ((v->readPos * ch) & 1) {
        bytes++;
    } else if ((frames & 1) && (ch & 1)) {
        back = 1;
        bytes++;
    }
    if (bytes <= 0)
        return;
    File_Read(v->file, gAdpcmBuffer, bytes);
    File_Flush(v->file);
    if (back) {
        File_Seek(v->file, -1, 1);
        File_Flush(v->file);
    }
    Adpcm_Decode(gAdpcmBuffer, dst, frames, &v->adpcm);
}
HOOK(0x496CE0, Voice_ReadAdpcm);

void Voice_ReadPcm(void *dst, Voice *v, int frames, int frameBytes)
{
    if (frames <= 0)
        return;
    File_Read(v->file, dst, frames * frameBytes);
    File_Flush(v->file);
}
HOOK(0x496D80, Voice_ReadPcm);

/* Length in seconds. */
long double Sound_GetDuration(int key1, int key2)
{
    Sound *s;

    if (!gSoundReady)
        return kZero.f;
    s = Sound_Find(key1, key2);
    if (!s)
        return kZero.f;
    return (long double)(unsigned)s->samples / s->rate;
}
HOOK(0x496DB0, Sound_GetDuration);

void Sound_SetListener(const float *m, const double *pos, const double *vel)
{
    memcpy(gListenerMatrix, m, 10 * 4);
    memcpy(gListenerPos, pos, 3 * 8);
    memcpy(gListenerVel, vel, 3 * 8);
}
HOOK(0x496E00, Sound_SetListener);

void Voice_Stop(Voice *v)
{
    if (gSoundReady)
        IDirectSoundBuffer_Stop(v->buffer);
}
HOOK(0x496E40, Voice_Stop);

char Voice_Update(Voice *v);

static int Voice_VolumeIndex(Voice *v, float vol)
{
    if (v->flags & 0x20)
        return (int)Round((long double)gMasterVolume * gMusicVolume * vol * kVolumeSteps.f);
    return (int)Round((long double)gSfxVolume * vol * kVolumeSteps.f);
}

static void Voice_Apply(Voice *v, SoundParams *l)
{
    int i, sign;

    IDirectSoundBuffer_SetVolume(v->buffer, gVolumeTable[Voice_VolumeIndex(v, l->volume)]);
    if (!(l->pan >= kZero.f)) {
        sign = -1;
        i = (int)Round(((long double)l->pan + kOne.f) * kVolumeSteps.f);
    } else {
        sign = 1;
        i = (int)Round(((long double)kOne.f - l->pan) * kVolumeSteps.f);
    }
    if (!(v->flags & 4))
        IDirectSoundBuffer_SetPan(v->buffer, gVolumeTable[i] * (gSoundSwapPan ? -1 : 1) * sign);
    if (!(l->pitch >= kHalf.f))
        l->pitch = kHalf.f;
    else if (l->pitch > kTwo.f)
        l->pitch = kTwo.f;
    IDirectSoundBuffer_SetFrequency(v->buffer, (DWORD)Round((long double)v->sound->rate * l->pitch));
}

/* Applies volume, pan and pitch. Positional sounds fade out between the min and max distance
   (both scaled by 4096 and written back to p) and pan from the listener's x axis. */
void Voice_SetParams(Voice *v, SoundParams *p)
{
    SoundParams l;
    double d[3];
    long double dist, k;
    volatile double r;

    if (!gSoundReady)
        return;
    if ((v->sound->format == 2 || v->sound->format == 0) && !Voice_Update(v)) {
        p->volume = 0;
        return;
    }
    memcpy(&l, p, 0x20);
    l.minDist = p->minDist * kDistScale;
    l.maxDist = p->maxDist * kDistScale;
    if ((v->flags & 2) && !(v->flags & 4)) {
        d[0] = p->pos.x - gListenerPos[0];
        d[1] = p->pos.y - gListenerPos[1];
        d[2] = p->pos.z - gListenerPos[2];
        dist = Vec3d_Length(d);
        if (dist > l.maxDist) {
            l.volume = 0;
        } else if (dist >= l.minDist) {
            k = (kOneD - (dist - l.minDist) / (l.maxDist - l.minDist)) * l.volume;
            l.volume = k;
            if (!(k >= kZero.f))
                l.volume = 0;
            else if (l.volume > kOne.f)
                l.volume = kOne.f;
        }
        if (dist > kZeroD) {
            r = gListenerMatrix[6] * d[2] + gListenerMatrix[3] * d[1] + gListenerMatrix[0] * d[0];
            k = dist / (l.maxDist + l.minDist);
            l.pan = r / dist * k;
            if (!(l.pan >= kMinusOne.f))
                l.pan = kMinusOne.f;
            else if (l.pan > kOne.f)
                l.pan = kOne.f;
        } else {
            l.pan = 0;
        }
    }
    Voice_Apply(v, &l);
    if (v->flags & 1)
        Voice_Apply(v, &l);
    memcpy(p, &l, 0x20);
}
HOOK(0x496E60, Voice_SetParams);

/* Tops up a streamed voice's ring buffer. Returns 0 once it has stopped. */
char Voice_Update(Voice *v)
{
    int step = v->sound->rate / gVoiceUpdateRate;
    DWORD play;
    int pos, ahead, n;

    IDirectSoundBuffer_GetCurrentPosition(v->buffer, &play, NULL);
    pos = (int)play / (v->sound->channels * v->sound->bits / 8);
    v->playPos = pos;
    if (v->state == 1 && gFlowFrames > v->endTime) {
        v->state = 2;
        return 0;
    }
    if (v->state != 0 && v->state != 1)
        return 0;
    if (pos > v->writePos)
        ahead = v->sound->rate * 4000 / 1000 - pos + v->writePos;
    else
        ahead = v->writePos - pos;
    for (n = (v->halfFrames - ahead) / step; n > 0; n--) {
        Voice_Fill(v, step);
        if (v->state == -1)
            return 0;
    }
    return 1;
}
HOOK(0x4972B0, Voice_Update);

/* "path\name.ext" -> "name". Names without a path give "". */
void Sound_GetName(char *out, const char *name)
{
    const char *dot = name, *p;

    *out = 0;
    if (!*name) {
        Warning_Show("SOUND - Invalid filename %s", name);
        return;
    }
    while (*dot != '.') {
        if (!*++dot) {
            Warning_Show("SOUND - Invalid filename %s", name);
            return;
        }
    }
    for (p = dot; p >= name; p--) {
        if (*p == '/' || *p == '\\' || *p == ':') {
            p++;
            crt_strncpy(out, p, dot - p);
            out[dot - p] = 0;
            return;
        }
    }
}
HOOK(0x497390, Sound_GetName);

/* Both hashes upper-case the key in place. */
int Sound_HashName(char *key)
{
    int h = 0;
    unsigned w = 1;

    for (; *key; key++) {
        if (*key >= 'a' && *key <= 'z')
            *key -= 0x20;
        h += *key * w;
        w = (w + 0x13D) & 0xFFFF;
    }
    return h;
}
HOOK(0x497410, Sound_HashName);

int Sound_HashBucket(char *key)
{
    unsigned h = 0;
    unsigned w = 1;

    for (; *key; key++) {
        if (*key >= 'a' && *key <= 'z')
            *key -= 0x20;
        h = (h + *key * w) % 101;
        w = (w + 0x13E) & 0xFFFF;
    }
    return h;
}
HOOK(0x497450, Sound_HashBucket);

void SoundParams_InitBasic(SoundParams *p);

void SoundParams_Init(SoundParams *p)
{
    SoundParams_InitBasic(p);
    memset(&p->pos, 0, 0x34);
}
HOOK(0x4974A0, SoundParams_Init);

void SoundParams_InitBasic(SoundParams *p)
{
    p->minDist = 0;
    p->volume = 1.0f;
    p->pitch = 1.0f;
    p->pan = 0;
    p->maxDist = 1.0;
}
HOOK(0x4974E0, SoundParams_InitBasic);

int Sound_GetVoiceCount(void)
{
    return gSoundVoiceCount;
}
HOOK(0x497510, Sound_GetVoiceCount);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(543904) float gMusicVolume INIT(1.0f);
DATA(543904) float gMasterVolume INIT(1.0f);
DATA(543904) float gSfxVolume INIT(1.0f);
PAD(igd, 543904, 543910, 0x4) INIT({0x00, 0x00, 0x80, 0x3F});
DATA(543904) int gSoundSwapPan INIT(1);
DATA(54391C) int gVoiceUpdateRate INIT(30);
STORE(igd, 543920, 1) const signed char store_543920[16] INIT({-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8});
ALIAS(gAdpcmIndexTable, store_543920, 0);
STORE(igd, 543920, 2) const short store_543930[152] INIT({7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767, 0, 25699, 30049, 26980, 111, 17475, 16672, 25717, 28521, 0, 0, 24919, 25974, 0, 0, 20307, 20053, 8260, 8237, 26950, 25964, 9504, 8307, 27745, 25970, 25697, 8313, 28524, 25697, 25701, 33, 20307, 20053, 8260, 8237, 26950, 25964, 9504, 8307, 28526, 8308, 24950, 26988, 8292, 28518, 28018, 29793, 33, 0, 20307, 20053, 8260, 8237, 28233, 24950, 26988, 8292, 26982, 25964, 24942, 25965, 9504, 115});
ALIAS(gAdpcmStepTable, store_543930, 0);
STORE(igb, 5CA138, 8) double store_5CA138[3];
ALIAS(gListenerPos, store_5CA138, 0);
STORE(igb, 5CA160, 4) float store_5CA160[10];
ALIAS(gListenerMatrix, store_5CA160, 0);
STORE(igb, 5CA1B8, 4) __typeof__(gSoundLists[0]) store_5CA1B8[101];
ALIAS(gSoundLists, store_5CA1B8, 0);
STORE(igb, 5CA1B8, 1) unsigned char store_5CA674[705600];
ALIAS(gAdpcmBuffer, store_5CA674, 0);
STORE(igb, 676AB8, 8) double store_676AB8[115];
ALIAS(gListenerVel, store_676AB8, 0);
STORE(igb, 67A4A0, 4) int store_67A4A0[10002];
ALIAS(gVolumeTable, store_67A4A0, 0);
BSS(6840F4) Voice *gActiveVoices;
BSS(6840F4) Voice *gFreeVoices;
BSS(C28C5C) int gFlowFrames;
