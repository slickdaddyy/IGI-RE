#ifndef COMPUTER_COMPUTERTYPES_H
#define COMPUTER_COMPUTERTYPES_H

#include "param/paramtypes.h"

/* one entry of DefineComputerObjective */
typedef struct Objective {
    char name[32];
    int taskId;         /* -1 = none */
    VarString visible;
    VarString done;
    int complete;       /* the values of the two expressions */
    int failed;
} Objective;
_Static_assert(sizeof(Objective) == 0xCC, "Objective");

typedef struct MapQuery {
    Vec3d *center;
    Matrix *view;
    float zoom;
    Vec3d *pos;
    Matrix *matrix;
    float radius;
    void *sprite;
    int marker;
    int color;
    char *text;
    char *text2;
} MapQuery;

#endif
