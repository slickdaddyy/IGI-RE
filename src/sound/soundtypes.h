#ifndef SOUND_SOUNDTYPES_H
#define SOUND_SOUNDTYPES_H

#include "task/tasktypes.h"

typedef struct SoundFindArg {
    const char *name;
    Task *found;
} SoundFindArg;

#endif
