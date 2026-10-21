#include <string.h>

#include "hook.h"
#include "qsc/qsc.h"

/* Token rules of the QVM assembler, matched as regular expressions. */
typedef struct QvmLexRule {
    Node node;
    char pattern[0x100];
    char prog[0x2800];
    char data[0x2000];
    int accept;
    int id;
} QvmLexRule;

ORIG(0x4AF9D0, int, Regex_Compile, (void *prog, void *data, const char *pattern));

void QvmLex_InitRules(List *rules)
{
    rules->tailpred = (Node *)rules;
    rules->head = (Node *)&rules->tail;
    rules->tail = NULL;
}
HOOK(0x4C0660, QvmLex_InitRules);

void QvmLex_AddRule(List *rules, const char *pattern, int id, int accept)
{
    QvmLexRule *r = Mem_Alloc(sizeof(QvmLexRule), 4);

    r->node.prev = NULL;
    r->node.next = NULL;
    strcpy(r->pattern, pattern);
    r->accept = accept;
    r->id = id;
    if (Regex_Compile(r->prog, r->data, pattern))
        List_AddTail(rules, &r->node);
    else
        Mem_Free(r);
}
HOOK(0x4C0680, QvmLex_AddRule);
