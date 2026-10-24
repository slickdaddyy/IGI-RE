#ifndef MATERIAL_MATERIAL_H
#define MATERIAL_MATERIAL_H

#include <stdint.h>

/* One entry of material.qsc. The effect types are task types whose
   handlers for the size and parse events read extra script arguments. */
typedef struct GameMaterial {
    float unk00;
    float unk04;
    float unk08;
    float unk0C;
    float unk10;
    float unk14;
    float unk18;
    float unk1C;
    char name[32];
    float unk40;
    float unk44;
    float unk48;
    char unk4C;
    float unk50;
    char unk54[6][32];
    char unk114[32];
    char unk134[32];
    char unk154[6][32];
    char unk214[2][32];
    uint16_t effectType;
    void *effectParams;
    uint16_t effect2Type;
    void *effect2Params;
    char used;
} GameMaterial;

_Static_assert(sizeof(GameMaterial) == 0x268, "GameMaterial size");

#endif
