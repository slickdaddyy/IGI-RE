#include <string.h>

#include <ddraw.h>
#include <d3d.h>

#include "hook.h"
#include "win/win.h"

typedef void (*ErrorHandler)(const char *text);
typedef struct RenderModeInfo {
    int index;
    const char *name;
} RenderModeInfo;

ORIG(0x4AF860, void, Error_SetHandler, (ErrorHandler fn));
ORIG(0x4AF880, void, Warning_SetHandler, (ErrorHandler fn));
ORIG(0x4AF890, void, sub_4AF890, (ErrorHandler fn));
ORIG(0x4B0E70, int, RenderMode_CreateMethod, (const char *name));
ORIG(0x4B0EE0, void, RenderMode_ForEach, (void (*fn)(RenderModeInfo *info, void *user), void *user));
ORIG(0x4B0F30, void, RenderMode_Select, (int mode));
ORIG(0x4B0F70, void, RenderMode_Deselect, (void));
ORIG(0x4B1000, int, RenderMode_Call, (int mode, int method, void *arg));
ORIG_STD(0x492230, HRESULT, Direct3D_FindDeviceCB, (char *desc, char *name, D3DDEVICEDESC7 *dev, void *ctx));
ORIG_STD(0x4922B0, HRESULT, Direct3D_ListDeviceCB, (char *desc, char *name, D3DDEVICEDESC7 *dev, void *ctx));
ORIG(0x492370, void, Guid_ToString, (char *out, GUID guid));
ORIG(0x492BE0, const char *, DDraw_GetErrorString, (HRESULT hr));
ORIG(0x493580, void, Direct3D_Init, (void));
ORIG(0x495060, void, Direct3D_Shutdown, (void));
ORIG(0x495220, void, Direct3D_RemoveCallback, (void (*fn)(void), void *list));
ORIG(0x497700, void, Scene_DrawAll, (void));
ORIG(0x4977F0, void, Frame_Reset, (void));
ORIG(0x497860, void, Primitive_Draw, (void *prim));
ORIG(0x4978D0, int, Primitive_Register, (const char *name));
ORIG(0x498080, void, Offscreen_Init, (void));
ORIG(0x498350, void, Offscreen_Shutdown, (void));
ORIG(0x4988D0, void, App_MessageBox, (const char *text));
ORIG(0x4C1780, void, Debug_Log, (void));

/* the exe's import thunks; DDRAW.dll is not linked into our DLL */
ORIG_STD(0x4AF74C, HRESULT, DirectDrawEnumerateExA_, (LPDDENUMCALLBACKEXA fn, void *ctx, DWORD flags));
ORIG_STD(0x4AF752, HRESULT, DirectDrawCreateEx_, (GUID *guid, void **out, REFIID iid, IUnknown *outer));

#define IID_IDirect3D7_ ((const GUID *)0x534A20)
#define IID_IDirectDraw7_ ((const GUID *)0x534A50)

typedef struct DisplayCallback {
    struct DisplayCallback *next;
    void (*fn)(void);
} DisplayCallback;

#define DISPLAY_CALLBACK_MAX 32

/* One DirectDraw driver. */
typedef struct DDDriver {
    char name[0x80];
    char desc[0x80];
    GUID *guid;
    GUID guidData;
    DDCAPS halCaps;
    DDCAPS helCaps;
    D3DDEVICEDESC7 d3dDesc;
} DDDriver;

/* What Display_EnumDrivers passes to its callback. */
typedef struct DDEnumCtx {
    int listDevices;
    const char *driverName;
    const char *deviceName;
    DDDriver *drivers;
    int *count;
    void *devices;
    int *deviceCount;
    int unk1C;
    int unk20;
    DDDriver cur;
} DDEnumCtx;

_Static_assert(sizeof(DisplayMode) == 0x11C, "DisplayMode");
_Static_assert(sizeof(DDDriver) == 0x4F8, "DDDriver");
_Static_assert(offsetof(DDEnumCtx, cur.d3dDesc) == 0x430, "DDEnumCtx");

#define gDisplayOpen VAR(char, 0x5CA02C)
#define gDisplayModeSet VAR(char, 0x5CA02D)
#define gDisplayDevice ((char *)0x5C9FA8)
#define gDisplayModes ((DisplayMode *)0xBCAE20)
#define gDisplayModeCount VAR(int, 0xC10384)
#define gDisplayFrame VAR(int, 0xC10380)
#define gDisplayMode VAR(DisplayMode, 0xC28B40)
#define gDisplayCallbacks ((DisplayCallback *)0xC28A40)
#define gDisplaySetModeCallbacks VAR(DisplayCallback *, 0xBCADEC)
#define gDisplayClearColor ((int *)0xBCADF0)
#define gDisplayEndFrameCallbacks VAR(DisplayCallback *, 0xBCADFC)
#define gDisplayFrameCallbacks VAR(DisplayCallback *, 0xBCAE00)
#define gD3DFoundHAL VAR(int, 0xBCACF4)
#define gD3DTnL VAR(int, 0xBCAD74)
#define gRenderFuncs ((void (**)(void *))0xA94E84)

/* RenderMode method ids */
#define gMethodGetGFXCaps VAR(int, 0x542310)
#define gMethodOpen VAR(int, 0x542314)
#define gMethodClose VAR(int, 0x542318)
#define gMethodSetMode VAR(int, 0x54231C)
#define gMethodUnsetMode VAR(int, 0x542320)
#define gMethodRestore VAR(int, 0x542324)
#define gMethodVSync VAR(int, 0x542328)
#define gMethodSwapBuffers VAR(int, 0x54232C)
#define gMethodClear VAR(int, 0x542330)
#define gMethodEnum VAR(int, 0x542334)
#define gPrimBlit VAR(int, 0x542338)
#define gPrimSnapshot VAR(int, 0x54233C)
#define gPrimClipWindow VAR(int, 0x542340)

static const char kDXError[] = "%s\n\nDirectX error message: %s";

void Display_SetPreferredDevice(const char *desc, const char *device)
{
    gDisplayPrefDesc = desc;
    gDisplayPrefDevice = device;
}
HOOK(0x491610, Display_SetPreferredDevice);

void Display_AddCallback(void (*fn)(void), DisplayCallback **list);

void Display_AddSetModeCallback(void (*fn)(void))
{
    Display_AddCallback(fn, &gDisplaySetModeCallbacks);
}
HOOK(0x491630, Display_AddSetModeCallback);

void Display_AddCallback(void (*fn)(void), DisplayCallback **list)
{
    int i;

    for (i = 0; i < DISPLAY_CALLBACK_MAX; i++) {
        if (!gDisplayCallbacks[i].fn) {
            gDisplayCallbacks[i].next = *list;
            gDisplayCallbacks[i].fn = fn;
            *list = &gDisplayCallbacks[i];
            return;
        }
    }
    Error_Show("No more Display callbacks available.");
    for (;;)
        ;
}
HOOK(0x491650, Display_AddCallback);

void Display_RemoveSetModeCallback(void (*fn)(void))
{
    Direct3D_RemoveCallback(fn, &gDisplaySetModeCallbacks);
}
HOOK(0x4916A0, Display_RemoveSetModeCallback);

void Display_AddFrameCallback(void (*fn)(void))
{
    Display_AddCallback(fn, &gDisplayFrameCallbacks);
}
HOOK(0x4916C0, Display_AddFrameCallback);

void Display_RemoveFrameCallback(void (*fn)(void))
{
    Direct3D_RemoveCallback(fn, &gDisplayFrameCallbacks);
}
HOOK(0x4916E0, Display_RemoveFrameCallback);

void Display_ClearCallbacks(void);
void Display_ErrorHandler(const char *text);
void Display_WarningHandler(const char *text);
void Display_CallMethodCB(RenderModeInfo *info, int *args);
int Display_GetOpenMethod(void);

void Display_Open(void)
{
    int args[2];

    if (gDisplayOpen) {
        Error_Show("Display can only be opened once!");
        for (;;)
            ;
    }
    gDisplayDevice[0] = 0;
    gMethodOpen = RenderMode_CreateMethod("DisplayOpen");
    gMethodClose = RenderMode_CreateMethod("DisplayClose");
    gMethodSetMode = RenderMode_CreateMethod("DisplaySetMode");
    gMethodUnsetMode = RenderMode_CreateMethod("DisplayUnsetMode");
    gMethodRestore = RenderMode_CreateMethod("DisplayRestore");
    gMethodVSync = RenderMode_CreateMethod("DisplayVSync");
    gMethodSwapBuffers = RenderMode_CreateMethod("DisplaySwapBuffers");
    gMethodClear = RenderMode_CreateMethod("DisplayClear");
    gMethodEnum = RenderMode_CreateMethod("Enum");
    gMethodGetGFXCaps = RenderMode_CreateMethod("GetGFXCaps");
    gDisplayModeSet = 0;
    gDisplayModeCount = 0;
    Display_ClearCallbacks();
    Debug_Log();
    gPrimClipWindow = Primitive_Register("ClipWindowPrimitive");
    gPrimBlit = Primitive_Register("BlitPrimitive");
    gPrimSnapshot = Primitive_Register("SnapshotPrimitive");
    Direct3D_Init();
    Offscreen_Init();
    args[0] = Display_GetOpenMethod();
    args[1] = 0;
    RenderMode_ForEach((void (*)(RenderModeInfo *, void *))Display_CallMethodCB, args);
    Error_SetHandler(Display_ErrorHandler);
    Warning_SetHandler(Display_WarningHandler);
    sub_4AF890(Display_WarningHandler);
    gDisplayOpen = 1;
}
HOOK(0x491700, Display_Open);

void Display_ClearCallbacks(void)
{
    int i;

    gDisplaySetModeCallbacks = NULL;
    gDisplayFrameCallbacks = NULL;
    gDisplayEndFrameCallbacks = NULL;
    for (i = 0; i < DISPLAY_CALLBACK_MAX; i++)
        gDisplayCallbacks[i].fn = NULL;
}
HOOK(0x491860, Display_ClearCallbacks);

void Display_UnsetMode(DisplayMode *next);

/* Warnings and errors drop out of the screen mode to show the message box. */
void Display_WarningHandler(const char *text)
{
    DisplayMode mode = gDisplayMode;

    if (gDisplayOpen)
        Display_UnsetMode(NULL);
    App_MessageBox(text);
    if (gDisplayOpen)
        Display_SetMode(&mode);
}
HOOK(0x491890, Display_WarningHandler);

void Display_ErrorHandler(const char *text)
{
    if (gDisplayOpen)
        Display_UnsetMode(NULL);
    App_MessageBox(text);
}
HOOK(0x4918F0, Display_ErrorHandler);

void Display_CallMethodCB(RenderModeInfo *info, int *args)
{
    RenderMode_Call(info->index, args[0], (void *)args[1]);
}
HOOK(0x491910, Display_CallMethodCB);

int Display_GetCloseMethod(void);

void Display_Close(void)
{
    int args[2];

    if (gDisplayModeSet)
        Display_UnsetMode(NULL);
    args[0] = Display_GetCloseMethod();
    args[1] = 0;
    RenderMode_ForEach((void (*)(RenderModeInfo *, void *))Display_CallMethodCB, args);
    Direct3D_Shutdown();
    Offscreen_Shutdown();
    gDisplayOpen = 0;
}
HOOK(0x491930, Display_Close);

/* Counts the known modes matching mode in the fields picked by flags
   (1 width, 2 height, 4 depth, 8 render mode) and passes each to fn. */
int Display_FindModes(DisplayMode *mode, int flags, void (*fn)(DisplayMode *mode))
{
    DisplayMode *m;
    int i, count = 0, match;

    for (i = 0; i < gDisplayModeCount; i++) {
        m = &gDisplayModes[i];
        if (mode) {
            match = 1;
            if ((flags & 1) && mode->width != m->width)
                match = 0;
            if ((flags & 2) && mode->height != m->height)
                match = 0;
            if ((flags & 4) && mode->depth != m->depth)
                match = 0;
            if ((flags & 8) && mode->renderMode != m->renderMode)
                continue;
            if (!match)
                continue;
        }
        if (fn)
            fn(m);
        count++;
    }
    return count;
}
HOOK(0x491980, Display_FindModes);

int Display_GetClearMethod(void);
int Display_GetUnsetModeMethod(void);
int Display_GetSetModeMethod(void);
int Display_GetSwapBuffersMethod(void);
void Display_RunCallbacks(DisplayCallback *list);
DisplayMode *Display_GetMode(void);

void Display_Clear(void)
{
    if (gDisplayModeSet)
        gRenderFuncs[Display_GetClearMethod()](NULL);
}
HOOK(0x491A30, Display_Clear);

void Display_UnsetMode(DisplayMode *next)
{
    if (gDisplayModeSet) {
        gRenderFuncs[Display_GetUnsetModeMethod()](next);
        gDisplayMode.renderMode = -1;
        RenderMode_Deselect();
        gDisplayModeSet = 0;
    }
}
HOOK(0x491A50, Display_UnsetMode);

void Display_SetMode(DisplayMode *mode)
{
    RECT r;
    HWND hwnd;
    LONG style;
    int sw, sh;

    if (gDisplayModeSet) {
        if (!mode->device[0])
            strcpy(mode->device, gDisplayDevice);
        if (mode->width == gDisplayMode.width && mode->height == gDisplayMode.height
            && mode->depth == gDisplayMode.depth && mode->renderMode == gDisplayMode.renderMode
            && mode->unk16 == gDisplayMode.unk16 && !strcmp(mode->device, gDisplayMode.device))
            return;
    }
    Display_RunCallbacks(gDisplaySetModeCallbacks);
    if (gDisplayModeSet)
        Display_UnsetMode(mode);
    if (AppContext_IsFullscreen()) {
        r.left = 0;
        r.top = 0;
        r.right = GetSystemMetrics(SM_CXSCREEN);
        r.bottom = GetSystemMetrics(SM_CYSCREEN);
    } else {
        sw = GetSystemMetrics(SM_CXSCREEN);
        r.left = (sw - mode->width) / 2;
        sh = GetSystemMetrics(SM_CYSCREEN);
        r.top = (sh - mode->height) / 2;
        r.right = r.left + mode->width;
        r.bottom = r.top + mode->height;
    }
    style = GetWindowLongA(AppContext_GethWnd(), GWL_STYLE);
    AdjustWindowRect(&r, style, FALSE);
    hwnd = AppContext_GethWnd();
    SetWindowPos(hwnd, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_SHOWWINDOW);
    UpdateWindow(AppContext_GethWnd());
    SetFocus(AppContext_GethWnd());
    RenderMode_Select(mode->renderMode);
    gRenderFuncs[Display_GetSetModeMethod()](mode);
    gDisplayModeSet = 1;
    strcpy(gDisplayDevice, Display_GetMode()->device);
}
HOOK(0x491A90, Display_SetMode);

void Display_RunCallbacks(DisplayCallback *list)
{
    for (; list; list = list->next)
        list->fn();
}
HOOK(0x491CA0, Display_RunCallbacks);

void Display_SwapBuffers(void)
{
    if (gDisplayModeSet)
        gRenderFuncs[Display_GetSwapBuffersMethod()](NULL);
    gDisplayFrame++;
}
HOOK(0x491CC0, Display_SwapBuffers);

DisplayMode *Display_GetMode(void)
{
    return &gDisplayMode;
}
HOOK(0x491CF0, Display_GetMode);

void Display_NextFrame(void)
{
    Display_SwapBuffers();
    Display_Clear();
    Frame_Reset();
    Display_RunCallbacks(gDisplayFrameCallbacks);
}
HOOK(0x491D00, Display_NextFrame);

void Display_EndFrame(void)
{
    Scene_DrawAll();
    Display_RunCallbacks(gDisplayEndFrameCallbacks);
}
HOOK(0x491D20, Display_EndFrame);

/* Pixel aspect compared to 4:3, returned unrounded in st(0). */
long double Display_GetAspect(void)
{
    int width = Display_GetMode()->width;

    return (long double)Display_GetMode()->height * 4.0f / ((long double)width * 3.0f);
}
HOOK(0x491D40, Display_GetAspect);

void Display_DrawPrimitive(void *prim)
{
    Primitive_Draw(prim);
}
HOOK(0x491D70, Display_DrawPrimitive);

int Display_GetSnapshotPrim(void);

void Display_Snapshot(void *dest)
{
    struct {
        int unk0;
        int prim;
        int unk8;
        void *dest;
    } p;

    p.prim = Display_GetSnapshotPrim();
    p.dest = dest;
    Primitive_Draw(&p);
}
HOOK(0x491D80, Display_Snapshot);

int Display_GetClipWindowPrim(void)
{
    return gPrimClipWindow;
}
HOOK(0x491DB0, Display_GetClipWindowPrim);

int Display_GetBlitPrim(void)
{
    return gPrimBlit;
}
HOOK(0x491DC0, Display_GetBlitPrim);

int Display_GetSnapshotPrim(void)
{
    return gPrimSnapshot;
}
HOOK(0x491DD0, Display_GetSnapshotPrim);

int Display_GetGFXCapsMethod(void)
{
    return gMethodGetGFXCaps;
}
HOOK(0x491DE0, Display_GetGFXCapsMethod);

int Display_GetOpenMethod(void)
{
    return gMethodOpen;
}
HOOK(0x491DF0, Display_GetOpenMethod);

int Display_GetCloseMethod(void)
{
    return gMethodClose;
}
HOOK(0x491E00, Display_GetCloseMethod);

int Display_GetSetModeMethod(void)
{
    return gMethodSetMode;
}
HOOK(0x491E10, Display_GetSetModeMethod);

int Display_GetUnsetModeMethod(void)
{
    return gMethodUnsetMode;
}
HOOK(0x491E20, Display_GetUnsetModeMethod);

int Display_GetVSyncMethod(void)
{
    return gMethodVSync;
}
HOOK(0x491E30, Display_GetVSyncMethod);

int Display_GetSwapBuffersMethod(void)
{
    return gMethodSwapBuffers;
}
HOOK(0x491E40, Display_GetSwapBuffersMethod);

int Display_GetClearMethod(void)
{
    return gMethodClear;
}
HOOK(0x491E50, Display_GetClearMethod);

int Display_GetEnumMethod(void)
{
    return gMethodEnum;
}
HOOK(0x491E60, Display_GetEnumMethod);

void Display_SetClearColor(int r, int g, int b)
{
    gDisplayClearColor[0] = r & 0xFF;
    gDisplayClearColor[1] = g & 0xFF;
    gDisplayClearColor[2] = b & 0xFF;
}
HOOK(0x491E70, Display_SetClearColor);

/* Asks the render mode for its list of screen modes. Returns how many it gave. */
int Display_EnumModes(void *out, int max)
{
    struct {
        void *out;
        int *count;
        int max;
    } args;
    int count = 0;

    args.out = out;
    args.count = &count;
    args.max = max;
    if (!gRenderFuncs[gMethodEnum])
        return 0;
    gRenderFuncs[gMethodEnum](&args);
    return count;
}
HOOK(0x491EA0, Display_EnumModes);

BOOL WINAPI Display_EnumDriverCB(GUID *guid, char *desc, char *name, void *context, HMONITOR monitor);

HRESULT Display_EnumDrivers(DDEnumCtx *ctx)
{
    return DirectDrawEnumerateExA_(Display_EnumDriverCB, ctx, DDENUM_ATTACHEDSECONDARYDEVICES
                                   | DDENUM_DETACHEDSECONDARYDEVICES | DDENUM_NONDISPLAYDEVICES);
}
HOOK(0x491EF0, Display_EnumDrivers);

BOOL WINAPI Display_EnumDriverCB(GUID *guid, char *desc, char *name, void *context, HMONITOR monitor)
{
    DDEnumCtx *ctx = context;
    IDirectDraw7 *dd;
    IDirect3D7 *d3d;
    DDDriver *d;
    char msg[0x400];
    HRESULT hr;

    hr = DirectDrawCreateEx_(guid, (void **)&dd, IID_IDirectDraw7_, NULL);
    if (hr) {
        crt_sprintf(msg, "Couldn't create DirectDraw object");
        Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
        for (;;)
            ;
    }
    hr = dd->lpVtbl->QueryInterface(dd, IID_IDirect3D7_, (void **)&d3d);
    if (hr) {
        crt_sprintf(msg, "Couldn't get the Direct3D interface");
        Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
        for (;;)
            ;
    }
    if (guid)
        ctx->cur.guidData = *guid;
    else
        memset(&ctx->cur.guidData, 0, sizeof(GUID));
    Guid_ToString(ctx->cur.name, ctx->cur.guidData);
    ctx->cur.halCaps.dwSize = sizeof(DDCAPS);
    ctx->cur.helCaps.dwSize = sizeof(DDCAPS);
    hr = dd->lpVtbl->GetCaps(dd, &ctx->cur.halCaps, &ctx->cur.helCaps);
    if (hr) {
        crt_sprintf(msg, "Couldn't get driver caps");
        Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
        for (;;)
            ;
    }
    if (!ctx->listDevices) {
        gD3DFoundHAL = 0;
        gD3DTnL = 0;
        hr = d3d->lpVtbl->EnumDevices(d3d, (LPD3DENUMDEVICESCALLBACK7)Direct3D_FindDeviceCB,
                                      &ctx->cur.d3dDesc);
        if (hr) {
            crt_sprintf(msg, "Couldn't enumerate Direct3D devices");
            Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
            for (;;)
                ;
        }
        /* keep drivers with a usable device, or only the named one */
        if (gD3DFoundHAL && (!ctx->driverName || !strcmp(ctx->cur.name, ctx->driverName))) {
            d = &ctx->drivers[*ctx->count];
            *d = ctx->cur;
            strcpy(d->desc, desc);
            strcpy(d->name, ctx->cur.name);
            d->guid = NULL;
            if (guid)
                d->guid = &d->guidData;
            (*ctx->count)++;
            if (ctx->driverName) {
                d3d->lpVtbl->Release(d3d);
                dd->lpVtbl->Release(dd);
                return FALSE;
            }
        }
    } else {
        hr = d3d->lpVtbl->EnumDevices(d3d, (LPD3DENUMDEVICESCALLBACK7)Direct3D_ListDeviceCB, ctx);
        if (hr) {
            crt_sprintf(msg, "Couldn't enumerate Direct3D devices");
            Error_Show(kDXError, msg, DDraw_GetErrorString(hr));
            for (;;)
                ;
        }
    }
    d3d->lpVtbl->Release(d3d);
    dd->lpVtbl->Release(dd);
    return TRUE;
}
HOOK(0x491F10, Display_EnumDriverCB);
