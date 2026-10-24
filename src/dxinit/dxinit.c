#include <string.h>

#include <windows.h>
#include <ddraw.h>

#include "hook.h"
#include "igi.h"

typedef void (*ErrorHandler)(const char *text);

ORIG(0x48F0A0, HWND, AppContext_GethWnd, (void));
ORIG(0x48F0B0, const char *, AppContext_GetName, (void));
ORIG(0x48F1B0, int, AppContext_IsDebugged, (void));
ORIG(0x4A5AD5, void, crt_exit, (int code));
ORIG(0x4AF860, void, Error_SetHandler, (ErrorHandler fn));
ORIG(0x4AF880, void, Warning_SetHandler, (ErrorHandler fn));
ORIG(0x4AF890, void, sub_4AF890, (ErrorHandler fn));
ORIG(0x4AF8A0, void, Error_SetExitHandler, (ErrorHandler fn));
ORIG(0x4AF8C0, void, Error_ResetHandlers, (void));
ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));

/* the exe's import table entries; OLE32 is not linked into our DLL */
extern HRESULT(WINAPI *CoCreateInstance_)(REFCLSID, IUnknown *, DWORD, REFIID, void **);
extern HRESULT(WINAPI *CoInitialize_)(void *);
extern void(WINAPI *CoUninitialize_)(void);

extern const GUID IID_IDirectDrawSurface4_[];
extern const GUID IID_IDirectDrawSurface3_[];
extern const GUID IID_IDirectDraw7_[];
extern const GUID IID_IDirectDraw2_[];
extern const GUID IID_IDirectMusic_[];
extern const GUID CLSID_DirectMusic_[];

/* A 32 bit offscreen image, the target of the "OffscreenBuffer" render mode. */
typedef struct OffscreenBuffer {
    int width;
    int height;
    unsigned *pixels;
} OffscreenBuffer;

extern OffscreenBuffer gOffscreenTarget;
extern float gOffscreenClip[];
extern unsigned short gFPUControlWord;

void OffscreenBuffer_New(OffscreenBuffer *b, int width, int height)
{
    unsigned *p = Mem_Alloc(width * height * 4, 4);

    b->pixels = p;
    b->height = height;
    b->width = width;
    memset(p, 0, width * height * 4);
}
HOOK(0x498380, OffscreenBuffer_New);

void OffscreenBuffer_Free(OffscreenBuffer *b)
{
    if (b->pixels == gOffscreenTarget.pixels) {
        gOffscreenTarget.pixels = NULL;
        gOffscreenTarget.height = 0;
        gOffscreenTarget.width = 0;
    }
    Mem_Free(b->pixels);
    b->pixels = NULL;
}
HOOK(0x4983D0, OffscreenBuffer_Free);

void OffscreenBuffer_SetTarget(OffscreenBuffer *b)
{
    gOffscreenTarget.width = b->width;
    gOffscreenTarget.height = b->height;
    gOffscreenClip[0] = 0.0f;
    gOffscreenTarget.pixels = b->pixels;
    gOffscreenClip[1] = 0.0f;
    gOffscreenClip[2] = (float)b->width;
    gOffscreenClip[3] = (float)b->height;
}
HOOK(0x498410, OffscreenBuffer_SetTarget);

/* Switches the x87 to round toward zero (float to int casts then truncate). */
void FPU_SetTruncate(void)
{
    __asm__ volatile("fstcw %0" : "=m"(gFPUControlWord));
    gFPUControlWord |= 0xC00;
    __asm__ volatile("fldcw %0" : : "m"(gFPUControlWord));
}
HOOK(0x498460, FPU_SetTruncate);

typedef HRESULT(WINAPI *DirectDrawCreateFn)(GUID *guid, IDirectDraw **out, IUnknown *outer);
typedef HRESULT(WINAPI *DirectDrawCreateExFn)(GUID *guid, void **out, REFIID iid, IUnknown *outer);

/* The DirectX SDK's GetDXVersion: version 0, 0x200, 0x300, 0x500, 0x600, 0x601 or 0x700,
   platform 1 (Windows 9x) or 2 (NT). */
void DirectX_GetVersion(DWORD *version, DWORD *platform)
{
    OSVERSIONINFOA osvi;
    DDSURFACEDESC ddsd;
    IDirectDraw *dd = NULL;
    IDirectDraw2 *dd2 = NULL;
    IDirectDrawSurface *surf = NULL;
    IDirectDrawSurface3 *surf3 = NULL;
    IDirectDrawSurface4 *surf4 = NULL;
    IUnknown *music;
    IUnknown *dd7;
    HINSTANCE ddraw, dinput;
    FARPROC fn;

    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (!GetVersionExA(&osvi)) {
        *platform = 0;
        *version = 0;
        return;
    }
    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        *platform = 2;
        if (osvi.dwMajorVersion < 4) {
            *version = 0;
            return;
        }
        if (osvi.dwMajorVersion == 4) {
            *version = 0x200;
            dinput = LoadLibraryA("DINPUT.DLL");
            if (!dinput) {
                OutputDebugStringA("Couldn't LoadLibrary DInput\r\n");
                return;
            }
            fn = GetProcAddress(dinput, "DirectInputCreateA");
            FreeLibrary(dinput);
            if (!fn) {
                OutputDebugStringA("Couldn't GetProcAddress DInputCreate\r\n");
                return;
            }
            *version = 0x300;
            return;
        }
    } else {
        *platform = 1;
    }

    ddraw = LoadLibraryA("DDRAW.DLL");
    if (!ddraw) {
        *version = 0;
        *platform = 0;
        FreeLibrary(ddraw);
        return;
    }
    fn = GetProcAddress(ddraw, "DirectDrawCreate");
    if (!fn) {
        *version = 0;
        *platform = 0;
        FreeLibrary(ddraw);
        OutputDebugStringA("Couldn't LoadLibrary DDraw\r\n");
        return;
    }
    if (((DirectDrawCreateFn)fn)(NULL, &dd, NULL) < 0) {
        *version = 0;
        *platform = 0;
        FreeLibrary(ddraw);
        OutputDebugStringA("Couldn't create DDraw\r\n");
        return;
    }
    *version = 0x100;
    if (dd->lpVtbl->QueryInterface(dd, IID_IDirectDraw2_, (void **)&dd2) < 0) {
        dd->lpVtbl->Release(dd);
        FreeLibrary(ddraw);
        OutputDebugStringA("Couldn't QI DDraw2\r\n");
        return;
    }
    dd2->lpVtbl->Release(dd2);
    *version = 0x200;

    dinput = LoadLibraryA("DINPUT.DLL");
    if (!dinput) {
        OutputDebugStringA("Couldn't LoadLibrary DInput\r\n");
        dd->lpVtbl->Release(dd);
        FreeLibrary(ddraw);
        return;
    }
    if (!GetProcAddress(dinput, "DirectInputCreateA")) {
        FreeLibrary(dinput);
        FreeLibrary(ddraw);
        dd->lpVtbl->Release(dd);
        OutputDebugStringA("Couldn't GetProcAddress DInputCreate\r\n");
        return;
    }
    *version = 0x300;
    FreeLibrary(dinput);

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    if (dd->lpVtbl->SetCooperativeLevel(dd, NULL, DDSCL_NORMAL) < 0) {
        dd->lpVtbl->Release(dd);
        FreeLibrary(ddraw);
        *version = 0;
        OutputDebugStringA("Couldn't Set coop level\r\n");
        return;
    }
    if (dd->lpVtbl->CreateSurface(dd, &ddsd, &surf, NULL) < 0) {
        dd->lpVtbl->Release(dd);
        FreeLibrary(ddraw);
        *version = 0;
        OutputDebugStringA("Couldn't CreateSurface\r\n");
        return;
    }
    if (surf->lpVtbl->QueryInterface(surf, IID_IDirectDrawSurface3_, (void **)&surf3) < 0) {
        dd->lpVtbl->Release(dd);
        FreeLibrary(ddraw);
        return;
    }
    *version = 0x500;
    if (surf->lpVtbl->QueryInterface(surf, IID_IDirectDrawSurface4_, (void **)&surf4) < 0) {
        dd->lpVtbl->Release(dd);
        FreeLibrary(ddraw);
        return;
    }
    *version = 0x600;
    surf->lpVtbl->Release(surf);
    dd->lpVtbl->Release(dd);

    music = NULL;
    CoInitialize_(NULL);
    if (CoCreateInstance_(CLSID_DirectMusic_, NULL, CLSCTX_INPROC_SERVER, IID_IDirectMusic_, (void **)&music) < 0) {
        OutputDebugStringA("Couldn't create CLSID_DirectMusic\r\n");
        FreeLibrary(ddraw);
        return;
    }
    *version = 0x601;
    music->lpVtbl->Release(music);
    CoUninitialize_();

    fn = GetProcAddress(ddraw, "DirectDrawCreateEx");
    if (fn && ((DirectDrawCreateExFn)fn)(NULL, (void **)&dd7, IID_IDirectDraw7_, NULL) >= 0) {
        *version = 0x700;
        dd7->lpVtbl->Release(dd7);
    }
    FreeLibrary(ddraw);
}
HOOK(0x498480, DirectX_GetVersion);

void App_MessageBox(const char *text)
{
    const char *caption;

    ShowCursor(TRUE);
    caption = AppContext_GetName();
    MessageBoxA(AppContext_GethWnd(), text, caption, MB_ICONHAND);
    ShowCursor(FALSE);
}
HOOK(0x4988D0, App_MessageBox);

void App_ErrorExit(const char *text);

void App_SetErrorHandlers(void)
{
    Error_SetExitHandler(App_ErrorExit);
    Error_SetHandler(App_MessageBox);
    Warning_SetHandler(App_MessageBox);
    sub_4AF890(App_MessageBox);
}
HOOK(0x498900, App_SetErrorHandlers);

void App_ErrorExit(const char *text)
{
    (void)text;
    if (AppContext_IsDebugged())
        DebugBreak();
    crt_exit(1);
}
HOOK(0x498930, App_ErrorExit);

void App_ResetErrorHandlers(void)
{
    Error_ResetHandlers();
    App_SetErrorHandlers();
}
HOOK(0x498950, App_ResetErrorHandlers);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
PAD(igd, 533218, 533218, 0x4) INIT({0x1A, 0x56, 0x13});
ALIAS(CoCreateInstance_, pad_533218, 0);
PAD(igd, 533218, 53321C, 0x4) INIT({0x0A, 0x56, 0x13});
ALIAS(CoInitialize_, pad_53321C, 0);
PAD(igd, 533218, 533220, 0x4) INIT({0xF8, 0x55, 0x13});
ALIAS(CoUninitialize_, pad_533220, 0);
PAD_CONST(igd, 5348E0, 5348E0, 0x10) INIT({0x5A, 0x11, 0x36, 0x65, 0x2D, 0x7B, 0xD2, 0x11, 0xBA, 0x18, 0x00, 0x00, 0xF8, 0x75, 0xAC, 0x12});
ALIAS(IID_IDirectMusic_, pad_5348E0, 0);
PAD_CONST(igd, 5348E0, 5348F0, 0x10) INIT({0x10, 0x9F, 0x6B, 0x63, 0x7D, 0x0C, 0xD1, 0x11, 0x95, 0xB2, 0x00, 0x20, 0xAF, 0xDC, 0x74, 0x21});
ALIAS(CLSID_DirectMusic_, pad_5348F0, 0);
PAD_CONST(igd, 534A30, 534A30, 0x10) INIT({0x30, 0x86, 0x2B, 0x0B, 0x35, 0xAD, 0xD0, 0x11, 0x8E, 0xA6, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B});
ALIAS(IID_IDirectDrawSurface4_, pad_534A30, 0);
PAD_CONST(igd, 534A30, 534A40, 0x10) INIT({0x00, 0x4E, 0x04, 0xDA, 0xB2, 0x69, 0xD0, 0x11, 0xA1, 0xD5, 0x00, 0xAA, 0x00, 0xB8, 0xDF, 0xBB});
ALIAS(IID_IDirectDrawSurface3_, pad_534A40, 0);
PAD_CONST(igd, 534A60, 534A60, 0x10) INIT({0xE0, 0xF3, 0xA6, 0xB3, 0x43, 0x2B, 0xCF, 0x11, 0xA2, 0xDE, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56});
ALIAS(IID_IDirectDraw2_, pad_534A60, 0);
BSS(6E5BC4) unsigned short gFPUControlWord;
PAD(igb, 6E5BC4, 6E5BC6, 0x2);
BSS(BCAAA0) OffscreenBuffer gOffscreenTarget;
ALIAS(gOffscreenHeight, gOffscreenTarget, 4);
ALIAS(gOffscreenPixels, gOffscreenTarget, 8);
ALIAS(gOffscreenWidth, gOffscreenTarget, 0);
