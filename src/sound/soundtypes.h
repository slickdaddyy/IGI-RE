#ifndef SOUND_SOUNDTYPES_H
#define SOUND_SOUNDTYPES_H

#include <stdint.h>

#include "task/tasktypes.h"
#include "transctx/transctxtypes.h"

typedef struct SoundFindArg {
    const char *name;
    Task *found;
} SoundFindArg;

typedef struct FileInfo {
    const char *path;
    void *data;
    char isDir;
} FileInfo;

typedef struct CellMask {
    struct CellMask **item;
    int shift;
    int size;
    uint8_t bits[];
} CellMask;

typedef struct CellMaskDrawArg {
    CellMask *mask;
    int size;
    double *center;
    float range;
} CellMaskDrawArg;

typedef struct SplineBuffer {
    void *data;
    int count;
} SplineBuffer;

/* "SplineObjSegment" (0x5B8 bytes), a RigidDynCubeObj whose model is bent
   along a curve; see SplineMesh_GetVerts */
typedef struct SplineObjSegment {
    Task task;
    Vec3d pos;
    char unk38[0x34];
    char **models;
    float rot[10];
    char unk98[0xC];
    int lod;
    Vec3f color1;
    Vec3f color2;
    char unkC0[0x30];
    void *tree;
    float radius;
    float length;
    float start;
    float end;
    float segPos[21][3];
    float segRot[20][10];
    float lastRot[9];
    float segLength[21];        /* [0] is lastRot's spare tenth word */
    int unk598;
    int segments;
    SplineBuffer buffers[2];
    char **lastModel;
    char rigid;
    char reversed;
    char pad5B6[2];
} SplineObjSegment;
_Static_assert(sizeof(SplineObjSegment) == 0x5B8, "SplineObjSegment");

typedef struct SoundInst {
    struct SoundInst *prev;
    struct SoundInst *next;
    uint32_t flags;
    int bucket;
    int hash;
    int id;
    uint32_t age;
    void *voice;
    SoundParams params;
} SoundInst;
_Static_assert(sizeof(SoundInst) == 0x78, "SoundInst");

/* the bounding spheres of one part of a spline segment */
typedef struct SegBuffer {
    struct SphereNode *data;
    int count;
} SegBuffer;

#endif
