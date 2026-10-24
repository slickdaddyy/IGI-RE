#ifndef CAMFEED_CAMFEEDTYPES_H
#define CAMFEED_CAMFEEDTYPES_H

#include "task/tasktypes.h"
#include "res/restypes.h"

/* horizontal scan lines in a colour, plus random noise streaks */
typedef struct LineFilter {
    Task task;
    float color[3];
    float depth;
    float level;
    int sorted;
} LineFilter;
_Static_assert(sizeof(LineFilter) == 0x38, "LineFilter");

/* black bars at the top and bottom of a clip window */
typedef struct WideScreenFilter {
    Task task;
    float color[3];
    float unk2C;
    float w, h, x, y;
    float barHeight;
} WideScreenFilter;
_Static_assert(sizeof(WideScreenFilter) == 0x44, "WideScreenFilter");

/* a 16x16 texture of colour bands */
typedef struct BandFilter {
    Task task;
    float depth;
    float level;
    float maxLevel;
    Texture tex;
    int counter;
    int period;
    int timer;
    int interval;
    int camera;
    float color[3];
    void *font;
} BandFilter;
_Static_assert(sizeof(BandFilter) == 0x74, "BandFilter");

typedef struct HandiCamFilter {
    Task task;
    float color[3];
    float depth;
    void *font;
    char battery[0x18];
    float blink;
    int lastMs;
} HandiCamFilter;
_Static_assert(sizeof(HandiCamFilter) == 0x54, "HandiCamFilter");

#endif
