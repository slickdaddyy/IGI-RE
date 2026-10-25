#ifndef CONFIG_CONFIGVARS_H
#define CONFIG_CONFIGVARS_H

#include "config/configtypes.h"

extern ConfigConstant gInputDevices[CONFIG_DEVICE_COUNT];
extern const char gEmptyString[];
extern ConfigDevice gDrivers[];
extern int gProfileCount;
extern int gActiveProfile;
extern PlayerProfile gProfiles[];
extern char gContentControlPassword[];

#endif
