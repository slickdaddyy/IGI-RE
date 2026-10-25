#ifndef CUTSCENE_CUTSCENEVARS_H
#define CUTSCENE_CUTSCENEVARS_H

#include <stdint.h>

extern uint16_t gRainObjType;
extern struct RainObj *gRainObjs[];
extern void (*gRainCallback)(char *state);
extern void *gRainParams;

#endif
