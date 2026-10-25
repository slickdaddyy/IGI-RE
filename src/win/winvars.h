#ifndef WIN_WINVARS_H
#define WIN_WINVARS_H

#include <stdint.h>

#include "d3d/d3dtypes.h"
#include "win/wintypes.h"

extern const GUID IID_IDirect3D7_[];
extern int gDisplayClearColor[];
extern DisplayMode gDisplayModes[];
extern char gKeyboardPresent;
extern uint8_t gKeys[];
extern uint8_t gKeysPrev[];
extern Mouse gMouse;

#endif
