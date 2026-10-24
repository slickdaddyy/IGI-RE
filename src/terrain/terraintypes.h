#ifndef TERRAIN_TERRAINTYPES_H
#define TERRAIN_TERRAINTYPES_H

#include <stdint.h>

#include "types.h"
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

typedef struct TerrainMaterial {
    int unk0;
    TerrainLayer layers[32];
} TerrainMaterial;

#endif
