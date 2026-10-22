#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

#include "igi.h"
#include "task/task.h"

#define DYNCUBE_TREES 6
#define DYNCUBE_MIN_SIZE 0x2000
#define DYNCUBE_MAX_SIZE 0x40000000

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct DynCubeObj DynCubeObj;

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

/* A task that lives in a dyncube tree. */
struct DynCubeObj {
    Task task;
    Vec3d pos;
    char unk38[0xC];
    float radius;
    int unk48;
    int unk4C;
    DynCubeNode *node;
    DynCubeObj *prev;
    DynCubeObj *next;
    uint16_t mask;             /* copy of the task type's event mask */
    uint16_t unk5E;
    int unk60;
};

/* 256 entries at the start of the .bit and .hmp files. */
typedef struct TerrainMap {
    uint8_t *data;
    char flag;
    char unk5[3];
    int size;
} TerrainMap;

/* 0x20 byte records of the .ctr file. */
typedef struct TerrainCtr {
    char unk0[0x1C];
    char *cmd;                 /* offset into the .cmd file, made a pointer on load */
} TerrainCtr;

typedef struct Terrain {
    int unk0;
    void *unk4;
    char *cmd;
    TerrainCtr *ctr;
    DynCubeNode *roots[DYNCUBE_TREES];
    DynCubeNode *start[DYNCUBE_TREES];      /* where queries start descending */
    Vec3d start_pos[DYNCUBE_TREES];
    int start_depth[DYNCUBE_TREES];
    int start_valid[DYNCUBE_TREES];
    int handler_changes[DYNCUBE_TREES];
    int16_t slot_count[DYNCUBE_TREES][TASK_MAX_SLOTS]; /* objects with a handler in each slot */
    Pool nodes;
    DynCubeObj *lists[9];                    /* index list + 1 */
    char unkD3C[0xDBC - 0xD3C];
    void *unkDBC;
    int *unkDC0;
    uint8_t *unkDC4;
    int unkDC8;
    TerrainMap *bit;
    TerrainMap *hmp;
    char unkDD4[0xC];
    int offset[3];
    int unkDEC;
    int unkDF0;
    void *palette;
    int *lmp;
    int unkDFC;
    char name[0x100];
} Terrain;

typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *s;
    int unk14;
} ScriptValue;

typedef struct TerrainLayer {
    int unk0;
    float unk4;
    float unk8;
    float unkC;
    int unk10;
    float unk14;
    int unk18;
} TerrainLayer;

typedef struct TerrainMaterial {
    int unk0;
    TerrainLayer layers[32];
} TerrainMaterial;

typedef struct TerrainLightmap {
    int size;
    void *data;
    char texture[0x24];
} TerrainLightmap;

/* fistp in the current rounding mode, low 32 bits */
static inline int Terrain_Round(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

unsigned int DynCube_CellSize(int x0, int y0, int z0, int x1, int y1, int z1);
void DynCube_ListRemove(Terrain *t, DynCubeObj *obj, int list);
void DynCube_ListAdd(Terrain *t, DynCubeObj *obj, int list);
void DynCube_Move(Terrain *t, DynCubeObj *obj, const Vec3d *pos, float radius);
void DynCube_Ascend(Terrain *t, unsigned int size, unsigned int cur, DynCubeNode **node);
void DynCubeNode_Unlink(DynCubeNode *node, DynCubeObj *obj);
void DynCube_Insert(Terrain *t, DynCubeNode *node, DynCubeObj *obj, unsigned int size,
                    unsigned int x, unsigned int y, unsigned int z);
void DynCubeNode_Link(DynCubeNode *node, DynCubeObj *obj);
void DynCube_MoveBox(Terrain *t, DynCubeObj *obj, const Vec3d *pos, const Vec3d *min, const Vec3d *max);
void DynCube_Add(Terrain *t, DynCubeObj *obj, const Vec3d *pos, float radius, int category);
int DynCube_GetTree(int category, uint16_t type);
void DynCube_AddBox(Terrain *t, DynCubeObj *obj, const Vec3d *pos, const Vec3d *min, const Vec3d *max,
                    int category);
void DynCube_Remove(Terrain *t, DynCubeObj *obj);
void DynCube_Prune(Terrain *t, DynCubeNode *node);
DynCubeNode *DynCube_GetRoot(Terrain *t, int tree);

#endif
