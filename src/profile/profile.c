#include <string.h>

#include "hook.h"
#include "igi.h"
#include "profile/profile.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x405300, void, Config_ResetProfiles, (void));
ORIG(0x417090, int, Mission_GetFirst, (void));
ORIG(0x485170, char *, Mission_Find, (int id));
ORIG(0x48F240, void, AppContext_SetLightmapsUsed, (char on));
ORIG(0x48F260, void, AppContext_SetTerrainLightmapsUsed, (char on));
ORIG(0x48F340, int, AppContext_GetQvmOnly, (void));
ORIG(0x495E70, void, Music_SetVolume, (float volume, float volume2));
ORIG(0x495F30, void, Music_SetSfxVolume, (float volume));
ORIG(0x499FD0, void, Rain_Enable, (int on));
ORIG(0x4B12F0, void, Alias_Set, (const char *name, const char *path));
ORIG(0x4B13A0, void, Alias_Remove, (const char *name));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B1700, int, File_Write, (void *f, const void *buf, int size));
ORIG(0x4B1AC0, void, File_Delete, (const char *path));
ORIG(0x4B2710, int, Texture_GetSetLODId, (void));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *s));
ORIG(0x4B8410, void, Script_Compile, (const char *path));
ORIG(0x4B85B0, void, Script_Run, (void *s));
ORIG(0x4D0EE0, void, Mesh3D_SetLodScale, (float v));
ORIG(0x4D2C20, void, QCube_SetDetail, (int v));

extern int gDriverCount;
extern ConfigDevice gDrivers[];
extern char gLanguageAliasSet;
extern void (*gRenderFuncs[])(int *);

void Config_AddMissingKeys(void);
void Config_FixDisplayModes(void);
void Config_Apply(void);
void Config_WriteStart(void *f);
void Config_WritePlayer(void *f, int i);
void Config_WriteGame(void *f, int i);
void Config_WriteSound(void *f, int i);
void Config_WriteGfx(void *f, int i);
void Config_WriteInput(void *f, int i);
void Config_WriteContentControl(void *f);
InputRemap *Profile_GetKeys(void);
int Profile_GetKeyCount(void);
void Profile_SetKeyCount(int count);
void Config_GetAction(int i, int *value, const char **name);
int Config_GetActionCount(void);

void Config_Read(const char *path)
{
    void *s = Script_Load(path);

    if (s) {
        Script_Run(s);
        Script_Free(s);
    } else {
        Config_ResetProfiles();
    }
    Config_AddMissingKeys();
    Config_FixDisplayModes();
    Config_Apply();
}
HOOK(0x405850, Config_Read);

/* gives every action without a binding its default one */
void Config_AddMissingKeys(void)
{
    int actions = Config_GetActionCount();
    int count = Profile_GetKeyCount();
    InputRemap *keys = Profile_GetKeys();
    InputRemap *d;
    int i, j, action;

    for (i = 0; i < actions; i++) {
        Config_GetAction(i, &action, NULL);
        for (j = 0; j < count; j++)
            if (keys[j].action == action)
                break;
        if (j < count)
            continue;
        for (d = gDefaultKeys; d < gDefaultKeys + CONFIG_ACTION_COUNT; d++) {
            if (d->action == action) {
                keys[count++] = *d;
                break;
            }
        }
    }
    Profile_SetKeyCount(count);
}
HOOK(0x4058A0, Config_AddMissingKeys);

static int Abs(int v)
{
    unsigned m = (unsigned)(v >> 31);

    return (int)(((unsigned)v ^ m) - m);
}

/* picks a driver and display mode that exist, closest to the configured one */
void Config_FixDisplayModes(void)
{
    int i, j, n, best, d;
    ConfigDisplayMode *m, *pick;
    PlayerProfile *p;

    for (i = 0; i < gProfileCount; i++) {
        p = &gProfiles[i];
        if (gDriverCount == 0) {
            Error_Show("No D3D drivers with hardware acceleration found.");
            for (;;)
                ;
        }
        for (j = 0; j < gDriverCount; j++)
            if (strcmp(p->gfx.device, gDrivers[j].name) == 0)
                break;
        if (j == gDriverCount) {
            strcpy(p->gfx.device_desc, gDrivers[0].desc);
            strcpy(p->gfx.device, gDrivers[0].name);
            j = 0;
        }
        pick = NULL;
        best = 0x7FFFFFFF;
        m = gDrivers[j].modes;
        for (n = gDrivers[j].mode_count; n > 0; n--, m++) {
            d = (int)(((unsigned)Abs(m->depth - p->gfx.display.depth) << 24) + Abs(m->height - p->gfx.display.height) +
                      Abs(m->width - p->gfx.display.width));
            if (d < best) {
                pick = m;
                best = d;
            }
        }
        p->gfx.display = *pick;
    }
}
HOOK(0x405980, Config_FixDisplayModes);

void Config_Apply(void)
{
    char path[256];
    float music;

    if (gLanguageAliasSet)
        Alias_Remove("LANGUAGE:");
    crt_sprintf(path, "LOCAL:language/%s/", gLanguageNames[gProfiles[gActiveProfile].language]);
    Alias_Set("LANGUAGE:", path);
    gLanguageAliasSet = 1;
    music = Profile_GetMusicVolume();
    Music_SetVolume(Profile_GetMusicVolume(), music);
    Music_SetSfxVolume(Profile_GetSfxVolume());
}
HOOK(0x405B30, Config_Apply);

void Config_Write(const char *path)
{
    void *f = File_Open(path, "w");
    int i;

    if (!f) {
        Error_Show("Could not create config file %s.", path);
        for (;;)
            ;
    }
    Config_WriteStart(f);
    Config_WriteContentControl(f);
    for (i = 0; i < gProfileCount; i++)
        Config_WritePlayer(f, i);
    File_Close(f);
    if (AppContext_GetQvmOnly()) {
        Script_Compile(path);
        File_Delete(path);
    }
}
HOOK(0x405BD0, Config_Write);

void Config_WriteStart(void *f)
{
    char buf[256];

    File_Write(f, buf, crt_sprintf(buf, "%s(%d);\n", "GOStart", gActiveProfile));
}
HOOK(0x405C50, Config_WriteStart);

void Config_WritePlayer(void *f, int i)
{
    char buf[256];

    File_Write(f, buf, crt_sprintf(buf, "%s(\"%s\");\n", "GOPlayer", gProfiles[i].name));
    File_Write(f, buf, crt_sprintf(buf, "%s(%d);\n", "GOActiveMission", gProfiles[i].active_mission));
    Config_WriteInput(f, i);
    Config_WriteGfx(f, i);
    Config_WriteGame(f, i);
    Config_WriteSound(f, i);
}
HOOK(0x405C90, Config_WritePlayer);

void Config_WriteGame(void *f, int i)
{
    PlayerProfile *p = &gProfiles[i];
    char buf[256];
    int n;

    n = crt_sprintf(buf, "%s(%s);\n", "GOGameLang", gLanguageNames[p->language]);
    n += crt_sprintf(buf + n, "%s(%s);\n", "GOGameDiff", gDifficultyNames[p->difficulty]);
    n += crt_sprintf(buf + n, "%s(%s);\n", "GOIsBlood", p->blood ? "TRUE" : "FALSE");
    File_Write(f, buf, n);
}
HOOK(0x405D40, Config_WriteGame);

void Config_WriteSound(void *f, int i)
{
    PlayerProfile *p = &gProfiles[i];
    char buf[1024];
    int n;

    n = crt_sprintf(buf, "%s(%d, %f);\n", "GOSoundSpeech", p->sound.speech, (double)p->sound.speech_volume);
    n += crt_sprintf(buf + n, "%s(%d, %f);\n", "GOSoundMusic", p->sound.music, (double)p->sound.music_volume);
    n += crt_sprintf(buf + n, "%s(%d, %f);\n", "GOSoundFX", p->sound.fx, (double)p->sound.fx_volume);
    File_Write(f, buf, n);
}
HOOK(0x405E00, Config_WriteSound);

void Config_WriteGfx(void *f, int i)
{
    PlayerProfile *p = &gProfiles[i];
    char buf[512];
    int n;

    n = crt_sprintf(buf, "%s(%d, %d, %d);\n", "GOGfxDisp", p->gfx.display.width, p->gfx.display.height,
                    p->gfx.display.depth);
    n += crt_sprintf(buf + n, "%s(\"%s\");\n", "GOGfxDevice", p->gfx.device);
    n += crt_sprintf(buf + n, "%s(%f);\n", "GOGfxGamma", (double)p->gfx.gamma);
    n += crt_sprintf(buf + n, "%s(%d);\n", "GOGfxPerformance", p->gfx.perf_flags);
    File_Write(f, buf, n);
}
HOOK(0x405EC0, Config_WriteGfx);

/* names come from the tables, unknown values are written as numbers */
void Config_WriteInput(void *f, int i)
{
    PlayerProfile *p = &gProfiles[i];
    InputRemap *k = p->remaps;
    char buf[256];
    int n, j, b;

    File_Write(f, buf, crt_sprintf(buf, "%s(%d);\n", "GOInMouInv", p->invert_mouse));
    File_Write(f, buf, crt_sprintf(buf, "%s(%f);\n", "GOInMouSens", (double)p->mouse_sensitivity));
    for (b = 0; b < p->remap_count; b++, k++) {
        n = crt_sprintf(buf, "%s(", "GOInRemap");
        for (j = 0; j < CONFIG_ACTION_COUNT; j++)
            if (gInputActions[j].value == k->action)
                break;
        if (j == CONFIG_ACTION_COUNT)
            n += crt_sprintf(buf + n, "%d,", k->action);
        else
            n += crt_sprintf(buf + n, "%s,", gInputActions[j].name);
        for (j = 0; j < CONFIG_DEVICE_COUNT; j++)
            if (gInputDevices[j].value == k->device)
                break;
        if (j == CONFIG_DEVICE_COUNT)
            n += crt_sprintf(buf + n, "%d,", k->device);
        else
            n += crt_sprintf(buf + n, "%s,", gInputDevices[j].name);
        for (j = 0; j < CONFIG_KEY_COUNT; j++)
            if (gInputKeys[j].value == (k->device << 30 | k->key))
                break;
        if (j == CONFIG_KEY_COUNT)
            n += crt_sprintf(buf + n, "%d);\n", k->key);
        else
            n += crt_sprintf(buf + n, "%s);\n", gInputKeys[j].name);
        File_Write(f, buf, n);
    }
}
HOOK(0x405F90, Config_WriteInput);

void Config_WriteContentControl(void *f)
{
    char buf[256];

    File_Write(f, buf, crt_sprintf(buf, "%s(\"%s\");\n", "GOContentControlPW", gContentControlPassword));
}
HOOK(0x406170, Config_WriteContentControl);

InputRemap *Config_GetDefaultKeys(void)
{
    return gDefaultKeys;
}
HOOK(0x4061B0, Config_GetDefaultKeys);

InputRemap *Profile_GetKeys(void)
{
    return gProfiles[gActiveProfile].remaps;
}
HOOK(0x4061C0, Profile_GetKeys);

int Profile_GetKeyCount(void)
{
    return gProfiles[gActiveProfile].remap_count;
}
HOOK(0x4061E0, Profile_GetKeyCount);

void Profile_SetKeyCount(int count)
{
    gProfiles[gActiveProfile].remap_count = count;
}
HOOK(0x406200, Profile_SetKeyCount);

PlayerProfile *Profile_GetActive(void)
{
    return &gProfiles[gActiveProfile];
}
HOOK(0x406220, Profile_GetActive);

void Config_GetKey(int i, int *value, const char **name)
{
    if (name)
        *name = gInputKeys[i].name;
    if (value)
        *value = gInputKeys[i].value;
}
HOOK(0x406240, Config_GetKey);

int Config_GetKeyCount(void)
{
    return CONFIG_KEY_COUNT;
}
HOOK(0x406270, Config_GetKeyCount);

void Config_GetAction(int i, int *value, const char **name)
{
    if (name)
        *name = gInputActions[i].name;
    if (value)
        *value = gInputActions[i].value;
}
HOOK(0x406280, Config_GetAction);

int Config_GetActionCount(void)
{
    return CONFIG_ACTION_COUNT;
}
HOOK(0x4062B0, Config_GetActionCount);

float Profile_GetSfxVolume(void)
{
    return Profile_GetActive()->sound.fx_volume;
}
HOOK(0x4062C0, Profile_GetSfxVolume);

float Profile_GetMusicVolume(void)
{
    return Profile_GetActive()->sound.music_volume;
}
HOOK(0x4062D0, Profile_GetMusicVolume);

int Profile_GetLanguage(void)
{
    return Profile_GetActive()->language;
}
HOOK(0x4062E0, Profile_GetLanguage);

void Profile_SetLanguage(int language)
{
    Profile_GetActive()->language = language;
    Config_Apply();
}
HOOK(0x4062F0, Profile_SetLanguage);

int Config_GetLanguageCount(void)
{
    return CONFIG_LANGUAGE_COUNT;
}
HOOK(0x406310, Config_GetLanguageCount);

const char *Config_GetLanguageName(int language)
{
    return gLanguageNames[language];
}
HOOK(0x406320, Config_GetLanguageName);

const char *Config_GetDifficultyName(int difficulty)
{
    return gDifficultyNames[difficulty];
}
HOOK(0x406330, Config_GetDifficultyName);

/* moves the active mission on to `mission` if it is earlier in the chain */
void Profile_AdvanceMission(int mission)
{
    int id = Mission_GetFirst();
    char *m;

    while (id != -1 && id != mission) {
        m = Mission_Find(id);
        if (id == Profile_GetActive()->active_mission)
            Profile_GetActive()->active_mission = mission;
        id = *(int *)(m + 0x4A4);
    }
}
HOOK(0x406340, Profile_AdvanceMission);

void Profile_ApplyPerformance(void)
{
    unsigned flags = Profile_GetActive()->gfx.perf_flags;
    int v = flags & 1 ? 1 : flags & 2;

    gRenderFuncs[Texture_GetSetLODId()](&v);
    AppContext_SetTerrainLightmapsUsed(~flags >> 2 & 1);
    AppContext_SetLightmapsUsed(~flags >> 3 & 1);
    QCube_SetDetail(flags & 0x10 ? 0x60 : flags & 0x20 ? 0x81 : 0xA3);
    Mesh3D_SetLodScale(flags & 0x80 ? 2.0f : flags & 0x40 ? 1.5f : 1.0f);
    Rain_Enable(flags & 0x100 ? 0 : 1);
}
HOOK(0x406390, Profile_ApplyPerformance);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 53331C, 1) char store_53331C[6][16] INIT({"ENGLISH", "GERMAN", "FRENCH", "ITALIAN", "SPANISH", "USA"});
ALIAS(gLanguageNames, store_53331C, 0);
STORE(igd, 53331C, 1) char store_53337C[3][16] INIT({"GD_1", "GD_2", "GD_3"});
ALIAS(gDifficultyNames, store_53337C, 0);
PAD(igd, 53331C, 5333AC, 0x4);
PAD(igd, 53554C, 53554C, 0x2) INIT({0x6F, 0x77});
DATA(53554C) char str_53554E[1] INIT("");
PAD(igd, 53554C, 53554F, 0x1);
STORE(igd, 536410, 4) __typeof__(gDefaultKeys[0]) store_536410[27] INIT({{.action = 1, .device = 1, .key = 203}, {.action = 2, .device = 1, .key = 205}, {.action = 4, .device = 1, .key = 200}, {.action = 8, .device = 1, .key = 208}, {.action = 256, .device = 3, .key = 1}, {.action = 512, .device = 1, .key = 28}, {.action = 1024, .device = 1, .key = 54}, {.action = 2048, .device = 1, .key = 157}, {.action = 4096, .device = 1, .key = 82}, {.action = 8192, .device = 3, .key = 2}, {.action = 524288, .device = 1, .key = 24}, {.action = 16384, .device = 1, .key = 14}, {.action = 32768, .device = 1, .key = 79}, {.action = 65536, .device = 1, .key = 57}, {.action = 131072, .device = 1, .key = 46}, {.action = 262144, .device = 1, .key = 17}, {.action = 1048576, .device = 1, .key = 201}, {.action = 2097152, .device = 1, .key = 209}, {.action = 4194304, .device = 1, .key = 2}, {.action = 8388608, .device = 1, .key = 3}, {.action = 16777216, .device = 1, .key = 4}, {.action = 33554432, .device = 1, .key = 5}, {.action = 67108864, .device = 1, .key = 6}, {.action = 134217728, .device = 1, .key = 7}, {.action = 268435456, .device = 1, .key = 8}, {.action = 536870912, .device = 1, .key = 9}, {.action = 1073741824, .device = 1, .key = 10}});
ALIAS(gDefaultKeys, store_536410, 0);
PAD(igd, 536410, 536554, 0x4);
STORE(igd, 536410, 4) __typeof__(gInputActions[0]) store_536558[27] INIT({{.name = "MoveLeft", .value = 1}, {.name = "MoveRight", .value = 2}, {.name = "MoveUp", .value = 4}, {.name = "MoveDown", .value = 8}, {.name = "Fire", .value = 256}, {.name = "AlternateFire", .value = 16384}, {.name = "Reload", .value = 512}, {.name = "Activate", .value = 1024}, {.name = "Crouch", .value = 2048}, {.name = "Jump", .value = 4096}, {.name = "NextWeapon", .value = 8192}, {.name = "PrevWeapon", .value = 524288}, {.name = "Peek", .value = 32768}, {.name = "Binoculars", .value = 65536}, {.name = "MapComputer", .value = 131072}, {.name = "ToggleWalkRun", .value = 262144}, {.name = "ZoomIn", .value = 1048576}, {.name = "ZoomOut", .value = 2097152}, {.name = "WeaponCategory1", .value = 4194304}, {.name = "WeaponCategory2", .value = 8388608}, {.name = "WeaponCategory3", .value = 16777216}, {.name = "WeaponCategory4", .value = 33554432}, {.name = "WeaponCategory5", .value = 67108864}, {.name = "WeaponCategory6", .value = 134217728}, {.name = "WeaponCategory7", .value = 268435456}, {.name = "WeaponCategory8", .value = 536870912}, {.name = "WeaponCategory9", .value = 1073741824}});
ALIAS(gInputActions, store_536558, 0);
STORE(igd, 536928, 4) __typeof__(gInputDevices[0]) store_536928[3] INIT({{.name = "INPUTPORT_DEVICE_KEYBOARD", .value = 1}, {.name = "INPUTPORT_DEVICE_JOYPAD", .value = 2}, {.name = "INPUTPORT_DEVICE_MOUSE", .value = 3}});
ALIAS(gInputDevices, store_536928, 0);
STORE(igd, 536998, 4) __typeof__(gInputKeys[0]) store_536998[127] INIT({{.name = "KEY_ANGLE", .value = 1073741910}, {.name = "KEY_ESC", .value = 1073741825}, {.name = "KEY_A", .value = 1073741854}, {.name = "KEY_B", .value = 1073741872}, {.name = "KEY_C", .value = 1073741870}, {.name = "KEY_D", .value = 1073741856}, {.name = "KEY_E", .value = 1073741842}, {.name = "KEY_F", .value = 1073741857}, {.name = "KEY_G", .value = 1073741858}, {.name = "KEY_H", .value = 1073741859}, {.name = "KEY_I", .value = 1073741847}, {.name = "KEY_J", .value = 1073741860}, {.name = "KEY_K", .value = 1073741861}, {.name = "KEY_L", .value = 1073741862}, {.name = "KEY_M", .value = 1073741874}, {.name = "KEY_N", .value = 1073741873}, {.name = "KEY_O", .value = 1073741848}, {.name = "KEY_P", .value = 1073741849}, {.name = "KEY_Q", .value = 1073741840}, {.name = "KEY_R", .value = 1073741843}, {.name = "KEY_S", .value = 1073741855}, {.name = "KEY_T", .value = 1073741844}, {.name = "KEY_U", .value = 1073741846}, {.name = "KEY_V", .value = 1073741871}, {.name = "KEY_W", .value = 1073741841}, {.name = "KEY_X", .value = 1073741869}, {.name = "KEY_Y", .value = 1073741845}, {.name = "KEY_Z", .value = 1073741868}, {.name = "KEY_0", .value = 1073741835}, {.name = "KEY_1", .value = 1073741826}, {.name = "KEY_2", .value = 1073741827}, {.name = "KEY_3", .value = 1073741828}, {.name = "KEY_4", .value = 1073741829}, {.name = "KEY_5", .value = 1073741830}, {.name = "KEY_6", .value = 1073741831}, {.name = "KEY_7", .value = 1073741832}, {.name = "KEY_8", .value = 1073741833}, {.name = "KEY_9", .value = 1073741834}, {.name = "KEY_SEPARATOR", .value = 1073741876}, {.name = "KEY_DECIMAL", .value = 1073741907}, {.name = "KEY_COMMA", .value = 1073741875}, {.name = "KEY_COLON", .value = 1073741970}, {.name = "KEY_SLASH", .value = 1073741877}, {.name = "KEY_CAPITAL", .value = 1073741882}, {.name = "KEY_BACKSLASH", .value = 1073741867}, {.name = "KEY_BACKSPACE", .value = 1073741838}, {.name = "KEY_UNDERLINE", .value = 1073741971}, {.name = "KEY_CAPSLOCK", .value = 1073741882}, {.name = "KEY_NUMLOCK", .value = 1073741893}, {.name = "KEY_SCROLL", .value = 1073741894}, {.name = "KEY_TAB", .value = 1073741839}, {.name = "KEY_RETURN", .value = 1073741852}, {.name = "KEY_LEFT_SHIFT", .value = 1073741866}, {.name = "KEY_RIGHT_SHIFT", .value = 1073741878}, {.name = "KEY_LEFT_CTRL", .value = 1073741853}, {.name = "KEY_RIGHT_CTRL", .value = 1073741981}, {.name = "KEY_LEFT_ALT", .value = 1073741880}, {.name = "KEY_RIGHT_ALT", .value = 1073742008}, {.name = "KEY_DIVIDE", .value = 1073742005}, {.name = "KEY_MULTIPLY", .value = 1073741879}, {.name = "KEY_SUBTRACT", .value = 1073741898}, {.name = "KEY_ADD", .value = 1073741902}, {.name = "KEY_MINUS", .value = 1073741836}, {.name = "KEY_EQUALS", .value = 1073741837}, {.name = "KEY_LEFT_BRACKET", .value = 1073741850}, {.name = "KEY_RIGHT_BRACKET", .value = 1073741851}, {.name = "KEY_SEMICOLON", .value = 1073741863}, {.name = "KEY_APOSTROPHE", .value = 1073741864}, {.name = "KEY_GRAVE", .value = 1073741865}, {.name = "KEY_LEFT_MENU", .value = 1073741880}, {.name = "KEY_RIGHT_MENU", .value = 1073742008}, {.name = "KEY_LEFT_WIN", .value = 1073742043}, {.name = "KEY_RIGHT_WIN", .value = 1073742044}, {.name = "KEY_SPACE", .value = 1073741881}, {.name = "KEY_END", .value = 1073742031}, {.name = "KEY_HOME", .value = 1073742023}, {.name = "KEY_LEFT", .value = 1073742027}, {.name = "KEY_UP", .value = 1073742024}, {.name = "KEY_RIGHT", .value = 1073742029}, {.name = "KEY_DOWN", .value = 1073742032}, {.name = "KEY_INSERT", .value = 1073742034}, {.name = "KEY_DELETE", .value = 1073742035}, {.name = "KEY_PAGEUP", .value = 1073742025}, {.name = "KEY_PAGEDOWN", .value = 1073742033}, {.name = "KEY_F1", .value = 1073741883}, {.name = "KEY_F2", .value = 1073741884}, {.name = "KEY_F3", .value = 1073741885}, {.name = "KEY_F4", .value = 1073741886}, {.name = "KEY_F5", .value = 1073741887}, {.name = "KEY_F6", .value = 1073741888}, {.name = "KEY_F7", .value = 1073741889}, {.name = "KEY_F8", .value = 1073741890}, {.name = "KEY_F9", .value = 1073741891}, {.name = "KEY_F10", .value = 1073741892}, {.name = "KEY_F11", .value = 1073741911}, {.name = "KEY_F12", .value = 1073741912}, {.name = "KEY_F13", .value = 1073741924}, {.name = "KEY_F14", .value = 1073741925}, {.name = "KEY_F15", .value = 1073741926}, {.name = "KEY_NUMPAD0", .value = 1073741906}, {.name = "KEY_NUMPAD1", .value = 1073741903}, {.name = "KEY_NUMPAD2", .value = 1073741904}, {.name = "KEY_NUMPAD3", .value = 1073741905}, {.name = "KEY_NUMPAD4", .value = 1073741899}, {.name = "KEY_NUMPAD5", .value = 1073741900}, {.name = "KEY_NUMPAD6", .value = 1073741901}, {.name = "KEY_NUMPAD7", .value = 1073741895}, {.name = "KEY_NUMPAD8", .value = 1073741896}, {.name = "KEY_NUMPAD9", .value = 1073741897}, {.name = "KEY_NUMPADENTER", .value = 1073741980}, {.name = "KEY_NUMPADCOMMA", .value = 1073742003}, {.name = "KEY_NUMPADEQUALS", .value = 1073741965}, {.name = "KEY_NUMPADSTAR", .value = 1073741879}, {.name = "KEY_NUMPADMINUS", .value = 1073741898}, {.name = "KEY_NUMPADPLUS", .value = 1073741902}, {.name = "KEY_NUMPADPERIOD", .value = 1073741907}, {.name = "KEY_NUMPADSLASH", .value = 1073742005}, {.name = "MOUSE_BUTTON_1", .value = -1073741823}, {.name = "MOUSE_BUTTON_2", .value = -1073741822}, {.name = "MOUSE_BUTTON_3", .value = -1073741820}, {.name = "MOUSE_BUTTON_4", .value = -1073741816}, {.name = "MOUSE_BUTTON_5", .value = -1073741808}, {.name = "MOUSE_BUTTON_6", .value = -1073741792}, {.name = "MOUSE_BUTTON_7", .value = -1073741760}, {.name = "MOUSE_BUTTON_8", .value = -1073741696}, {.name = "MOUSE_WHEEL_UP", .value = -1073741568}, {.name = "MOUSE_WHEEL_DOWN", .value = -1073741312}});
ALIAS(gInputKeys, store_536998, 0);
DATA(537B90) char gLanguageAliasSet INIT(1);
PAD(igd, 537B90, 537B91, 0x3);
BSS(567C90) int gDriverCount;
