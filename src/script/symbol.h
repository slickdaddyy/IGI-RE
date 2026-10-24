#ifndef SCRIPT_SYMBOL_H
#define SCRIPT_SYMBOL_H

#include "igi.h"
#include "core/list.h"
#include "script/symtab.h"
#include "script/scripttypes.h"

struct Expr;

typedef ScriptValue *(*ScriptHandler)(ScriptValue *out, void *data, ScriptArgs *args);

enum { SCRIPT_VARIABLE, SCRIPT_FUNCTION, SCRIPT_CONSTANT };
enum { SCRIPT_VOID, SCRIPT_INT, SCRIPT_REAL, SCRIPT_STRING };

extern SymbolTable *gScriptSymbols;
extern const char gScriptEmptyString[];

/* fistp with the current rounding mode, low 32 bits of the 64 bit result */
static inline int Script_Round(long double d)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(d) : "st");
    return (int)r;
}

void Script_RegisterInt(const char *name, int *var);
ScriptValue *Script_IntValue(ScriptValue *out, int *var);
void Script_RegisterBool8(const char *name, char *var);
ScriptValue *Script_Bool8Value(ScriptValue *out, char *var);
void Script_RegisterInt16(const char *name, short *var);
ScriptValue *Script_Int16Value(ScriptValue *out, short *var);
void Script_RegisterInt2(const char *name, int *var);
void Script_RegisterReal(const char *name, double *var);
ScriptValue *Script_RealValue(ScriptValue *out, double *var);
void Script_RegisterString(const char *name, char *buf, int size);
ScriptValue *Script_StringValue(ScriptValue *out, char *buf);
void Script_RemoveSymbol(const char *name);
void Script_RegisterConstant(const char *name, int value);
ScriptValue *Script_ConstantValue(ScriptValue *out, int value);
void Script_RegisterRealConstant(const char *name, double *value);
void Script_RegisterFunction(const char *name, void *fn, void *data, int argc);
void Script_RegisterIntFunction(const char *name, void *fn, void *data, int argc);
void Script_RegisterFunctionEx(const char *name, void *fn, void *data, int argc);
void Script_UnregisterSymbol(const char *name);
void Script_SetSymbolContext(const char *name, void *context);
void Script_ClearRefs(ScriptSymbol *sym);
void Script_RegisterSymbol(const char *name, void *handler, void *data, int size, int kind, int type, int dynamic);
void Script_DeleteSymbol(const char *name);
const char *Script_FindSymbol(const char *name, ScriptSymbol **out);
SymbolTable *Script_GetSymbolTable(void);
ScriptRef *Script_RefSymbol(const char *name);
void Script_AllocRefs(int count);
void Script_UnrefSymbol(ScriptRef *ref);
void Script_InitSymbols(void);
void Script_FreeSymbols(void);
void Script_FreeRefBlock(Node *block);

#endif
