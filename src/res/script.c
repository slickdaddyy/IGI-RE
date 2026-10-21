#include <stdarg.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"

ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));

#define gScriptErrors VAR(int, 0xA965A8)
#define gScriptVerbose VAR(int, 0xA96598)
typedef void (*ScriptErrorFn)(const char *msg, void *data);

#define gScriptErrorFn VAR(ScriptErrorFn, 0xA43E70)
#define gScriptErrorData VAR(void *, 0xA43E68)

void Script_Error(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    gScriptErrors = 1;
    if (!gScriptVerbose)
        return;
    va_start(ap, fmt);
    crt_vsprintf(buf, fmt, ap);
    va_end(ap);
    if (!gScriptErrorFn) {
        Error_Show(buf);
        for (;;)
            ;
    }
    gScriptErrorFn(buf, gScriptErrorData);
}
HOOK(0x4B7E10, Script_Error);
