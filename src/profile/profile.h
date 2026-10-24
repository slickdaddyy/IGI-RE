#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"

#define PROFILE_MAX 16
#define PROFILE_KEYS_MAX 64
#define CONFIG_ACTION_COUNT 27
#define CONFIG_DEVICE_COUNT 3
#define CONFIG_KEY_COUNT 127
#define CONFIG_LANGUAGE_COUNT 6

typedef struct KeyBinding {
    int action;
    int device;
    int key;
} KeyBinding;

typedef struct DisplayMode {
    int width;
    int height;
    int depth;
} DisplayMode;

/* one player of config.qsc, written back by Config_Write */
typedef struct PlayerProfile {
    int difficulty;
    int language;
    char blood;
    char unk09[3];
    DisplayMode display;
    int performance;
    char unk1C[3];
    char gfxDevice[0x100];
    char gfxDriver[0x100];
    char unk21F;
    float gamma;
    float sfxVolume;
    float musicVolume;
    float speechVolume;
    char sfxOn;
    char musicOn;
    char speechOn;
    char unk233;
    KeyBinding keys[PROFILE_KEYS_MAX];
    int keyCount;
    int mouseInvert;
    float mouseSensitivity;
    char name[0x20];
    int activeMission;
} PlayerProfile;

/* a script name and its value: actions, input devices, keys */
typedef struct ConfigName {
    char name[0x20];
    int value;
} ConfigName;

typedef struct D3DDriver {
    char name[0x80];
    char desc[0x80];
    DisplayMode modes[0x40];
    int modeCount;
} D3DDriver;

typedef void (*TaskHandler)(void *task, void *arg);

extern TaskHandler gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;

extern int gProfileCount;
extern int gActiveProfile;
extern PlayerProfile gProfiles[];
extern char gContentControlPassword[];
extern KeyBinding gDefaultKeys[];
extern ConfigName gInputActions[];
extern ConfigName gInputDevices[];
extern ConfigName gInputKeys[];
extern char gLanguageNames[][16];
extern char gDifficultyNames[][16];

PlayerProfile *Profile_GetActive(void);
float Profile_GetSfxVolume(void);
float Profile_GetMusicVolume(void);
const char *Config_GetDifficultyName(int difficulty);

#endif
