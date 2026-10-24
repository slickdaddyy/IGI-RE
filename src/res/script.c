#include <stdarg.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"

ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));

#define gScriptError VAR(int, 0xA965A8)
#define gScriptWarningLevel VAR(int, 0xA96598)
typedef void (*ScriptErrorFn)(const char *msg, void *data);

#define gScriptErrorHandler VAR(ScriptErrorFn, 0xA43E70)
#define gScriptErrorContext VAR(void *, 0xA43E68)

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
