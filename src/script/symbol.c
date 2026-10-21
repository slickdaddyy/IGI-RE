#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "core/mem.h"
#include "script/symbol.h"
#include "script/symtab.h"

#define gScriptSymbols VAR(SymbolTable *, 0xA43E88)

#define IntValueHandler ((void *)0x4B7EA0)
#define Bool8ValueHandler ((void *)0x4B7F00)
#define RealValueHandler ((void *)0x4B7FE0)
#define ConstantValueHandler ((void *)0x4B8830)

void Script_RegisterSymbol(const char *name, void *handler, void *data, int size, int kind, int flags1, int flags2)
{
    ScriptSymbol *s;

    Mem_PushMode(1);
    s = Mem_Alloc(sizeof(ScriptSymbol), 4);
    memset(s, 0, sizeof(*s));
    s->name = SymbolTable_Add(gScriptSymbols, name, s);
    s->handler = handler;
    s->data = data;
    s->size = size;
    s->kind = kind;
    s->flags1 = flags1;
    s->flags2 = flags2;
    Mem_PopMode();
}
HOOK(0x4BAAC0, Script_RegisterSymbol);

const char *Script_FindSymbol(const char *name, ScriptSymbol **out)
{
    return SymbolTable_Lookup(gScriptSymbols, (void **)out, name);
}
HOOK(0x4BAB80, Script_FindSymbol);

void Script_SetSymbolContext(const char *name, void *context)
{
    ScriptSymbol *s;

    if (Script_FindSymbol(name, &s)) {
        s->data = context;
        return;
    }
    Error_Show("Script_SetSymbolContext(): Symbol not found: %s", name);
    for (;;)
        ;
}
HOOK(0x4B8930, Script_SetSymbolContext);

void Script_RegisterInt(const char *name, int *var)
{
    Script_RegisterSymbol(name, IntValueHandler, var, 4, 0, 1, 1);
}
HOOK(0x4B7E80, Script_RegisterInt);

void Script_RegisterInt2(const char *name, int *var)
{
    Script_RegisterSymbol(name, IntValueHandler, var, 4, 0, 1, 1);
}
HOOK(0x4B7FA0, Script_RegisterInt2);

void Script_RegisterBool8(const char *name, char *var)
{
    Script_RegisterSymbol(name, Bool8ValueHandler, var, 1, 0, 1, 1);
}
HOOK(0x4B7EE0, Script_RegisterBool8);

void Script_RegisterReal(const char *name, double *var)
{
    Script_RegisterSymbol(name, RealValueHandler, var, 8, 0, 2, 1);
}
HOOK(0x4B7FC0, Script_RegisterReal);

void Script_RegisterConstant(const char *name, int value)
{
    Script_RegisterSymbol(name, ConstantValueHandler, (void *)(intptr_t)value, 4, 2, 1, 0);
}
HOOK(0x4B8810, Script_RegisterConstant);

void Script_RegisterFunction(const char *name, void *fn, void *data, int argc)
{
    Script_RegisterSymbol(name, fn, data, argc, 1, 0, 0);
}
HOOK(0x4B8890, Script_RegisterFunction);

void Script_RegisterFunctionEx(const char *name, void *fn, void *data, int argc)
{
    Script_RegisterSymbol(name, fn, data, argc, 1, 2, 0);
}
HOOK(0x4B88F0, Script_RegisterFunctionEx);
