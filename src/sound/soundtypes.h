#ifndef SOUND_SOUNDTYPES_H
#define SOUND_SOUNDTYPES_H

#include <stdint.h>

#include "task/tasktypes.h"

typedef struct SoundFindArg {
    const char *name;
    Task *found;
} SoundFindArg;

typedef struct FileInfo {
    const char *path;
    void *data;
    char isDir;
} FileInfo;

typedef struct CellMask {
    struct CellMask **item;
    int shift;
    int size;
    uint8_t bits[];
} CellMask;

typedef struct CellMaskDrawArg {
    CellMask *mask;
    int size;
    double *center;
    float range;
} CellMaskDrawArg;

#endif
