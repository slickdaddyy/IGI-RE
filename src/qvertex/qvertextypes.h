#ifndef QVERTEX_QVERTEXTYPES_H
#define QVERTEX_QVERTEXTYPES_H

/* What the QVertex drawing functions hand to the render driver. */
typedef struct QVertex {
    float x, y, z;
    float u, v;
    float a, r, g, b;
} QVertex;

typedef struct QVertexStrip {
    int lines;          /* a line strip instead of a triangle strip */
    unsigned int flags;
    int queue;          /* render queue, < 0 draws at once */
    int count;
    QVertex *verts;
} QVertexStrip;

#endif
