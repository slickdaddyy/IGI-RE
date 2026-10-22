#ifndef COLL_COLL_H
#define COLL_COLL_H

#include <stdint.h>

#include "igi.h"

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* Terrain octree node. At the deepest level, leaf points at the triangles. */
typedef struct CollNode {
    uint16_t child[8]; /* index into Terrain.nodes */
    uint8_t code[8];   /* orientation change when entering each child */
    uint8_t mask;      /* which children exist */
    uint8_t pad19[3];
    const short *leaf;
} CollNode;

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

typedef struct Terrain {
    char unk0[0xC];
    CollNode *nodes;      /* 0x0C, node 1 is the root */
    char unk10[0x10];
    ObjNode *objects;     /* 0x20 */
    char unk24[0xD18];
    void *materials[41];  /* 0xD3C */
    int origin[3];        /* 0xDE0, added to world coordinates */
} Terrain;

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

typedef struct CollHit {
    int unk0;
    int unk4;
    Vec3d pos;       /* 0x08 */
    char unk20[0x18];
    Vec3f normal;    /* 0x38 */
    int unk44;
    int unk48;
    void *material;  /* 0x4C */
    void *material2; /* 0x50 */
    int unk54;
    int found;       /* 0x58 */
    int unk5C;
    double dist;     /* 0x60 */
    char unk68[8];
} CollHit;

/* Passed to the tasks' collision handlers */
typedef struct CollQuery {
    const Vec3d *points;
    int count;
    void *results;
    char active[128]; /* a handler clears an entry to take over that point */
    char unk8C;
    int unk90;
    int unk94;
} CollQuery;

typedef struct CollLineQuery {
    const Vec3d *starts;
    const Vec3d *ends;
    int count;
    char unkC;
    int unk10;
    int unk14;
    int unk18;
    int magic; /* 'ABCD' */
    CollHit *results;
} CollLineQuery;

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
