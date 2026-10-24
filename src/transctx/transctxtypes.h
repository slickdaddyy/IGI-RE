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

/* the view the frame is drawn with (0xBCAAE0) */
typedef struct TransCtx {
    Matrix matrix;
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

#endif
