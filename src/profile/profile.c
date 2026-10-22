#include <string.h>

#include "hook.h"
#include "igi.h"
#include "profile/profile.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x405300, void, Config_SetDefaults, (void));
ORIG(0x417090, int, Mission_GetFirst, (void));
ORIG(0x485170, char *, Mission_Find, (int id));
ORIG(0x48F240, void, sub_48F240, (char on));
ORIG(0x48F260, void, sub_48F260, (char on));
ORIG(0x48F340, int, Config_GetQvmOnly, (void));
ORIG(0x495E70, void, Music_SetVolume, (float volume, float volume2));
ORIG(0x495F30, void, Music_SetSfxVolume, (float volume));
ORIG(0x499FD0, void, sub_499FD0, (int on));
ORIG(0x4B12F0, void, Alias_Set, (const char *name, const char *path));
ORIG(0x4B13A0, void, Alias_Remove, (const char *name));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B1700, int, File_Write, (void *f, const void *buf, int size));
ORIG(0x4B1AC0, void, File_Delete, (const char *path));
ORIG(0x4B2710, int, sub_4B2710, (void));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *s));
ORIG(0x4B8410, void, Script_Compile, (const char *path));
ORIG(0x4B85B0, void, Script_Run, (void *s));
ORIG(0x4D0EE0, void, sub_4D0EE0, (float v));
ORIG(0x4D2C20, void, sub_4D2C20, (int v));

#define gDriverCount VAR(int, 0x567C90)
#define gDrivers ((D3DDriver *)0x567C98)
#define gLanguageAliasSet VAR(char, 0x537B90)
#define gUnkA94E84 ((void (**)(int *))0xA94E84)

_Static_assert(sizeof(PlayerProfile) == 0x564, "PlayerProfile");
_Static_assert(sizeof(D3DDriver) == 0x404, "D3DDriver");

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
KeyBinding *Profile_GetKeys(void);
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
        Config_SetDefaults();
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
    KeyBinding *keys = Profile_GetKeys();
    KeyBinding *d;
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
    DisplayMode *m, *pick;
    PlayerProfile *p;

    for (i = 0; i < gProfileCount; i++) {
        p = &gProfiles[i];
        if (gDriverCount == 0) {
            Error_Show("No D3D drivers with hardware acceleration found.");
            for (;;)
                ;
        }
        for (j = 0; j < gDriverCount; j++)
            if (strcmp(p->gfxDevice, gDrivers[j].name) == 0)
                break;
        if (j == gDriverCount) {
            strcpy(p->gfxDriver, gDrivers[0].desc);
            strcpy(p->gfxDevice, gDrivers[0].name);
            j = 0;
        }
        pick = NULL;
        best = 0x7FFFFFFF;
        m = gDrivers[j].modes;
        for (n = gDrivers[j].modeCount; n > 0; n--, m++) {
            d = (int)(((unsigned)Abs(m->depth - p->display.depth) << 24) + Abs(m->height - p->display.height) +
                      Abs(m->width - p->display.width));
            if (d < best) {
                pick = m;
                best = d;
            }
        }
        p->display = *pick;
    }
}
HOOK(0x405980, Config_FixDisplayModes);

void Config_Apply(void)
{
    char path[256];
    float music;

    if (gLanguageAliasSet)
        Alias_Remove("LANGUAGE:");
    crt_sprintf(path, "LOCAL:language/%s/", gLanguageNames[gProfiles[gProfileActive].language]);
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
    if (Config_GetQvmOnly()) {
        Script_Compile(path);
        File_Delete(path);
    }
}
HOOK(0x405BD0, Config_Write);

void Config_WriteStart(void *f)
{
    char buf[256];

    File_Write(f, buf, crt_sprintf(buf, "%s(%d);\n", "GOStart", gProfileActive));
}
HOOK(0x405C50, Config_WriteStart);

void Config_WritePlayer(void *f, int i)
{
    char buf[256];

    File_Write(f, buf, crt_sprintf(buf, "%s(\"%s\");\n", "GOPlayer", gProfiles[i].name));
    File_Write(f, buf, crt_sprintf(buf, "%s(%d);\n", "GOActiveMission", gProfiles[i].activeMission));
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

    n = crt_sprintf(buf, "%s(%d, %f);\n", "GOSoundSpeech", p->speechOn, (double)p->speechVolume);
    n += crt_sprintf(buf + n, "%s(%d, %f);\n", "GOSoundMusic", p->musicOn, (double)p->musicVolume);
    n += crt_sprintf(buf + n, "%s(%d, %f);\n", "GOSoundFX", p->sfxOn, (double)p->sfxVolume);
    File_Write(f, buf, n);
}
HOOK(0x405E00, Config_WriteSound);

void Config_WriteGfx(void *f, int i)
{
    PlayerProfile *p = &gProfiles[i];
    char buf[512];
    int n;

    n = crt_sprintf(buf, "%s(%d, %d, %d);\n", "GOGfxDisp", p->display.width, p->display.height, p->display.depth);
    n += crt_sprintf(buf + n, "%s(\"%s\");\n", "GOGfxDevice", p->gfxDevice);
    n += crt_sprintf(buf + n, "%s(%f);\n", "GOGfxGamma", (double)p->gamma);
    n += crt_sprintf(buf + n, "%s(%d);\n", "GOGfxPerformance", p->performance);
    File_Write(f, buf, n);
}
HOOK(0x405EC0, Config_WriteGfx);

/* names come from the tables, unknown values are written as numbers */
void Config_WriteInput(void *f, int i)
{
    PlayerProfile *p = &gProfiles[i];
    KeyBinding *k = p->keys;
    char buf[256];
    int n, j, b;

    File_Write(f, buf, crt_sprintf(buf, "%s(%d);\n", "GOInMouInv", p->mouseInvert));
    File_Write(f, buf, crt_sprintf(buf, "%s(%f);\n", "GOInMouSens", (double)p->mouseSensitivity));
    for (b = 0; b < p->keyCount; b++, k++) {
        n = crt_sprintf(buf, "%s(", "GOInRemap");
        for (j = 0; j < CONFIG_ACTION_COUNT; j++)
            if (gConfigActions[j].value == k->action)
                break;
        if (j == CONFIG_ACTION_COUNT)
            n += crt_sprintf(buf + n, "%d,", k->action);
        else
            n += crt_sprintf(buf + n, "%s,", gConfigActions[j].name);
        for (j = 0; j < CONFIG_DEVICE_COUNT; j++)
            if (gConfigDevices[j].value == k->device)
                break;
        if (j == CONFIG_DEVICE_COUNT)
            n += crt_sprintf(buf + n, "%d,", k->device);
        else
            n += crt_sprintf(buf + n, "%s,", gConfigDevices[j].name);
        for (j = 0; j < CONFIG_KEY_COUNT; j++)
            if (gConfigKeys[j].value == (k->device << 30 | k->key))
                break;
        if (j == CONFIG_KEY_COUNT)
            n += crt_sprintf(buf + n, "%d);\n", k->key);
        else
            n += crt_sprintf(buf + n, "%s);\n", gConfigKeys[j].name);
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

KeyBinding *Config_GetDefaultKeys(void)
{
    return gDefaultKeys;
}
HOOK(0x4061B0, Config_GetDefaultKeys);

KeyBinding *Profile_GetKeys(void)
{
    return gProfiles[gProfileActive].keys;
}
HOOK(0x4061C0, Profile_GetKeys);

int Profile_GetKeyCount(void)
{
    return gProfiles[gProfileActive].keyCount;
}
HOOK(0x4061E0, Profile_GetKeyCount);

void Profile_SetKeyCount(int count)
{
    gProfiles[gProfileActive].keyCount = count;
}
HOOK(0x406200, Profile_SetKeyCount);

PlayerProfile *Profile_GetActive(void)
{
    return &gProfiles[gProfileActive];
}
HOOK(0x406220, Profile_GetActive);

void Config_GetKey(int i, int *value, const char **name)
{
    if (name)
        *name = gConfigKeys[i].name;
    if (value)
        *value = gConfigKeys[i].value;
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
        *name = gConfigActions[i].name;
    if (value)
        *value = gConfigActions[i].value;
}
HOOK(0x406280, Config_GetAction);

int Config_GetActionCount(void)
{
    return CONFIG_ACTION_COUNT;
}
HOOK(0x4062B0, Config_GetActionCount);

float Profile_GetSfxVolume(void)
{
    return Profile_GetActive()->sfxVolume;
}
HOOK(0x4062C0, Profile_GetSfxVolume);

float Profile_GetMusicVolume(void)
{
    return Profile_GetActive()->musicVolume;
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
        if (id == Profile_GetActive()->activeMission)
            Profile_GetActive()->activeMission = mission;
        id = *(int *)(m + 0x4A4);
    }
}
HOOK(0x406340, Profile_AdvanceMission);

void Profile_ApplyPerformance(void)
{
    unsigned flags = Profile_GetActive()->performance;
    int v = flags & 1 ? 1 : flags & 2;

    gUnkA94E84[sub_4B2710()](&v);
    sub_48F260(~flags >> 2 & 1);
    sub_48F240(~flags >> 3 & 1);
    sub_4D2C20(flags & 0x10 ? 0x60 : flags & 0x20 ? 0x81 : 0xA3);
    sub_4D0EE0(flags & 0x80 ? 2.0f : flags & 0x40 ? 1.5f : 1.0f);
    sub_499FD0(flags & 0x100 ? 0 : 1);
}
HOOK(0x406390, Profile_ApplyPerformance);
