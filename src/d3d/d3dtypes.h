#ifndef D3D_D3DTYPES_H
#define D3D_D3DTYPES_H

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

typedef struct DynVB {
    struct IDirect3DVertexBuffer7 *vb;
    int size;
    int used;
} DynVB;

/* One DirectDraw driver, filled by the DirectDrawEnumerateEx callback at 0x491F10. */
typedef struct DDDriver {
    char name[0x80];
    char desc[0x80];
    GUID *guid;
    GUID guidData;
    DDCAPS halCaps;
    DDCAPS helCaps;
    D3DDEVICEDESC7 d3dDesc;
} DDDriver;

#endif
