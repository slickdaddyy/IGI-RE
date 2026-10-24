#ifndef LEVEL_LEVELTYPES_H
#define LEVEL_LEVELTYPES_H

#include "task/tasktypes.h"

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
