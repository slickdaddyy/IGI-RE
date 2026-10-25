#ifndef EDITCAM_EDITCAMVARS_H
#define EDITCAM_EDITCAMVARS_H

#include <stdint.h>

#include "coll/colltypes.h"

extern uint8_t gMenuItemCommitEvent;
extern uint8_t gMenuItemSelectEvent;
extern uint8_t gMenuItemFindEvent;
extern char gHmpResident[];
extern char gBitResident[];
extern int gCollHoleList[];
extern int gCollBumpDepth;
extern int gCollHoleCount;
extern CollBump gCollBumps[];
extern unsigned int gCollHoleBits;
extern CollHole gCollHoles[];

#endif
