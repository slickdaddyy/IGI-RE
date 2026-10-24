#ifndef HUMANPLAYER_HUMANPLAYERTYPES_H
#define HUMANPLAYER_HUMANPLAYERTYPES_H

#include <stdint.h>

typedef struct HumanMotion {
    int unk0;
    uint32_t flags;
    char unk8[0x44];
    int unk4C[5];
} HumanMotion;

#endif
