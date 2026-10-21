#include <string.h>

#include "hook.h"
#include "qasm/qasm.h"

ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));
ORIG(0x4B0270, int, Regex_Run, (int *end, int *groups, const char *str, void *prog, const char *classes, int nocase));
ORIG(0x4BF1E0, int, Reader_Refill, (QvmReader *r));

void QvmLex_Init(QvmLexer *lex, List *rules, QvmReader *reader)
{
    lex->rules = rules;
    lex->reader = reader;
    lex->pending.tailpred = (Node *)&lex->pending;
    lex->pending.head = (Node *)&lex->pending.tail;
    lex->pending.tail = NULL;
}
HOOK(0x4C0720, QvmLex_Init);

void QvmLex_Free(QvmLexer *lex)
{
    Node *n, *next;

    for (n = lex->pending.head; (next = n->next); n = next) {
        List_Remove(n);
        Mem_Free(n);
    }
}
HOOK(0x4C0750, QvmLex_Free);

/* Reads the next token, whitespace included. The longest match wins, ties
   go to the later rule, and an accept rule ends the search. */
int QvmLex_Scan(QvmToken *tok, QvmLexer *lex)
{
    QvmToken *p = (QvmToken *)lex->pending.head;
    QvmLexRule *r, *next, *best = NULL;
    int bestLen = 0, len;
    unsigned int n;

    if (p->node.next) {
        strcpy(tok->text, p->text);
        tok->type = p->type;
        List_Remove(&p->node);
        Mem_Free(p);
        return 1;
    }
    while (!*lex->reader->cur)
        if (!Reader_Refill(lex->reader))
            return 0;
    r = (QvmLexRule *)lex->rules->head;
    if (!r->node.next)
        return 0;
    for (; (next = (QvmLexRule *)r->node.next); r = next) {
        if (Regex_Run(&len, NULL, lex->reader->cur, r->prog, r->classes, 0) && len >= bestLen) {
            bestLen = len;
            best = r;
            if (r->accept)
                break;
        }
    }
    if (!best)
        return 0;
    n = (unsigned int)bestLen < QVM_TOKEN_TEXT ? (unsigned int)bestLen : QVM_TOKEN_TEXT;
    crt_strncpy(tok->text, lex->reader->cur, n);
    /* at the full length this is the first byte of type */
    ((char *)tok)[offsetof(QvmToken, text) + n] = 0;
    lex->reader->cur += bestLen;
    tok->type = best->id;
    return 1;
}
HOOK(0x4C0780, QvmLex_Scan);
