#ifndef IGI_H
#define IGI_H

#include <stddef.h>
#include <stdint.h>

/* Code and data that still live in the original IGI.exe. */
#define ORIG(addr, ret, name, args) \
    static ret (*const name) args __attribute__((unused)) = (ret (*) args)(uintptr_t)(addr)
#define ORIG_STD(addr, ret, name, args) \
    static ret (__stdcall *const name) args __attribute__((unused)) = (ret (__stdcall *) args)(uintptr_t)(addr)
#define VAR(type, addr) (*(type *)(uintptr_t)(addr))

#endif
