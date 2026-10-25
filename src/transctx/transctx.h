#ifndef TRANSCTX_TRANSCTX_H
#define TRANSCTX_TRANSCTX_H

#include "igi.h"
#include "transctx/transctxtypes.h"
#include "core/coretypes.h"

extern RenderMode gRenderModeCurrent;
extern Scene gScene;

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

void *Frame_Alloc(int size);
void RenderQueue_Add(int queue, void *node);
void TransContext_Invalidate(void);

#endif
