#ifndef HOOK_H
#define HOOK_H

#include <stdint.h>

/* Replace the original function at addr with fn when the DLL loads. */
#define HOOK(addr, fn) \
    static void __attribute__((constructor)) hook_##fn(void) { Hook_Add(addr, (void *)fn); }

void Hook_Add(uint32_t addr, void *fn);
int Hook_IsSupportedExe(void);
void Hook_InstallAll(void);

#endif
