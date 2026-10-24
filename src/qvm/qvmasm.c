#include <stdarg.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "qvm/qvm.h"

/* Assembles QVM assembler text into a "LOOP" file. */

typedef struct QvmToken {
    int unk00[2];
    char text[0x2BC];
    int type;
} QvmToken;

enum {
    TOKEN_LABEL = 10,
    TOKEN_WORD = 9,
};

typedef struct QvmAsm {
    int pc;
    int failed;
    int unk08;
    int pass;
    void *file;
    QvmToken *token;
    void *lexer;
    void *labelTable;
    List labels;
    List symbols;   /* names used by the code */
    List strings;   /* string constants */
    List fixups;
} QvmAsm;

typedef struct QvmLabelRef {
    Node node;
    int offset;     /* where the 4 byte displacement goes */
    int base;       /* pc it is relative to */
} QvmLabelRef;

typedef struct QvmLabel {
    Node node;
    List refs;
    int defined;
    int addr;
    char name[0x80];
} QvmLabel;

typedef struct QvmFixup {
    Node node;
    int offset;
    int value;
} QvmFixup;

typedef struct QvmText {
    Node node;
    char text[];
} QvmText;

typedef struct QvmHeader {
    char magic[4];
    int version;
    int unk08;
    int symOffsets;
    int symText;
    int symTableSize;
    int symTextSize;
    int strOffsets;
    int strText;
    int strTableSize;
    int strTextSize;
    int code;
    int codeSize;
    int stackSize;
    int typeStackSize;
} QvmHeader;

typedef void (*QvmAsmCallback)(const char *msg, void *data);

ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));
ORIG(0x4AD73D, int, crt_toupper, (int c));
ORIG(0x4B08A0, int, Regex_Match, (int *end, int *groups, const char *pattern, const char *str, int nocase));
ORIG(0x4B0A80, void, Regex_Substitute, (char *out, int size, const char *tmpl, const char *src));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B1700, void, File_Write, (void *f, const void *buf, int size));
ORIG(0x4B18B0, char *, File_Load, (const char *path, int *size));
ORIG(0x4B1A30, void, File_Unload, (void *data));
ORIG(0x4B1AC0, void, File_Delete, (const char *path));
ORIG(0x4B5B60, int, Resource_Exists, (const char *name));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, int *size));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4BAE00, QvmLabel *, Asm_DefineLabel, (QvmAsm *a, const char *name));
ORIG(0x4BAF30, void, Asm_Error, (QvmAsm *a, const char *fmt, ...));
ORIG(0x4BB240, int, Asm_NextToken, (QvmToken *token, void *lexer));
ORIG(0x4BEFC0, void, Reader_Free, (void *reader));
ORIG(0x4BF1C0, void, Reader_InitMem, (void *reader, const char *src, int size));
ORIG(0x4C0360, void *, SymbolTable_Create, (unsigned int count));
ORIG(0x4C03C0, void, SymbolTable_Destroy, (void *table));
ORIG(0x4C0460, void, SymbolTable_Remove, (void *table, const char *name));
ORIG(0x4C0660, void, QvmLex_InitRules, (List *rules));
ORIG(0x4C0680, void, QvmLex_AddRule, (List *rules, const char *pattern, int id, int accept));
ORIG(0x4C0720, void, QvmLex_Init, (void *lexer, List *rules, void *reader));
ORIG(0x4C0750, void, QvmLex_Free, (void *lexer));
ORIG(0x4C0900, void, QvmAsm_RegisterOps, (void));
ORIG(0x4F1D80, void, ParamEnum_Free, (List *list));

#define gQvmAsmErrorData VAR(void *, 0xA43EC0)
#define gQvmAsmLogData VAR(void *, 0xA43EC8)
#define gQvmAsmErrors VAR(int, 0xA43ED0)
#define gQvmAsmLogFn VAR(QvmAsmCallback, 0xA43ED4)
#define gQvmAsmErrorFn VAR(QvmAsmCallback, 0xA43ED8)

void QvmAsm_Log(const char *fmt, ...);

static void QvmAsm_InitList(List *l)
{
    l->head = (Node *)&l->tail;
    l->tail = NULL;
    l->tailpred = (Node *)l;
}

static void QvmAsm_FreeList(List *l)
{
    Node *n, *next;

    for (n = l->head; (next = n->next); n = next) {
        List_Remove(n);
        Mem_Free(n);
    }
}

static void QvmAsm_WriteTable(void *f, List *l)
{
    QvmText *t, *next;
    int off = 0;

    for (t = (QvmText *)l->head; (next = (QvmText *)t->node.next); t = next) {
        File_Write(f, &off, 4);
        off += strlen(t->text) + 1;
    }
    for (t = (QvmText *)l->head; (next = (QvmText *)t->node.next); t = next)
        File_Write(f, t->text, strlen(t->text) + 1);
}

/* The code goes to a temporary file first, then labels and fixups are
   patched in and the file is written with its header and tables. */
int QvmAsm_Assemble(const char *path, const char *source)
{
    char tmp[0x104];
    char names[QVM_OPCODES][8];
    char name[8];
    List rules;
    int reader[8];
    int lexer[5];
    QvmToken token;
    QvmAsm a;
    QvmHeader h;
    QvmLabel *l, *nl;
    QvmLabelRef *r, *nr;
    QvmFixup *x, *nx;
    QvmText *t, *nt;
    void *out, *tf;
    char *data, *code, *p;
    int size, codeSize, pc, op, n, v;

    if (Regex_Match(NULL, NULL, "{.*:(.*//)*}.*", path, 0))
        Regex_Substitute(tmp, sizeof(tmp), "/0qvmbin.tmp", path);
    else
        strcpy(tmp, "LOCAL:qvmbin.tmp");
    gQvmAsmErrors = 0;
    out = File_Open(path, "w");
    if (!out) {
        Asm_Error(&a, "Failed to open file '%s' for writing.", path);
        return gQvmAsmErrors;
    }
    if (!Resource_Exists(source)) {
        Asm_Error(&a, "Failed to load assembler source file: '%s'", source);
        File_Close(out);
        return gQvmAsmErrors;
    }
    data = Resource_Load(source, &size);
    QvmAsm_Log("Initialising parser.\n");
    QvmLex_InitRules(&rules);
    QvmLex_AddRule(&rules, "[ \\t\\n\\r]+", 1, 1);
    QvmLex_AddRule(&rules, "\"([~\\\\\"]*(\\.)*)*\"", 2, 1);
    QvmLex_AddRule(&rules, "[a-zA-Z][a-zA-Z0-9_]*:", TOKEN_LABEL, 1);
    QvmLex_AddRule(&rules, "-?[0-9]+/.[0-9]+", 5, 1);
    QvmLex_AddRule(&rules, ",", 6, 1);
    QvmLex_AddRule(&rules, "/(", 7, 1);
    QvmLex_AddRule(&rules, "/)", 8, 1);
    QvmLex_AddRule(&rules, "0x[0-9a-fA-F]+", 3, 1);
    QvmLex_AddRule(&rules, "[0-9a-zA-Z]+:", 11, 1);
    QvmLex_AddRule(&rules, "@[0-9a-zA-Z]+", 12, 1);
    QvmLex_AddRule(&rules, "[a-zA-Z][a-zA-Z0-9_.]*", TOKEN_WORD, 0);
    QvmLex_AddRule(&rules, "-?[0-9]+", 4, 0);
    Reader_InitMem(reader, data, size);
    QvmLex_Init(lexer, &rules, reader);
    memset(&a, 0, sizeof(a));
    a.token = &token;
    a.lexer = lexer;
    QvmAsm_InitList(&a.labels);
    QvmAsm_InitList(&a.symbols);
    QvmAsm_InitList(&a.strings);
    QvmAsm_InitList(&a.fixups);
    pc = 0;
    a.file = out;
    a.labelTable = SymbolTable_Create(0x12CCB);
    for (op = 0; op < QVM_OPCODES; op++) {
        Qvm_CallHandler(op, 1, name, NULL);
        strcpy(names[op], name);
    }
    tf = File_Open(tmp, "w");
    if (!tf) {
        Asm_Error(&a, "Failed to create temporary file '%s'.", tmp);
        goto cleanup;
    }
    QvmAsm_Log("Assembling\n");
    a.pass = 2;
    a.file = tf;
    while (Asm_NextToken(&token, lexer) && !a.failed) {
        if (token.type == TOKEN_LABEL) {
            l = Asm_DefineLabel(&a, token.text);
            if (l) {
                l->defined = 1;
                l->addr = pc;
            }
        } else if (token.type == TOKEN_WORD) {
            for (p = token.text; *p; p++)
                *p = crt_toupper(*p);
            for (op = 0; op < QVM_OPCODES; op++)
                if (strcmp(token.text, names[op]) == 0)
                    break;
            if (op < QVM_OPCODES) {
                n = 0;
                Qvm_CallHandler(op, a.pass, &n, &a);
                pc += n;
                a.pc = pc;
            } else {
                a.failed = 1;
                Asm_Error(&a, "Unknown opcode: %s\n", token.text);
            }
        }
    }
    File_Close(tf);
    if (a.failed || !(code = File_Load(tmp, &codeSize)))
        goto cleanup;
    if (codeSize != pc) {
        Error_Show("INTERNAL ERROR: QVMAsm bin file size does not match computed program size!!!");
        for (;;)
            ;
    }
    QvmAsm_Log("Patching code\n");
    for (l = (QvmLabel *)a.labels.head; (nl = (QvmLabel *)l->node.next); l = nl) {
        for (r = (QvmLabelRef *)l->refs.head; (nr = (QvmLabelRef *)r->node.next); r = nr) {
            if (!l->defined)
                Asm_Error(&a, "Unknown label '%s' at PC:%08x\n", l->name, r->offset);
            v = l->addr - r->base;
            memcpy(code + r->offset, &v, 4);
            List_Remove(&r->node);
            Mem_Free(r);
        }
        SymbolTable_Remove(a.labelTable, l->name);
        List_Remove(&l->node);
        Mem_Free(l);
    }
    for (x = (QvmFixup *)a.fixups.head; (nx = (QvmFixup *)x->node.next); x = nx) {
        memcpy(code + x->offset, &x->value, 4);
        List_Remove(&x->node);
        Mem_Free(x);
    }
    memcpy(h.magic, "LOOP", 4);
    h.version = 8;
    h.stackSize = 0;
    h.typeStackSize = 0;
    h.unk08 = 5;
    h.symTableSize = 0;
    h.symTextSize = 0;
    for (t = (QvmText *)a.symbols.head; (nt = (QvmText *)t->node.next); t = nt) {
        h.symTableSize++;
        h.symTextSize += strlen(t->text) + 1;
    }
    h.symTableSize *= 4;
    h.symOffsets = sizeof(h);
    h.symText = sizeof(h) + h.symTableSize;
    h.strOffsets = h.symText + h.symTextSize;
    h.strTableSize = 0;
    h.strTextSize = 0;
    for (t = (QvmText *)a.strings.head; (nt = (QvmText *)t->node.next); t = nt) {
        h.strTableSize++;
        h.strTextSize += strlen(t->text) + 1;
    }
    h.strTableSize *= 4;
    h.strText = h.strOffsets + h.strTableSize;
    h.code = h.strText + h.strTextSize;
    h.codeSize = pc;
    File_Write(out, &h, sizeof(h));
    QvmAsm_WriteTable(out, &a.symbols);
    QvmAsm_WriteTable(out, &a.strings);
    File_Write(out, code, pc);
    File_Unload(code);
cleanup:
    QvmAsm_Log("Cleaning up\n");
    QvmAsm_FreeList(&a.symbols);
    QvmAsm_FreeList(&a.strings);
    QvmAsm_FreeList(&a.fixups);
    for (l = (QvmLabel *)a.labels.head; (nl = (QvmLabel *)l->node.next); l = nl) {
        QvmAsm_FreeList(&l->refs);
        SymbolTable_Remove(a.labelTable, l->name);
        List_Remove(&l->node);
        Mem_Free(l);
    }
    SymbolTable_Destroy(a.labelTable);
    File_Delete(tmp);
    Reader_Free(reader);
    QvmLex_Free(lexer);
    ParamEnum_Free(&rules);
    Resource_Flush(data);
    File_Close(out);
    return gQvmAsmErrors;
}
HOOK(0x4BB270, QvmAsm_Assemble);

void QvmAsm_Log(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    if (gQvmAsmLogFn) {
        va_start(ap, fmt);
        crt_vsprintf(buf, fmt, ap);
        va_end(ap);
        gQvmAsmLogFn(buf, gQvmAsmLogData);
    }
}
HOOK(0x4BBC30, QvmAsm_Log);

void QvmAsm_Init(void)
{
    gQvmAsmErrorFn = NULL;
    VAR(int, 0xA43ECC) = 0;
    gQvmAsmLogFn = NULL;
    gQvmAsmErrorData = NULL;
    VAR(int, 0xA43EC4) = 0;
    gQvmAsmLogData = NULL;
    QvmAsm_RegisterOps();
}
HOOK(0x4BBC80, QvmAsm_Init);
