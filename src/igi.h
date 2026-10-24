#ifndef IGI_H
#define IGI_H

#include <stddef.h>
#include <stdint.h>

/* Code and data that still live in the original IGI.exe. The standalone exe calls the
   rewrite directly instead, so the linker proves every call has one. */
#ifdef IGI_STANDALONE
#define ORIG(addr, ret, name, args) ret name args
#define ORIG_STD(addr, ret, name, args) ret __stdcall name args
#else
#define ORIG(addr, ret, name, args) \
    static ret (*const name) args __attribute__((unused)) = (ret (*) args)(uintptr_t)(addr)
#define ORIG_STD(addr, ret, name, args) \
    static ret (__stdcall *const name) args __attribute__((unused)) = (ret (__stdcall *) args)(uintptr_t)(addr)
#endif
#define VAR(type, addr) (*(type *)(uintptr_t)(addr))

/* A rewritten function used by address (handler tables). The DLL keeps the original number,
   which its tests compare; the standalone exe takes the function. FN_EXTERN(name) at file
   scope declares it by its assembler name, so no prototype has to be in scope. FN_ASM is
   the same as an immediate operand for basic asm. */
#define FN_EXTERN(name) extern char fn_##name[] __asm__("_" #name)
#ifdef IGI_STANDALONE
#define FN(addr, name) ((void *)fn_##name)
#define FN_ASM(addr, name) "$_" #name
#else
#define FN(addr, name) ((void *)(uintptr_t)(addr))
#define FN_ASM(addr, name) "$" #addr
#endif

#endif
