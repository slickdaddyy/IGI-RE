#ifndef VEHICLE_VEHICLETYPES_H
#define VEHICLE_VEHICLETYPES_H

#include "magicobj/magicobjtypes.h"

/* node of a list of task references (vehicle seats, targets) */
typedef struct RefNode {
    struct RefNode *next;
    struct RefNode *prev;
    Task **ref;
    char unkC[0x1D];
    char active;
} RefNode;

#endif
