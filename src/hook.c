#include <windows.h>
#include <string.h>

#include "hook.h"

/* EU build of IGI.exe, the one all addresses refer to. */
int Hook_IsSupportedExe(void)
{
    return memcmp((const void *)0x541ECC, "IGIMUTEX", 9) == 0;
}

static void patch_jump(uint32_t at, void *to)
{
    uint8_t *p = (uint8_t *)(uintptr_t)at;
    DWORD old;

    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    *(int32_t *)(p + 1) = (int32_t)((uintptr_t)to - at - 5);
    VirtualProtect(p, 5, old, &old);
}

typedef struct Hook {
    uint32_t addr;
    void *fn;
} Hook;

static Hook hooks[8192];
static int hook_count;

void Hook_Add(uint32_t addr, void *fn)
{
    if (hook_count < (int)(sizeof(hooks) / sizeof(hooks[0])))
        hooks[hook_count++] = (Hook){addr, fn};
}

void Hook_InstallAll(void)
{
    int i;

    if (!Hook_IsSupportedExe()) {
        MessageBoxA(NULL, "Unsupported IGI.exe version. The EU version is required.", "IGI", MB_OK | MB_ICONERROR);
        return;
    }
    for (i = 0; i < hook_count; i++)
        patch_jump(hooks[i].addr, hooks[i].fn);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
}
