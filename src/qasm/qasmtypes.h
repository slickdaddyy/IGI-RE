#ifndef QASM_QASMTYPES_H
#define QASM_QASMTYPES_H

#include "task/tasktypes.h"

typedef struct QTaskList {
    int count;
    int max;
    Task *items[];
} QTaskList;

typedef struct QvmLabelRef {
    Node node;
    int offset;
    int base;
} QvmLabelRef;

typedef struct QvmFixup {
    Node node;
    int offset;
    int value;
} QvmFixup;

/* Token rule, built by QvmLex_AddRule. */
typedef struct QvmLexRule {
    Node node;
    char pattern[0x100];
    char prog[0x2800];
    char classes[0x2000];
    int accept; /* stop at this rule when it matches */
    int id;
} QvmLexRule;

/* Also the node of a pushed back token. */
typedef struct QvmToken {
    Node node;
    char text[0x2BC];
    int type;
} QvmToken;

typedef struct QvmAsm {
    int pc;
    int failed;
    int unk08;
    int pass;
    void *file;
    QvmToken *token;
    struct QvmLexer *lexer;
    struct SymbolTable *labelTable;
    List labels;
    List symbols;
    List strings;
    List fixups;
} QvmAsm;

#endif
