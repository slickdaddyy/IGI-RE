#include <stdarg.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"

ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));

extern int gScriptError;
extern int gScriptWarningLevel;
typedef void (*ScriptErrorFn)(const char *msg, void *data);

extern void (*gScriptErrorHandler)(const char *msg, void *data);
extern void *gScriptErrorContext;

void Script_Error(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    gScriptError = 1;
    if (!gScriptWarningLevel)
        return;
    va_start(ap, fmt);
    crt_vsprintf(buf, fmt, ap);
    va_end(ap);
    if (!gScriptErrorHandler) {
        Error_Show(buf);
        for (;;)
            ;
    }
    gScriptErrorHandler(buf, gScriptErrorContext);
}
HOOK(0x4B7E10, Script_Error);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A43E70) void (*gScriptErrorHandler)(const char *msg, void *data);
