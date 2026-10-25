#include <stdarg.h>
#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "core/list.h"
#include "core/mem.h"
#include "script/symtab.h"
#include "qasm/qasmtypes.h"
#include "script/scriptvars.h"
#include "qvm/qvmvars.h"

/* Helpers of the QVM assembler (QvmAssemble at 0x4BB270). */

ORIG(0x4A5702, int, crt_vsprintf, (char *buf, const char *fmt, va_list ap));
ORIG(0x4C0780, int, QvmLex_Scan, (void *lex, void *token));

typedef void (*AsmErrorHandler)(const char *text, void *context);

typedef struct AsmLabel {
    Node node;
    List refs;
    int defined;
    int unk18;
    char name[128];
} AsmLabel;

/* a code position to patch later: a label address or the argument count of a CALL */
typedef struct AsmRef {
    Node node;
    int pos;
    int value;
} AsmRef;

typedef struct AsmName {
    Node node;
    char text[128];
} AsmName;

typedef struct AsmString {
    Node node;
    char text[0x800];
} AsmString;

void Asm_Error(QvmAsm *a, const char *fmt, ...);
void Asm_Unescape(char *dst, const char *src);

AsmRef *Asm_AddArgCountFixup(QvmAsm *a, int pos)
{
    AsmRef *r = Mem_Alloc(sizeof(AsmRef), 4);

    r->pos = pos;
    r->node.prev = NULL;
    r->node.next = NULL;
    List_AddTail(&a->fixups, &r->node);
    return r;
}
HOOK(0x4BADC0, Asm_AddArgCountFixup);

AsmLabel *Asm_DefineLabel(QvmAsm *a, const char *name)
{
    char buf[128];
    AsmLabel *l = NULL;
    size_t n;

    strcpy(buf, name);
    n = strlen(buf);
    if (n && buf[n - 1] == ':')
        buf[n - 1] = 0;
    SymbolTable_Lookup(a->labelTable, (void **)&l, buf);
    if (l) {
        if (l->defined) {
            a->failed = 1;
            Asm_Error(a, "Duplicate label: '%s'", buf);
            return NULL;
        }
        return l;
    }
    l = Mem_Alloc(sizeof(AsmLabel), 4);
    SymbolTable_Add(a->labelTable, buf, l);
    l->node.prev = NULL;
    l->node.next = NULL;
    List_AddTail(&a->labels, &l->node);
    l->defined = 0;
    strcpy(l->name, buf);
    l->refs.tail = NULL;
    l->refs.tailpred = (Node *)&l->refs;
    l->refs.head = (Node *)&l->refs.tail;
    return l;
}
HOOK(0x4BAE00, Asm_DefineLabel);

void Asm_Error(QvmAsm *a, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);
    crt_vsprintf(buf, fmt, ap);
    va_end(ap);
    gAsmErrorCount++;
    if (gAsmErrorHandler) {
        gAsmErrorHandler(buf, gAsmErrorContext);
        return;
    }
    Error_Show("%s", buf);
    for (;;)
        ;
}
HOOK(0x4BAF30, Asm_Error);

AsmLabel *Asm_FindLabel(QvmAsm *a, const char *name)
{
    AsmLabel *l = NULL;

    SymbolTable_Lookup(a->labelTable, (void **)&l, name);
    return l;
}
HOOK(0x4BAFA0, Asm_FindLabel);

AsmRef *Asm_AddLabelRef(QvmAsm *a, const char *name)
{
    AsmLabel *l = Asm_FindLabel(a, name);
    AsmRef *r;

    if (!l)
        l = Asm_DefineLabel(a, name);
    r = Mem_Alloc(sizeof(AsmRef), 4);
    r->node.prev = NULL;
    r->node.next = NULL;
    List_AddTail(&l->refs, &r->node);
    return r;
}
HOOK(0x4BAFD0, Asm_AddLabelRef);

/* index of the unescaped string in the string table, added if new */
int Asm_AddString(QvmAsm *a, const char *quoted)
{
    char buf[0x800];
    AsmString *s;
    Node *n;
    int i = 0;

    Asm_Unescape(buf, quoted);
    for (n = a->strings.head; n->next; n = n->next, i++) {
        if (strcmp(((AsmString *)n)->text, buf) == 0)
            return i;
    }
    s = Mem_Alloc(sizeof(AsmString), 4);
    s->node.prev = NULL;
    s->node.next = NULL;
    List_AddTail(&a->strings, &s->node);
    strcpy(s->text, buf);
    return i;
}
HOOK(0x4BB030, Asm_AddString);

/* src points at the opening quote */
void Asm_Unescape(char *dst, const char *src)
{
    int i = 0;
    char c;

    if (src[1] == '"') {
        dst[0] = 0;
        return;
    }
    do {
        src++;
        if (*src == '\\') {
            c = *++src;
            dst[i] = c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c;
        } else {
            dst[i] = *src;
        }
        i++;
    } while (src[1] != '"');
    dst[i] = 0;
}
HOOK(0x4BB110, Asm_Unescape);

int Asm_AddName(QvmAsm *a, const char *name)
{
    AsmName *s;
    Node *n;
    int i = 0;

    for (n = a->symbols.head; n->next; n = n->next, i++) {
        if (strcmp(((AsmName *)n)->text, name) == 0)
            return i;
    }
    s = Mem_Alloc(sizeof(AsmName), 4);
    s->node.prev = NULL;
    s->node.next = NULL;
    List_AddTail(&a->symbols, &s->node);
    strcpy(s->text, name);
    return i;
}
HOOK(0x4BB180, Asm_AddName);

/* skips tokens of type 1 */
int Asm_NextToken(void *lex, void *token)
{
    do {
        if (!QvmLex_Scan(lex, token))
            return 0;
    } while (*(int *)((char *)lex + 0x2C4) == 1);
    return 1;
}
HOOK(0x4BB240, Asm_NextToken);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(A43EC0) void *gAsmErrorContext;
BSS(A43ED8) void (*gAsmErrorHandler)(const char *msg, void *data);
