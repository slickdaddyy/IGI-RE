#ifndef D3D_D3DTYPES_H
#define D3D_D3DTYPES_H

typedef struct DynVB {
    struct IDirect3DVertexBuffer7 *vb;
    int size;
    int used;
} DynVB;

#endif
