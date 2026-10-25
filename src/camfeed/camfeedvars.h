#ifndef CAMFEED_CAMFEEDVARS_H
#define CAMFEED_CAMFEEDVARS_H

#include "d3d/d3dtypes.h"

extern struct IDirect3DDevice7 *gD3DDevice;
extern int gHandiCamDrawMethod;
extern int gBandDrawMethod;
extern int gHandiCamFilterPrim;
extern int gBandFilterPrim;
extern int gLineDrawMethod;
extern int gLineFilterPrim;
extern int gWideScreenPrim;
extern int gBatteryX;
extern int gBatteryY;
extern int gTimecodeX;
extern int gTimecodeY;
extern float gTimecode;
extern float gBandTextX;
extern float gBandTextY;
extern float gBandTextDX;
extern float gBandTextDY;
extern DynVB gVBTL;
extern float gNearPlane;

#endif
