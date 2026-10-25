#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "qvm/qvm.h"
#include "qsc/qsctypes.h"
#include "script/scriptvars.h"

/* Turns parsed QSC statements into QVM assembler text. */

enum {
    EXPR_INT = 1,
    EXPR_STRING,
    EXPR_REAL,
    EXPR_NAME,
    EXPR_CALL,
    EXPR_PAREN,
    EXPR_COND,
    EXPR_OP,
};

typedef double QscReal __attribute__((aligned(4)));

enum {
    BLOCK_LIST = 1,
    BLOCK_IF,
    BLOCK_ELSEIF,
    BLOCK_WHILE,
    BLOCK_DO,
    BLOCK_FOR,
    BLOCK_EXPR,
};

typedef struct QscScript {
    char name[0x80];
    char *data;
    int size;
} QscScript;

typedef void (*QscGenCallback)(const char *msg, void *data);

ORIG(0x4AF8F0, void, List_AddTail, (List *list, Node *node));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B1700, void, File_Write, (void *f, const void *buf, int size));
ORIG(0x4B1AC0, void, File_Delete, (const char *path));
ORIG(0x4BEFC0, void, Reader_Free, (void *reader));
ORIG(0x4BF1C0, void, Reader_InitMem, (void *reader, const char *src, int size));
ORIG(0x4C0200, void, Lexer_Init, (void *lexer, void *reader, char *buf, unsigned int size));
ORIG(0x4BFC70, int, Lexer_Next, (void *lexer, void *token));
ORIG(0x4BF1F0, int, Parse_Block, (void *lexer, Block **out));
ORIG(0x4BF8F0, void, Block_Free, (Block *b));

extern void *gQscGenErrorData;
extern int gQscGenUnused0;
extern int gQscGenUnused1;
extern QscGenCallback gQscGenLogFn;
extern QscGenCallback gQscGenErrorFn;
extern int gQscGenErrors;
extern void *gQscGenLogData;

/* "           NAME    ", one per opcode, and 20 spaces for continued lines */
extern char gQscGenOpText[][20];
extern char gQscGenIndent[];

void QscGen_Error(const char *fmt, ...);
void QscGen_Log(const char *fmt, ...);
void QscGen_WriteOpcode(int op, void *f);
void QscGen_WriteNewline(void *f);
void QscGen_Statement(void *f, Block *b);
void QscGen_MakeLabel(char *buf, const char *prefix, const void *p);
void QscGen_WriteText(void *f, const char *s);
void QscGen_Expression(void *f, Expr *e);
void QscGen_WriteIndent(void *f);
void QscGen_WriteString(void *f, const char *s);
void QscGen_WriteInt(void *f, int v);
void QscGen_WriteFloat(void *f, double v);

/* Returns the number of errors. */
int QscGen_Compile(const char *path, QscScript *script)
{
    List blocks;
    int reader[8];
    double lexer[6];
    char buf[0x800];
    char name[16];
    Block *b;
    Node *n, *next;
    void *f;
    int op;

    File_Delete(path);
    memset(gQscGenIndent, ' ', 20);
    gQscGenIndent[20] = 0;
    blocks.head = (Node *)&blocks.tail;
    blocks.tail = NULL;
    blocks.tailpred = (Node *)&blocks;
    gQscGenErrors = 0;
    for (op = 0; op < QVM_OPCODES; op++) {
        char *d = gQscGenOpText[op] + 11;
        const char *s = name;

        Qvm_CallHandler(op, 1, name, NULL);
        memset(gQscGenOpText[op], ' ', 19);
        gQscGenOpText[op][19] = 0;
        while (*s)
            *d++ = *s++;
    }
    gScriptError = 0;
    Lexer_Init(lexer, reader, buf, sizeof(buf));
    Reader_InitMem(reader, script->data, script->size);
    f = File_Open(path, "w");
    if (!f) {
        QscGen_Error("Failed to open file '%s' for writing.", path);
    } else {
        QscGen_Log("Parsing script and generating assembly code.\n");
        for (;;) {
            b = NULL;
            if (!Lexer_Next(lexer, (char *)lexer + 8))
                break;
            if (!Parse_Block(lexer, &b)) {
                gQscGenErrors++;
                break;
            }
            List_AddTail(&blocks, &b->node);
            QscGen_Statement(f, b);
        }
        QscGen_WriteOpcode(OP_BRK, f);
        QscGen_WriteNewline(f);
        File_Close(f);
    }
    for (n = blocks.head; (next = n->next); n = next) {
        List_Remove(n);
        Block_Free((Block *)n);
    }
    Reader_Free(reader);
    return gQscGenErrors;
}
HOOK(0x4BBCB0, QscGen_Compile);

void QscGen_Error(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);
    crt_vsprintf(buf, fmt, ap);
    va_end(ap);
    gQscGenErrors++;
    if (gQscGenErrorFn) {
        gQscGenErrorFn(buf, gQscGenErrorData);
        return;
    }
    Error_Show(buf);
    for (;;)
        ;
}
HOOK(0x4BBE90, QscGen_Error);

void QscGen_Log(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    if (gQscGenLogFn) {
        va_start(ap, fmt);
        crt_vsprintf(buf, fmt, ap);
        va_end(ap);
        gQscGenLogFn(buf, gQscGenLogData);
    }
}
HOOK(0x4BBEF0, QscGen_Log);

void QscGen_WriteOpcode(int op, void *f)
{
    File_Write(f, gQscGenOpText[op], 19);
}
HOOK(0x4BBF40, QscGen_WriteOpcode);

void QscGen_WriteNewline(void *f)
{
    File_Write(f, "\n", 1);
}
HOOK(0x4BBF60, QscGen_WriteNewline);

static void QscGen_WriteLabel(void *f, char *buf, const char *prefix, const void *p)
{
    QscGen_MakeLabel(buf, prefix, p);
    QscGen_WriteText(f, buf);
}

/* Labels are a prefix and the address of the node they belong to. */
void QscGen_Statement(void *f, Block *b)
{
    char label[64];
    Block *prev, *e;
    Node *n, *next;

    switch (b->type) {
    case BLOCK_LIST:
        for (n = b->stmts.head; (next = n->next); n = next)
            QscGen_Statement(f, (Block *)n);
        return;
    case BLOCK_IF:
        QscGen_Expression(f, b->cond.cond);
        QscGen_WriteOpcode(OP_BF, f);
        QscGen_WriteLabel(f, label, "EL", b);
        QscGen_WriteNewline(f);
        QscGen_Statement(f, b->cond.then);
        QscGen_WriteOpcode(OP_BRA, f);
        QscGen_WriteLabel(f, label, "IF", b);
        QscGen_WriteNewline(f);
        prev = b;
        for (e = (Block *)b->cond.elifs.head; e->node.next; e = (Block *)e->node.next) {
            QscGen_WriteLabel(f, label, "EL", prev);
            QscGen_WriteText(f, ":");
            QscGen_WriteNewline(f);
            QscGen_Expression(f, e->loop.cond);
            QscGen_WriteOpcode(OP_BF, f);
            QscGen_WriteLabel(f, label, "EL", e);
            QscGen_WriteNewline(f);
            QscGen_Statement(f, e->loop.body);
            QscGen_WriteOpcode(OP_BRA, f);
            QscGen_WriteLabel(f, label, "IF", b);
            QscGen_WriteNewline(f);
            prev = e;
        }
        QscGen_WriteLabel(f, label, "EL", prev);
        QscGen_WriteText(f, ":");
        QscGen_WriteNewline(f);
        if (b->cond.otherwise)
            QscGen_Statement(f, b->cond.otherwise);
        QscGen_WriteLabel(f, label, "IF", b);
        QscGen_WriteText(f, ":");
        QscGen_WriteNewline(f);
        return;
    case BLOCK_WHILE:
        QscGen_WriteLabel(f, label, "WL", b);
        QscGen_WriteText(f, ":");
        QscGen_WriteNewline(f);
        QscGen_Expression(f, b->loop.cond);
        QscGen_WriteOpcode(OP_BF, f);
        QscGen_WriteLabel(f, label, "WE", b);
        QscGen_WriteNewline(f);
        QscGen_Statement(f, b->loop.body);
        QscGen_WriteOpcode(OP_BRA, f);
        QscGen_WriteLabel(f, label, "WL", b);
        QscGen_WriteNewline(f);
        QscGen_WriteLabel(f, label, "WE", b);
        QscGen_WriteText(f, ":");
        QscGen_WriteNewline(f);
        return;
    case BLOCK_EXPR:
        if (b->expr) {
            QscGen_Expression(f, b->expr);
            QscGen_WriteOpcode(OP_POP, f);
            QscGen_WriteNewline(f);
        }
        return;
    default:
        Error_Show("Internal error: Unknown statement block type.");
        for (;;)
            ;
    }
}
HOOK(0x4BBF80, QscGen_Statement);

void QscGen_MakeLabel(char *buf, const char *prefix, const void *p)
{
    crt_sprintf(buf, "%s%p", prefix, p);
}
HOOK(0x4BC2A0, QscGen_MakeLabel);

void QscGen_WriteText(void *f, const char *s)
{
    File_Write(f, s, strlen(s));
}
HOOK(0x4BC2C0, QscGen_WriteText);

/* Script operator to opcode, 0 for operators that are not allowed */
static const unsigned char QscGen_OpCodes[0x1D] = {
    [1] = OP_ADD, [2] = OP_SUB, [3] = OP_PLUS, [4] = OP_MINUS, [5] = OP_MUL,
    [6] = OP_DIV, [8] = OP_OR, [9] = OP_XOR, [10] = OP_AND, [11] = OP_SHL,
    [12] = OP_SHR, [13] = OP_INV, [14] = OP_EQ, [15] = OP_NE, [16] = OP_LT,
    [17] = OP_GT, [18] = OP_LE, [19] = OP_GE, [20] = OP_NOT, [25] = OP_ASSIGN,
    [27] = OP_LOR, [28] = OP_LAND,
};

/* A call pushes the function name, then CALL with the address of the code
   of each argument. That code follows, each argument ending in BRK. */
void QscGen_Expression(void *f, Expr *e)
{
    char label[64];
    ScriptArgs *a;
    int i, n, col;

    if (!e)
        return;
    for (;;) {
        switch (e->type) {
        case EXPR_INT:
            QscGen_WriteOpcode(OP_PUSH, f);
            QscGen_WriteInt(f, e->i);
            QscGen_WriteNewline(f);
            return;
        case EXPR_STRING:
            QscGen_WriteOpcode(OP_PUSHS, f);
            QscGen_WriteString(f, e->s);
            QscGen_WriteNewline(f);
            return;
        case EXPR_REAL:
            QscGen_WriteOpcode(OP_PUSHF, f);
            QscGen_WriteFloat(f, e->d);
            QscGen_WriteNewline(f);
            return;
        case EXPR_NAME:
        case EXPR_CALL:
            QscGen_WriteOpcode(OP_PUSHI, f);
            QscGen_WriteText(f, e->name);
            QscGen_WriteNewline(f);
            a = e->args;
            if (!a)
                return;
            n = a->count;
            QscGen_WriteOpcode(OP_CALL, f);
            QscGen_WriteText(f, "(");
            for (i = 0, col = 0; i < n; i++, col++) {
                if (col >= 5) {
                    col = 0;
                    QscGen_WriteNewline(f);
                    QscGen_WriteIndent(f);
                }
                QscGen_WriteLabel(f, label, "Arg", a->argv[i]);
                if (i < n - 1)
                    QscGen_WriteText(f, ", ");
            }
            QscGen_WriteText(f, ")");
            QscGen_WriteNewline(f);
            QscGen_MakeLabel(label, "AC", a);
            QscGen_WriteOpcode(OP_BRA, f);
            QscGen_WriteText(f, label);
            QscGen_WriteNewline(f);
            for (i = 0; i < n; i++) {
                QscGen_WriteLabel(f, label, "Arg", a->argv[i]);
                QscGen_WriteText(f, ":");
                QscGen_WriteNewline(f);
                QscGen_Expression(f, a->argv[i]);
                QscGen_WriteOpcode(OP_BRK, f);
                QscGen_WriteNewline(f);
            }
            QscGen_WriteLabel(f, label, "AC", a);
            QscGen_WriteText(f, ":");
            QscGen_WriteNewline(f);
            return;
        case EXPR_PAREN:
            e = e->child;
            if (!e)
                return;
            continue;
        case EXPR_COND:
            /* the false branch gets no label line, as in the original */
            QscGen_Expression(f, e->child);
            QscGen_WriteOpcode(OP_BF, f);
            QscGen_WriteLabel(f, label, "CBF", e->right);
            QscGen_WriteNewline(f);
            QscGen_Expression(f, e->left);
            QscGen_WriteOpcode(OP_BRA, f);
            QscGen_WriteLabel(f, label, "CON", e);
            QscGen_WriteLabel(f, label, "CBF", e->right);
            QscGen_WriteNewline(f);
            QscGen_Expression(f, e->right);
            QscGen_WriteLabel(f, label, "CON", e);
            QscGen_WriteText(f, ":");
            QscGen_WriteNewline(f);
            return;
        case EXPR_OP:
            if (e->left)
                QscGen_Expression(f, e->left);
            if (e->op == 0x18) {
                QscGen_WriteOpcode(OP_POP, f);
                QscGen_WriteNewline(f);
            }
            if (e->right)
                QscGen_Expression(f, e->right);
            if (e->op != 0x18) {
                if (e->op < 1 || e->op >= (int)sizeof(QscGen_OpCodes) || !QscGen_OpCodes[e->op]) {
                    Error_Show("Unknown operator in expression.");
                    for (;;)
                        ;
                }
                QscGen_WriteOpcode(QscGen_OpCodes[e->op], f);
            }
            QscGen_WriteNewline(f);
            return;
        default:
            Error_Show("Unknown expression type");
            for (;;)
                ;
        }
    }
}
HOOK(0x4BC2F0, QscGen_Expression);

void QscGen_WriteIndent(void *f)
{
    File_Write(f, gQscGenIndent, 20);
}
HOOK(0x4BC770, QscGen_WriteIndent);

void QscGen_WriteString(void *f, const char *s)
{
    File_Write(f, "\"", 1);
    for (; *s; s++) {
        switch (*s) {
        case '\t':
        case '\n':
        case '\r':
            File_Write(f, "\\n", 2);
            break;
        case '"':
            File_Write(f, "\\\"", 2);
            break;
        case '\\':
            File_Write(f, "\\\\", 2);
            break;
        default:
            File_Write(f, s, 1);
            break;
        }
    }
    File_Write(f, "\"", 1);
}
HOOK(0x4BC790, QscGen_WriteString);

void QscGen_WriteInt(void *f, int v)
{
    char buf[256];

    crt_sprintf(buf, "%d", v);
    File_Write(f, buf, strlen(buf));
}
HOOK(0x4BC880, QscGen_WriteInt);

void QscGen_WriteFloat(void *f, double v)
{
    char buf[256];

    crt_sprintf(buf, "%f", v);
    File_Write(f, buf, strlen(buf));
}
HOOK(0x4BC8D0, QscGen_WriteFloat);

void QscGen_Init(void)
{
    gQscGenErrorFn = NULL;
    gQscGenLogFn = NULL;
    gQscGenUnused0 = 0;
    gQscGenErrorData = NULL;
    gQscGenUnused1 = 0;
    gQscGenLogData = NULL;
}
HOOK(0x4BC920, QscGen_Init);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
STORE(igb, A43EE0, 1) char store_A43EE0[49][20];
ALIAS(gQscGenOpText, store_A43EE0, 0);
BSS(A43EE0) void *gQscGenErrorData;
BSS(A43EE0) int gQscGenUnused0;
BSS(A43EE0) int gQscGenUnused1;
BSS(A43EE0) QscGenCallback gQscGenLogFn;
BSS(A43EE0) QscGenCallback gQscGenErrorFn;
BSS(A43EE0) int gQscGenErrors;
BSS(A43EE0) void *gQscGenLogData;
STORE(igb, A43EE0, 1) char store_A442D0[28];
ALIAS(gQscGenIndent, store_A442D0, 0);
