#ifndef VIEWPORT_VIEWPORTTYPES_H
#define VIEWPORT_VIEWPORTTYPES_H

#include <stdint.h>

#include "transctx/transctxtypes.h"

/* what callers keep to restart or query a sound of theirs */
typedef struct SoundHandle {
    int bucket;
    int hash;
    int id; /* 0 until first played */
    uint32_t flags;
    SoundParams params;
} SoundHandle;

#endif
