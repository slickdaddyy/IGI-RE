#ifndef D3D_D3DVARS_H
#define D3D_D3DVARS_H

#include "d3d/d3dtypes.h"

extern const GUID IID_IDirectDraw7_[];
extern IDirectDrawSurface7 *gD3DBack;
extern IDirectDraw7 *gDDraw;
extern struct IDirect3D7 *gD3D;
extern int gSoundVoiceCount;
extern struct IDirectSound *gDSound;
extern char gSoundReady;
extern int gD3DFoundHAL;
extern int gD3DTnL;
extern float gNearZ;
extern int gDisplayModeCount;

#endif
