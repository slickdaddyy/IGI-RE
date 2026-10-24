#ifndef MISSILE_MISSILETYPES_H
#define MISSILE_MISSILETYPES_H

#include "vehbase/vehbasetypes.h"
#include "param/paramtypes.h"

typedef struct Train {
    Vehicle v;
    float frac;         /* 0x748, model front as a fraction of its length */
    float length;       /* 0x74C */
    float accel;        /* 0x750 */
    float max_speed;    /* 0x754 */
    float start_speed;  /* 0x758 */
    int unk75C;
    int unk760;
    float velocity;     /* 0x764 */
    float dist;         /* 0x768, along the path */
    float start_dist;   /* 0x76C */
    float offset[2];    /* 0x770 */
    int wagons;         /* 0x778 */
    int path;           /* 0x77C */
    char model[0x100];  /* 0x780 */
    char reverse;       /* 0x880 */
    char unk881[3];
    float *sound_params; /* 0x884 */
    int last_tick;      /* 0x888 */
    Task *loop_sound;   /* 0x88C */
    VarString script;   /* 0x890 */
} Train;
_Static_assert(sizeof(Train) == 0x8E0, "Train");

#endif
