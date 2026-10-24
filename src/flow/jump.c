#include "igi.h"
#include "hook.h"

ORIG(0x402880, void *, Flow_Get, (void));
ORIG(0x48F480, void, AppMain_SetRunning, (char running));
ORIG(0x4A507C, void, Crt_LongJmp, (void *buf, int value));

typedef void (*GameRunHandler)(void *task);

/* the game's own jmp_buf; Game_Quit jumps back into Game_Update */
extern char gGameJmpBuf[];
/* task run handlers (slot 1 of the handler table) by task type */
extern GameRunHandler gTaskRunHandlers[];

/* tail jump into the exe's _setjmp3 (0x4A5000), so it records our own frame */
static int __attribute__((naked, noinline, returns_twice)) Crt_SetJmp3(void *buf, int count)
{
    __asm__("movl " FN_ASM(0x4A5000, crt_setjmp3) ", %eax\n\tjmp *%eax");
}

void __attribute__((optimize("no-omit-frame-pointer"))) Game_Update(void)
{
    GameRunHandler run;

    if (Crt_SetJmp3(gGameJmpBuf, 0)) {
        AppMain_SetRunning(0);
        return;
    }
    run = gTaskRunHandlers[*(uint16_t *)((char *)Flow_Get() + 0x1C)];
    run(Flow_Get());
}
HOOK(0x401050, Game_Update);

void Game_Quit(void)
{
    Crt_LongJmp(gGameJmpBuf, 1);
}
HOOK(0x4010A0, Game_Quit);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igd, 54F8E0, 1) char store_54F8E0[72];
ALIAS(gGameJmpBuf, store_54F8E0, 0);
STORE(igb, A970E0, 4) __typeof__(gTaskRunHandlers[0]) store_A970E0[1152];
ALIAS(gTaskRunHandlers, store_A970E0, 0);
