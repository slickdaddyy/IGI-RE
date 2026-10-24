#ifndef HUMANAI_HUMANAITYPES_H
#define HUMANAI_HUMANAITYPES_H

#include "types.h"

/* a point of the walk queue */
typedef struct MoveItem {
    Node node;
    Vec3d pos;
    int door;
    int id;
} MoveItem;

#endif
