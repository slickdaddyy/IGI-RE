#include "hook.h"
#include "orig.h"
#include "qsc/qsc.h"

/* Same layout as script/symtab.h. */
typedef struct Symbol {
    Node node;
    char *name;
    void *value;
} Symbol;

typedef struct SymbolTable {
    List *buckets;
    unsigned int count;
} SymbolTable;

typedef void (*SymbolFunc)(const char *name, void *value, void *data);
typedef int (*SymbolFilter)(const char *name, void *value, void *data);

ORIG(0x4C04B0, char, SymbolTable_Find, (SymbolTable *table, Symbol **out, const char *name));

extern SymbolFilter gSymbolFilter;
extern void *gSymbolFilterData;

void SymbolTable_ForEach(SymbolTable *table, SymbolFunc fn, void *data)
{
    Node *n, *next;
    int i;

    for (i = 0; i < (int)table->count; i++) {
        for (n = table->buckets[i].head; (next = n->next); n = next)
            fn(((Symbol *)n)->name, ((Symbol *)n)->value, data);
    }
}
HOOK(0x4C0260, SymbolTable_ForEach);

void SymbolTable_ForEachFiltered(SymbolTable *table, SymbolFunc fn, void *data)
{
    Node *n, *next;
    Symbol *s;
    int i;

    if (!gSymbolFilter) {
        SymbolTable_ForEach(table, fn, data);
        return;
    }
    for (i = 0; i < (int)table->count; i++) {
        for (n = table->buckets[i].head; (next = n->next); n = next) {
            s = (Symbol *)n;
            if (gSymbolFilter(s->name, s->value, gSymbolFilterData))
                fn(s->name, s->value, data);
        }
    }
}
HOOK(0x4C02C0, SymbolTable_ForEachFiltered);

SymbolTable *SymbolTable_Create(unsigned int count)
{
    SymbolTable *table = Mem_Alloc(sizeof(SymbolTable), 4);
    unsigned int i;

    table->count = count;
    table->buckets = Mem_Alloc(count * sizeof(List), 4);
    for (i = 0; i < count; i++) {
        table->buckets[i].head = (Node *)&table->buckets[i].tail;
        table->buckets[i].tailpred = (Node *)&table->buckets[i];
        table->buckets[i].tail = NULL;
    }
    return table;
}
HOOK(0x4C0360, SymbolTable_Create);

void SymbolTable_Clear(SymbolTable *table);

void SymbolTable_Destroy(SymbolTable *table)
{
    SymbolTable_Clear(table);
    Mem_Free(table->buckets);
    table->buckets = NULL;
    Mem_Free(table);
}
HOOK(0x4C03C0, SymbolTable_Destroy);

/* Values are freed with the symbols. */
void SymbolTable_Clear(SymbolTable *table)
{
    Node *n, *next;
    Symbol *s;
    unsigned int i;

    for (i = 0; i < table->count; i++) {
        for (n = table->buckets[i].head; (next = n->next); n = next) {
            s = (Symbol *)n;
            List_Remove(n);
            if (s->value) {
                Mem_Free(s->value);
                s->value = NULL;
            }
            Mem_Free(s);
        }
    }
}
HOOK(0x4C03F0, SymbolTable_Clear);

void SymbolTable_Remove(SymbolTable *table, const char *name)
{
    Symbol *s;

    if (SymbolTable_Find(table, &s, name)) {
        List_Remove(&s->node);
        Mem_Free(s);
        return;
    }
    Error_Show("Unable to remove symbol \"%s\" (not registered)", name);
    for (;;)
        ;
}
HOOK(0x4C0460, SymbolTable_Remove);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A44314) SymbolFilter gSymbolFilter;
BSS(A44314) void *gSymbolFilterData;
