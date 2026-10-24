#include <windows.h>

#include "hook.h"

typedef HRESULT (WINAPI *CreateA)(HINSTANCE, DWORD, void **, void *);
typedef HRESULT (WINAPI *CreateEx)(HINSTANCE, DWORD, const GUID *, void **, void *);

static HMODULE real_dinput;

static FARPROC real_proc(const char *name)
{
    char path[MAX_PATH];

    if (!real_dinput) {
        GetSystemDirectoryA(path, MAX_PATH);
        lstrcatA(path, "\\dinput.dll");
        real_dinput = LoadLibraryA(path);
    }
    return real_dinput ? GetProcAddress(real_dinput, name) : NULL;
}

HRESULT WINAPI DirectInputCreateA(HINSTANCE inst, DWORD version, void **out, void *outer)
{
    CreateA fn = (CreateA)real_proc("DirectInputCreateA");
    return fn ? fn(inst, version, out, outer) : E_FAIL;
}

HRESULT WINAPI DirectInputCreateEx(HINSTANCE inst, DWORD version, const GUID *iid, void **out, void *outer)
{
    CreateEx fn = (CreateEx)real_proc("DirectInputCreateEx");
    return fn ? fn(inst, version, iid, out, outer) : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        Hook_InstallAll(inst);
    return TRUE;
}
