#ifndef HUMANPLAYER_HUMANPLAYERTYPES_H
#define HUMANPLAYER_HUMANPLAYERTYPES_H

#include <stdint.h>

typedef struct HumanMotion {
    int unk0;
    uint32_t flags;
    char unk8[0x44];
    int unk4C[5];
    char unk60[0x7F8];
    float throwPos[3];
} HumanMotion;
_Static_assert(sizeof(HumanMotion) == 0x864, "HumanMotion");

#endif
