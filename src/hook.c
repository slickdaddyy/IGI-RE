#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook.h"

/* EU build of IGI.exe, the one all addresses refer to. */
#define EXE_BASE 0x400000
#define EXE_SIZE 0x835000
#define EXE_TIME 0x3A1D74C6
#define EXE_MARK 0x541ECC

int hook_exe_mapped;

int Hook_IsSupportedExe(void)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)GetModuleHandleA(NULL);
    const IMAGE_NT_HEADERS32 *nt;

    if (hook_exe_mapped) /* the standalone exe checked the file's headers itself */
        return memcmp((const void *)EXE_MARK, "IGIMUTEX", 9) == 0;
    if ((uintptr_t)dos != EXE_BASE || dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    nt = (const IMAGE_NT_HEADERS32 *)((const char *)dos + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp != EXE_TIME ||
        nt->OptionalHeader.SizeOfImage != EXE_SIZE)
        return 0;
    return memcmp((const void *)EXE_MARK, "IGIMUTEX", 9) == 0;
}

static int patch_jump(uint32_t at, void *to)
{
    uint8_t *p = (uint8_t *)(uintptr_t)at;
    DWORD old;

    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    p[0] = 0xE9;
    *(int32_t *)(p + 1) = (int32_t)((uintptr_t)to - at - 5);
    VirtualProtect(p, 5, old, &old);
    return 1;
}

typedef struct Hook {
    uint32_t addr;
    void *fn;
    const char *name;
    const char *file;
} Hook;

static Hook hooks[8192];
static int hook_count;
static int hook_dropped;

void Hook_Add(uint32_t addr, void *fn, const char *name, const char *file)
{
    if (hook_count < (int)(sizeof(hooks) / sizeof(hooks[0])))
        hooks[hook_count++] = (Hook){addr, fn, name, file};
    else
        hook_dropped++;
}

/* The src subfolder a hook comes from, e.g. "haze" for src/haze/bonecoll.c. */
static void hook_folder(const Hook *h, char *out, int size)
{
    const char *s = h->file, *p;
    int n;

    for (p = s; *p; p++)
        if ((p == s || p[-1] == '/' || p[-1] == '\\') && strncmp(p, "src", 3) == 0 && (p[3] == '/' || p[3] == '\\'))
            s = p + 4;
    for (n = 0; s[n] && s[n] != '/' && s[n] != '\\' && n < size - 1; n++)
        out[n] = s[n];
    out[s[n] == '/' || s[n] == '\\' ? n : 0] = 0;
}

/* --- dinput.ini ------------------------------------------------------------ */

static int opt_enabled = 1;
static int opt_log;
static int opt_first, opt_last = 0x7FFFFFFF;
static char opt_only[4096], opt_skip[4096], only_buf[4096], skip_buf[4096];
static char *only_list[256], *skip_list[256];
static int only_count, skip_count;

static int split(char *s, char **out, int max)
{
    int n = 0;
    char *t;

    for (t = strtok(s, ", \t;"); t && n < max; t = strtok(NULL, ", \t;"))
        out[n++] = t;
    return n;
}

static int matches(const Hook *h, char **list, int count)
{
    char folder[64];
    int i;

    hook_folder(h, folder, sizeof(folder));
    for (i = 0; i < count; i++) {
        if ((list[i][0] == '0' && (list[i][1] == 'x' || list[i][1] == 'X')) ? strtoul(list[i], NULL, 16) == h->addr
                                                                             : !strcmp(list[i], h->name) || !strcmp(list[i], folder))
            return 1;
    }
    return 0;
}

static int selected(int i)
{
    if (i < opt_first || i > opt_last)
        return 0;
    if (only_count && !matches(&hooks[i], only_list, only_count))
        return 0;
    return !matches(&hooks[i], skip_list, skip_count);
}

static void load_settings(const char *ini)
{
    char range[64];

    opt_enabled = GetPrivateProfileIntA("hooks", "enabled", 1, ini);
    opt_log = GetPrivateProfileIntA("hooks", "log", 0, ini);
    GetPrivateProfileStringA("hooks", "only", "", opt_only, sizeof(opt_only), ini);
    GetPrivateProfileStringA("hooks", "skip", "", opt_skip, sizeof(opt_skip), ini);
    GetPrivateProfileStringA("hooks", "range", "", range, sizeof(range), ini);
    if (range[0]) {
        char *dash = strchr(range, '-');
        opt_first = atoi(range);
        opt_last = dash ? (dash[1] ? atoi(dash + 1) : 0x7FFFFFFF) : opt_first;
    }
    strcpy(only_buf, opt_only);
    only_count = split(only_buf, only_list, 256);
    strcpy(skip_buf, opt_skip);
    skip_count = split(skip_buf, skip_list, 256);
}

/* --- dinput.log ------------------------------------------------------------ */

static HANDLE log_file = INVALID_HANDLE_VALUE;
static uintptr_t dll_lo, dll_hi;

static void log_line(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    DWORD n;
    int len;

    if (log_file == INVALID_HANDLE_VALUE)
        return;
    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (len < 0 || len > (int)sizeof(buf) - 3)
        len = sizeof(buf) - 3;
    strcpy(buf + len, "\r\n");
    WriteFile(log_file, buf, len + 2, &n, NULL);
}

/* Name the code at addr: our function, the game's, or another module. */
static void describe(uintptr_t addr, char *out, int size)
{
    const Hook *best = NULL;
    char mod[MAX_PATH];
    HMODULE m;
    int i;

    if (addr >= dll_lo && addr < dll_hi) {
        for (i = 0; i < hook_count; i++)
            if ((uintptr_t)hooks[i].fn <= addr && (!best || hooks[i].fn > best->fn))
                best = &hooks[i];
        if (best)
            snprintf(out, size, "%s+0x%X (dinput.dll+0x%X)", best->name, (unsigned)(addr - (uintptr_t)best->fn),
                     (unsigned)(addr - dll_lo));
        else
            snprintf(out, size, "dinput.dll+0x%X", (unsigned)(addr - dll_lo));
    } else if (addr >= EXE_BASE && addr < EXE_BASE + EXE_SIZE) {
        snprintf(out, size, "IGI.exe 0x%06X", (unsigned)addr);
    } else if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (LPCSTR)addr, &m) &&
               GetModuleFileNameA(m, mod, sizeof(mod))) {
        const char *base = strrchr(mod, '\\');
        snprintf(out, size, "%s+0x%X", base ? base + 1 : mod, (unsigned)(addr - (uintptr_t)m));
    } else {
        snprintf(out, size, "0x%08X", (unsigned)addr);
    }
}

/* Logs the first few error exceptions, then lets the game handle them as before. */
static LONG CALLBACK on_exception(EXCEPTION_POINTERS *ep)
{
    static LONG count;
    const EXCEPTION_RECORD *er = ep->ExceptionRecord;
    const CONTEXT *c = ep->ContextRecord;
    const DWORD *sp = (const DWORD *)c->Esp, *top = sp;
    MEMORY_BASIC_INFORMATION mbi;
    char where[MAX_PATH + 64];
    int n = 0;

    if ((er->ExceptionCode & 0xF0000000) != 0xC0000000 || InterlockedIncrement(&count) > 16)
        return EXCEPTION_CONTINUE_SEARCH;
    if (VirtualQuery(sp, &mbi, sizeof(mbi)))
        top = (const DWORD *)((const char *)mbi.BaseAddress + mbi.RegionSize);
    describe((uintptr_t)er->ExceptionAddress, where, sizeof(where));
    log_line("exception %08lX at %s", er->ExceptionCode, where);
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        log_line("  %s 0x%08lX", er->ExceptionInformation[0] ? "writing" : "reading", er->ExceptionInformation[1]);
    log_line("  eax %08lX ebx %08lX ecx %08lX edx %08lX esi %08lX edi %08lX ebp %08lX esp %08lX fpu cw %03lX",
             c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, c->FloatSave.ControlWord & 0xFFFF);
    for (; sp < top && sp < (const DWORD *)c->Esp + 2048 && n < 12; sp++) {
        if (*sp >= dll_lo && *sp < dll_hi) {
            describe(*sp, where, sizeof(where));
            log_line("  stack %08lX: %s", (DWORD)(uintptr_t)sp, where);
            n++;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void open_log(void *module, const char *path)
{
    const IMAGE_DOS_HEADER *dos = module;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)((const char *)dos + dos->e_lfanew);

    dll_lo = (uintptr_t)module;
    dll_hi = dll_lo + nt->OptionalHeader.SizeOfImage;
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

/* --- installation ---------------------------------------------------------- */

/* msvc kept string literals in writable .data and the game upper-cases some in place */
static void make_rdata_writable(void *module)
{
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)module + ((IMAGE_DOS_HEADER *)module)->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    DWORD old;
    int i;

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, s++)
        if (!memcmp(s->Name, ".rdata", 7))
            VirtualProtect((char *)module + s->VirtualAddress, s->Misc.VirtualSize, PAGE_READWRITE, &old);
}

/* Settings come from dinput.ini next to the DLL; without it every hook is installed. */
void Hook_InstallAll(void *module)
{
    char ini[MAX_PATH], log[MAX_PATH], folder[64], *dot;
    int i, installed = 0, failed = 0;

    if (GetModuleFileNameA(module, ini, MAX_PATH - 4) && (dot = strrchr(ini, '.'))) {
        strcpy(dot, ".ini");
        strcpy(log, ini);
        strcpy(log + (dot - ini), ".log");
        load_settings(ini);
    }
    if (opt_log) {
        open_log(module, log);
        AddVectoredExceptionHandler(1, on_exception);
    }
    log_line("%d hooks in this DLL%s", hook_count, hook_dropped ? ", some dropped: table full" : "");
    log_line("enabled=%d only=%s skip=%s range=%d-%d", opt_enabled, opt_only, opt_skip, opt_first, opt_last);

    if (!Hook_IsSupportedExe()) {
        log_line("unsupported IGI.exe, nothing installed");
        MessageBoxA(NULL, "Unsupported IGI.exe version. The EU version is required.", "IGI", MB_OK | MB_ICONERROR);
        return;
    }
    if (!opt_enabled) {
        log_line("hooks turned off, nothing installed");
        return;
    }
    make_rdata_writable(module);
    for (i = 0; i < hook_count; i++) {
        if (!selected(i))
            continue;
        if (!patch_jump(hooks[i].addr, hooks[i].fn)) {
            log_line("failed %d %06X %s (error %lu)", i, hooks[i].addr, hooks[i].name, GetLastError());
            failed++;
            continue;
        }
        installed++;
        if (opt_log >= 2) {
            hook_folder(&hooks[i], folder, sizeof(folder));
            log_line("%d %06X %s %s", i, hooks[i].addr, folder, hooks[i].name);
        }
    }
    if (!FlushInstructionCache(GetCurrentProcess(), NULL, 0))
        log_line("FlushInstructionCache failed (error %lu)", GetLastError());
    log_line("installed %d, failed %d", installed, failed);
}
