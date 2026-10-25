#ifndef GLOWMESH_GLOWMESHTYPES_H
#define GLOWMESH_GLOWMESHTYPES_H

#include <stdint.h>

#include "lod/lodtypes.h"

typedef struct MeshDrawArg {
    void *obj;          /* the object drawn */
    int *lights;
    int lightCount;
    int *haze;
    int hazeCount;
} MeshDrawArg;

typedef struct TerrainMeshVertex {
    float x, y, z;
    union {
        float height; /* height map height */
        int parent;   /* for the later vertices, the parent vertex index */
    };
} TerrainMeshVertex;

typedef struct TerrainMesh {
    double center[3];
    uint32_t vertCount; /* first-kind count << 16 | count */
    int level;          /* only level 0x10 meshes count */
    TerrainMeshVertex verts[];
} TerrainMesh;
_Static_assert(sizeof(TerrainMesh) == 0x20, "TerrainMesh");

/* a terrain cell QCube wants built this frame */
typedef struct TerrainCell {
    double pos[3];
    int level;
    unsigned int flags;      /* 1 swaps x and y, 2 and 4 mirror */
    const int16_t *shape;    /* face count, code offset, vertex counts, faces, codes */
    unsigned int maps;       /* low 16 bits lightmap (0xFFFF none), bits 16-23 height map */
    unsigned int materials;  /* four material maps, one per byte */
    QCacheEntry *cache;
} TerrainCell;
_Static_assert(sizeof(TerrainCell) == 0x30, "TerrainCell");

#endif
