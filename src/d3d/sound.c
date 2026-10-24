#include <string.h>

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include "hook.h"
#include "igi.h"

ORIG(0x48F0A0, HWND, AppContext_GethWnd, (void));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B16D0, int, File_Read, (void *f, void *buf, int size));
ORIG(0x4B1730, int, File_Seek, (void *f, int pos, int whence));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x496250, void, SoundList_Add, (void *lists, int key1, int key2, void *sound));
ORIG(0x496280, void, Adpcm_Decode, (void *src, void *dst, int samples, void *state));
ORIG(0x496420, void, Adpcm_Init, (void *state, int channels));
ORIG(0x496450, int, Sound_Find, (int key1, int key2));
ORIG(0x4964A0, char, Sound_Upload, (int unk, void *sound, int unk2));
ORIG(0x497390, void, Sound_GetName, (void *key, const char *name));
ORIG(0x497410, int, Sound_HashName, (void *key));
ORIG(0x497450, int, Sound_HashBucket, (void *key));
/* the exe's import thunk and table entries; DSOUND and WINMM are not linked into our DLL */
ORIG_STD(0x4AF746, HRESULT, DirectSoundCreate_, (GUID *guid, IDirectSound **out, IUnknown *outer));
extern MMRESULT (WINAPI *mixerGetLineControlsA_)(HMIXEROBJ, MIXERLINECONTROLSA *, DWORD);
extern MMRESULT (WINAPI *mixerSetControlDetails_)(HMIXEROBJ, MIXERCONTROLDETAILS *, DWORD);
extern MMRESULT (WINAPI *mixerClose_)(HMIXER);
extern UINT (WINAPI *mixerGetNumDevs_)(void);
extern MMRESULT (WINAPI *mixerOpen_)(HMIXER *, UINT, DWORD_PTR, DWORD_PTR, DWORD);
extern MMRESULT (WINAPI *mixerGetDevCapsA_)(UINT_PTR, MIXERCAPSA *, UINT);
extern MMRESULT (WINAPI *mixerGetLineInfoA_)(HMIXEROBJ, MIXERLINEA *, DWORD);
extern MMRESULT (WINAPI *mixerGetControlDetailsA_)(HMIXEROBJ, MIXERCONTROLDETAILS *, DWORD);
extern MCIERROR (WINAPI *mciSendCommandA_)(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR);

typedef struct SoundNode {
    struct SoundNode *next;
    struct SoundNode *prev;
} SoundNode;

typedef struct SoundList {
    SoundNode *head;
    SoundNode *tail;
    SoundNode *tailpred;
} SoundList;

/* Volume controls of one mixer, saved at startup and put back at shutdown. */
typedef struct Mixer {
    HMIXER hmx;
    MIXERCAPSA caps;
    MIXERLINEA dest;
    MIXERLINEA waveLine;
    MIXERLINEA cdLine;
    MIXERCONTROLA waveCtrl;
    MIXERCONTROLA cdCtrl;
    DWORD waveLeft;
    DWORD waveRight;
    DWORD cdLeft;
    DWORD cdRight;
} Mixer;

_Static_assert(sizeof(Mixer) == 0x364, "Mixer");
_Static_assert(sizeof(DSBUFFERDESC) == 0x24 && sizeof(DSCAPS) == 0x60, "DirectSound 7 structs");

/* A loaded sample, 0x30 bytes. The header is copied from the 'ILSF' resource. */
typedef struct Sound {
    int unk0;
    int unk4;
    int unk8;
    int unkC;
    int magic;
    short format;
    short bits;
    short channels;
    short unk1A;
    int rate;
    int samples;
    IDirectSoundBuffer *buffer;
    char *name;
    void *data;
} Sound;

extern MCIDEVICEID gCDDeviceID;
extern int gSoundVoiceCount;
extern int gMixerCount;
extern IDirectSoundBuffer *gDSPrimary;
extern char gCDUnk5CA188;
extern IDirectSoundBuffer *gStreamBuffer;
extern void *gStreamFile;
extern DWORD gStreamHalf;
extern DWORD gStreamEnd;
extern DWORD gStreamPos;
extern DWORD gStreamHalfSize;
extern int gStreamLoop;
extern char gStreamReady;
extern SoundList gSoundLists[];
extern char gSoundStereo;
extern float gMusicVolume;
extern float gMasterVolume;
extern float gSfxVolume;
extern int gCDTrack;
extern int gUnk54391C;
extern char gCDUnk676AB4;
extern int gSoundUnk676E50;
extern Mixer gMixers[];
extern IDirectSound *gDSound;
extern IUnknown *gDSListener;
extern int gVolumeTable[];
extern char gCDAudioOpen;
extern char gSoundReady;
extern char gSound3D;
extern char *gSoundVoices;
extern int gSoundUnk6840F4;
extern SoundNode *gSoundFreeVoices;

void CD_Open(void);
void Mixer_Init(void);
void Sound_InitDirectSound(void);
void Stream_Init(void);

void Sound_Init(void)
{
    CD_Open();
    Mixer_Init();
    Sound_InitDirectSound();
    Stream_Init();
}
HOOK(0x495280, Sound_Init);

void CD_Open(void)
{
    MCI_OPEN_PARMSA p;

    p.lpstrDeviceType = "cdaudio";
    if (!mciSendCommandA_(0, MCI_OPEN, MCI_OPEN_TYPE, (DWORD_PTR)&p)) {
        gCDAudioOpen = 1;
        gCDDeviceID = p.wDeviceID;
        gCDUnk5CA188 = 0;
        gCDUnk676AB4 = 0;
    }
}
HOOK(0x4952A0, CD_Open);

static long double Ln(long double x)
{
    long double r;

    __asm__("fldln2\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
    return r;
}

static long long Round(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return r;
}

void Sound_InitLists(SoundList *lists);
void Sound_ListPush(SoundNode *node, SoundNode **head);

void Sound_InitDirectSound(void)
{
    DSCAPS caps;
    DSBUFFERDESC desc;
    DSBCAPS bcaps;
    WAVEFORMATEX wf;
    int i, v;

    gSoundReady = 0;
    Sound_InitLists(gSoundLists);
    /* linear volume in 1/10000 steps to DirectSound hundredths of a decibel */
    gVolumeTable[0] = -10000;
    for (i = 10000; i > 0; i--) {
        v = (int)Round(Ln((long double)i * 0.0001) * 1442.6950408888495);
        if (v < -10000)
            v = -10000;
        else if (v > 0)
            v = 0;
        gVolumeTable[i] = v;
    }
    if (DirectSoundCreate_(NULL, &gDSound, NULL))
        goto fail;
    if (IDirectSound_SetCooperativeLevel(gDSound, AppContext_GethWnd(), DSSCL_EXCLUSIVE))
        goto release;
    caps.dwSize = sizeof(caps);
    if (IDirectSound_GetCaps(gDSound, &caps)) {
        IDirectSound_Release(gDSound);
        gDSound = NULL;
        return;
    }
    gSound3D = 0;
    gSoundStereo = (caps.dwFlags & DSCAPS_PRIMARYSTEREO) != 0;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_LOCSOFTWARE;
    if (IDirectSound_CreateSoundBuffer(gDSound, &desc, &gDSPrimary, NULL)) {
        IDirectSound_Release(gDSound);
        gDSound = NULL;
        return;
    }
    memset(&bcaps, 0, sizeof(bcaps));
    bcaps.dwSize = sizeof(bcaps);
    if (IDirectSoundBuffer_GetCaps(gDSPrimary, &bcaps))
        goto release_primary;
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = gSoundStereo ? 2 : 1;
    wf.nSamplesPerSec = 22050;
    gSoundUnk676E50 = 1;
    gSoundVoiceCount = 32;
    wf.wBitsPerSample = (caps.dwFlags & DSCAPS_PRIMARY16BIT) ? 16 : 8;
    wf.nBlockAlign = (wf.wBitsPerSample >> 3) * wf.nChannels;
    wf.cbSize = sizeof(WAVEFORMATEX);
    wf.nAvgBytesPerSec = wf.nBlockAlign * 22050;
    if (IDirectSoundBuffer_SetFormat(gDSPrimary, &wf))
        goto release_primary;
    if (IDirectSoundBuffer_Play(gDSPrimary, 0, 0, DSBPLAY_LOOPING))
        goto release_primary;
    gSoundVoices = Mem_Alloc(gSoundVoiceCount * 0x4C, 4);
    memset(gSoundVoices, 0, gSoundVoiceCount * 0x4C);
    for (i = 0; i < gSoundVoiceCount; i++)
        Sound_ListPush((SoundNode *)(gSoundVoices + i * 0x4C), &gSoundFreeVoices);
    gSoundUnk6840F4 = 0;
    gSoundReady = 1;
    return;

release_primary:
    IDirectSoundBuffer_Release(gDSPrimary);
release:
    IDirectSound_Release(gDSound);
fail:
    gDSound = NULL;
}
HOOK(0x4952F0, Sound_InitDirectSound);

void Sound_InitLists(SoundList *lists)
{
    int i;

    for (i = 0; i < 101; i++) {
        lists[i].head = (SoundNode *)&lists[i].tail;
        lists[i].tailpred = (SoundNode *)&lists[i].head;
        lists[i].tail = NULL;
    }
}
HOOK(0x4955F0, Sound_InitLists);

void Sound_ListPush(SoundNode *node, SoundNode **head)
{
    node->prev = NULL;
    node->next = *head;
    if (*head)
        (*head)->prev = node;
    *head = node;
}
HOOK(0x495620, Sound_ListPush);

/* Mixers are stored at the running count of saved controls, not at their device number. */
void Mixer_Init(void)
{
    int count = 0, dev, numDevs = mixerGetNumDevs_();
    DWORD dst, src, i;

    for (dev = 0; dev < numDevs; dev++) {
        Mixer *m = &gMixers[count];

        memset(m, 0, sizeof(*m));
        if (mixerOpen_(&m->hmx, dev, 0, 0, 0) || mixerGetDevCapsA_((UINT_PTR)m->hmx, &m->caps, sizeof(m->caps)))
            continue;
        for (dst = 0; dst < m->caps.cDestinations; dst++) {
            MIXERLINEA line;

            memset(&line, 0, sizeof(line));
            line.cbStruct = sizeof(line);
            line.dwDestination = dst;
            line.dwSource = 0;
            if (mixerGetLineInfoA_((HMIXEROBJ)m->hmx, &line, MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_DESTINATION))
                continue;
            if (line.Target.dwType != MIXERLINE_TARGETTYPE_WAVEOUT && line.Target.dwType != MIXERLINE_TARGETTYPE_AUX)
                continue;
            m->dest = line;
            for (src = 0; src < line.cConnections; src++) {
                MIXERLINEA sl;
                MIXERLINEA *lineDst = NULL;
                MIXERCONTROLA *ctrlDst = NULL;
                DWORD *leftDst = NULL, *rightDst = NULL;
                MIXERLINECONTROLSA mlc;

                memset(&sl, 0, sizeof(sl));
                sl.cbStruct = sizeof(sl);
                sl.dwDestination = dst;
                sl.dwSource = src;
                if (mixerGetLineInfoA_((HMIXEROBJ)m->hmx, &sl, MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_SOURCE))
                    continue;
                if (sl.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT || !strcmp(sl.szName, "Wave")) {
                    lineDst = &m->waveLine;
                    leftDst = &m->waveLeft;
                    rightDst = &m->waveRight;
                    ctrlDst = &m->waveCtrl;
                }
                if (sl.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY ||
                    sl.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC || !strcmp(sl.szName, "CD Audio")) {
                    if (gCDAudioOpen) {
                        lineDst = &m->cdLine;
                        leftDst = &m->cdLeft;
                        rightDst = &m->cdRight;
                        ctrlDst = &m->cdCtrl;
                    }
                }
                if (!ctrlDst)
                    continue;
                mlc.cbStruct = sizeof(mlc);
                mlc.dwLineID = sl.dwLineID;
                mlc.cControls = sl.cControls;
                mlc.cbmxctrl = sizeof(MIXERCONTROLA);
                mlc.pamxctrl = Mem_Alloc(sl.cControls * sizeof(MIXERCONTROLA), 4);
                if (mixerGetLineControlsA_((HMIXEROBJ)m->hmx, &mlc, MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ALL))
                    continue;
                for (i = 0; i < mlc.cControls; i++) {
                    MIXERCONTROLDETAILS d;
                    DWORD *vals;

                    if (mlc.pamxctrl[i].dwControlType != MIXERCONTROL_CONTROLTYPE_VOLUME)
                        continue;
                    *lineDst = sl;
                    *ctrlDst = mlc.pamxctrl[i];
                    d.cbStruct = sizeof(d);
                    d.dwControlID = ctrlDst->dwControlID;
                    d.cChannels = lineDst->cChannels;
                    d.cMultipleItems = ctrlDst->cMultipleItems;
                    d.cbDetails = sizeof(DWORD);
                    vals = d.paDetails = Mem_Alloc(d.cChannels * sizeof(DWORD), 4);
                    if (!mixerGetControlDetailsA_((HMIXEROBJ)m->hmx, &d,
                                                  MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE)) {
                        if (lineDst->cChannels > 0)
                            *leftDst = vals[0];
                        if (lineDst->cChannels > 1)
                            *rightDst = vals[1];
                    }
                    Mem_Free(vals);
                    count++;
                }
                Mem_Free(mlc.pamxctrl);
            }
            if (m->waveCtrl.cbStruct && m->cdCtrl.cbStruct)
                count++;
        }
    }
    gMixerCount = count;
}
HOOK(0x495640, Mixer_Init);

void Stream_Init(void)
{
    gStreamReady = 0;
    if (gSoundReady) {
        gStreamFile = NULL;
        gStreamBuffer = NULL;
        gStreamReady = 1;
    }
}
HOOK(0x495A60, Stream_Init);

void Stream_Close(void);
void Sound_ShutdownDirectSound(void);
void Mixer_Shutdown(void);
void CD_Close(void);
void CD_Stop(void);

void Sound_Shutdown(void)
{
    Stream_Close();
    Sound_ShutdownDirectSound();
    Mixer_Shutdown();
    CD_Close();
}
HOOK(0x495A90, Sound_Shutdown);

void CD_Close(void)
{
    if (gCDAudioOpen) {
        CD_Stop();
        mciSendCommandA_(gCDDeviceID, MCI_CLOSE, 0, 0);
    }
}
HOOK(0x495AB0, CD_Close);

void Sound_ShutdownDirectSound(void)
{
    if (gSoundVoices) {
        Mem_Free(gSoundVoices);
        gSoundVoices = NULL;
        gSoundFreeVoices = NULL;
        gSoundUnk6840F4 = 0;
    }
    if (gDSound) {
        if (gSound3D)
            gDSListener->lpVtbl->Release(gDSListener);
        IDirectSoundBuffer_Stop(gDSPrimary);
        IDirectSoundBuffer_Release(gDSPrimary);
        IDirectSound_Release(gDSound);
    }
}
HOOK(0x495AE0, Sound_ShutdownDirectSound);

MMRESULT Mixer_SetVolume(HMIXEROBJ hmx, MIXERLINEA *line, MIXERCONTROLA *ctrl, DWORD left, DWORD right);

void Mixer_Shutdown(void)
{
    int i;

    for (i = 0; i < gMixerCount; i++) {
        Mixer *m = &gMixers[i];

        if (!m->hmx)
            continue;
        if (m->dest.cbStruct) {
            if (m->waveLine.cbStruct && m->waveCtrl.cbStruct)
                Mixer_SetVolume((HMIXEROBJ)m->hmx, &m->waveLine, &m->waveCtrl, m->waveLeft, m->waveRight);
            if (m->cdLine.cbStruct)
                Mixer_SetVolume((HMIXEROBJ)m->hmx, &m->cdLine, &m->cdCtrl, m->cdLeft, m->cdRight);
        }
        mixerClose_(m->hmx);
    }
}
HOOK(0x495B50, Mixer_Shutdown);

MMRESULT Mixer_SetVolume(HMIXEROBJ hmx, MIXERLINEA *line, MIXERCONTROLA *ctrl, DWORD left, DWORD right)
{
    MIXERCONTROLDETAILS d;
    DWORD vals[2];

    d.cbStruct = sizeof(d);
    d.cbDetails = sizeof(DWORD);
    d.dwControlID = ctrl->dwControlID;
    d.cChannels = line->cChannels;
    d.cMultipleItems = ctrl->cMultipleItems;
    d.paDetails = vals;
    if (d.cChannels > 0)
        vals[0] = left;
    if (d.cChannels > 1)
        vals[1] = right;
    return mixerSetControlDetails_(hmx, &d, MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE);
}
HOOK(0x495C10, Mixer_SetVolume);

/* Like Stream_Stop but without the ready check, and the pointers are kept. */
void Stream_Close(void)
{
    if (gStreamFile) {
        File_Close(gStreamFile);
        IDirectSoundBuffer_Stop(gStreamBuffer);
        IDirectSoundBuffer_Release(gStreamBuffer);
    }
}
HOOK(0x495C80, Stream_Close);

char Sound_IsReady(void)
{
    return gSoundReady;
}
HOOK(0x495CB0, Sound_IsReady);

void Sound_SetUpdateRate(int v)
{
    gUnk54391C = v;
}
HOOK(0x495CC0, Sound_SetUpdateRate);

/* Fills part of the stream buffer from the file, wrapping to the data start (offset 0x2C) at the end.
   Returns 1 when it wrapped. */
int Stream_Fill(IDirectSoundBuffer *buf, DWORD offset, DWORD size)
{
    void *p;
    DWORD n;
    DWORD left;
    int wrapped;

    if (IDirectSoundBuffer_Lock(buf, offset, size, &p, &n, NULL, NULL, 0))
        return 0;
    if (gStreamEnd >= gStreamPos + size) {
        File_Read(gStreamFile, p, size);
        gStreamPos += size;
        wrapped = 0;
    } else {
        left = gStreamEnd - gStreamPos;
        File_Read(gStreamFile, p, left);
        File_Seek(gStreamFile, 0x2C, 0);
        File_Read(gStreamFile, (char *)p + left, size - left);
        gStreamPos = size - left;
        wrapped = 1;
    }
    IDirectSoundBuffer_Unlock(buf, p, n, NULL, 0);
    return wrapped;
}
HOOK(0x495CD0, Stream_Fill);

void Stream_Stop(void)
{
    if (gStreamReady && gStreamFile) {
        File_Close(gStreamFile);
        gStreamFile = NULL;
        IDirectSoundBuffer_Stop(gStreamBuffer);
        IDirectSoundBuffer_Release(gStreamBuffer);
        gStreamBuffer = NULL;
    }
}
HOOK(0x495DA0, Stream_Stop);

/* Double buffering: refills the half that is not playing. */
void Stream_Update(void)
{
    DWORD play;

    if (!gStreamReady || !gStreamFile)
        return;
    IDirectSoundBuffer_GetCurrentPosition(gStreamBuffer, &play, NULL);
    if (play < gStreamHalfSize) {
        if (!gStreamHalf)
            return;
    } else if (gStreamHalf) {
        return;
    }
    if (Stream_Fill(gStreamBuffer, gStreamHalf, gStreamHalfSize) && !gStreamLoop)
        Stream_Stop();
    gStreamHalf ^= gStreamHalfSize;
}
HOOK(0x495DF0, Stream_Update);

/* Scales the CD audio mixer volume saved at startup. */
void Music_SetVolume(float music, float master)
{
    int i;

    gMusicVolume = music;
    gMasterVolume = master;
    for (i = 0; i < gMixerCount; i++) {
        Mixer *m = &gMixers[i];
        long double scale;
        DWORD left, right;

        if (!m->hmx || !m->dest.cbStruct || !m->cdLine.cbStruct)
            continue;
        scale = (long double)music * master;
        right = (DWORD)Round(m->cdRight * scale);
        left = (DWORD)Round(m->cdLeft * scale);
        Mixer_SetVolume((HMIXEROBJ)m->hmx, &m->cdLine, &m->cdCtrl, left, right);
    }
}
HOOK(0x495E70, Music_SetVolume);

void Music_SetSfxVolume(float v)
{
    gSfxVolume = v;
}
HOOK(0x495F30, Music_SetSfxVolume);

void CD_Stop(void)
{
    MCI_GENERIC_PARMS p;

    p.dwCallback = 0;
    mciSendCommandA_(gCDDeviceID, MCI_STOP, 0, (DWORD_PTR)&p);
    gCDTrack = -1;
}
HOOK(0x495F40, CD_Stop);

void Sound_Load(const char *name)
{
    char key[32];
    char state[12];
    int size, key1, key2;
    int *res;
    Sound *s;
    WAVEFORMATEX wf;
    DSBUFFERDESC desc;

    if (!gSoundReady)
        return;
    res = Resource_Load(name, &size);
    if (res[0] != 0x46534C49) {
        Warning_Show("SOUND - File %s not valid format!", name);
        Resource_Unload(name);
        return;
    }
    Sound_GetName(key, name);
    key1 = Sound_HashBucket(key);
    key2 = Sound_HashName(key);
    if (Sound_Find(key1, key2)) {
        Warning_Show("SOUND - File %s already loaded!", name);
        Resource_Unload(name);
        return;
    }
    s = Mem_Alloc(sizeof(Sound), 4);
    memset(s, 0, sizeof(*s));
    memcpy(&s->magic, res, 0x14);
    switch (s->format) {
    case 0:
    case 2:
        s->name = Mem_Alloc(strlen(name) + 1, 4);
        if (!s->name) {
            Mem_Free(s);
            s = NULL;
            Resource_Unload(name);
        }
        strcpy(s->name, name);
        break;
    case 1:
        s->data = res + 5;
        break;
    case 3:
        s->data = Mem_Alloc((unsigned)(s->channels * s->bits * s->samples) >> 3, 4);
        if (!s->data) {
            Mem_Free(s);
            s = NULL;
            Resource_Unload(name);
        }
        Adpcm_Init(state, s->channels);
        Adpcm_Decode(res + 5, s->data, s->samples, state);
        break;
    }
    if (s->data) {
        wf.cbSize = sizeof(WAVEFORMATEX);
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = s->channels;
        wf.nSamplesPerSec = s->rate;
        wf.wBitsPerSample = s->bits;
        wf.nBlockAlign = s->channels * s->bits / 8;
        wf.nAvgBytesPerSec = wf.nBlockAlign * s->rate;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_CTRLFREQUENCY |
                       DSBCAPS_LOCSOFTWARE;
        desc.lpwfxFormat = &wf;
        desc.dwBufferBytes = s->samples * wf.nBlockAlign;
        if (IDirectSound_CreateSoundBuffer(gDSound, &desc, &s->buffer, NULL)) {
            Mem_Free(s);
            Resource_Unload(name);
            return;
        }
        if (!Sound_Upload(0, s, 0)) {
            IDirectSoundBuffer_Release(s->buffer);
            Mem_Free(s);
            Resource_Unload(name);
            return;
        }
        if (s->format == 3)
            Mem_Free(s->data);
        s->data = NULL;
    }
    SoundList_Add(gSoundLists, key1, key2, s);
    Resource_Unload(name);
}
HOOK(0x495F70, Sound_Load);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5331F0) MMRESULT (WINAPI *mixerGetLineControlsA_)(HMIXEROBJ, MIXERLINECONTROLSA *, DWORD) INIT(mixerGetLineControlsA);
DATA(5331F0) MMRESULT (WINAPI *mixerSetControlDetails_)(HMIXEROBJ, MIXERCONTROLDETAILS *, DWORD) INIT(mixerSetControlDetails);
DATA(5331F0) MMRESULT (WINAPI *mixerClose_)(HMIXER) INIT(mixerClose);
DATA(5331F0) UINT (WINAPI *mixerGetNumDevs_)(void) INIT(mixerGetNumDevs);
DATA(5331F0) MMRESULT (WINAPI *mixerOpen_)(HMIXER *, UINT, DWORD_PTR, DWORD_PTR, DWORD) INIT(mixerOpen);
DATA(5331F0) MMRESULT (WINAPI *mixerGetDevCapsA_)(UINT_PTR, MIXERCAPSA *, UINT) INIT(mixerGetDevCapsA);
DATA(5331F0) MMRESULT (WINAPI *mixerGetLineInfoA_)(HMIXEROBJ, MIXERLINEA *, DWORD) INIT(mixerGetLineInfoA);
DATA(5331F0) MMRESULT (WINAPI *mixerGetControlDetailsA_)(HMIXEROBJ, MIXERCONTROLDETAILS *, DWORD) INIT(mixerGetControlDetailsA);
DATA(5331F0) MCIERROR (WINAPI *mciSendCommandA_)(MCIDEVICEID, UINT, DWORD_PTR, DWORD_PTR) INIT(mciSendCommandA);
DATA(543900) char gSoundStereo INIT(1);
PAD(igd, 543900, 543901, 0x3);
DATA(543918) int gCDTrack INIT(-1);
BSS(5CA130) MCIDEVICEID gCDDeviceID;
BSS(5CA130) int gSoundVoiceCount;
BSS(5CA150) int gMixerCount;
BSS(5CA150) IDirectSoundBuffer *gDSPrimary;
BSS(5CA188) char gCDUnk5CA188;
PAD(igb, 5CA188, 5CA189, 0x7);
BSS(5CA188) IDirectSoundBuffer *gStreamBuffer;
BSS(5CA188) void *gStreamFile;
BSS(5CA188) DWORD gStreamHalf;
BSS(5CA188) DWORD gStreamEnd;
BSS(5CA188) DWORD gStreamPos;
BSS(5CA188) DWORD gStreamHalfSize;
PAD(igb, 5CA188, 5CA1A8, 0x8);
BSS(5CA188) int gStreamLoop;
BSS(5CA188) char gStreamReady;
PAD(igb, 5CA188, 5CA1B5, 0x3);
BSS(676AB4) char gCDUnk676AB4;
PAD(igb, 676AB4, 676AB5, 0x3);
BSS(676E50) int gSoundUnk676E50;
PAD(igb, 676E50, 676E54, 0x4);
STORE(igb, 676E50, 4) __typeof__(gMixers[0]) store_676E58[16];
ALIAS(gMixers, store_676E58, 0);
BSS(676E50) IDirectSound *gDSound;
BSS(676E50) IUnknown *gDSListener;
BSS(6840E8) char gCDAudioOpen;
BSS(6840E8) char gSoundReady;
BSS(6840E8) char gSound3D;
PAD(igb, 6840E8, 6840EB, 0x5);
BSS(6840E8) char *gSoundVoices;
BSS(6840E8) int gSoundUnk6840F4;
BSS(6840E8) SoundNode *gSoundFreeVoices;
ALIAS(gActiveVoices, gSoundUnk6840F4, 0);
ALIAS(gFreeVoices, gSoundFreeVoices, 0);
