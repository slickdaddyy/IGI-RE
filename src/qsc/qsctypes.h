#ifndef QSC_QSCTYPES_H
#define QSC_QSCTYPES_H

#include "types.h"

typedef struct SymbolTable {
    List *buckets;
    unsigned int count;
} SymbolTable;

#endif
