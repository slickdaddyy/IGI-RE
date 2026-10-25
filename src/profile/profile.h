#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

#include "igi.h"
#include "task/tasktypes.h"
#include "config/configtypes.h"

#define PROFILE_MAX 16
#define PROFILE_KEYS_MAX 64
#define CONFIG_LANGUAGE_COUNT 6

typedef void (*TaskHandler)(void *task, void *arg);

PlayerProfile *Profile_GetActive(void);
float Profile_GetSfxVolume(void);
float Profile_GetMusicVolume(void);
const char *Config_GetDifficultyName(int difficulty);

#endif
