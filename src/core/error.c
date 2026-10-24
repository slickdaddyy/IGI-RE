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
extern void *gUnk93625C;
extern void *gUnk936270;
extern int gUnk93627C;
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

void sub_4AF870(void *p)
{
    gUnk936270 = p;
}
HOOK(0x4AF870, sub_4AF870);

void Warning_SetHandler(ErrorHandler fn)
{
    gWarningHandler = fn;
}
HOOK(0x4AF880, Warning_SetHandler);

void sub_4AF890(void *p)
{
    gUnk93625C = p;
}
HOOK(0x4AF890, sub_4AF890);

void Error_SetExitHandler(ErrorHandler fn)
{
    gErrorExitHandler = fn;
}
HOOK(0x4AF8A0, Error_SetExitHandler);

void sub_4AF8B0(int v)
{
    gUnk93627C = v;
}
HOOK(0x4AF8B0, sub_4AF8B0);

void Error_ResetHandlers(void)
{
    Error_SetHandler(NULL);
    Error_SetExitHandler(NULL);
    sub_4AF870(NULL);
    Warning_SetHandler(NULL);
    sub_4AF890(NULL);
    sub_4AF8B0(0);
}
HOOK(0x4AF8C0, Error_ResetHandlers);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(93625C) void *gUnk93625C;
PAD(igb, 93625C, 936260, 0x8);
BSS(93625C) ErrorHandler gErrorHandler;
BSS(93625C) ErrorHandler gErrorExitHandler;
BSS(93625C) void *gUnk936270;
BSS(93625C) ErrorHandler gWarningHandler;
PAD(igb, 93625C, 936278, 0x4);
BSS(93625C) int gUnk93627C;
STORE(igb, A84640, 1) __typeof__(gErrorText[0]) store_A84640[1024];
ALIAS(gErrorText, store_A84640, 0);
