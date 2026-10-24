#include <stdlib.h>
#include <string.h>

#include <ddraw.h>
#include <vfw.h>

#include "hook.h"
#include "win/win.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B1020, int, File_ResolvePath, (char *out, const char *path, int index));
ORIG(0x4B1E70, const char *, Path_SkipDevice, (const char *path));
ORIG(0x4B1F30, char *, Path_StripExtension, (char *out, int size, const char *path));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int *size));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4B64E0, int, Resource_Find, (const char *path));
ORIG(0x492BE0, const char *, DDraw_GetErrorString, (HRESULT hr));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x495CB0, char, Sound_IsReady, (void));
ORIG(0x495CC0, void, Sound_SetUpdateRate, (int rate));
ORIG(0x496650, void, Voice_Free, (void *sound));
ORIG(0x4966E0, char, Voice_IsPlaying, (void *sound));
ORIG(0x496770, void *, Voice_Play, (int a, int b, int c, int d));
ORIG(0x496E40, void, Voice_Stop, (void *sound));
ORIG(0x496E60, void, Voice_SetParams, (void *sound, void *buf));
ORIG(0x497390, void, Sound_GetName, (char *out, const char *name));
ORIG(0x497410, int, Sound_HashName, (const char *name));
ORIG(0x497450, int, Sound_HashBucket, (const char *name));
ORIG(0x4974A0, void, SoundParams_Init, (void *buf));
ORIG(0x497570, void, Scene_Init, (void *scene, int on));
ORIG(0x4975F0, void, Scene_Free, (void *scene));
ORIG(0x497620, void, Scene_Select, (void *scene, int on));
ORIG(0x497680, void, Scene_SetRect, (void *scene, float *rect));
ORIG(0x497700, void, Scene_DrawAll, (void));
ORIG(0x4977F0, void, Frame_Reset, (void));
ORIG(0x4C1780, void, Debug_Log, (void));

typedef struct Sprite Sprite;
typedef struct Text {
    void *grid;
    float x, y, z;
    float alpha;
    float color[3];
    int unk20;
    unsigned flags;
} Text;

ORIG(0x4B6EC0, Text *, Text_New, (Sprite *s, float x, float y, const char *str));
ORIG(0x4B6F30, void, Text_Free, (Text *t));
ORIG(0x4B6F50, void, Text_DrawAlt, (Text *t));
ORIG(0x4B7000, Sprite *, Sprite_New, (const char *name));
ORIG(0x4B7030, void, Sprite_Delete, (Sprite *s));
ORIG(0x4B7050, int, Sprite_StringWidth, (Sprite *s, const char *text));
ORIG(0x4B7E00, int, Text_GetHeight, (Text *t));

/* the exe's import thunks for AVIFIL32 and MSVFW32 */
ORIG_STD(0x4AF758, void, AVIFileInit_, (void));
ORIG_STD(0x4AF75E, void, AVIFileExit_, (void));
ORIG_STD(0x4AF764, ULONG, AVIFileRelease_, (PAVIFILE f));
ORIG_STD(0x4AF76A, ULONG, AVIStreamRelease_, (PAVISTREAM s));
ORIG_STD(0x4AF770, LRESULT, ICClose_, (HIC hic));
ORIG_STD(0x4AF776, LRESULT, ICSendMessage_, (HIC hic, UINT msg, DWORD_PTR a, DWORD_PTR b));
ORIG_STD(0x4AF77C, HRESULT, AVIStreamReadFormat_, (PAVISTREAM s, LONG pos, void *fmt, LONG *size));
ORIG_STD(0x4AF782, HRESULT, AVIStreamInfoA_, (PAVISTREAM s, AVISTREAMINFOA *info, LONG size));
ORIG_STD(0x4AF788, HRESULT, AVIFileGetStream_, (PAVIFILE f, PAVISTREAM *s, DWORD type, LONG n));
ORIG_STD(0x4AF78E, HRESULT, AVIFileInfoA_, (PAVIFILE f, AVIFILEINFOA *info, LONG size));
ORIG_STD(0x4AF794, HRESULT, AVIFileOpenA_, (PAVIFILE *f, const char *name, UINT mode, CLSID *handler));
ORIG_STD(0x4AF79A, HIC, ICLocate_, (DWORD type, DWORD handler, BITMAPINFOHEADER *in, BITMAPINFOHEADER *out,
                                    WORD flags));
ORIG_STD(0x4AF7A6, HRESULT, AVIStreamRead_, (PAVISTREAM s, LONG start, LONG count, void *buf, LONG size,
                                             LONG *bytes, LONG *samples));
ORIG(0x4AF7A0, DWORD, ICDecompress_, (HIC hic, DWORD flags, BITMAPINFOHEADER *infmt, void *in,
                                      BITMAPINFOHEADER *outfmt, void *out));

#define gBackBuffer VAR(IDirectDrawSurface7 *, 0x5CA100)
#define gDDraw VAR(IDirectDraw7 *, 0x5CA10C)

#define MOVIE_SUBTITLE_MAX 32
#define MOVIE_LINE_MAX 8

typedef struct MovieSubtitle {
    int font;
    int x;
    int y;
    int fadeInOn;   /* -1 turns the fade off */
    int fadeOutOn;
    int start;      /* all times in ms */
    int duration;
    int fadeIn;
    int fadeOut;
    int lineCount;
    float color[3];
    char *text;
    char *lines[MOVIE_LINE_MAX];
    Text *texts[MOVIE_LINE_MAX];
    char centered;
} MovieSubtitle;

/* What the MovieSubtitle_RegisterText script function passes in. */
typedef struct MovieSubtitleDef {
    const char *text;
    int font;
    int x;
    int y;
    int fadeInOn;
    int fadeOutOn;
    float color[3];
    float start;    /* seconds */
    float duration;
    float fadeIn;
    float fadeOut;
    char centered;
} MovieSubtitleDef;

/* Output format of the decompressor: a header plus the 3 colour masks. */
typedef struct MovieFormat {
    BITMAPINFOHEADER h;
    DWORD masks[3];
} MovieFormat;

typedef struct Movie {
    unsigned time;
    int startTime;
    int pausedTime;
    unsigned duration;
    int unk10;
    int soundFrames;
    int unk18;
    int frame;
    int drawnFrame;
    int unk24;
    char playing;
    char soundPlaying;
    char winProcAdded;
    PAVIFILE file;
    AVIFILEINFOA fileInfo;
    PAVISTREAM stream;
    AVISTREAMINFOA streamInfo;
    BITMAPINFOHEADER format;
    MovieFormat *outFormat;
    void *sound;
    HIC hic;
    void *inBuf;
    void *outBuf;
    char hasSubtitles;
    int scene[0x13];
    Sprite *fonts[4];
    MovieSubtitle subs[MOVIE_SUBTITLE_MAX];
    int subCount;
    int curSub;
} Movie;

_Static_assert(sizeof(MovieSubtitle) == 0x7C, "MovieSubtitle");
_Static_assert(offsetof(Movie, scene) == 0x16C, "Movie.scene");
_Static_assert(sizeof(Movie) == 0x1150, "Movie");

#define gMovie VAR(Movie, 0x5C8E48)
#define gMoviePauseStart VAR(int, 0x5C8E40)

static const char kDXError[] = "%s\n\nDirectX error message: %s";

/* fistp in the current rounding mode */
static int Float_ToInt(float f)
{
    long long v;

    __asm__ volatile("fistpll %0" : "=m"(v) : "t"(f) : "st");
    return (int)v;
}

void Movie_Init(void)
{
    AVIFileInit_();
    memset(&gMovie, 0, sizeof(Movie));
}
HOOK(0x490380, Movie_Init);

void Movie_Shutdown(void)
{
    AVIFileExit_();
}
HOOK(0x4903A0, Movie_Shutdown);

int Movie_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

void Movie_Close(Movie *m)
{
    int i, j;

    if (m->winProcAdded)
        AppMain_RemoveWinProcCallback(Movie_WinProcCB);
    if (m->hasSubtitles) {
        for (i = 0; i < m->subCount; i++) {
            Mem_Free(m->subs[i].text);
            m->subs[i].text = NULL;
            for (j = 0; j < m->subs[i].lineCount; j++)
                Text_Free(m->subs[i].texts[j]);
        }
        for (i = 0; i < 4; i++) {
            if (m->fonts[i])
                Sprite_Delete(m->fonts[i]);
        }
        Scene_Free(m->scene);
    }
    if (m->sound && m->soundPlaying) {
        Voice_Stop(m->sound);
        Voice_Free(m->sound);
    }
    if (m->inBuf) {
        Mem_Free(m->inBuf);
        m->inBuf = NULL;
    }
    if (m->outBuf) {
        Mem_Free(m->outBuf);
        m->outBuf = NULL;
    }
    if (m->outFormat) {
        Mem_Free(m->outFormat);
        m->outFormat = NULL;
    }
    if (m->hic) {
        ICSendMessage_(m->hic, ICM_DECOMPRESS_END, 0, 0);
        ICClose_(m->hic);
    }
    if (m->stream)
        AVIStreamRelease_(m->stream);
    if (m->file)
        AVIFileRelease_(m->file);
    memset(m, 0, sizeof(Movie));
}
HOOK(0x4903B0, Movie_Close);

/* Keeps the movie clock still while the window is inactive. */
int Movie_WinProcCB(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_ACTIVATE) {
        if (LOWORD(wparam) == WA_INACTIVE || HIWORD(wparam)) {
            if (!gMoviePauseStart) {
                gMoviePauseStart = Timer_GetMs();
                return 1;
            }
        } else if (!gMoviePauseStart) {
            return 1;
        }
        /* an inactive message while paused also ends the pause */
        gMovie.pausedTime += Timer_GetMs() - gMoviePauseStart;
        gMoviePauseStart = 0;
    }
    return 1;
}
HOOK(0x490530, Movie_WinProcCB);

char Movie_OpenFile(Movie *m, const char *path);
char Movie_InitVideo(Movie *m);
char Movie_LoadSubtitles(Movie *m, const char *path, char (*load)(Movie *m, const char *path));
char Movie_OpenSound(Movie *m, const char *path);

Movie *Movie_Open(const char *path, char (*loadSubtitles)(Movie *m, const char *path), int unk)
{
    memset(&gMovie, 0, sizeof(Movie));
    gMovie.unk18 = unk;
    gMoviePauseStart = 0;
    if (!Movie_OpenFile(&gMovie, path)) {
        Movie_Close(&gMovie);
        return NULL;
    }
    if (!Movie_InitVideo(&gMovie)) {
        Movie_Close(&gMovie);
        return NULL;
    }
    if (loadSubtitles && !Movie_LoadSubtitles(&gMovie, path, loadSubtitles)) {
        Movie_Close(&gMovie);
        return NULL;
    }
    if (!Movie_OpenSound(&gMovie, path)) {
        Movie_Close(&gMovie);
        return NULL;
    }
    AppMain_AddWinProcCallback(Movie_WinProcCB);
    gMovie.winProcAdded = 1;
    gMovie.playing = 1;
    return &gMovie;
}
HOOK(0x4905A0, Movie_Open);

char Movie_OpenFile(Movie *m, const char *path)
{
    char buf[0x400];
    AVISTREAMINFOA info;
    PAVISTREAM s;
    LONG size;
    unsigned i;
    int found = 0;

    File_ResolvePath(buf, path, 0);
    if (AVIFileOpenA_(&m->file, Path_SkipDevice(buf), OF_SHARE_DENY_WRITE, NULL))
        return 0;
    if (AVIFileInfoA_(m->file, &m->fileInfo, sizeof(m->fileInfo))) {
        Movie_Close(&gMovie);
        return 0;
    }
    for (i = 0; i < m->fileInfo.dwStreams; i++) {
        s = NULL;
        AVIFileGetStream_(m->file, &s, 0, i);
        if (s && !AVIStreamInfoA_(s, &info, sizeof(info))) {
            if (info.fccType == streamtypeVIDEO) {
                m->streamInfo = info;
                m->stream = s;
                found = 1;
            } else {
                AVIStreamRelease_(s);
            }
        }
    }
    if (!found) {
        Movie_Close(&gMovie);
        return 0;
    }
    m->frame = -1;
    m->drawnFrame = -1;
    m->duration = m->streamInfo.dwLength * 1000 / (m->streamInfo.dwRate / m->streamInfo.dwScale);
    Sound_SetUpdateRate(60);
    AVIStreamReadFormat_(m->stream, 0, NULL, &size);
    if (size != sizeof(BITMAPINFOHEADER)) {
        Movie_Close(&gMovie);
        return 0;
    }
    if (AVIStreamReadFormat_(m->stream, 0, &m->format, &size)) {
        Movie_Close(&gMovie);
        return 0;
    }
    return 1;
}
HOOK(0x490680, Movie_OpenFile);

char Movie_InitVideo(Movie *m)
{
    DisplayMode mode;
    DDPIXELFORMAT pf;
    MovieFormat *out;
    char msg[0x400];
    HRESULT hr;
    HDC dc;
    int scale = 1, depth = 16, size, h;

    if (!AppContext_IsFullscreen()) {
        dc = GetDC(NULL);
        depth = GetDeviceCaps(dc, BITSPIXEL);
        ReleaseDC(NULL, dc);
        if (depth == 8)
            goto fail;
    }
    /* small movies are shown at double size */
    if (m->format.biWidth < 640)
        scale = 2;
    size = m->format.biBitCount * m->format.biHeight * m->format.biWidth / 8;
    if (m->format.biSizeImage < (DWORD)size)
        m->format.biSizeImage = size;

    memset(&mode, 0, sizeof(mode));
    mode.width = m->format.biWidth * scale;
    mode.height = m->format.biHeight * scale;
    mode.unkC = 1;
    mode.depth = depth;
    mode.unk14 = 0;
    mode.unk15 = 0;
    mode.renderMode = Direct3D_GetRenderMode();
    mode.renderMode = Direct3D_GetRenderMode();
    mode.unk16 = 1;
    if (gDisplayPrefDesc && gDisplayPrefDevice) {
        strcpy(mode.desc, gDisplayPrefDesc);
        strcpy(mode.device, gDisplayPrefDevice);
    }
    if (!Display_FindModes(&mode, 7, NULL))
        goto fail;
    Display_SetMode(&mode);
    if (!gBackBuffer)
        goto fail;

    memset(&pf, 0, sizeof(pf));
    pf.dwSize = sizeof(pf);
    pf.dwFlags = DDPF_RGB;
    hr = gBackBuffer->lpVtbl->GetPixelFormat(gBackBuffer, &pf);
    if (hr) {
        crt_sprintf(msg, "Couldn't read pixel format");
        Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
        for (;;)
            ;
    }
    out = Mem_Alloc(sizeof(MovieFormat), 4);
    out->h = m->format;
    out->h.biWidth = m->format.biWidth * scale;
    h = m->format.biHeight * scale;
    out->h.biBitCount = depth;
    out->h.biSizeImage = (depth & 0xFFFF) * h * out->h.biWidth / 8;
    out->h.biHeight = -h;
    if ((short)depth == 16 || (short)depth == 32) {
        out->h.biCompression = BI_BITFIELDS;
        out->masks[0] = pf.dwRBitMask;
        out->masks[1] = pf.dwGBitMask;
        out->masks[2] = pf.dwBBitMask;
    }
    m->outFormat = out;
    m->hic = ICLocate_(ICTYPE_VIDEO, m->streamInfo.fccHandler, &m->format, &out->h, ICMODE_FASTDECOMPRESS);
    if (!m->hic)
        goto fail;
    if (ICSendMessage_(m->hic, ICM_DECOMPRESS_BEGIN, (DWORD_PTR)&m->format, (DWORD_PTR)m->outFormat))
        goto fail;
    m->inBuf = Mem_Alloc(m->format.biSizeImage, 4);
    if (!m->inBuf)
        goto fail;
    m->outBuf = Mem_Alloc(m->outFormat->h.biSizeImage, 4);
    if (!m->outBuf)
        goto fail;
    return 1;
fail:
    Movie_Close(&gMovie);
    return 0;
}
HOOK(0x490860, Movie_InitVideo);

char Movie_OpenSound(Movie *m, const char *path)
{
    char name[0x400], res[0x400];
    int a, b;

    if (Sound_IsReady()) {
        Path_StripExtension(name, sizeof(name), path);
        strcat(name, ".wav");
        Sound_GetName(res, name);
        a = Sound_HashBucket(res);
        b = Sound_HashName(res);
        m->sound = Voice_Play(a, b, 1, 0);
        if (m->sound)
            m->soundPlaying = 1;
    }
    return 1;
}
HOOK(0x490B80, Movie_OpenSound);

char Movie_LoadSubtitles(Movie *m, const char *path, char (*load)(Movie *m, const char *path))
{
    char name[0x400];
    float rect[4];

    Path_StripExtension(name, sizeof(name), path);
    strcat(name, ".qsc");
    m->hasSubtitles = 1;
    Scene_Init(gMovie.scene, 1);
    rect[0] = 0;
    rect[1] = 0;
    rect[2] = m->outFormat->h.biWidth;
    rect[3] = abs(m->outFormat->h.biHeight);
    Scene_SetRect(gMovie.scene, rect);
    return load(m, name) != 0;
}
HOOK(0x490C40, Movie_LoadSubtitles);

char Movie_ReadFrame(Movie *m, int frame, DWORD flags);
char Movie_Draw(Movie *m);

/* Advances the clock and decodes the frame for the current time. Returns 0 when done. */
char Movie_Update(Movie *m)
{
    char buf[0x58];
    unsigned n, frame;

    if (!m->startTime)
        m->startTime = Timer_GetMs();
    else
        m->time = Timer_GetMs() - m->pausedTime - m->startTime;
    if (m->sound) {
        m->soundPlaying = Voice_IsPlaying(m->sound);
        if (m->soundPlaying) {
            for (n = m->time * 60 / 1000 - m->soundFrames; n; n--) {
                SoundParams_Init(buf);
                Voice_SetParams(m->sound, buf);
                m->soundFrames++;
            }
        } else {
            Voice_Stop(m->sound);
            Voice_Free(m->sound);
            m->sound = NULL;
        }
    }
    m->time = Timer_GetMs() - m->pausedTime - m->startTime;
    if (m->time < m->duration) {
        frame = m->streamInfo.dwRate / m->streamInfo.dwScale * m->time / 1000;
        if ((unsigned)m->frame != frame) {
            if (m->frame == -1 || frame - m->frame == 1) {
                if (!Movie_ReadFrame(m, frame, 0))
                    return 0;
            } else {
                /* skipped frames still have to go through the decoder */
                do {
                    if (!Movie_ReadFrame(m, m->frame + 1, ICDECOMPRESS_HURRYUP))
                        return 0;
                } while ((unsigned)m->frame != frame);
            }
            Movie_Draw(m);
            return 1;
        }
    } else {
        m->playing = 0;
    }
    return m->soundPlaying || m->playing;
}
HOOK(0x490D10, Movie_Update);

char Movie_ReadFrame(Movie *m, int frame, DWORD flags)
{
    if (AVIStreamRead_(m->stream, frame, 1, m->inBuf, m->format.biSizeImage, NULL, NULL))
        return 0;
    if (ICDecompress_(m->hic, flags, &m->format, m->inBuf, &m->outFormat->h, m->outBuf))
        return 0;
    m->frame = frame;
    return 1;
}
HOOK(0x490E90, Movie_ReadFrame);

void Movie_BlitFrame(Movie *m);

char Movie_Draw(Movie *m)
{
    MovieSubtitle *s;
    Text *t;
    float a;
    int time, i;

    if (m->drawnFrame == m->frame)
        return 1;
    Movie_BlitFrame(m);
    if (m->hasSubtitles) {
        time = m->time;
        Frame_Reset();
        Scene_Select(m->scene, 1);
        if (m->curSub < m->subCount) {
            s = &m->subs[m->curSub];
            if (time > s->start + s->duration + s->fadeOut) {
                m->curSub++;
            } else if (time >= s->start) {
                a = 1.0f;
                if (s->fadeInOn != -1 && time < s->fadeIn + s->start)
                    a = 1.0f - (long double)(s->fadeIn + s->start - time) / s->fadeIn;
                if (s->fadeOutOn != -1 && time >= s->duration + s->start)
                    a = (long double)(s->fadeOut - time + s->duration + s->start) / s->fadeOut;
                for (i = 0; i < s->lineCount; i++) {
                    t = s->texts[i];
                    t->color[0] = (long double)a * s->color[0] * 128.0f;
                    t->color[1] = (long double)a * s->color[1] * 128.0f;
                    t->color[2] = (long double)a * s->color[2] * 128.0f;
                    t->flags |= 0x80000;
                    t->alpha = a;
                    Text_DrawAlt(t);
                }
            }
        }
        Scene_DrawAll();
        Debug_Log();
    }
    Display_SwapBuffers();
    m->drawnFrame = m->frame;
    return 1;
}
HOOK(0x490F10, Movie_Draw);

void Movie_BlitFrame(Movie *m)
{
    DDSURFACEDESC2 sd;
    IDirectDrawSurface7 *surf;
    RECT r;
    char msg[0x400];
    HRESULT hr;

    memset(&sd, 0, sizeof(sd));
    sd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    hr = gBackBuffer->lpVtbl->GetPixelFormat(gBackBuffer, &sd.ddpfPixelFormat);
    if (hr) {
        crt_sprintf(msg, "Couldn't read pixel format");
        Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
        for (;;)
            ;
    }
    sd.dwSize = sizeof(sd);
    sd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_LPSURFACE;
    sd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_TEXTURE;
    sd.dwWidth = m->outFormat->h.biWidth;
    sd.dwHeight = abs(m->outFormat->h.biHeight);
    sd.lPitch = (unsigned)(m->outFormat->h.biWidth * sd.ddpfPixelFormat.dwRGBBitCount) >> 3;
    sd.lpSurface = m->outBuf;
    hr = gDDraw->lpVtbl->CreateSurface(gDDraw, &sd, &surf, NULL);
    if (hr) {
        crt_sprintf(msg, "Couldn't create texture surface");
        Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
        for (;;)
            ;
    }
    r.left = 0;
    r.top = 0;
    r.right = m->outFormat->h.biWidth;
    r.bottom = abs(m->outFormat->h.biHeight);
    gBackBuffer->lpVtbl->Blt(gBackBuffer, NULL, surf, &r, DDBLT_DONOTWAIT | DDBLT_ASYNC, NULL);
    surf->lpVtbl->Release(surf);
}
HOOK(0x4910C0, Movie_BlitFrame);

int Movie_IsPlaying(void)
{
    return gMovie.soundPlaying || gMovie.playing;
}
HOOK(0x491220, Movie_IsPlaying);

void Movie_SetFont(Movie *m, int index, const char *name)
{
    if (index >= 0 && index < 4 && name && m && !m->fonts[index])
        m->fonts[index] = Sprite_New(name);
}
HOOK(0x491240, Movie_SetFont);

void Movie_LayoutSubtitle(Movie *m, MovieSubtitle *sub);

void Movie_AddSubtitle(Movie *m, MovieSubtitleDef *def)
{
    MovieSubtitle *sub = &m->subs[m->subCount++];
    char *data;
    int size;

    sub->start = Float_ToInt(def->start) * 1000;
    sub->duration = Float_ToInt(def->duration) * 1000;
    sub->fadeInOn = def->fadeInOn;
    sub->fadeIn = Float_ToInt(def->fadeIn) * 1000;
    sub->fadeOutOn = def->fadeOutOn;
    sub->fadeOut = Float_ToInt(def->fadeOut) * 1000;
    sub->color[0] = def->color[0];
    sub->color[1] = def->color[1];
    sub->color[2] = def->color[2];
    sub->x = def->x;
    sub->y = def->y;
    sub->font = def->font;
    sub->centered = def->centered;
    /* the text is a resource name, or shown as is with a marker */
    if (strlen(def->text) < 0x80 && Resource_Find(def->text)) {
        size = 0;
        data = Resource_Load(def->text, &size);
        if (data) {
            sub->text = Mem_Alloc(size, 4);
            strcpy(sub->text, data);
            Resource_Flush(data);
        }
    } else {
        sub->text = Mem_Alloc(strlen(def->text) + 5, 4);
        crt_sprintf(sub->text, "RM: %s", def->text);
    }
    Movie_LayoutSubtitle(m, sub);
}
HOOK(0x491280, Movie_AddSubtitle);

/* Word wraps the text into at most 8 lines and makes a Text for each. */
void Movie_LayoutSubtitle(Movie *m, MovieSubtitle *sub)
{
    char *p = sub->text, *line = p, *cut = p;
    char **slot = sub->lines;
    int n = 0, i, width = m->outFormat->h.biWidth;
    float x, y, maxWidth;
    Text *t;

    x = sub->centered ? width / 2 : sub->x;
    y = sub->y;
    maxWidth = sub->centered ? width : width - sub->x;
    for (; *p && n < 7; p++) {
        if (*p == '\r')
            continue;
        if (*p == ' ') {
            *p = 0;
            if ((long double)Sprite_StringWidth(m->fonts[sub->font], line) > maxWidth) {
                *slot++ = line;
                n++;
                *cut = 0;
                line = cut = cut + 1;
            } else {
                cut = p;
            }
            *p = ' ';
        }
        if (*p == '\n') {
            *slot++ = line;
            n++;
            *p = 0;
            line = cut = p + 1;
        }
    }
    sub->lines[n++] = line;
    for (i = 0; i < n; i++) {
        t = Text_New(m->fonts[sub->font], x, y, sub->lines[i]);
        sub->texts[i] = t;
        if (sub->centered)
            t->flags |= 0x80;
        t->flags |= 8;
        t->color[0] = sub->color[0] * 128.0f;
        t->color[1] = sub->color[1] * 128.0f;
        t->color[2] = sub->color[2] * 128.0f;
        y = (long double)Text_GetHeight(t) + y;
    }
    sub->lineCount = n;
}
HOOK(0x491420, Movie_LayoutSubtitle);
