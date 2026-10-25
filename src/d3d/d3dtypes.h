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

typedef struct DDEnumCtx {
    int listDevices;
    const char *driverName;
    const char *deviceName;
    DDDriver *drivers;
    int *count;
    struct D3DDeviceEntry *devices;
    int *deviceCount;
    int unk1C;
    int unk20;
    DDDriver cur;
} DDEnumCtx;

#endif
