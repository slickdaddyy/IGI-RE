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

/* Data definitions. In igidecomp.exe a global defined with BSS(run) or DATA(run) goes in the
   section of its run (the address the run starts at), which the linker places at that IGI.exe
   address with fill.c around it (tools/gendefs.py, tools/genfill.py). In the DLL the line is
   only a declaration: the global is IGI.exe's own (src/addr.c). */
#ifdef IGI_STANDALONE
#define BSS(run) __attribute__((section(".igb$" #run)))
#define DATA(run) __attribute__((section(".igd$" #run)))
#define INIT(...) = __VA_ARGS__
#define PAD(sec, run, addr, n) static uint8_t pad_##addr[n] __attribute__((section("." #sec "$" #run), used, aligned(1)))
#define PAD_CONST(sec, run, addr, n) \
    static const uint8_t pad_##addr[n] __attribute__((section("." #sec "$" #run), used, aligned(1)))
#define STORE(sec, run, align) static __attribute__((section("." #sec "$" #run), used, aligned(align)))
#define ALIAS(name, of, off) __asm__(".globl _" #name "\n.set _" #name ", _" #of "+" #off)
#else
#define BSS(run) extern
#define DATA(run) extern
#define INIT(...)
#define PAD(sec, run, addr, n) _Static_assert(1, "")
#define PAD_CONST(sec, run, addr, n) _Static_assert(1, "")
#define STORE(sec, run, align) extern
#define ALIAS(name, of, off) _Static_assert(1, "")
#endif
/* a global or function by its assembler name, for initialisers that point at it */
#define REF_EXTERN(name) extern char ref_##name[] __asm__("_" #name)

#endif
