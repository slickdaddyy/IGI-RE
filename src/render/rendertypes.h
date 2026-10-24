#ifndef RENDER_RENDERTYPES_H
#define RENDER_RENDERTYPES_H

#include <stdint.h>

typedef struct MeshGroup {
    float center[3];
    uint16_t indexCount;
    uint16_t next;      /* bytes from the indices to the next group, 0xFFFF = last */
    uint16_t texture;   /* 0xFFFF = untextured */
    uint16_t base;
    uint16_t vertexCount;
    uint16_t sorted;
    uint8_t flags;      /* bit 0: additive */
    uint8_t unk19[2];
    uint8_t alpha;
    uint16_t indices[];
} MeshGroup;

#endif
