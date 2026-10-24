#ifndef CONFIG_H
#define CONFIG_H

#include "igi.h"
#include "script/scripttypes.h"
#include "config/configtypes.h"

/* A screen mode as the display driver lists it. */
typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
    int unkC;
    int depth;
    int unk14;
    int unk18;
    char device[0x100];
} DisplayMode;

/* name/value pairs registered as script constants */
typedef struct ConfigConstant {
    char name[0x20];
    int value;
} ConfigConstant;

#define CONFIG_MAX_PROFILES 16
#define CONFIG_MAX_DEVICES 5
#define CONFIG_LANG_USA 5

extern int gProfileCount;
extern int gActiveProfile;
extern PlayerProfile gProfiles[];
extern char gContentControlPassword[];
extern int gDriverCount;
extern ConfigDevice gDrivers[];

typedef ScriptValue *ScriptFn(ScriptValue *out, void *data, void *args);

void Config_RegisterScriptSymbols(void);
ScriptFn GOStart, GOPlayer, GOActiveMission, GOInRemap, GOInMouInv, GOInMouSens, GOGfxDisp, GOGameLang,
    GOGameDiff, GOContentControlPW, GOIsBlood, GOSoundSpeech, GOSoundMusic, GOSoundFX, GOGfxDevice,
    GOGfxGamma, GOGfxPerformance;
ScriptValue *Config_IsGermany(ScriptValue *out);
ScriptValue *Config_ResetGraphicOptions(ScriptValue *out);
ScriptValue *Config_GetContentControlPassword(ScriptValue *out);
ScriptFn Config_VerifyContentControlPassword;
ScriptValue *Config_IsContentControlPasswordEnabled(ScriptValue *out);
ScriptFn Config_GameOptionsInputSetInvertMouse, Config_GameOptionsInputSetMouseSensitivity,
    Config_GameOptionsInputSetBloodEnabled;
ScriptValue *Config_GameOptionsInputGetInvertMouse(ScriptValue *out);
ScriptValue *Config_GameOptionsInputGetMouseSensitivity(ScriptValue *out);
ScriptValue *Config_GameOptionsInputGetBloodEnabled(ScriptValue *out);
void Config_AddDisplayMode(DisplayMode *mode);
ScriptFn Config_FillScreenResolutionListBox, Config_GraphicOptionsSetResolution,
    Config_GraphicOptionsGetResolution, Config_GraphicOptionsSetDevice, Config_GraphicOptionsSetGamma,
    Config_GraphicOptionsSetTransparency, Config_GraphicOptionsSetPerformanceFlags;
GraphicOptions *Config_GetGraphicOptions(void);
ScriptValue *Config_FillRenderDeviceListBox(ScriptValue *out);
ScriptValue *Config_GraphicOptionsGetDevice(ScriptValue *out);
ScriptValue *Config_GraphicOptionsGetGamma(ScriptValue *out);
ScriptValue *Config_GraphicOptionsGetTransparency(ScriptValue *out);
ScriptValue *Config_GraphicOptionsGetPerformanceFlags(ScriptValue *out);
SoundOptions *Config_GetSoundOptions(void);
ScriptFn Config_SoundOptionsSetSoundsEffects, Config_SoundOptionsSetSoundsEffectsVolume,
    Config_SoundOptionsSetMusic, Config_SoundOptionsSetMusicVolume, Config_SoundOptionsSetSpeech,
    Config_SoundOptionsSetSpeechVolume, Config_SoundOptionsSetReverseStereo;
ScriptValue *Config_SoundOptionsGetSoundsEffects(ScriptValue *out);
ScriptValue *Config_SoundOptionsGetSoundsEffectsVolume(ScriptValue *out);
ScriptValue *Config_SoundOptionsGetMusic(ScriptValue *out);
ScriptValue *Config_SoundOptionsGetMusicVolume(ScriptValue *out);
ScriptValue *Config_SoundOptionsGetSpeech(ScriptValue *out);
ScriptValue *Config_SoundOptionsGetSpeechVolume(ScriptValue *out);
ScriptValue *Config_SoundOptionsGetReverseStereo(ScriptValue *out);
ScriptValue *Config_FillPlayerProfileListBox(ScriptValue *out);
ScriptValue *Config_GetActivePlayerProfileIndex(ScriptValue *out);
ScriptFn Config_SetActivePlayerProfileIndex, Config_CreateNewPlayerProfile, Config_DeletePlayerProfile,
    Config_GraphicOptionsGetPerfLevelFromFlags, Config_GraphicOptionsGetPerfFlagsFromLevel,
    Config_PlayerGetActiveMission;
void Config_ResetProfiles(void);
ScriptValue *Config_GetNumberOfPlayerProfiles(ScriptValue *out);
ScriptValue *Config_GraphicOptionsNumPerfLevels(ScriptValue *out);
ScriptValue *Config_FillMissionSelectionBox(ScriptValue *out);
ScriptValue *Config_FillMissionPictureBox(ScriptValue *out);
ScriptValue *Config_FillMissionTextBox(ScriptValue *out);
void Config_RemoveLanguageAlias(void);

#endif
