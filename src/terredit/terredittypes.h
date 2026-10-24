#ifndef TERREDIT_TERREDITTYPES_H
#define TERREDIT_TERREDITTYPES_H

#include <stdint.h>

#include "types.h"

typedef struct WaterNode {
    double x, y;
    float fade;
    uint32_t flags;     /* which side of the parent: 2/4 y, 8/0x10 x */
    uint32_t edges;     /* sides that border finer neighbours */
    uint32_t level;
    uint32_t size;
    int visible;
    int active;
    struct WaterNode *parent;
} WaterNode;

typedef struct WaterMesh {
    char unk0[0x10];
    WaterNode root;         /* 0x10 */
    WaterNode top[8][8];    /* 0x40 */
    WaterNode grid[20][4][4]; /* 0xC40: the children of the nodes split at each level */
    double originX, originY; /* 0x4840 */
    char unk4850[0x20];
    Vec3d cam;              /* 0x4870 */
    double dirX, dirY;      /* 0x4888 wave direction */
    double side[2];         /* 0x4898: the direction turned by 90 degrees */
    double projX, projY;    /* 0x48A8 */
    double waves[25][2];    /* 0x48B8 */
    float camDir[3];        /* 0x4A48 */
    float ambient[3];       /* 0x4A54 */
    char unk4A60[0xC];
    float lit[3];           /* 0x4A6C: ambient plus all lights */
    double camHeight;       /* 0x4A78: camera above the water, at least 0 */
    float amp;              /* 0x4A80 */
    float wavelength;       /* 0x4A84 */
    float cell;             /* 0x4A88: wavelength / 10 */
    float phase;            /* 0x4A8C */
    float edgeZ;
    float level;            /* 0x4A94 */
    float slope;            /* 0x4A98: amp / wavelength */
    uint32_t size;          /* 0x4A9C */
    void *texture[2];       /* 0x4AA0: water, then the environment map */
    uint32_t time;          /* 0x4AA8 */
    uint32_t period1;       /* 0x4AAC */
    uint32_t period2;       /* 0x4AB0 */
    uint32_t n;             /* 0x4AB4: top grid is n x n */
    int unk4AB8;
    uint32_t count;         /* 0x4ABC */
    int envMap;             /* 0x4AC0 */
    int unk4AC4;
    int debugColors;        /* 0x4AC8 */
    void *hazeList;         /* 0x4ACC */
    void *hazeCount;        /* 0x4AD0 */
} WaterMesh;

typedef struct WaterTri {
    Vec3d pos[3];
    float color[3][3];
    float uv[3][2];
    void *hazeList;
    int envMap;
    void *texture;
    void *hazeCount;
} WaterTri;

typedef struct WaterVertex {
    char unk0[8];
    Vec3d pos;
    char unk20[0x18];
    float normal[3];        /* 0x38 */
    char unk44[8];
    uint32_t color;         /* 0x4C */
    uint32_t color2;        /* 0x50 */
    int unk54;
    int lit;                /* 0x58 */
    int unk5C;
    double depth;           /* 0x60 */
} WaterVertex;

#endif
