#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "core/list.h"
#include "core/mem.h"
#include "script/symbol.h"
#include "script/symtab.h"
#include "config/configvars.h"

FN_EXTERN(Script_Bool8Value);
FN_EXTERN(Script_ConstantValue);
FN_EXTERN(Script_Int16Value);
FN_EXTERN(Script_IntValue);
FN_EXTERN(Script_RealValue);
FN_EXTERN(Script_StringValue);

ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4C0360, SymbolTable *, SymbolTable_Create, (unsigned int count));
ORIG(0x4C03C0, void, SymbolTable_Destroy, (SymbolTable *table));
ORIG(0x4C0460, void, SymbolTable_Remove, (SymbolTable *table, const char *name));

/* symbols keep the original handler addresses, which are hooked to the functions below */
#define IntValueHandler (FN(0x4B7EA0, Script_IntValue))
#define Bool8ValueHandler (FN(0x4B7F00, Script_Bool8Value))
#define Int16ValueHandler (FN(0x4B7F60, Script_Int16Value))
#define RealValueHandler (FN(0x4B7FE0, Script_RealValue))
#define StringValueHandler (FN(0x4B8060, Script_StringValue))
#define ConstantValueHandler (FN(0x4B8830, Script_ConstantValue))

extern unsigned int gScriptRefCount;
extern List gScriptRefs;
extern List gScriptRefBlocks;
extern unsigned int gScriptRefPeak;
extern List gScriptRefFree;

void Script_RegisterInt(const char *name, int *var)
{
    Script_RegisterSymbol(name, IntValueHandler, var, 4, SCRIPT_VARIABLE, SCRIPT_INT, 1);
}
HOOK(0x4B7E80, Script_RegisterInt);

ScriptValue *Script_IntValue(ScriptValue *out, int *var)
{
    out->i = *var;
    out->d = *var;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4B7EA0, Script_IntValue);

void Script_RegisterBool8(const char *name, char *var)
{
    Script_RegisterSymbol(name, Bool8ValueHandler, var, 1, SCRIPT_VARIABLE, SCRIPT_INT, 1);
}
HOOK(0x4B7EE0, Script_RegisterBool8);

ScriptValue *Script_Bool8Value(ScriptValue *out, char *var)
{
    out->i = *var;
    out->d = *var;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4B7F00, Script_Bool8Value);

void Script_RegisterInt16(const char *name, short *var)
{
    Script_RegisterSymbol(name, Int16ValueHandler, var, 2, SCRIPT_VARIABLE, SCRIPT_INT, 1);
}
HOOK(0x4B7F40, Script_RegisterInt16);

ScriptValue *Script_Int16Value(ScriptValue *out, short *var)
{
    out->i = *var;
    out->d = *var;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4B7F60, Script_Int16Value);

void Script_RegisterInt2(const char *name, int *var)
{
    Script_RegisterSymbol(name, IntValueHandler, var, 4, SCRIPT_VARIABLE, SCRIPT_INT, 1);
}
HOOK(0x4B7FA0, Script_RegisterInt2);

void Script_RegisterReal(const char *name, double *var)
{
    Script_RegisterSymbol(name, RealValueHandler, var, 8, SCRIPT_VARIABLE, SCRIPT_REAL, 1);
}
HOOK(0x4B7FC0, Script_RegisterReal);

ScriptValue *Script_RealValue(ScriptValue *out, double *var)
{
    out->i = Script_Round(*var);
    memcpy(&out->d, var, sizeof(double));
    out->s = gEmptyString;
    return out;
}
HOOK(0x4B7FE0, Script_RealValue);

void Script_RegisterString(const char *name, char *buf, int size)
{
    Script_RegisterSymbol(name, StringValueHandler, buf, size, SCRIPT_VARIABLE, SCRIPT_STRING, 1);
}
HOOK(0x4B8030, Script_RegisterString);

ScriptValue *Script_StringValue(ScriptValue *out, char *buf)
{
    out->i = 0;
    out->d = 0;
    out->s = buf;
    return out;
}
HOOK(0x4B8060, Script_StringValue);

void Script_RemoveSymbol(const char *name)
{
    Script_UnregisterSymbol(name);
}
HOOK(0x4B80A0, Script_RemoveSymbol);

void Script_RegisterConstant(const char *name, int value)
{
    Script_RegisterSymbol(name, ConstantValueHandler, (void *)(intptr_t)value, 4, SCRIPT_CONSTANT, SCRIPT_INT, 0);
}
HOOK(0x4B8810, Script_RegisterConstant);

ScriptValue *Script_ConstantValue(ScriptValue *out, int value)
{
    out->i = value;
    out->d = value;
    out->s = gEmptyString;
    return out;
}
HOOK(0x4B8830, Script_ConstantValue);

void Script_RegisterRealConstant(const char *name, double *value)
{
    Script_RegisterSymbol(name, RealValueHandler, value, 8, SCRIPT_VARIABLE, SCRIPT_REAL, 0);
}
HOOK(0x4B8870, Script_RegisterRealConstant);

void Script_RegisterFunction(const char *name, void *fn, void *data, int argc)
{
    Script_RegisterSymbol(name, fn, data, argc, SCRIPT_FUNCTION, SCRIPT_VOID, 0);
}
HOOK(0x4B8890, Script_RegisterFunction);

void Script_RegisterIntFunction(const char *name, void *fn, void *data, int argc)
{
    Script_RegisterSymbol(name, fn, data, argc, SCRIPT_FUNCTION, SCRIPT_INT, 0);
}
HOOK(0x4B88C0, Script_RegisterIntFunction);

/* registers a function that returns a real */
void Script_RegisterFunctionEx(const char *name, void *fn, void *data, int argc)
{
    Script_RegisterSymbol(name, fn, data, argc, SCRIPT_FUNCTION, SCRIPT_REAL, 0);
}
HOOK(0x4B88F0, Script_RegisterFunctionEx);

void Script_UnregisterSymbol(const char *name)
{
    Script_DeleteSymbol(name);
}
HOOK(0x4B8920, Script_UnregisterSymbol);

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

void Script_ClearRefs(ScriptSymbol *sym)
{
    Node *n;

    for (n = gScriptRefs.head; n->next; n = n->next) {
        if (((ScriptRef *)n)->sym == sym)
            ((ScriptRef *)n)->sym = NULL;
    }
}
HOOK(0x4BAA90, Script_ClearRefs);

void Script_RegisterSymbol(const char *name, void *handler, void *data, int size, int kind, int type, int dynamic)
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
    s->type = type;
    s->dynamic = dynamic;
    Mem_PopMode();
}
HOOK(0x4BAAC0, Script_RegisterSymbol);

void Script_DeleteSymbol(const char *name)
{
    ScriptSymbol *s;

    if (Script_FindSymbol(name, &s)) {
        SymbolTable_Remove(gScriptSymbols, name);
        Script_ClearRefs(s);
        Mem_Free(s);
        return;
    }
    Error_Show("Undefined symbol: '%s'", name);
    for (;;)
        ;
}
HOOK(0x4BAB30, Script_DeleteSymbol);

const char *Script_FindSymbol(const char *name, ScriptSymbol **out)
{
    return SymbolTable_Lookup(gScriptSymbols, (void **)out, name);
}
HOOK(0x4BAB80, Script_FindSymbol);

SymbolTable *Script_GetSymbolTable(void)
{
    return gScriptSymbols;
}
HOOK(0x4BABA0, Script_GetSymbolTable);

ScriptRef *Script_RefSymbol(const char *name)
{
    ScriptSymbol *s;
    ScriptRef *ref;

    if (!Script_FindSymbol(name, &s))
        return NULL;
    if (gScriptRefFree.head == (Node *)&gScriptRefFree.tail)
        Script_AllocRefs(0x400);
    ref = (ScriptRef *)List_RemHead(&gScriptRefFree);
    ref->sym = s;
    List_AddHead(&gScriptRefs, &ref->node);
    gScriptRefCount++;
    if (gScriptRefCount > gScriptRefPeak)
        gScriptRefPeak = gScriptRefCount;
    return ref;
}
HOOK(0x4BABB0, Script_RefSymbol);

typedef struct ScriptRefBlock {
    Node node;
    ScriptRef *refs;
} ScriptRefBlock;

void Script_AllocRefs(int count)
{
    ScriptRefBlock *block = Mem_Alloc((count * 3 + 3) * 4, 4);
    ScriptRef *ref = (ScriptRef *)(block + 1);

    block->node.prev = NULL;
    block->node.next = NULL;
    List_AddTail(&gScriptRefBlocks, &block->node);
    block->refs = ref;
    for (; count > 0; count--, ref++) {
        ref->node.prev = NULL;
        ref->node.next = NULL;
        List_AddTail(&gScriptRefFree, &ref->node);
    }
}
HOOK(0x4BAC30, Script_AllocRefs);

void Script_UnrefSymbol(ScriptRef *ref)
{
    List_Remove(&ref->node);
    List_AddHead(&gScriptRefFree, &ref->node);
    gScriptRefCount--;
}
HOOK(0x4BACA0, Script_UnrefSymbol);

static void List_Init(List *l)
{
    l->head = (Node *)&l->tail;
    l->tailpred = (Node *)l;
    l->tail = NULL;
}

void Script_InitSymbols(void)
{
    gScriptSymbols = SymbolTable_Create(4999);
    gScriptRefPeak = 0;
    gScriptRefCount = 0;
    List_Init(&gScriptRefBlocks);
    List_Init(&gScriptRefFree);
    List_Init(&gScriptRefs);
}
HOOK(0x4BACD0, Script_InitSymbols);

void Script_FreeSymbols(void)
{
    Node *n, *next;

    if (gScriptSymbols) {
        SymbolTable_Destroy(gScriptSymbols);
        gScriptSymbols = NULL;
    }
    for (n = gScriptRefBlocks.head; (next = n->next) != NULL; n = next)
        Script_FreeRefBlock(n);
    if (gScriptRefCount)
        Warning_Show("%d script symbol references not deallocated.", gScriptRefCount);
}
HOOK(0x4BAD40, Script_FreeSymbols);

void Script_FreeRefBlock(Node *block)
{
    List_Remove(block);
    Mem_Free(block);
}
HOOK(0x4BADA0, Script_FreeRefBlock);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A43E88) SymbolTable *gScriptSymbols;
BSS(A43E88) unsigned int gScriptRefCount;
BSS(A43E88) List gScriptRefs;
PAD(igb, A43E88, A43E9C, 0x4);
BSS(A43E88) List gScriptRefBlocks;
BSS(A43E88) unsigned int gScriptRefPeak;
BSS(A43E88) List gScriptRefFree;
