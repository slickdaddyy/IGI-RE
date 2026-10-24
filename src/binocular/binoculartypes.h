#ifndef BINOCULAR_BINOCULARTYPES_H
#define BINOCULAR_BINOCULARTYPES_H

#include "task/tasktypes.h"
#include "param/paramtypes.h"

/* "BinocularOverlayHolder" (0x78 bytes): shows an overlay while "active" is true */
typedef struct BinocularHolder {
    Task task;
    Task **overlay;         /* 0x20 */
    VarString active;       /* 0x24 */
    char isActive;          /* 0x74 */
    char pad75[3];
} BinocularHolder;

#endif
