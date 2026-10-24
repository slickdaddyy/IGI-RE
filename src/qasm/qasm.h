#ifndef QASM_QASM_H
#define QASM_QASM_H

#include <stddef.h>

#include "igi.h"
#include "qasm/qasmtypes.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B1700, void, File_Write, (void *f, const void *buf, int size));
ORIG(0x4BD3D0, void, Qvm_SetHandler, (int op, int slot, void *fn));

/* Only cur is used here, see qsc/qsc.h for the rest. */
typedef struct QvmReader {
    char *cur;
} QvmReader;

#define QVM_TOKEN_TEXT 0x2BC

typedef struct QvmLexer {
    List *rules;
    QvmReader *reader;
    List pending;
} QvmLexer;

/* Token types, the rule ids QvmAsm_Assemble registers */
enum {
    TOKEN_SPACE = 1,
    TOKEN_STRING = 2,
    TOKEN_HEX = 3,
    TOKEN_INT = 4,
    TOKEN_FLOAT = 5,
    TOKEN_COMMA = 6,
    TOKEN_OPEN = 7,
    TOKEN_CLOSE = 8,
    TOKEN_WORD = 9,
    TOKEN_LABEL = 10,
    TOKEN_ADDRESS = 12, /* @hex */
};

/* Name or string table of a loaded Qvm (at 0x30 and 0x48). */
typedef struct QvmTable {
    int unk00[2];
    int size;
    int textSize;
    int *offsets;
    char *text;
} QvmTable;

/* The part of a Qvm the disassembler reads. */
typedef struct QvmImage {
    unsigned char *code;
    int unk04[11];
    QvmTable strings;
    QvmTable symbols;
} QvmImage;

typedef struct QvmDis {
    void *file;
    int indent;
    int pc;
} QvmDis;

/* Opcodes the assembler emits */
enum {
    OP_PUSH = 2,
    OP_PUSHB = 3,
    OP_PUSHW = 4,
    OP_PUSHF = 5,
    OP_PUSHSI = 8,
    OP_PUSHSIB = 9,
    OP_PUSHSIW = 10,
    OP_PUSHI = 11,
    OP_PUSHII = 12,
    OP_PUSHIIB = 13,
    OP_PUSHIIW = 14,
    OP_PUSH0 = 15,
    OP_PUSH1 = 16,
    OP_PUSHM = 17,
    OP_BRA = 20,
    OP_BF = 21,
    OP_BT = 22,
    OP_CALL = 24,
};

#define QVM_OPCODES 0x31

void QvmLex_Init(QvmLexer *lex, List *rules, QvmReader *reader);
void QvmLex_Free(QvmLexer *lex);
int QvmLex_Scan(QvmToken *tok, QvmLexer *lex);

unsigned int QvmAsm_ParseHex(const char *s);

#endif
