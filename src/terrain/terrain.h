#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

#include "igi.h"
#include "task/task.h"
#include "script/scripttypes.h"
#include "terrain/terraintypes.h"

#define DYNCUBE_TREES 6
#define DYNCUBE_MIN_SIZE 0x2000
#define DYNCUBE_MAX_SIZE 0x40000000

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
void DynCube_SendEventBox(Terrain *t, uint8_t slot, void *arg, const Vec3d *min, const Vec3d *max,
                          unsigned int flags, int tree);
void DynCube_SendEventLines(Terrain *t, uint8_t slot, void *arg, const Vec3d *starts, const Vec3d *ends, int count,
                            unsigned int flags, int tree);

#endif
