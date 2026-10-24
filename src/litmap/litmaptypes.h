#ifndef LITMAP_LITMAPTYPES_H
#define LITMAP_LITMAPTYPES_H

#include <stdint.h>

#include "res/restypes.h"
#include "task/tasktypes.h"
#include "model/modeltypes.h"

/* one lightmap texture of a LOD */
typedef struct LightmapTex {
    Texture texture;
    void *pixels;
    short width;
    short height;
} LightmapTex;
_Static_assert(sizeof(LightmapTex) == 0x2C, "LightmapTex");

/* what the builder keeps per lightmap texture (0x28 bytes) */
typedef struct LightmapTexInfo {
    Vec3f *normals;
    Vec3f *positions;
    Vec3f *unk8;
    Vec3f *unkC;
    float *weights;
    uint8_t *unk14;
    uint8_t *unk18;
    int firstTexel;
    int litCount;
    int *texelIds;
} LightmapTexInfo;
_Static_assert(sizeof(LightmapTexInfo) == 0x28, "LightmapTexInfo");

/* the lightmaps of one LOD; loaded from lightmaps.res or allocated, also
   the layout of a .olm file */
typedef struct Lightmaps {
    int magic;
    float version;
    int time[7];
    int fromResource;
    int built;
    int count;
    int texels;
    int texels2;
    LightmapTexInfo *info;
    LightmapTex tex[];
} Lightmaps;
_Static_assert(sizeof(Lightmaps) == 0x3C, "Lightmaps");

/* one LOD of an object in a lightmap set (0x24 bytes) */
typedef struct LightmapRef {
    Lightmaps *lightmaps;
    struct ModelLod *lod;
    struct RigidDynCubeObj *obj;
    Matrix *matrix;
    Vec3d *pos;
    int unk14;
    int lodIndex;
    int lodCount;
    int uploaded;
} LightmapRef;
_Static_assert(sizeof(LightmapRef) == 0x24, "LightmapRef");

typedef struct LightmapSet {
    int count;
    struct VirModel *model;
    float radius;
    Vec3f center;
    LightmapRef refs[];
} LightmapSet;

/* per vertex weights of one light source */
typedef struct VertexLight {
    float *weights;
    int *verts;
    int count;
} VertexLight;

typedef struct VertexLights {
    int count;
    VertexLight lights[];
} VertexLights;

typedef struct LightmapInfo {
    Task task;
    char unk20[0x48];
    char unk68, unk69, unk6A, unk6B, unk6C, unk6D, unk6E;
    char unk6F, unk70, unk71;
    char unk72[2];
    char *slot;
    char name[0x10];
    LightmapSet *set;
    float scale;
    float unk90;
    float unk94;
    float lastScale;
    float unk9C;
    int unkA0;
    int unkA4;
    int texelsA8;
    int texelsAC;
    int vertexLightSize;
    int unkB4;
    Task *owner;
    void *unkBC;
    int unkC0;
    int unkC4;
    VertexLights *vertexLights;
    int unkCC;
    int unkD0, unkD4, unkD8;    /* an RGB parameter */
    int unkDC;
} LightmapInfo;
_Static_assert(sizeof(LightmapInfo) == 0xE0, "LightmapInfo");

typedef struct LightmapVertex {
    Vec3f pos;
    char unkC[0x14];
    float uv[2];
} LightmapVertex;

typedef struct LightmapChunk {
    char unk0[0xC];
    uint16_t indexCount;
    uint16_t next;
    uint8_t unk10;
    char unk11;
    uint16_t texture;
    uint16_t base;
    char unk16[2];
    uint16_t unk18;
    char unk1A[4];
    uint8_t unk1E;
    char unk1F;
    uint16_t index[];
} LightmapChunk;
_Static_assert(sizeof(LightmapChunk) == 0x20, "LightmapChunk");

typedef struct LightmapMesh {
    int unk0;
    int texCount;
    int unk8;
    int unkC;
    int vertCount;
    LightmapVertex *verts;
    short (*sizes)[4];
    LightmapChunk *chunks;
    char unk20[8];
    int unk28;
} LightmapMesh;
_Static_assert(sizeof(LightmapMesh) == 0x2C, "LightmapMesh");

typedef struct Lightmap {
    int size;
    int *data;
    Texture texture;
} Lightmap;

#endif
