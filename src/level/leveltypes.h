#ifndef LEVEL_LEVELTYPES_H
#define LEVEL_LEVELTYPES_H

#include "task/tasktypes.h"

#define LEVEL_MAX_IDS 0x1000
#define LEVEL_NOTE_LISTS 251

typedef struct Level {
    Task task;
    int unk20;
    int time;
    int tick;
    int fps;
    int msPerTick;
    int unk34;
} Level;

#endif
