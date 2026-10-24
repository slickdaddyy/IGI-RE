#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "config/configtypes.h"

#define PROFILE_MAX 16
#define PROFILE_KEYS_MAX 64
#define CONFIG_ACTION_COUNT 27
#define CONFIG_DEVICE_COUNT 3
#define CONFIG_KEY_COUNT 127
#define CONFIG_LANGUAGE_COUNT 6

/* a script name and its value: actions, input devices, keys */
typedef struct ConfigName {
    char name[0x20];
    int value;
} ConfigName;

typedef void (*TaskHandler)(void *task, void *arg);

extern TaskHandler gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;

extern int gProfileCount;
extern int gActiveProfile;
extern PlayerProfile gProfiles[];
extern char gContentControlPassword[];
extern InputRemap gDefaultKeys[];
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
