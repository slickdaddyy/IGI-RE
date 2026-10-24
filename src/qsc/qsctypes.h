#ifndef QSC_QSCTYPES_H
#define QSC_QSCTYPES_H

#include "types.h"

typedef struct SymbolTable {
    List *buckets;
    unsigned int count;
} SymbolTable;

typedef struct Symbol {
    Node node;
    char *name;
    void *value;
    char text[];
} Symbol;

typedef struct Block {
    Node node;
    int type;
    union {
        List stmts;
        struct Expr *expr;
        struct {
            struct Expr *cond;
            struct Block *body;
        } loop; /* also used by else-if entries */
        struct {
            struct Expr *cond;
            struct Block *then;
            List elifs;
            struct Block *otherwise;
        } cond;
        struct {
            struct Expr *init;
            struct Expr *cond;
            struct Expr *step;
            struct Block *body;
        } loopfor;
    };
} Block;

#endif
