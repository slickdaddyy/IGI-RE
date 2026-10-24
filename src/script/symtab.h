#ifndef SYMTAB_H
#define SYMTAB_H

#include "core/list.h"
#include "qsc/qsctypes.h"

unsigned int SymbolTable_Hash(const char *name, unsigned int count);
char SymbolTable_Find(SymbolTable *table, Symbol **out, const char *name);
char *SymbolTable_Add(SymbolTable *table, const char *name, void *value);
char *SymbolTable_Lookup(SymbolTable *table, void **value, const char *name);

#endif
