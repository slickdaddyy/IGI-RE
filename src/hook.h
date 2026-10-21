#ifndef HOOK_H
#define HOOK_H

#include <stdint.h>

typedef struct Hook {
    uint32_t addr;
    void *fn;
} Hook;

extern const Hook hooks[];

int Hook_IsSupportedExe(void);
void Hook_InstallAll(void);

#endif
