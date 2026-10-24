#ifndef DXINIT_DXINITTYPES_H
#define DXINIT_DXINITTYPES_H

typedef struct RainCell {
    double x, y, z;
    float depth;
    unsigned flags; /* 0x20 = valid, bits 0-4 outcodes */
} RainCell;

#endif
