#ifndef GLASS_GLASSTYPES_H
#define GLASS_GLASSTYPES_H

#include <stdint.h>

/* the collision mesh of a model LOD */
typedef struct TriMesh {
    int count;
    char unk4[0xC];
    uint16_t (*tris)[4]; /* 0x10 */
    float (*verts)[4];  /* 0x14 */
} TriMesh;

#endif
