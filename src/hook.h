#ifndef HOOK_H
#define HOOK_H

#include <stdint.h>

/* Replace the original function at addr with fn when the DLL loads. */
#define HOOK(addr, fn) \
    static void __attribute__((constructor)) hook_##fn(void) { Hook_Add(addr, (void *)fn, #fn, __FILE__); }

void Hook_Add(uint32_t addr, void *fn, const char *name, const char *file);
/* set by the standalone exe, which lays out IGI.exe's data itself */
extern int hook_exe_mapped;
int Hook_IsSupportedExe(void);
void Hook_InstallAll(void *module);

#endif
