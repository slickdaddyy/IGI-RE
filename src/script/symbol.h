#ifndef SCRIPT_SYMBOL_H
#define SCRIPT_SYMBOL_H

typedef struct ScriptSymbol {
    int kind;
    int flags1;
    int flags2;
    int size;
    void *handler;
    const char *name;
    void *data;
} ScriptSymbol;

void Script_RegisterSymbol(const char *name, void *handler, void *data, int size, int kind, int flags1, int flags2);
const char *Script_FindSymbol(const char *name, ScriptSymbol **out);
void Script_SetSymbolContext(const char *name, void *context);
void Script_RegisterInt(const char *name, int *var);
void Script_RegisterInt2(const char *name, int *var);
void Script_RegisterBool8(const char *name, char *var);
void Script_RegisterReal(const char *name, double *var);
void Script_RegisterConstant(const char *name, int value);
void Script_RegisterFunction(const char *name, void *fn, void *data, int argc);
void Script_RegisterFunctionEx(const char *name, void *fn, void *data, int argc);

#endif
