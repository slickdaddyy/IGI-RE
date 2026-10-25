#ifndef RENDER_RENDERVARS_H
#define RENDER_RENDERVARS_H

#include <stdint.h>

#include "d3d/d3dtypes.h"

extern int gTexStageIdx;
extern int gAlphaBlendIdx;
extern int gLitAlphaIdx;
extern int gLitIdx;
extern int gLmStageIdx;
extern int gUnlitAlphaIdx;
extern int gMeshVBCount;
extern int gOpaqueBlendIdx;
extern MeshVB gMeshVBs[];
extern int gUnlitIdx;
extern float gFogColor[];
extern float gWorldScale;
extern int gD3DClippingOnIndex;
extern uint32_t gD3DZEnable;
extern struct PtrList *gD3DBlendStates;
extern DynVB gVBNormal;
extern struct PtrList *gD3DRenderStates;
extern struct PtrList *gD3DClipStates;
extern int gD3DClippingOffIndex;
extern struct PtrList *gD3DTextureStates;

#endif
