#include <string.h>

#include "orig.h"
#include "core/mem.h"
#include "script/symtab.h"

unsigned int SymbolTable_Hash(const char *name, unsigned int count)
{
    unsigned int h = 0;

    while (*name)
        h = h * 32 + (unsigned char)*name++;
    return h % count;
}

char SymbolTable_Find(SymbolTable *table, Symbol **out, const char *name)
{
    Node *n = table->buckets[SymbolTable_Hash(name, table->count)].head;

    for (; n->next; n = n->next) {
        if (strcmp(name, ((Symbol *)n)->name) == 0) {
            *out = (Symbol *)n;
            return 1;
        }
    }
    *out = NULL;
    return 0;
}

char *SymbolTable_Add(SymbolTable *table, const char *name, void *value)
{
    Symbol *s;
    size_t len = strlen(name) + 1;

    if (SymbolTable_Find(table, &s, name)) {
        Error_Show("Symbol \"%s\" already registered in hash table", name);
        for (;;)
            ;
    }
    s = Mem_Alloc(sizeof(Symbol) + len, 4);
    s->name = s->text;
    s->value = value;
    memcpy(s->name, name, len);
    s->node.prev = NULL;
    s->node.next = NULL;
    List_AddHead(&table->buckets[SymbolTable_Hash(name, table->count)], &s->node);
    return s->name;
}

char *SymbolTable_Lookup(SymbolTable *table, void **value, const char *name)
{
    Symbol *s;

    if (SymbolTable_Find(table, &s, name)) {
        if (value)
            *value = s->value;
        return s->name;
    }
    if (value)
        *value = NULL;
    return NULL;
}
