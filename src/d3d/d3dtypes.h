#ifndef D3D_D3DTYPES_H
#define D3D_D3DTYPES_H

#include <stddef.h>
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdint.h>

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

typedef struct D3DDevice D3DDevice;

/* the slots of IDirect3DDevice7's method table that are called directly */
typedef struct D3DDeviceVtbl {
    void *unk0[25];
    long(__stdcall *DrawPrimitive)(D3DDevice *dev, int type, uint32_t fvf, void *verts, uint32_t count,
                                   uint32_t flags);
    void *unk68[11];
    long(__stdcall *SetTextureStageState)(D3DDevice *dev, int stage, int state, uint32_t value);
} D3DDeviceVtbl;
_Static_assert(offsetof(D3DDeviceVtbl, DrawPrimitive) == 0x64, "DrawPrimitive");
_Static_assert(offsetof(D3DDeviceVtbl, SetTextureStageState) == 0x94, "SetTextureStageState");

typedef struct D3DDevice {
    const D3DDeviceVtbl *vtbl;
} D3DDevice;

#endif
