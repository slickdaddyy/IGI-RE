/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "igi.h"
#include "game/game.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (void *parent, uint16_t type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4028A0, void, Flow_SetRequest, (int request));
ORIG(0x406220, char *, Profile_GetActive, (void));
ORIG(0x48F2D0, int, AppContext_IsUSA, (void));
ORIG(0x4903B0, void, Movie_Close, (void *movie));
ORIG(0x4905A0, void *, Movie_Open, (const char *path, char (*loadSubtitles)(void *movie, const char *path), int unk));
ORIG(0x490D10, char, Movie_Update, (void *movie));
ORIG(0x491240, void, Movie_SetFont, (void *movie, int index, const char *name));
ORIG(0x491280, void, Movie_AddSubtitle, (void *movie, void *def));
ORIG(0x491610, void, Display_SetPreferredDevice, (const char *desc, const char *device));
ORIG(0x4B12F0, void, Alias_Set, (const char *name, const char *path));
ORIG(0x4B13A0, void, Alias_Remove, (const char *name));
ORIG(0x4B5B60, int, Resource_Exists, (const char *name));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int *size));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *s));
ORIG(0x4B85B0, void, Script_Run, (void *s));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4E68D0, void, SoundLoad, (const char *path));
ORIG(0x4E69C0, void, SoundFree, (const char *path));

/* the task that plays the intro or outro movies */
typedef struct IntroScreen {
    Task task;
    int count;
    int index;
    const char **movies;
    void *movie;
    void *subtitles;
    char loop;
} IntroScreen;

typedef struct IntroArgs {
    int count;
    const char **movies;
    char loop;
} IntroArgs;

/* MovieSubtitle_RegisterText's arguments, for Movie_AddSubtitle */
typedef struct SubtitleDef {
    const char *text;
    int font;
    int x;
    int y;
    int fadeInOn;
    int fadeOutOn;
    float color[3];
    float start;
    float duration;
    float fadeIn;
    float fadeOut;
    char centered;
} SubtitleDef;

extern uint16_t gIntroScreenType;
extern const char *gIntroMoviesEU[];
extern const char *gIntroMoviesUS[];
extern const char *gOutroMovies[];

#define KEY_ESCAPE 0x01

void IntroScreen_Create(IntroScreen *s, IntroArgs *a);
void IntroScreen_Run(IntroScreen *s);
void IntroScreen_Delete(IntroScreen *s);
ScriptValue *MovieSubtitle_RegisterFont(ScriptValue *out, void *movie, void *args);
ScriptValue *MovieSubtitle_RegisterText(ScriptValue *out, void *movie, void *args);

void IntroScreen_Init(void)
{
    uint16_t type = TaskType_New("IntroScreen", sizeof(IntroScreen), Task_GetRootType(), 0);

    gIntroScreenType = type;
    TaskType_SetHandler(type, 0, IntroScreen_Create);
    TaskType_SetHandler2(gIntroScreenType, 1, IntroScreen_Run);
    TaskType_SetHandler2(gIntroScreenType, 2, IntroScreen_Delete);
    Alias_Set("INTROSCREEN:", "LOCAL:screens/intro/");
    SoundLoad("LOCAL:screens/intro/");
}
HOOK(0x415070, IntroScreen_Init);

/* runs the movie's subtitle script */
char IntroScreen_LoadSubtitles(void *movie, const char *path)
{
    void *s;

    Script_RegisterFunction("MovieSubtitle_RegisterFont", MovieSubtitle_RegisterFont, movie, 0);
    Script_RegisterFunction("MovieSubtitle_RegisterText", MovieSubtitle_RegisterText, movie, 0);
    s = Script_Load(path);
    if (s) {
        Script_Run(s);
        Script_Free(s);
    }
    Script_UnregisterSymbol("MovieSubtitle_RegisterFont");
    Script_UnregisterSymbol("MovieSubtitle_RegisterText");
    return 1;
}
HOOK(0x415170, IntroScreen_LoadSubtitles);

void IntroScreen_Create(IntroScreen *s, IntroArgs *a)
{
    char *p;

    if (Resource_Exists("LANGUAGE:movies.res"))
        s->subtitles = Resource_Load("LANGUAGE:movies.res", NULL);
    else
        s->subtitles = NULL;
    p = Profile_GetActive();
    if (p)
        Display_SetPreferredDevice(p + 0x11F, p + 0x1F);
    s->count = a->count;
    s->loop = a->loop;
    s->movies = a->movies;
    s->index = 1;
    s->movie = Movie_Open(a->movies[0], IntroScreen_LoadSubtitles, 0);
}
HOOK(0x4150E0, IntroScreen_Create);

ScriptValue *MovieSubtitle_RegisterFont(ScriptValue *out, void *movie, void *args)
{
    char name[0x400];
    int index = Script_GetArgInt(args, 0);

    Script_GetArgString(args, 1, name, sizeof(name));
    Movie_SetFont(movie, index, name);
    return Game_ReturnOne(out, 2, 2.0);
}
HOOK(0x4151E0, MovieSubtitle_RegisterFont);

ScriptValue *MovieSubtitle_RegisterText(ScriptValue *out, void *movie, void *args)
{
    char text[0x400];
    SubtitleDef d;

    d.font = Script_GetArgInt(args, 0);
    d.text = Script_GetArgString(args, 1, text, sizeof(text));
    d.x = Script_GetArgInt(args, 2);
    d.y = Script_GetArgInt(args, 3);
    d.color[0] = Script_GetArgReal(args, 4);
    d.color[1] = Script_GetArgReal(args, 5);
    d.color[2] = Script_GetArgReal(args, 6);
    d.centered = Script_GetArgInt(args, 7);
    d.start = Script_GetArgReal(args, 8);
    d.duration = Script_GetArgReal(args, 9);
    d.fadeInOn = Script_GetArgInt(args, 10);
    d.fadeIn = Script_GetArgReal(args, 11);
    d.fadeOutOn = Script_GetArgInt(args, 12);
    d.fadeOut = Script_GetArgReal(args, 13);
    Movie_AddSubtitle(movie, &d);
    return Game_ReturnOne(out, 14, 14.0);
}
HOOK(0x415260, MovieSubtitle_RegisterText);

void IntroScreen_Run(IntroScreen *s)
{
    int done = s->movie == NULL;

    if (s->movie && !Movie_Update(s->movie))
        done = 1;
    if (gKeys[KEY_ESCAPE] & 0x80) {
        if (s->movie) {
            Movie_Close(s->movie);
            s->movie = NULL;
        }
    } else if (done) {
        if (s->movie) {
            Movie_Close(s->movie);
            s->movie = NULL;
        }
        if (s->index < s->count) {
            s->index++;
            s->movie = Movie_Open(s->movies[s->index - 1], IntroScreen_LoadSubtitles, 0);
        } else if (s->loop) {
            s->index = 1;
            s->movie = Movie_Open(s->movies[0], IntroScreen_LoadSubtitles, 0);
        }
    }
    if (!s->movie) {
        Flow_SetRequest(3);
        Task_Delete(s);
    }
}
HOOK(0x415380, IntroScreen_Run);

void IntroScreen_Delete(IntroScreen *s)
{
    if (s->subtitles)
        Resource_Flush(s->subtitles);
    if (s->movie)
        Movie_Close(s->movie);
}
HOOK(0x415450, IntroScreen_Delete);

void IntroScreen_Shutdown(void)
{
    SoundFree("LOCAL:screens/intro/");
    TaskType_Free(gIntroScreenType);
    Alias_Remove("INTROSCREEN:");
}
HOOK(0x415480, IntroScreen_Shutdown);

/* mode 0 plays the logos and the intro, mode 1 the outro */
Task *IntroScreen_New(Task *parent, int mode, char loop)
{
    IntroArgs a;

    if (mode == 0) {
        a.movies = AppContext_IsUSA() ? gIntroMoviesUS : gIntroMoviesEU;
        a.count = 4;
    } else if (mode == 1) {
        a.movies = gOutroMovies;
        a.count = 1;
    } else {
        Error_Show("Invalid introscreen mode");
        for (;;)
            ;
    }
    a.loop = loop;
    return Task_New(parent, gIntroScreenType, &a);
}
HOOK(0x4154B0, IntroScreen_New);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 5393E8, 5393E8, 0x10) INIT({0x8C, 0x94, 0x53, 0x00, 0x70, 0x94, 0x53, 0x00, 0x5C, 0x94, 0x53, 0x00, 0x40, 0x94, 0x53});
ALIAS(gIntroMoviesEU, pad_5393E8, 0);
PAD(igd, 5393E8, 5393F8, 0x10) INIT({0x8C, 0x94, 0x53, 0x00, 0x70, 0x94, 0x53, 0x00, 0x5C, 0x94, 0x53, 0x00, 0x24, 0x94, 0x53});
ALIAS(gIntroMoviesUS, pad_5393F8, 0);
PAD(igd, 5393E8, 539408, 0x138) INIT({0x0C, 0x94, 0x53, 0x00, 0x49, 0x4E, 0x54, 0x52, 0x4F, 0x53, 0x43, 0x52, 0x45, 0x45, 0x4E, 0x3A, 0x6F, 0x75, 0x74, 0x72, 0x6F, 0x2E, 0x61, 0x76, 0x69, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x52, 0x4F, 0x53, 0x43, 0x52, 0x45, 0x45, 0x4E, 0x3A, 0x69, 0x6E, 0x74, 0x72, 0x6F, 0x5F, 0x75, 0x73, 0x2E, 0x61, 0x76, 0x69, 0x00, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x52, 0x4F, 0x53, 0x43, 0x52, 0x45, 0x45, 0x4E, 0x3A, 0x69, 0x6E, 0x74, 0x72, 0x6F, 0x5F, 0x65, 0x75, 0x2E, 0x61, 0x76, 0x69, 0x00, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x52, 0x4F, 0x53, 0x43, 0x52, 0x45, 0x45, 0x4E, 0x3A, 0x76, 0x70, 0x2E, 0x61, 0x76, 0x69, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x52, 0x4F, 0x53, 0x43, 0x52, 0x45, 0x45, 0x4E, 0x3A, 0x69, 0x6E, 0x6E, 0x65, 0x72, 0x6C, 0x6F, 0x6F, 0x70, 0x2E, 0x61, 0x76, 0x69, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x52, 0x4F, 0x53, 0x43, 0x52, 0x45, 0x45, 0x4E, 0x3A, 0x65, 0x69, 0x64, 0x6F, 0x73, 0x2E, 0x61, 0x76, 0x69, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x54, 0x52, 0x4F, 0x53, 0x43, 0x52, 0x45, 0x45, 0x4E, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x4F, 0x43, 0x41, 0x4C, 0x3A, 0x73, 0x63, 0x72, 0x65, 0x65, 0x6E, 0x73, 0x2F, 0x69, 0x6E, 0x74, 0x72, 0x6F, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x49, 0x6E, 0x74, 0x72, 0x6F, 0x53, 0x63, 0x72, 0x65, 0x65, 0x6E, 0x00, 0x4C, 0x41, 0x4E, 0x47, 0x55, 0x41, 0x47, 0x45, 0x3A, 0x6D, 0x6F, 0x76, 0x69, 0x65, 0x73, 0x2E, 0x72, 0x65, 0x73, 0x00, 0x4D, 0x6F, 0x76, 0x69, 0x65, 0x53, 0x75, 0x62, 0x74, 0x69, 0x74, 0x6C, 0x65, 0x5F, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x54, 0x65, 0x78, 0x74, 0x00, 0x00, 0x4D, 0x6F, 0x76, 0x69, 0x65, 0x53, 0x75, 0x62, 0x74, 0x69, 0x74, 0x6C, 0x65, 0x5F, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x46, 0x6F, 0x6E, 0x74, 0x00, 0x00, 0x49, 0x6E, 0x76, 0x61, 0x6C, 0x69, 0x64, 0x20, 0x69, 0x6E, 0x74, 0x72, 0x6F, 0x73, 0x63, 0x72, 0x65, 0x65, 0x6E, 0x20, 0x6D, 0x6F, 0x64, 0x65});
ALIAS(gOutroMovies, pad_539408, 0);
BSS(57B198) uint16_t gIntroScreenType;
PAD(igb, 57B198, 57B19A, 0x2);
