#ifndef TRANSCTX_TRANSCTXTYPES_H
#define TRANSCTX_TRANSCTXTYPES_H

#include "types.h"

typedef struct SoundParams {
    float volume;
    float pitch;
    float pan;
    int unkC;
    double minDist;
    double maxDist;
    Vec3d pos;
    Vec3d vel;
    float start;
    int unk54;
} SoundParams;
_Static_assert(sizeof(SoundParams) == 0x58, "SoundParams");

typedef struct SoundNode {
    struct SoundNode *next;
    struct SoundNode *prev;
} SoundNode;

typedef struct SoundList {
    SoundNode *head;
    SoundNode *tail;
    SoundNode *tailpred;
} SoundList;

typedef struct TransCtx {
    float m[9];
    int unk24;
    Vec3d pos;
    float fov;
    float fovY;
    float scaleX, scaleY;
    float centerX, centerY;
    int unk58;
    float unk5C;
    int layer;
    float proj[16];
    int unkA4;
} TransCtx;
_Static_assert(sizeof(TransCtx) == 0xA8, "TransCtx");

typedef struct SceneArea {
    char unk0[0x18];
    float x, y, w, h;
    float halfW, halfH;
    float minX, minY, maxX, maxY;
} SceneArea;

typedef struct RenderNode {
    struct RenderNode *next;
    int type; /* method index, -1 for a queue head */
    int unk8;
} RenderNode;

#endif
