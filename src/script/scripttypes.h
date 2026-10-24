#ifndef SCRIPT_SCRIPTTYPES_H
#define SCRIPT_SCRIPTTYPES_H

#include "types.h"

/* A value returned by a symbol handler. Bytes 4 and 20 are never set. */
typedef struct ScriptValue {
    int i;
    int unk4;
    double d;
    const char *s;
    int unk14;
} ScriptValue;
_Static_assert(sizeof(ScriptValue) == 0x18, "ScriptValue");

/* the arguments of a script function. vm is 0 for parsed text scripts, the
   QVM fills a longer QvmArgs */
typedef struct ScriptArgs {
    int count;
    void *vm;
    struct Expr **argv;
} ScriptArgs;

typedef struct ScriptSymbol {
    int kind;       /* 0 variable, 1 function, 2 constant */
    int type;       /* 1 int, 2 real, 3 string */
    int dynamic;
    int size;       /* bytes of a variable, args of a function */
    void *handler;
    const char *name;
    void *data;
} ScriptSymbol;
_Static_assert(sizeof(ScriptSymbol) == 0x1C, "ScriptSymbol");

/* a reference from a parsed expression to a symbol */
typedef struct ScriptRef {
    Node node;
    ScriptSymbol *sym;
} ScriptRef;

/* a parsed script expression */
typedef struct Expr {
    signed char type;
    signed char op;
    char prec;
    char pad[5];
    union {
        double d;
        struct {
            union {
                int i;
                char *s;
                ScriptRef *ref;
                struct Expr *child;
            };
            ScriptArgs *args;
        };
    };
    char *name;
    int unk14;
    struct Expr *left;
    struct Expr *right;
    struct Expr *parent;
    int line;
    char text[];
} Expr;
_Static_assert(offsetof(Expr, left) == 0x18, "Expr");

#endif
