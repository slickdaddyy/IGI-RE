#ifndef MISSION_MISSIONTYPES_H
#define MISSION_MISSIONTYPES_H

#include "param/paramtypes.h"

typedef struct ConditionalContainer {
    Task task;
    VarString cond;
    VarString onTrue;
    VarString onFalse;
    char active;
    char registered;
    char shown;
    char pad113;
} ConditionalContainer;

#endif
