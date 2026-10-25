#ifndef TERRAIN_TERRAINTYPES_H
#define TERRAIN_TERRAINTYPES_H

#include <stdint.h>

#include "task/tasktypes.h"

/* A task that lives in a dyncube tree. */
typedef struct DynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0xC];
    float radius;
    int unk48;
    int unk4C;
    struct DynCubeNode *node;
    struct DynCubeObj *prev;
    struct DynCubeObj *next;
    uint16_t mask;             /* copy of the task type's event mask */
    uint16_t unk5E;
    int unk60;
} DynCubeObj;
_Static_assert(sizeof(DynCubeObj) == 0x68, "DynCubeObj");
_Static_assert(offsetof(DynCubeObj, node) == 0x50, "DynCubeObj");
_Static_assert(offsetof(DynCubeObj, unk60) == 0x60, "DynCubeObj");

typedef struct TerrainMap {
    uint8_t *data;
    char flag;
    char unk5[3];
    int size;
} TerrainMap;

/* texture placement of a material at one level, 0xC28FE4 + material * 900 + level * 28 */
typedef struct TerrainLayer {
    int unk0;
    float unk4;
    float unk8;
    float scaleA;
    int tileA;
    float scaleB;
    int tileB;
} TerrainLayer;
_Static_assert(sizeof(TerrainLayer) == 0x1C, "TerrainLayer");

typedef struct TerrainMaterial {
    int unk0;
    TerrainLayer layers[32];
} TerrainMaterial;
_Static_assert(sizeof(TerrainMaterial) == 900, "TerrainMaterial");

/* Octree node. Children are indexed by x | y << 1 | z << 2, where each bit
   is the coordinate bit equal to the node's size. */
typedef struct DynCubeNode {
    unsigned int size;
    int child;                 /* index in the parent's children */
    struct DynCubeNode *parent;
    struct DynCubeNode *children[8];
    DynCubeObj *objs;
    uint8_t child_mask;        /* bit n set while children[n] exists */
    char unk31[3];
    int tree;
    uint16_t mask;             /* event mask of objs, plus bit 15 */
    uint16_t sub_mask;         /* event masks of all children */
    void *unk3C;
} DynCubeNode;

typedef struct TerrainLightmap {
    int size;
    void *data;
    char texture[0x24];
} TerrainLightmap;

typedef struct Terrain {
    int unk0;
    void *unk4;
    char *cmd;
    struct CollNode *ctr;                    /* the .ctr file, node 1 is the root */
    struct DynCubeNode *roots[6];
    struct DynCubeNode *start[6];      /* where queries start descending */
    Vec3d start_pos[6];
    int start_depth[6];
    int start_valid[6];
    int handler_changes[6];
    int16_t slot_count[6][254]; /* objects with a handler in each slot */
    Pool nodes;
    DynCubeObj *lists[9];                    /* index list + 1 */
    struct GameMaterial *materials[32];      /* per collision material */
    void *textures;
    int *unkDC0;
    uint8_t *unkDC4;
    int ready;           /* the terrain mesh lighting is set up */
    TerrainMap *bit;
    TerrainMap *hmp;
    char unkDD4[0xC];
    int offset[3];
    int lightMapCount;
    int unkDF0;
    void *palette;
    int *lmp;
    double *waterLevel; /* the water surface height, NULL without water */
    char name[0x100];
} Terrain;
_Static_assert(sizeof(Terrain) == 0xF00, "Terrain");
_Static_assert(offsetof(Terrain, slot_count) == 0x118, "Terrain");
_Static_assert(offsetof(Terrain, nodes) == 0xD00, "Terrain");
_Static_assert(offsetof(Terrain, textures) == 0xDBC, "Terrain");
_Static_assert(offsetof(Terrain, offset) == 0xDE0, "Terrain");

#endif
