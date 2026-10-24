#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

/* Standalone igidecomp.exe. IGI.exe's data sits at its original addresses: the globals the
   code defines, and fill.c around them (sections .igd and .igb, see tools/genfill.py). The
   code range before it is filled with int3, so anything that still reaches for original code
   stops with its address. The exe is linked at 0x400000 with this section first, so the
   range is ours from the start (see CMakeLists.txt); our own headers take the place of
   IGI.exe's. Calls link straight to the rewrite (src/igi.h, link.c). */
__asm__(".section .igi,\"bw\"\n\t.space 0x132000\n\t.text");

#define EXE_BASE 0x400000
#define EXE_SIZE 0x835000
#define TEXT_LO 0x401000
#define TEXT_HI 0x533000

int WINAPI AppMain_WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int show);
int Link_Bind(HMODULE msvcrt, HMODULE dinput);

extern const uint32_t igi_imports;

static void fail(const char *msg)
{
    MessageBoxA(NULL, msg, "IGI", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

static void load_imports(uint32_t at);
static void clear_code(void);

/* the code range is int3; then the import table */
static void load_data(void)
{
    DWORD old;

    if (!VirtualProtect((void *)TEXT_LO, EXE_SIZE - 0x1000, PAGE_EXECUTE_READWRITE, &old))
        fail("Couldn't open the game's memory at 0x401000.");
    clear_code();
    load_imports(igi_imports);
}

/* fill the exe's import table; some code calls through its slots */
static void load_imports(uint32_t at)
{
    const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)(uintptr_t)at;

    for (; d->Name; d++) {
        HMODULE m = LoadLibraryA((const char *)(EXE_BASE + d->Name));
        DWORD *name = (DWORD *)(EXE_BASE + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        DWORD *slot = (DWORD *)(EXE_BASE + d->FirstThunk);

        for (; *name; name++, slot++) {
            const char *fn = IMAGE_SNAP_BY_ORDINAL32(*name) ? (const char *)(uintptr_t)IMAGE_ORDINAL32(*name)
                                                            : (const char *)((IMAGE_IMPORT_BY_NAME *)(EXE_BASE + *name))->Name;
            *slot = m ? (DWORD)(uintptr_t)GetProcAddress(m, fn) : 0;
        }
    }
}

/* fill the code range with int3 */
static void clear_code(void)
{
    memset((void *)TEXT_LO, 0xCC, TEXT_HI - TEXT_LO);
}

/* an int3 in the code range is original code nothing rewrote */
static LONG CALLBACK on_trap(EXCEPTION_POINTERS *ep)
{
    uintptr_t at = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    char msg[128];

    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT || at < TEXT_LO || at >= TEXT_HI)
        return EXCEPTION_CONTINUE_SEARCH;
    wsprintfA(msg, "No rewrite for IGI.exe code at 0x%06X, called from 0x%08X.", (unsigned)at,
              (unsigned)*(DWORD *)ep->ContextRecord->Esp);
    fail(msg);
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int show)
{
    HMODULE msvcrt = LoadLibraryA("msvcrt.dll"), dinput;
    unsigned short cw = 0x27F; /* msvc's startup precision */
    char sys[MAX_PATH];

    (void)prev;
    /* the system dinput, not this project's proxy dll if it sits in the game folder; the
       import table below then finds it already loaded */
    GetSystemDirectoryA(sys, MAX_PATH - 12);
    dinput = LoadLibraryA(strcat(sys, "\\dinput.dll"));
    load_data();
    hook_exe_mapped = 1;
    Hook_InstallAll(inst);
    if (!msvcrt || !dinput || !Link_Bind(msvcrt, dinput))
        fail("msvcrt.dll is missing a function the game needs.");
    AddVectoredExceptionHandler(0, on_trap);
    __asm__ volatile("fldcw %0" : : "m"(cw));
    exit(AppMain_WinMain(inst, NULL, cmdLine, show));
}
