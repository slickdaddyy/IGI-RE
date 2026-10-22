#ifndef D3D_D3D_H
#define D3D_D3D_H

#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "igi.h"

/* A screen mode. The active one is at 0xC28B40, enumerated ones at 0xBCAE20. */
typedef struct DisplayMode {
    int unk0;
    int width;
    int height;
    int backBuffers;
    int depth;
    char unk14[4];
    int renderMode;
    char device[0x80];
    char desc[0x80];
} DisplayMode;

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

/* One Direct3D device of a driver, listed by Direct3D_ListDeviceCB. */
typedef struct D3DDeviceEntry {
    DDDriver driver;
    char desc[0x80];
    D3DDEVICEDESC7 d3dDesc;
    int hwRasterization;
} D3DDeviceEntry;

/* Context of the driver enumeration (0x491EF0 / 0x491F10). */
typedef struct DDEnumCtx {
    int listDevices;
    const char *driverName;
    const char *deviceName;
    DDDriver *drivers;
    int *count;
    D3DDeviceEntry *devices;
    int *deviceCount;
    int unk1C;
    int unk20;
    DDDriver cur;
} DDEnumCtx;

typedef struct D3DCallback {
    struct D3DCallback *next;
    void (*fn)(void);
} D3DCallback;

_Static_assert(sizeof(DisplayMode) == 0x11C, "DisplayMode");
_Static_assert(sizeof(DDDriver) == 0x4F8, "DDDriver");
_Static_assert(sizeof(D3DDeviceEntry) == 0x668, "D3DDeviceEntry");
_Static_assert(sizeof(DDEnumCtx) == 0x51C, "DDEnumCtx");

const char *DDraw_GetErrorString(HRESULT hr);

#endif
