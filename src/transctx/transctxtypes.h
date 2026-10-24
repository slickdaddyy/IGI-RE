#ifndef TRANSCTX_TRANSCTXTYPES_H
#define TRANSCTX_TRANSCTXTYPES_H

#include <stdint.h>

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

/* A 32 bit offscreen image, the target of the "OffscreenBuffer" render mode. */
typedef struct OffscreenBuffer {
    int width;
    int height;
    uint32_t *pixels;
} OffscreenBuffer;

typedef struct RenderNode {
    struct RenderNode *next;
    int type; /* method index, -1 for a queue head */
    int unk8;
} RenderNode;

typedef struct Sound {
    SoundNode node;
    int key1;
    int key2;
    int magic;
    short format; /* 0 / 2 streamed (2 = ADPCM), 1 PCM, 3 ADPCM decoded at load */
    short bits;
    short channels;
    short unk1A;
    int rate;
    int samples;
    struct IDirectSoundBuffer *buffer;
    char *name;
    void *data;
} Sound;

typedef struct BlitPrim {
    RenderNode node;
    int pitch; /* bytes */
    int x, y, w, h;
    uint32_t *pixels;
    int unk24;
} BlitPrim;

/* Camera for one view. gTransContext (0xBCAAE0) is the active one. */
typedef struct TransContext {
    Matrix matrix; /* world to view, transposed from the camera matrix */
    Vec3d pos;
    float fov; /* tangent of half the horizontal field of view */
    float fovY;
    float scaleX, scaleY;
    float centerX, centerY;
    int qcubeActive;
    float unk5C;
    int layer; /* 0..2, picks the viewport depth range and near plane */
    float proj[16];
    int unkA4;
} TransContext;
_Static_assert(sizeof(TransContext) == 0xA8, "TransContext");

typedef struct Scene {
    struct Scene *next;
    struct Scene *prev;
    int renderMode; /* -1 = the current one */
    RenderNode clip; /* ClipWindowPrimitive for the area below */
    float x, y, w, h;
    float halfW, halfH;
    float minX, minY, maxX, maxY;
    int queueCount;
    RenderNode *queues; /* drawn from the last one down */
    RenderNode *extra;
} Scene;

#endif
