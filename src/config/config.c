#include <stdio.h>
#include <string.h>

#include "igi.h"
#include "hook.h"
#include "config/config.h"
#include "config/configvars.h"

ORIG(0x4A5980, int, crt_stricmp, (const char *a, const char *b));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B88F0, void, Script_RegisterFunctionEx, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (void *args, int i, char *buf, int size));
ORIG(0x4B0FD0, void, RenderMode_Push, (int mode));
ORIG(0x4B0F90, void, RenderMode_Pop, (void));
ORIG(0x4B13A0, void, Alias_Remove, (const char *name));
ORIG(0x406220, PlayerProfile *, Profile_GetActive, (void));
ORIG(0x417090, int, Mission_GetFirst, (void));
ORIG(0x485170, char *, Mission_Find, (int mission));
ORIG(0x48F2C0, int, AppContext_IsGerman, (void));
ORIG(0x48F2F0, const char *, AppContext_GetCountry, (void));
ORIG(0x4950E0, int, Direct3D_GetRenderMode, (void));
ORIG(0x491EA0, int, Display_EnumModes, (char (*names)[0x100], int max));
ORIG(0x491CF0, DisplayMode *, Display_GetMode, (void));
ORIG(0x491980, int, Display_FindModes, (DisplayMode *mode, int flags, void (*fn)(DisplayMode *mode)));
ORIG(0x41F2D0, void, ListBox_AddItem, (const char *text, int id));
ORIG(0x41DC20, void, InputBox_SetText, (const char *text));
ORIG(0x41C690, void, ScrollListBox_AddItem, (const char *text, int id));
ORIG(0x41B870, void, PictureBox_AddItem, (const char *text, int id));
ORIG(0x41B290, void, TextBox_AddItem, (const char *text, int id));

extern char gLanguageAliasSet;
extern int gPerfLevelFlags[];
extern InputRemap gDefaultKeys[];
extern ConfigConstant gInputActions[CONFIG_ACTION_COUNT];
extern ConfigConstant gInputKeys[CONFIG_KEY_COUNT];
extern char gLanguageNames[][16];
extern char gDifficultyNames[][16];

#define NUM_PERF_LEVELS 7
#define NUM_LANGUAGES 6
#define NUM_DIFFICULTIES 3
#define NUM_DEFAULT_REMAPS 27

/* mission record fields used here */
#define MISSION_NAME 0x4
#define MISSION_TEXT 0x24
#define MISSION_PICTURE 0x484
#define MISSION_NEXT 0x4A4

/* fistp with the current rounding mode */
static inline int round_x87(long double d)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(d) : "st");
    return (int)r;
}

static ScriptValue *ret_int(ScriptValue *out, int v)
{
    out->i = v;
    out->d = v;
    out->s = gEmptyString;
    return out;
}

static PlayerProfile *last_profile(void)
{
    return &gProfiles[gProfileCount - 1];
}

void Config_RegisterScriptSymbols(void)
{
    char names[CONFIG_MAX_DEVICES][0x100];
    ConfigConstant *c;
    int i, n;

    Script_RegisterFunction("GOStart", GOStart, NULL, 1);
    Script_RegisterFunction("GOPlayer", GOPlayer, NULL, 1);
    Script_RegisterFunction("GOInRemap", GOInRemap, NULL, 3);
    Script_RegisterFunction("GOInMouInv", GOInMouInv, NULL, 1);
    Script_RegisterFunction("GOInMouSens", GOInMouSens, NULL, 1);
    Script_RegisterFunction("GOGfxDisp", GOGfxDisp, NULL, 3);
    Script_RegisterFunction("GOGfxDevice", GOGfxDevice, NULL, 1);
    Script_RegisterFunction("GOGfxGamma", GOGfxGamma, NULL, 1);
    Script_RegisterFunction("GOGameLang", GOGameLang, NULL, 1);
    Script_RegisterFunction("GOGameDiff", GOGameDiff, NULL, 1);
    Script_RegisterFunction("GOIsBlood", GOIsBlood, NULL, 1);
    Script_RegisterFunction("GOSoundSpeech", GOSoundSpeech, NULL, 2);
    Script_RegisterFunction("GOSoundMusic", GOSoundMusic, NULL, 2);
    Script_RegisterFunction("GOSoundFX", GOSoundFX, NULL, 2);
    Script_RegisterFunction("GOGfxPerformance", GOGfxPerformance, NULL, 1);
    Script_RegisterFunction("GOActiveMission", GOActiveMission, NULL, 1);
    Script_RegisterFunction("GOContentControlPW", GOContentControlPW, NULL, 1);
    Script_RegisterFunction("Config_FillPlayerProfileListBox", Config_FillPlayerProfileListBox, NULL, 0);
    Script_RegisterFunction("Config_GetActivePlayerProfileIndex", Config_GetActivePlayerProfileIndex, NULL, 0);
    Script_RegisterFunction("Config_SetActivePlayerProfileIndex", Config_SetActivePlayerProfileIndex, NULL, 1);
    Script_RegisterFunction("Config_CreateNewPlayerProfile", Config_CreateNewPlayerProfile, NULL, 2);
    Script_RegisterFunction("Config_DeletePlayerProfile", Config_DeletePlayerProfile, NULL, 1);
    Script_RegisterFunction("Config_PlayerGetActiveMission", Config_PlayerGetActiveMission, NULL, 1);
    Script_RegisterFunction("Config_GetNumberOfPlayerProfiles", Config_GetNumberOfPlayerProfiles, NULL, 1);
    Script_RegisterFunction("Config_FillMissionSelectionBox", Config_FillMissionSelectionBox, NULL, 0);
    Script_RegisterFunction("Config_FillMissionPictureBox", Config_FillMissionPictureBox, NULL, 0);
    Script_RegisterFunction("Config_FillMissionTextBox", Config_FillMissionTextBox, NULL, 0);
    Script_RegisterFunction("Config_FillScreenResolutionListBox", Config_FillScreenResolutionListBox, NULL, 1);
    Script_RegisterFunction("Config_FillRenderDeviceListBox", Config_FillRenderDeviceListBox, NULL, 0);
    Script_RegisterFunction("Config_GameOptionsInputGetInvertMouse", Config_GameOptionsInputGetInvertMouse, NULL, 0);
    Script_RegisterFunction("Config_GameOptionsInputGetMouseSensitivity",
                            Config_GameOptionsInputGetMouseSensitivity, NULL, 0);
    Script_RegisterFunction("Config_GameOptionsInputGetBloodEnabled", Config_GameOptionsInputGetBloodEnabled,
                            NULL, 0);
    Script_RegisterFunction("Config_GameOptionsInputSetInvertMouse", Config_GameOptionsInputSetInvertMouse, NULL, 1);
    Script_RegisterFunction("Config_GameOptionsInputSetMouseSensitivity",
                            Config_GameOptionsInputSetMouseSensitivity, NULL, 1);
    Script_RegisterFunction("Config_GameOptionsInputSetBloodEnabled", Config_GameOptionsInputSetBloodEnabled,
                            NULL, 1);
    Script_RegisterFunction("Config_GraphicOptionsSetResolution", Config_GraphicOptionsSetResolution, NULL, 1);
    Script_RegisterFunction("Config_GraphicOptionsSetTransparency", Config_GraphicOptionsSetTransparency, NULL, 1);
    Script_RegisterFunction("Config_GraphicOptionsSetDevice", Config_GraphicOptionsSetDevice, NULL, 1);
    Script_RegisterFunctionEx("Config_GraphicOptionsSetGamma", Config_GraphicOptionsSetGamma, NULL, 1);
    Script_RegisterFunction("Config_GraphicOptionsSetPerformanceFlags", Config_GraphicOptionsSetPerformanceFlags,
                            NULL, 1);
    Script_RegisterFunction("Config_GraphicOptionsGetResolution", Config_GraphicOptionsGetResolution, NULL, 0);
    Script_RegisterFunction("Config_GraphicOptionsGetTransparency", Config_GraphicOptionsGetTransparency, NULL, 0);
    Script_RegisterFunction("Config_GraphicOptionsGetDevice", Config_GraphicOptionsGetDevice, NULL, 0);
    Script_RegisterFunction("Config_GraphicOptionsGetGamma", Config_GraphicOptionsGetGamma, NULL, 0);
    Script_RegisterFunction("Config_GraphicOptionsGetPerformanceFlags", Config_GraphicOptionsGetPerformanceFlags,
                            NULL, 0);
    Script_RegisterFunction("Config_GraphicOptionsGetPerfFlagsFromLevel", Config_GraphicOptionsGetPerfFlagsFromLevel,
                            NULL, 1);
    Script_RegisterFunction("Config_GraphicOptionsGetPerfLevelFromFlags", Config_GraphicOptionsGetPerfLevelFromFlags,
                            NULL, 1);
    Script_RegisterFunction("Config_GraphicOptionsNumPerfLevels", Config_GraphicOptionsNumPerfLevels, NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsSetSoundsEffects", Config_SoundOptionsSetSoundsEffects, NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsSetSoundsEffectsVolume", Config_SoundOptionsSetSoundsEffectsVolume,
                            NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsSetMusic", Config_SoundOptionsSetMusic, NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsSetMusicVolume", Config_SoundOptionsSetMusicVolume, NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsSetSpeech", Config_SoundOptionsSetSpeech, NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsSetSpeechVolume", Config_SoundOptionsSetSpeechVolume, NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsSetReverseStereo", Config_SoundOptionsSetReverseStereo, NULL, 1);
    Script_RegisterFunction("Config_SoundOptionsGetSoundsEffects", Config_SoundOptionsGetSoundsEffects, NULL, 0);
    Script_RegisterFunction("Config_SoundOptionsGetSoundsEffectsVolume", Config_SoundOptionsGetSoundsEffectsVolume,
                            NULL, 0);
    Script_RegisterFunction("Config_SoundOptionsGetMusic", Config_SoundOptionsGetMusic, NULL, 0);
    Script_RegisterFunction("Config_SoundOptionsGetMusicVolume", Config_SoundOptionsGetMusicVolume, NULL, 0);
    Script_RegisterFunction("Config_SoundOptionsGetSpeech", Config_SoundOptionsGetSpeech, NULL, 0);
    Script_RegisterFunction("Config_SoundOptionsGetSpeechVolume", Config_SoundOptionsGetSpeechVolume, NULL, 0);
    Script_RegisterFunction("Config_SoundOptionsGetReverseStereo", Config_SoundOptionsGetReverseStereo, NULL, 0);
    Script_RegisterFunction("Config_SetContentControlPassword", GOContentControlPW, NULL, 1);
    Script_RegisterFunction("Config_GetContentControlPassword", Config_GetContentControlPassword, NULL, 0);
    Script_RegisterFunction("Config_VerifyContentControlPassword", Config_VerifyContentControlPassword, NULL, 1);
    Script_RegisterFunction("Config_IsContentControlPasswordEnabled", Config_IsContentControlPasswordEnabled,
                            NULL, 0);
    Script_RegisterFunction("Config_IsGermany", Config_IsGermany, NULL, 0);
    Script_RegisterFunction("Config_ResetGraphicOptions", Config_ResetGraphicOptions, NULL, 0);
    Script_RegisterFunction("DefineGameOptions", NULL, NULL, 1);

    for (c = gInputActions; c < gInputActions + CONFIG_ACTION_COUNT; c++)
        Script_RegisterConstant(c->name, c->value);
    for (c = gInputDevices; c < gInputDevices + CONFIG_DEVICE_COUNT; c++)
        Script_RegisterConstant(c->name, c->value);
    for (c = gInputKeys; c < gInputKeys + CONFIG_KEY_COUNT; c++)
        Script_RegisterConstant(c->name, c->value);
    for (i = 0; i < NUM_LANGUAGES; i++)
        Script_RegisterConstant(gLanguageNames[i], i);
    for (i = 0; i < NUM_DIFFICULTIES; i++)
        Script_RegisterConstant(gDifficultyNames[i], i);

    RenderMode_Push(Direct3D_GetRenderMode());
    n = Display_EnumModes(names, CONFIG_MAX_DEVICES);
    gDriverCount = n;
    for (i = 0; i < n; i++) {
        memcpy(gDrivers[i].name, names[i], sizeof(names[i]));
        gDrivers[i].mode_count = 0;
    }
    Display_FindModes(Display_GetMode(), 8, Config_AddDisplayMode);
    RenderMode_Pop();
    gContentControlPassword[0] = 0;
}
HOOK(0x402FF0, Config_RegisterScriptSymbols);

ScriptValue *GOStart(ScriptValue *out, void *data, void *args)
{
    gProfileCount = 0;
    gActiveProfile = Script_GetArgInt(args, 0);
    return ret_int(out, 0);
}
HOOK(0x403660, GOStart);

ScriptValue *GOPlayer(ScriptValue *out, void *data, void *args)
{
    gProfileCount++;
    last_profile()->remap_count = 0;
    last_profile()->active_mission = Mission_GetFirst();
    Script_GetArgString(args, 0, last_profile()->name, sizeof(last_profile()->name));
    return ret_int(out, 0);
}
HOOK(0x4036B0, GOPlayer);

ScriptValue *GOActiveMission(ScriptValue *out, void *data, void *args)
{
    last_profile()->active_mission = Script_GetArgInt(args, 0);
    return ret_int(out, 0);
}
HOOK(0x403750, GOActiveMission);

ScriptValue *GOInRemap(ScriptValue *out, void *data, void *args)
{
    PlayerProfile *p = last_profile();
    InputRemap *r = &p->remaps[p->remap_count];

    r->action = Script_GetArgInt(args, 0);
    r->device = Script_GetArgInt(args, 1);
    r->key = Script_GetArgInt(args, 2) & 0x3FFFFFFF;
    p->remap_count++;
    return ret_int(out, 0);
}
HOOK(0x4037C0, GOInRemap);

ScriptValue *GOInMouInv(ScriptValue *out, void *data, void *args)
{
    last_profile()->invert_mouse = Script_GetArgInt(args, 0);
    return ret_int(out, 0);
}
HOOK(0x403870, GOInMouInv);

ScriptValue *GOInMouSens(ScriptValue *out, void *data, void *args)
{
    last_profile()->mouse_sensitivity = Script_GetArgReal(args, 0);
    return ret_int(out, 0);
}
HOOK(0x4038E0, GOInMouSens);

ScriptValue *GOGfxDisp(ScriptValue *out, void *data, void *args)
{
    last_profile()->gfx.display.width = Script_GetArgInt(args, 0);
    last_profile()->gfx.display.height = Script_GetArgInt(args, 1);
    last_profile()->gfx.display.depth = Script_GetArgInt(args, 2);
    return ret_int(out, 0);
}
HOOK(0x403950, GOGfxDisp);

/* the US build always uses the USA language, other builds never do */
ScriptValue *GOGameLang(ScriptValue *out, void *data, void *args)
{
    if (crt_stricmp(AppContext_GetCountry(), "USA") == 0) {
        last_profile()->language = CONFIG_LANG_USA;
    } else {
        last_profile()->language = Script_GetArgInt(args, 0);
        if (last_profile()->language == CONFIG_LANG_USA)
            last_profile()->language = 0;
    }
    return ret_int(out, 0);
}
HOOK(0x403A00, GOGameLang);

ScriptValue *GOGameDiff(ScriptValue *out, void *data, void *args)
{
    last_profile()->difficulty = Script_GetArgInt(args, 0);
    return ret_int(out, 0);
}
HOOK(0x403AC0, GOGameDiff);

ScriptValue *Config_IsGermany(ScriptValue *out)
{
    out->i = AppContext_IsGerman();
    out->d = AppContext_IsGerman();
    out->s = gEmptyString;
    return out;
}
HOOK(0x403B30, Config_IsGermany);

ScriptValue *Config_ResetGraphicOptions(ScriptValue *out)
{
    Profile_GetActive()->gfx.unk11 = 1;
    Profile_GetActive()->gfx.unk12 = 1;
    Profile_GetActive()->gfx.transparency = 1;
    Profile_GetActive()->gfx.display.height = 480;
    Profile_GetActive()->gfx.display.width = 640;
    Profile_GetActive()->gfx.display.depth = 32;
    Profile_GetActive()->gfx.device_desc[0] = 0;
    Profile_GetActive()->gfx.device[0] = 0;
    Profile_GetActive()->gfx.gamma = 1.0f;
    Profile_GetActive()->gfx.perf_flags = 0;
    return ret_int(out, 0);
}
HOOK(0x403B70, Config_ResetGraphicOptions);

ScriptValue *GOContentControlPW(ScriptValue *out, void *data, void *args)
{
    Script_GetArgString(args, 0, gContentControlPassword, 0x20);
    return ret_int(out, 0);
}
HOOK(0x403C10, GOContentControlPW);

ScriptValue *Config_GetContentControlPassword(ScriptValue *out)
{
    InputBox_SetText(gContentControlPassword);
    return ret_int(out, 0);
}
HOOK(0x403C70, Config_GetContentControlPassword);

ScriptValue *Config_VerifyContentControlPassword(ScriptValue *out, void *data, void *args)
{
    char buf[0x20];

    Script_GetArgString(args, 0, buf, sizeof(buf));
    return ret_int(out, strcmp(buf, gContentControlPassword) == 0);
}
HOOK(0x403CC0, Config_VerifyContentControlPassword);

ScriptValue *Config_IsContentControlPasswordEnabled(ScriptValue *out)
{
    return ret_int(out, strlen(gContentControlPassword));
}
HOOK(0x403D90, Config_IsContentControlPasswordEnabled);

ScriptValue *GOIsBlood(ScriptValue *out, void *data, void *args)
{
    last_profile()->blood = Script_GetArgInt(args, 0);
    return ret_int(out, 0);
}
HOOK(0x403DF0, GOIsBlood);

ScriptValue *GOSoundSpeech(ScriptValue *out, void *data, void *args)
{
    last_profile()->sound.speech = Script_GetArgInt(args, 0);
    last_profile()->sound.speech_volume = Script_GetArgReal(args, 1);
    return ret_int(out, 0);
}
HOOK(0x403E60, GOSoundSpeech);

ScriptValue *GOSoundMusic(ScriptValue *out, void *data, void *args)
{
    last_profile()->sound.music = Script_GetArgInt(args, 0);
    last_profile()->sound.music_volume = Script_GetArgReal(args, 1);
    return ret_int(out, 0);
}
HOOK(0x403EF0, GOSoundMusic);

ScriptValue *GOSoundFX(ScriptValue *out, void *data, void *args)
{
    last_profile()->sound.fx = Script_GetArgInt(args, 0);
    last_profile()->sound.fx_volume = Script_GetArgReal(args, 1);
    return ret_int(out, 0);
}
HOOK(0x403F80, GOSoundFX);

ScriptValue *GOGfxDevice(ScriptValue *out, void *data, void *args)
{
    Script_GetArgString(args, 0, last_profile()->gfx.device, sizeof(last_profile()->gfx.device));
    return ret_int(out, 0);
}
HOOK(0x404010, GOGfxDevice);

ScriptValue *GOGfxGamma(ScriptValue *out, void *data, void *args)
{
    last_profile()->gfx.gamma = Script_GetArgReal(args, 0);
    return ret_int(out, 0);
}
HOOK(0x404080, GOGfxGamma);

ScriptValue *GOGfxPerformance(ScriptValue *out, void *data, void *args)
{
    last_profile()->gfx.perf_flags = Script_GetArgInt(args, 0);
    return ret_int(out, 0);
}
HOOK(0x4040F0, GOGfxPerformance);

/* the script passes 0 for an inverted mouse */
ScriptValue *Config_GameOptionsInputSetInvertMouse(ScriptValue *out, void *data, void *args)
{
    int invert = Script_GetArgReal(args, 0) == 0.0;

    Profile_GetActive()->invert_mouse = invert;
    return ret_int(out, 0);
}
HOOK(0x404160, Config_GameOptionsInputSetInvertMouse);

ScriptValue *Config_GameOptionsInputGetInvertMouse(ScriptValue *out)
{
    out->i = Profile_GetActive()->invert_mouse == 0;
    out->d = Profile_GetActive()->invert_mouse == 0;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4041D0, Config_GameOptionsInputGetInvertMouse);

ScriptValue *Config_GameOptionsInputSetMouseSensitivity(ScriptValue *out, void *data, void *args)
{
    float sens = Script_GetArgReal(args, 0);

    Profile_GetActive()->mouse_sensitivity = sens;
    return ret_int(out, 0);
}
HOOK(0x404230, Config_GameOptionsInputSetMouseSensitivity);

ScriptValue *Config_GameOptionsInputGetMouseSensitivity(ScriptValue *out)
{
    out->i = round_x87(Profile_GetActive()->mouse_sensitivity);
    out->d = Profile_GetActive()->mouse_sensitivity;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404290, Config_GameOptionsInputGetMouseSensitivity);

ScriptValue *Config_GameOptionsInputSetBloodEnabled(ScriptValue *out, void *data, void *args)
{
    char blood = round_x87(Script_GetArgReal(args, 0));

    Profile_GetActive()->blood = blood;
    return ret_int(out, 0);
}
HOOK(0x4042E0, Config_GameOptionsInputSetBloodEnabled);

ScriptValue *Config_GameOptionsInputGetBloodEnabled(ScriptValue *out)
{
    out->i = Profile_GetActive()->blood;
    out->d = Profile_GetActive()->blood;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404340, Config_GameOptionsInputGetBloodEnabled);

/* Called for each screen mode of the current display; adds modes of at
   least 640x480 to the device with the same name. */
void Config_AddDisplayMode(DisplayMode *mode)
{
    int i;

    if ((uintptr_t)mode == (uintptr_t)-offsetof(DisplayMode, device) || mode->width < 640 || mode->height < 480)
        return;
    for (i = 0; i < gDriverCount; i++) {
        ConfigDevice *d = &gDrivers[i];

        if (strcmp(mode->device, d->name) == 0) {
            d->modes[d->mode_count].width = mode->width;
            d->modes[d->mode_count].height = mode->height;
            d->modes[d->mode_count].depth = mode->depth;
            d->mode_count++;
        }
    }
}
HOOK(0x404390, Config_AddDisplayMode);

static int pack_mode(const ConfigDisplayMode *m)
{
    return ((unsigned)m->depth << 13 | m->width) << 13 | m->height;
}

ScriptValue *Config_FillScreenResolutionListBox(ScriptValue *out, void *data, void *args)
{
    ConfigDevice *d = &gDrivers[Script_GetArgInt(args, 0)];
    char text[256];
    int i;

    for (i = 0; i < d->mode_count; i++) {
        ConfigDisplayMode *m = &d->modes[i];

        crt_sprintf(text, "%dx%dx%d", m->width, m->height, m->depth);
        ListBox_AddItem(text, pack_mode(m));
    }
    return ret_int(out, 0);
}
HOOK(0x404450, Config_FillScreenResolutionListBox);

ScriptValue *Config_GraphicOptionsSetResolution(ScriptValue *out, void *data, void *args)
{
    int v = Script_GetArgInt(args, 0);

    Config_GetGraphicOptions()->display.depth = v >> 26 & 0x3F;
    Config_GetGraphicOptions()->display.width = v >> 13 & 0x1FFF;
    Config_GetGraphicOptions()->display.height = v & 0x1FFF;
    return ret_int(out, 0);
}
HOOK(0x404510, Config_GraphicOptionsSetResolution);

GraphicOptions *Config_GetGraphicOptions(void)
{
    return &gProfiles[gActiveProfile].gfx;
}
HOOK(0x404590, Config_GetGraphicOptions);

static unsigned absdiff(int a, int b)
{
    int d = a - b;

    return d < 0 ? -(unsigned)d : (unsigned)d;
}

/* Packs the device's mode closest to the current one. Crashes like the
   original when the device has no modes. */
ScriptValue *Config_GraphicOptionsGetResolution(ScriptValue *out, void *data, void *args)
{
    int dev = Script_GetArgInt(args, 0);
    int width = Config_GetGraphicOptions()->display.width;
    int height = Config_GetGraphicOptions()->display.height;
    int depth = Config_GetGraphicOptions()->display.depth;
    ConfigDevice *d = &gDrivers[dev];
    ConfigDisplayMode *best = NULL;
    int best_score = 0x7FFFFFFF;
    int i;

    for (i = 0; i < d->mode_count; i++) {
        ConfigDisplayMode *m = &d->modes[i];
        int score = (int)(absdiff(m->height, height) * absdiff(m->width, width)
                          + (absdiff(m->depth, depth) << 24));

        if (score < best_score) {
            best = m;
            best_score = score;
        }
    }
    return ret_int(out, pack_mode(best));
}
HOOK(0x4045B0, Config_GraphicOptionsGetResolution);

ScriptValue *Config_FillRenderDeviceListBox(ScriptValue *out)
{
    int i;

    for (i = 0; i < gDriverCount; i++)
        ListBox_AddItem(gDrivers[i].desc, i);
    return ret_int(out, 0);
}
HOOK(0x4046A0, Config_FillRenderDeviceListBox);

ScriptValue *Config_GraphicOptionsGetDevice(ScriptValue *out)
{
    int i;

    for (i = 0; i < gDriverCount; i++) {
        if (strcmp(Config_GetGraphicOptions()->device, gDrivers[i].name) == 0)
            return ret_int(out, i);
    }
    return ret_int(out, 0);
}
HOOK(0x404710, Config_GraphicOptionsGetDevice);

ScriptValue *Config_GraphicOptionsSetDevice(ScriptValue *out, void *data, void *args)
{
    int i = Script_GetArgInt(args, 0);

    strcpy(Config_GetGraphicOptions()->device_desc, gDrivers[i].desc);
    strcpy(Config_GetGraphicOptions()->device, gDrivers[i].name);
    return ret_int(out, 0);
}
HOOK(0x4047F0, Config_GraphicOptionsSetDevice);

ScriptValue *Config_GraphicOptionsSetGamma(ScriptValue *out, void *data, void *args)
{
    float gamma = Script_GetArgReal(args, 0);

    Config_GetGraphicOptions()->gamma = gamma;
    return ret_int(out, 0);
}
HOOK(0x4048B0, Config_GraphicOptionsSetGamma);

ScriptValue *Config_GraphicOptionsGetGamma(ScriptValue *out)
{
    out->i = round_x87(Config_GetGraphicOptions()->gamma);
    out->d = Config_GetGraphicOptions()->gamma;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404910, Config_GraphicOptionsGetGamma);

ScriptValue *Config_GraphicOptionsSetTransparency(ScriptValue *out, void *data, void *args)
{
    char v = Script_GetArgInt(args, 0);

    Config_GetGraphicOptions()->transparency = v;
    return ret_int(out, 0);
}
HOOK(0x404960, Config_GraphicOptionsSetTransparency);

ScriptValue *Config_GraphicOptionsGetTransparency(ScriptValue *out)
{
    out->i = Config_GetGraphicOptions()->transparency;
    out->d = Config_GetGraphicOptions()->transparency;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4049C0, Config_GraphicOptionsGetTransparency);

ScriptValue *Config_SoundOptionsSetSoundsEffects(ScriptValue *out, void *data, void *args)
{
    char v = Script_GetArgInt(args, 0);

    Config_GetSoundOptions()->fx = v;
    return ret_int(out, 0);
}
HOOK(0x404A10, Config_SoundOptionsSetSoundsEffects);

SoundOptions *Config_GetSoundOptions(void)
{
    return &gProfiles[gActiveProfile].sound;
}
HOOK(0x404A70, Config_GetSoundOptions);

ScriptValue *Config_SoundOptionsGetSoundsEffects(ScriptValue *out)
{
    out->i = Config_GetSoundOptions()->fx;
    out->d = Config_GetSoundOptions()->fx;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404A90, Config_SoundOptionsGetSoundsEffects);

ScriptValue *Config_SoundOptionsSetSoundsEffectsVolume(ScriptValue *out, void *data, void *args)
{
    float v = Script_GetArgReal(args, 0);

    Config_GetSoundOptions()->fx_volume = v;
    return ret_int(out, 0);
}
HOOK(0x404AE0, Config_SoundOptionsSetSoundsEffectsVolume);

ScriptValue *Config_SoundOptionsGetSoundsEffectsVolume(ScriptValue *out)
{
    out->i = round_x87(Config_GetSoundOptions()->fx_volume);
    out->d = Config_GetSoundOptions()->fx_volume;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404B40, Config_SoundOptionsGetSoundsEffectsVolume);

ScriptValue *Config_SoundOptionsSetMusic(ScriptValue *out, void *data, void *args)
{
    char v = Script_GetArgInt(args, 0);

    Config_GetSoundOptions()->music = v;
    return ret_int(out, 0);
}
HOOK(0x404B90, Config_SoundOptionsSetMusic);

ScriptValue *Config_SoundOptionsGetMusic(ScriptValue *out)
{
    out->i = Config_GetSoundOptions()->music;
    out->d = Config_GetSoundOptions()->music;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404BF0, Config_SoundOptionsGetMusic);

ScriptValue *Config_SoundOptionsSetMusicVolume(ScriptValue *out, void *data, void *args)
{
    float v = Script_GetArgReal(args, 0);

    Config_GetSoundOptions()->music_volume = v;
    return ret_int(out, 0);
}
HOOK(0x404C40, Config_SoundOptionsSetMusicVolume);

ScriptValue *Config_SoundOptionsGetMusicVolume(ScriptValue *out)
{
    out->i = round_x87(Config_GetSoundOptions()->music_volume);
    out->d = Config_GetSoundOptions()->music_volume;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404CA0, Config_SoundOptionsGetMusicVolume);

ScriptValue *Config_SoundOptionsSetSpeech(ScriptValue *out, void *data, void *args)
{
    char v = Script_GetArgInt(args, 0);

    Config_GetSoundOptions()->speech = v;
    return ret_int(out, 0);
}
HOOK(0x404CF0, Config_SoundOptionsSetSpeech);

ScriptValue *Config_SoundOptionsGetSpeech(ScriptValue *out)
{
    out->i = Config_GetSoundOptions()->speech;
    out->d = Config_GetSoundOptions()->speech;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404D50, Config_SoundOptionsGetSpeech);

/* sets the effects volume, like the getter below reads it */
ScriptValue *Config_SoundOptionsSetSpeechVolume(ScriptValue *out, void *data, void *args)
{
    float v = Script_GetArgReal(args, 0);

    Config_GetSoundOptions()->fx_volume = v;
    return ret_int(out, 0);
}
HOOK(0x404DA0, Config_SoundOptionsSetSpeechVolume);

ScriptValue *Config_SoundOptionsGetSpeechVolume(ScriptValue *out)
{
    out->i = round_x87(Config_GetSoundOptions()->fx_volume);
    out->d = Config_GetSoundOptions()->fx_volume;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404E00, Config_SoundOptionsGetSpeechVolume);

ScriptValue *Config_SoundOptionsSetReverseStereo(ScriptValue *out, void *data, void *args)
{
    char v = Script_GetArgInt(args, 0);

    Config_GetSoundOptions()->reverse_stereo = v;
    return ret_int(out, 0);
}
HOOK(0x404E50, Config_SoundOptionsSetReverseStereo);

ScriptValue *Config_SoundOptionsGetReverseStereo(ScriptValue *out)
{
    out->i = Config_GetSoundOptions()->reverse_stereo;
    out->d = Config_GetSoundOptions()->reverse_stereo;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404EB0, Config_SoundOptionsGetReverseStereo);

ScriptValue *Config_GraphicOptionsSetPerformanceFlags(ScriptValue *out, void *data, void *args)
{
    int v = Script_GetArgInt(args, 0);

    Config_GetGraphicOptions()->perf_flags = v;
    return ret_int(out, 1);
}
HOOK(0x404F00, Config_GraphicOptionsSetPerformanceFlags);

ScriptValue *Config_GraphicOptionsGetPerformanceFlags(ScriptValue *out)
{
    out->i = Config_GetGraphicOptions()->perf_flags;
    out->d = Config_GetGraphicOptions()->perf_flags;
    out->s = gEmptyString;
    return out;
}
HOOK(0x404F60, Config_GraphicOptionsGetPerformanceFlags);

ScriptValue *Config_FillPlayerProfileListBox(ScriptValue *out)
{
    int i;

    for (i = 0; i < gProfileCount; i++)
        ListBox_AddItem(gProfiles[i].name, i);
    return ret_int(out, 0);
}
HOOK(0x404FA0, Config_FillPlayerProfileListBox);

ScriptValue *Config_GetActivePlayerProfileIndex(ScriptValue *out)
{
    return ret_int(out, gActiveProfile);
}
HOOK(0x405010, Config_GetActivePlayerProfileIndex);

ScriptValue *Config_SetActivePlayerProfileIndex(ScriptValue *out, void *data, void *args)
{
    gActiveProfile = Script_GetArgInt(args, 0);
    return ret_int(out, 1);
}
HOOK(0x405050, Config_SetActivePlayerProfileIndex);

/* args: name, difficulty. The new profile copies the active one's settings. */
ScriptValue *Config_CreateNewPlayerProfile(ScriptValue *out, void *data, void *args)
{
    char name[0x20];
    int n, i;

    if (gProfileCount >= CONFIG_MAX_PROFILES)
        return ret_int(out, 0);
    Script_GetArgString(args, 0, name, sizeof(name));
    n = strlen(name);
    while (n > 0 && (name[n - 1] == ' ' || name[n - 1] == '\t'))
        n--;
    if (n == 0)
        return ret_int(out, 0);
    name[n] = 0;
    for (i = 0; i < gProfileCount; i++) {
        if (crt_stricmp(name, gProfiles[i].name) == 0)
            return ret_int(out, 0);
    }
    if (gProfileCount > 0) {
        PlayerProfile *p = &gProfiles[gProfileCount];

        memcpy(p, &gProfiles[gActiveProfile], offsetof(PlayerProfile, name));
        p->difficulty = Script_GetArgInt(args, 1);
        gActiveProfile = gProfileCount++;
        Script_GetArgString(args, 0, p->name, sizeof(p->name));
        p->active_mission = Mission_GetFirst();
    } else {
        Config_ResetProfiles();
        Script_GetArgString(args, 0, gProfiles[0].name, sizeof(gProfiles[0].name));
        gProfiles[0].difficulty = Script_GetArgInt(args, 1);
    }
    return ret_int(out, 1);
}
HOOK(0x4050A0, Config_CreateNewPlayerProfile);

/* Leaves a single default profile "Joe" as the active one. */
void Config_ResetProfiles(void)
{
    PlayerProfile *p = &gProfiles[0];
    ScriptValue unused;

    gActiveProfile = 0;
    memcpy(p->name, "Joe", 4);
    gProfileCount = 1;
    p->active_mission = Mission_GetFirst();
    p->difficulty = 1;
    if (crt_stricmp(AppContext_GetCountry(), "USA") == 0) {
        p->language = CONFIG_LANG_USA;
        p->blood = 1;
    } else {
        p->language = AppContext_IsGerman() != 0;
        p->blood = AppContext_IsGerman() == 0;
    }
    Config_ResetGraphicOptions(&unused);
    p->sound.music = 1;
    p->sound.reverse_stereo = 0;
    p->sound.fx = 1;
    p->sound.speech = 1;
    p->sound.music_volume = 1.0f;
    p->sound.fx_volume = 1.0f;
    p->sound.speech_volume = 1.0f;
    p->remap_count = NUM_DEFAULT_REMAPS;
    memcpy(p->remaps, gDefaultKeys, NUM_DEFAULT_REMAPS * sizeof(InputRemap));
    p->invert_mouse = 1;
    p->mouse_sensitivity = 0.4f;
}
HOOK(0x405300, Config_ResetProfiles);

ScriptValue *Config_DeletePlayerProfile(ScriptValue *out, void *data, void *args)
{
    int i = Script_GetArgInt(args, 0);
    volatile int *count = &gProfileCount; /* i = -1 copies a profile over it, as in the original */

    if (i >= gProfileCount)
        return ret_int(out, 0);
    if (gActiveProfile >= i)
        gActiveProfile--;
    for (i++; i < *count; i++)
        gProfiles[i - 1] = gProfiles[i];
    (*count)--;
    return ret_int(out, 1);
}
HOOK(0x405400, Config_DeletePlayerProfile);

ScriptValue *Config_GetNumberOfPlayerProfiles(ScriptValue *out)
{
    return ret_int(out, gProfileCount);
}
HOOK(0x4054F0, Config_GetNumberOfPlayerProfiles);

/* the level whose flags differ from the given ones in the fewest bits */
ScriptValue *Config_GraphicOptionsGetPerfLevelFromFlags(ScriptValue *out, void *data, void *args)
{
    int flags = Script_GetArgInt(args, 0);
    int best = 31, level = 0;
    int i;

    for (i = 0; i < NUM_PERF_LEVELS; i++) {
        int bits = __builtin_popcount(gPerfLevelFlags[i] ^ flags);

        if (bits < best) {
            best = bits;
            level = i;
        }
    }
    return ret_int(out, level);
}
HOOK(0x405530, Config_GraphicOptionsGetPerfLevelFromFlags);

ScriptValue *Config_GraphicOptionsGetPerfFlagsFromLevel(ScriptValue *out, void *data, void *args)
{
    out->i = gPerfLevelFlags[Script_GetArgInt(args, 0)];
    out->d = gPerfLevelFlags[Script_GetArgInt(args, 0)];
    out->s = gEmptyString;
    return out;
}
HOOK(0x4055C0, Config_GraphicOptionsGetPerfFlagsFromLevel);

ScriptValue *Config_GraphicOptionsNumPerfLevels(ScriptValue *out)
{
    return ret_int(out, NUM_PERF_LEVELS);
}
HOOK(0x405620, Config_GraphicOptionsNumPerfLevels);

/* missions up to and including the active profile's current one */
ScriptValue *Config_FillMissionSelectionBox(ScriptValue *out)
{
    int id = Mission_GetFirst();

    while (id != -1) {
        char *m = Mission_Find(id);

        ScrollListBox_AddItem(m + MISSION_NAME, id);
        if (id == Profile_GetActive()->active_mission)
            break;
        id = *(int *)(m + MISSION_NEXT);
    }
    return ret_int(out, 0);
}
HOOK(0x405660, Config_FillMissionSelectionBox);

ScriptValue *Config_FillMissionPictureBox(ScriptValue *out)
{
    int id = Mission_GetFirst();

    while (id != -1) {
        char *m = Mission_Find(id);

        PictureBox_AddItem(m + MISSION_PICTURE, id);
        id = *(int *)(m + MISSION_NEXT);
    }
    return ret_int(out, 0);
}
HOOK(0x4056F0, Config_FillMissionPictureBox);

ScriptValue *Config_FillMissionTextBox(ScriptValue *out)
{
    int id = Mission_GetFirst();

    while (id != -1) {
        char *m = Mission_Find(id);

        TextBox_AddItem(m + MISSION_TEXT, id);
        id = *(int *)(m + MISSION_NEXT);
    }
    return ret_int(out, 0);
}
HOOK(0x405760, Config_FillMissionTextBox);

ScriptValue *Config_PlayerGetActiveMission(ScriptValue *out, void *data, void *args)
{
    return ret_int(out, gProfiles[Script_GetArgInt(args, 0)].active_mission);
}
HOOK(0x4057D0, Config_PlayerGetActiveMission);

void Config_RemoveLanguageAlias(void)
{
    if (gLanguageAliasSet)
        Alias_Remove("LANGUAGE:");
}
HOOK(0x405830, Config_RemoveLanguageAlias);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 537B74, 4) int store_537B74[7] INIT({414, 430, 13, 96, 77, 65});
ALIAS(gPerfLevelFlags, store_537B74, 0);
STORE(igb, 567C74, 1) const char store_567C74[4];
ALIAS(gEmptyString, store_567C74, 0);
PAD(igb, 567C98, 567C98, 0x1418);
ALIAS(gDrivers, pad_567C98, 0);
BSS(BC2380) int gProfileCount;
BSS(BC2380) int gActiveProfile;
STORE(igb, BC2380, 4) __typeof__(gProfiles[0]) store_BC2388[16];
ALIAS(gProfiles, store_BC2388, 0);
STORE(igb, BC2380, 1) char store_BC79C8[32];
ALIAS(gContentControlPassword, store_BC79C8, 0);
