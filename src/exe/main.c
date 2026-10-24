#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

/* Standalone igidecomp.exe. It copies the data of the player's IGI.exe (never its code) to
   the original addresses, fills the code range with int3 and puts a jump to our rewrite at
   every original function, so ORIG calls and code pointers in the data reach our code.
   The exe is linked at 0x400000 with this section first, so the range is ours from the start
   (see CMakeLists.txt); our own headers take the place of IGI.exe's. ORIG calls link to the
   rewrite directly (link.c); the jumps serve code addresses held in data. */
__asm__(".section .igi,\"bw\"\n\t.space 0x834000\n\t.text");

#define EXE_BASE 0x400000
#define EXE_SIZE 0x835000
#define EXE_TIME 0x3A1D74C6
#define TEXT_LO 0x401000
#define TEXT_HI 0x533000

int WINAPI AppMain_WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int show);
int Link_Bind(HMODULE msvcrt, HMODULE dinput);

/* the exe's import thunks (jmp [slot]) that our code calls by address */
static const uint32_t thunks[][2] = {
    {0x4AE730, 0x53303C}, {0x4AF740, 0x533038}, {0x4AF746, 0x533044}, {0x4AF74C, 0x53302C},
    {0x4AF752, 0x533030}, {0x4AF758, 0x53301C}, {0x4AF75E, 0x533018}, {0x4AF764, 0x533020},
    {0x4AF76A, 0x53300C}, {0x4AF770, 0x533168}, {0x4AF776, 0x533164}, {0x4AF77C, 0x533024},
    {0x4AF782, 0x533004}, {0x4AF788, 0x533000}, {0x4AF78E, 0x533008}, {0x4AF794, 0x533014},
    {0x4AF79A, 0x533160}, {0x4AF7A0, 0x53316C}, {0x4AF7A6, 0x533010},
};

/* the exe's c runtime, served by msvcrt.dll (the same runtime generation) */
static const struct {
    uint32_t addr;
    const char *name;
} crt[] = {
    {0x4A5000, "_setjmp3"}, {0x4A507C, "longjmp"}, {0x4A5234, "fclose"}, {0x4A528A, "fgetc"},
    {0x4A52A4, "fseek"}, {0x4A5350, "fopen"}, {0x4A53B3, "sprintf"}, {0x4A5490, "atoi"},
    {0x4A54D0, "strrchr"}, {0x4A5500, "strncpy"}, {0x4A55FE, "floor"}, {0x4A5702, "vsprintf"},
    {0x4A5980, "_stricmp"}, {0x4A5A0C, "strtok"}, {0x4A5AD5, "exit"}, {0x4A5BB0, "strstr"},
    {0x4A5C30, "ceil"}, {0x4A6A50, "free"}, {0x4A6CA4, "_filbuf"}, {0x4A6E17, "ftell"},
    {0x4A760B, "_flsbuf"}, {0x4A7F2A, "_isctype"}, {0x4AA0A0, "strchr"}, {0x4AA580, "strcpy"},
    {0x4AA670, "malloc"}, {0x4AA730, "strlen"}, {0x4AA7B0, "memcpy"}, {0x4AAD10, "strncmp"},
    {0x4AD73D, "toupper"}, {0x516824, "acos"}, {0x5168F4, "asin"},
    /* x87 versions with the arguments on the fpu stack */
    {0x4A5760, "_CIpow"}, {0x4A56DA, "_CIfmod"}, {0x516810, "_CIacos"}, {0x5168E0, "_CIasin"},
    /* c runtime routines terrdraw/crt.c rewrites for the dll build */
    {0x516770, "_strlwr"}, {0x5169B0, "strncat"}, {0x516AD3, "atof"}, {0x516B20, "_splitpath"},
    {0x516C67, "remove"}, {0x516C91, "fread"}, {0x516D79, "fwrite"}, {0x516F14, "_mbsnbcpy"},
};

/* data that dinput.lib left in the code section: the mouse and keyboard object formats */
static const uint32_t text_data[][2] = {{0x4AE680, 0xB0}, {0x4AE740, 0x1000}};

static void fail(const char *msg)
{
    MessageBoxA(NULL, msg, "IGI", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

static void jump(uint32_t at, const void *to)
{
    uint8_t *p = (uint8_t *)(uintptr_t)at;

    p[0] = 0xE9;
    *(int32_t *)(p + 1) = (int32_t)((uintptr_t)to - at - 5);
}

static void load_imports(uint32_t dir);

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

    memset((void *)TEXT_LO, 0xCC, TEXT_HI - TEXT_LO);
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
    unsigned i;
    char sys[MAX_PATH];
    void *fn;

    (void)prev;
    /* the system dinput, not this project's proxy dll if it sits in the game folder; the
       import table below then finds it already loaded */
    GetSystemDirectoryA(sys, MAX_PATH - 12);
    dinput = LoadLibraryA(strcat(sys, "\\dinput.dll"));
    load_exe();
    hook_exe_mapped = 1;
    Hook_InstallAll(inst);
    for (i = 0; i < sizeof(thunks) / sizeof(thunks[0]); i++) {
        uint8_t *p = (uint8_t *)(uintptr_t)thunks[i][0];
        p[0] = 0xFF;
        p[1] = 0x25;
        *(uint32_t *)(p + 2) = thunks[i][1];
    }
    if (!msvcrt || !dinput || !Link_Bind(msvcrt, dinput))
        fail("msvcrt.dll is missing a function the game needs.");
    for (i = 0; i < sizeof(crt) / sizeof(crt[0]); i++) {
        if (!(fn = (void *)GetProcAddress(msvcrt, crt[i].name)))
            fail("msvcrt.dll is missing a function the game needs.");
        jump(crt[i].addr, fn);
    }
    jump(0x4B0B90, (void *)0x4C1780); /* a jump thunk to a hooked function */
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    AddVectoredExceptionHandler(0, on_trap);
    __asm__ volatile("fldcw %0" : : "m"(cw));
    exit(AppMain_WinMain(inst, NULL, cmdLine, show));
}
