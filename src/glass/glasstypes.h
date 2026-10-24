#ifndef GLASS_GLASSTYPES_H
#define GLASS_GLASSTYPES_H

#include <stdint.h>

#include "types.h"
#include "task/tasktypes.h"

/* the collision mesh of a model LOD */
typedef struct TriMesh {
    int count;
    char unk4[0xC];
    uint16_t (*tris)[4]; /* 0x10 */
    float (*verts)[4];  /* 0x14 */
} TriMesh;

typedef struct HitArg {
    void *hit;
    float damage;
    Vec3f dir;
} HitArg;

typedef struct Shard {
    Vec3f color0;
    Vec3f color;        /* 0x0C, gets color0 each frame */
    Vec3f center;       /* 0x18 */
    Vec3f v[3];         /* 0x24 */
    int spin;           /* 0x48, index into gShardSpin */
    Vec3f vel;          /* 0x4C */
    float unk58;
    int active;         /* 0x5C */
} Shard;

/* the drawn polygon of a shard */
typedef struct ShardPoly {
    float *v[3];
    float *color[3];    /* 0x0C */
    char unk18[0x18];
    float fade[3];      /* 0x30 */
    Vec3f normal;       /* 0x3C */
    int unk48;
    int unk4C;
} ShardPoly;

/* what PolyList_New makes from the polygons */
typedef struct ShardMesh {
    char unk0[0x6C];
    ShardPoly *polys;   /* 0x6C */
    char unk70[0x2C];
    int count;          /* 0x9C */
} ShardMesh;

typedef struct GlassShatter {
    Task task;
    char unk20[0x48];
    void **model;       /* 0x68 */
    Vec3f dir;          /* 0x6C, 10 units long, in model space */
    Vec3d pos;          /* 0x78 */
    Matrix matrix;      /* 0x90 */
    int count;          /* 0xB8 */
    int frames;         /* 0xBC */
    Shard shards[1000]; /* 0xC0 */
    ShardPoly polys[1000]; /* 0x177C0 */
    ShardMesh *mesh;    /* 0x2B040 */
    char unk2B044[4];
} GlassShatter;
_Static_assert(sizeof(GlassShatter) == 0x2B048, "GlassShatter");

#endif
