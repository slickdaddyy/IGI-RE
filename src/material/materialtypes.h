#ifndef MATERIAL_MATERIALTYPES_H
#define MATERIAL_MATERIALTYPES_H

#include <stdint.h>

typedef struct GameMaterial {
    float unk00;
    float unk04;
    float unk08;
    float bounce;
    float unk10;
    float unk14;
    float unk18;
    float unk1C;
    char name[32];
    float hardness;
    float ricochet;         /* degrees */
    float unk48;
    char unk4C;
    float unk50;
    char unk54[6][32];
    char unk114[32];
    char unk134[32];
    char sounds[6][32];     /* impact sounds */
    char unk214[2][32];
    uint16_t effect;        /* effect task types and their parameters */
    void *params;
    uint16_t backEffect;
    void *backParams;
    char used;
} GameMaterial;
_Static_assert(offsetof(GameMaterial, backParams) == 0x260, "GameMaterial");
_Static_assert(sizeof(GameMaterial) == 0x268, "GameMaterial");

#endif
