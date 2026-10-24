#ifndef D3D_D3D_H
#define D3D_D3D_H

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "igi.h"
#include "win/wintypes.h"
#include "d3d/d3dtypes.h"

/* One Direct3D device of a driver, listed by Direct3D_ListDeviceCB. */
typedef struct D3DDeviceEntry {
    DDDriver driver;
    char desc[0x80];
    D3DDEVICEDESC7 d3dDesc;
    int hwRasterization;
} D3DDeviceEntry;

typedef struct D3DCallback {
    struct D3DCallback *next;
    void (*fn)(void);
} D3DCallback;

_Static_assert(sizeof(D3DDeviceEntry) == 0x668, "D3DDeviceEntry");

const char *DDraw_GetErrorString(HRESULT hr);

#endif
