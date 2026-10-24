#include <stdarg.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "core/error.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));

extern ErrorHandler gErrorHandler;
extern ErrorHandler gErrorExitHandler;
extern ErrorHandler gWarningHandler;
extern void *gMessageBoxHandler;
extern void *gErrorUnusedHandler;
extern int gErrorUnusedFlag;
extern char gErrorText[];

void Error_Show(const char *fmt, ...)
{
    if (gErrorHandler) {
        va_list ap;

        va_start(ap, fmt);
        crt_sprintf(gErrorText, "Fatal error:\n\n");
        crt_vsprintf(gErrorText + strlen(gErrorText), fmt, ap);
        gErrorHandler(gErrorText);
        va_end(ap);
    }
    /* the original jumps here with the caller's arguments still on the stack */
    if (gErrorExitHandler)
        gErrorExitHandler(fmt);
}
HOOK(0x4AF7B0, Error_Show);

void Warning_Show(const char *fmt, ...)
{
    if (gWarningHandler) {
        va_list ap;

        va_start(ap, fmt);
        crt_sprintf(gErrorText, "Warning:\n\n");
        crt_vsprintf(gErrorText + strlen(gErrorText), fmt, ap);
        gWarningHandler(gErrorText);
        va_end(ap);
    }
}
HOOK(0x4AF810, Warning_Show);

void Error_SetHandler(ErrorHandler fn)
{
    gErrorHandler = fn;
}
HOOK(0x4AF860, Error_SetHandler);

void Error_SetUnusedHandler(void *p)
{
    gErrorUnusedHandler = p;
}
HOOK(0x4AF870, Error_SetUnusedHandler);

void Warning_SetHandler(ErrorHandler fn)
{
    gWarningHandler = fn;
}
HOOK(0x4AF880, Warning_SetHandler);

void Error_SetMessageBoxHandler(void *p)
{
    gMessageBoxHandler = p;
}
HOOK(0x4AF890, Error_SetMessageBoxHandler);

void Error_SetExitHandler(ErrorHandler fn)
{
    gErrorExitHandler = fn;
}
HOOK(0x4AF8A0, Error_SetExitHandler);

void Error_SetUnusedFlag(int v)
{
    gErrorUnusedFlag = v;
}
HOOK(0x4AF8B0, Error_SetUnusedFlag);

void Error_ResetHandlers(void)
{
    Error_SetHandler(NULL);
    Error_SetExitHandler(NULL);
    Error_SetUnusedHandler(NULL);
    Warning_SetHandler(NULL);
    Error_SetMessageBoxHandler(NULL);
    Error_SetUnusedFlag(0);
}
HOOK(0x4AF8C0, Error_ResetHandlers);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(93625C) void *gMessageBoxHandler;
PAD(igb, 93625C, 936260, 0x8);
BSS(93625C) ErrorHandler gErrorHandler;
BSS(93625C) ErrorHandler gErrorExitHandler;
BSS(93625C) void *gErrorUnusedHandler;
BSS(93625C) ErrorHandler gWarningHandler;
PAD(igb, 93625C, 936278, 0x4);
BSS(93625C) int gErrorUnusedFlag;
STORE(igb, A84640, 1) char store_A84640[1024];
ALIAS(gErrorText, store_A84640, 0);
