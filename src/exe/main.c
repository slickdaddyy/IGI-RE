#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

/* Standalone igidecomp.exe. It copies the data of the player's IGI.exe (never its code) to
   the original addresses and fills the code range with int3, so anything that still reaches
   for original code stops with its address. The exe is linked at 0x400000 with this section
   first, so the range is ours from the start (see CMakeLists.txt); our own headers take the
   place of IGI.exe's. Calls link straight to the rewrite (src/igi.h, link.c). */
__asm__(".section .igi,\"bw\"\n\t.space 0x834000\n\t.text");

#define EXE_BASE 0x400000
#define EXE_SIZE 0x835000
#define EXE_TIME 0x3A1D74C6
#define TEXT_LO 0x401000
#define TEXT_HI 0x533000

int WINAPI AppMain_WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int show);
int Link_Bind(HMODULE msvcrt, HMODULE dinput);

/* tables in IGI.exe's data that hold code addresses (handlers, the lzss pair) */
static const uint32_t code_tables[][2] = {
    {0x5385B4, 0x5386AC}, {0x53E400, 0x53E434}, {0x53E6A8, 0x53E6E4},
    {0x53E8B8, 0x53E950}, {0x53EC10, 0x53ECA8}, {0x54F780, 0x54F790},
};

/* data that dinput.lib left in the code section: the mouse and keyboard object formats */
static const uint32_t text_data[][2] = {{0x4AE680, 0xB0}, {0x4AE740, 0x1000}};

static void fail(const char *msg)
{
    MessageBoxA(NULL, msg, "IGI", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

static void load_imports(uint32_t dir);
static void clear_code(void);

/* copy the non-code sections of the IGI.exe next to us to their original addresses */
static void load_exe(void)
{
    char path[MAX_PATH], *slash;
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *s;
    uint8_t *file;
    DWORD size, got, n;
    HANDLE f;
    int i;

    GetModuleFileNameA(NULL, path, MAX_PATH - 8);
    slash = strrchr(path, '\\');
    strcpy(slash ? slash + 1 : path, "IGI.exe");
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        fail("IGI.exe not found. Put this program in the game folder.");
    size = GetFileSize(f, NULL);
    file = size == INVALID_FILE_SIZE ? NULL : malloc(size);
    if (!file || !ReadFile(f, file, size, &got, NULL) || got != size)
        fail("Couldn't read IGI.exe.");
    CloseHandle(f);

    dos = (const IMAGE_DOS_HEADER *)file;
    nt = (const IMAGE_NT_HEADERS32 *)(file + dos->e_lfanew);
    if (size < 0x1000 || dos->e_magic != IMAGE_DOS_SIGNATURE || (DWORD)dos->e_lfanew > 0x800 ||
        nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp != EXE_TIME ||
        nt->OptionalHeader.ImageBase != EXE_BASE || nt->OptionalHeader.SizeOfImage != EXE_SIZE)
        fail("Unsupported IGI.exe version. The EU version is required.");
    if (!VirtualProtect((void *)TEXT_LO, EXE_SIZE - 0x1000, PAGE_EXECUTE_READWRITE, &got))
        fail("Couldn't open the game's memory at 0x401000.");

    s = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        n = s->SizeOfRawData < s->Misc.VirtualSize ? s->SizeOfRawData : s->Misc.VirtualSize;
        if (s->PointerToRawData > size || n > size - s->PointerToRawData || s->VirtualAddress + n > EXE_SIZE)
            fail("IGI.exe is damaged.");
        if (!(s->Characteristics & IMAGE_SCN_CNT_CODE))
            memcpy((void *)(EXE_BASE + s->VirtualAddress), file + s->PointerToRawData, n);
        else if (s->VirtualAddress == TEXT_LO - EXE_BASE)
            for (unsigned k = 0; k < sizeof(text_data) / sizeof(text_data[0]); k++)
                if (text_data[k][0] - TEXT_LO + text_data[k][1] <= n)
                    memcpy((void *)text_data[k][0], file + s->PointerToRawData + text_data[k][0] - TEXT_LO, text_data[k][1]);
    }
    load_imports(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    free(file);
    clear_code();
}

/* fill the exe's import table; some code calls through its slots */
static void load_imports(uint32_t dir)
{
    const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)(EXE_BASE + dir);

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

/* point the data's code addresses at our functions, through the jumps the hooks just wrote */
static void patch_code_tables(void)
{
    unsigned i;
    uint32_t a, *w;

    for (i = 0; i < sizeof(code_tables) / sizeof(code_tables[0]); i++)
        for (a = code_tables[i][0]; a < code_tables[i][1]; a += 4) {
            w = (uint32_t *)(uintptr_t)a;
            if (*w >= TEXT_LO && *w < TEXT_HI && *(uint8_t *)(uintptr_t)*w == 0xE9)
                *w += 5 + *(int32_t *)(uintptr_t)(*w + 1);
        }
}

/* fill the code range with int3 again, except text_data (in address order) */
static void clear_code(void)
{
    uint32_t a = TEXT_LO;
    unsigned k;

    for (k = 0; k < sizeof(text_data) / sizeof(text_data[0]); k++) {
        memset((void *)(uintptr_t)a, 0xCC, text_data[k][0] - a);
        a = text_data[k][0] + text_data[k][1];
    }
    memset((void *)(uintptr_t)a, 0xCC, TEXT_HI - a);
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
    load_exe();
    hook_exe_mapped = 1;
    Hook_InstallAll(inst);
    patch_code_tables();
    clear_code(); /* the hooks' jumps were only needed to find the functions */
    if (!msvcrt || !dinput || !Link_Bind(msvcrt, dinput))
        fail("msvcrt.dll is missing a function the game needs.");
    AddVectoredExceptionHandler(0, on_trap);
    __asm__ volatile("fldcw %0" : : "m"(cw));
    exit(AppMain_WinMain(inst, NULL, cmdLine, show));
}
