#ifndef TRANSCTX_TRANSCTX_H
#define TRANSCTX_TRANSCTX_H

#include "igi.h"
#include "transctx/transctxtypes.h"

/* Camera for one view, 0xA8 bytes. gTransContext (0xBCAAE0) is the active one. */
typedef struct TransContext {
    float m[9]; /* world to view, transposed from the camera matrix */
    int unk24;
    double pos[3];
    float fov; /* tangent of half the horizontal field of view */
    float fovY;
    float scaleX, scaleY;
    float centerX, centerY;
    int unk58;
    float unk5C;
    int layer; /* 0..2, picks the viewport depth range and near plane */
    float proj[16];
    int unkA4;
} TransContext;

_Static_assert(sizeof(TransContext) == 0xA8, "TransContext");

extern void (*gRenderFuncs[])(void *);
extern TransContext gTransContext;
extern Scene gScene;

/* a float constant from the exe, bit exact */
#define F32(name, bits) static const union { uint32_t u; float f; } name = { bits }

void *Frame_Alloc(int size);
void RenderQueue_Add(int queue, void *node);
void TransContext_Invalidate(void);

#endif
