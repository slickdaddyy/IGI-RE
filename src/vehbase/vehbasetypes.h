#ifndef VEHBASE_VEHBASETYPES_H
#define VEHBASE_VEHBASETYPES_H

typedef struct AnimKey {
    char channel;       /* ANIM_WAIT holds the value rounded to an int */
    char unk1[3];
    float value;
} AnimKey;

#endif
