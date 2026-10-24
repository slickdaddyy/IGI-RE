#ifndef COLL_COLL_H
#define COLL_COLL_H

#include <stdint.h>

#include "igi.h"
#include "types.h"
#include "coll/colltypes.h"
#include "terrain/terraintypes.h"

/* A task in the object octree (only the fields used here) */
typedef struct CollTask {
    char unk0[0x1C];
    uint16_t type;
    char unk1E[0x2A];
    int arg;               /* 0x48 */
    char unk4C[0xC];
    struct CollTask *next; /* 0x58 */
    uint16_t flags;        /* 0x5C */
} CollTask;

/* Object octree node */
typedef struct ObjNode {
    char unk0[0xC];
    struct ObjNode *child[8]; /* 0x0C */
    CollTask *tasks;          /* 0x2C */
    char unk30[8];
    uint16_t flags;           /* 0x38 */
} ObjNode;

/* Position while walking down the terrain octree. Coordinates are
   world + origin, and the root cell covers the whole 2^31 range. */
typedef struct CollCursor {
    int size;   /* half the width of the current cell */
    int level;  /* coordinate bit that selects the child */
    int orient; /* 0-7, children are stored in a rotated order */
    int x, y, z;
    CollNode *node;
    Terrain *terrain;
    ObjNode *objects;
} CollCursor;

void Coll_GetPointMaterials(Terrain *t, void **results, const Vec3d *points, int count, char unk, int unk2,
                            unsigned int flags);
int Coll_Descend(Terrain *t, CollCursor *c, CollCursor *unused, const Vec3d *p, int depth);
void *Coll_GetLeafMaterial(CollCursor *c, const Vec3d *p, int depth);
int Coll_LeafHit(const short *leaf, const Vec3d *base, const Vec3d *p, Vec3d *hit, int shift, int *side,
                 unsigned int flags, int up, Vec3f *normal);
void Coll_DropPoints(Terrain *t, CollHit *results, const Vec3d *points, int count, const double *dz, int unk,
                     int unk2, unsigned int flags);
int Coll_VerticalHit(CollHit *out, CollCursor *c, const Vec3d *p, int up, int range, int depth, int material);
unsigned int Coll_GetHitMaterial(Terrain *t, const Vec3d *p);
void Coll_DropPointsMat(Terrain *t, CollHit *results, const Vec3d *points, int count, const double *dz, int unk,
                        int unk2, unsigned int flags);
void Coll_DropPointsEx(Terrain *t, CollHit *results, const Vec3d *points, int count, const double *dz, int unk,
                       int unk2, unsigned int flags, int unk3);
void Coll_Lines(Terrain *t, CollHit *results, const Vec3d *starts, const Vec3d *ends, int count, char unk,
                int unk2, unsigned int flags);

#endif
