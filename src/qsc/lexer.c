#include <string.h>

#include "hook.h"
#include "qsc/qsc.h"

ORIG(0x517260, int, Lex_ReadToken, (const char *s, Reader *r, char *out, unsigned int size));
ORIG(0x517510, int, Str_ToInt, (const char *s));
ORIG(0x517570, double, Str_ToFloat, (const char *s));

int Lexer_Line(Lexer *lex)
{
    return lex->reader->textline + 1;
}
HOOK(0x4BF390, Lexer_Line);

void Lexer_Unget(List *pending, Token *tok)
{
    PendingToken *p = Mem_Alloc(strlen(tok->text) + 1 + sizeof(PendingToken), 4);

    p->node.prev = NULL;
    p->node.next = NULL;
    p->text = p->buf;
    p->type = tok->type;
    p->op = tok->op;
    p->ival = tok->ival;
    p->num = tok->num;
    strcpy(p->text, tok->text);
    List_AddTail(pending, &p->node);
}
HOOK(0x4BFBE0, Lexer_Unget);

static void SkipSpace(Reader *r)
{
    while (*r->cur == ' ' || *r->cur == '\t' || *r->cur == '\n' || *r->cur == '\r')
        r->cur++;
}

int Lexer_Next(Lexer *lex, Token *tok)
{
    PendingToken *p = (PendingToken *)tok->pending.head;
    Reader *r;
    unsigned int len = 0, n;
    int skip = 1;

    if (p->node.next) {
        tok->type = p->type;
        tok->op = p->op;
        tok->ival = p->ival;
        tok->num = p->num;
        strcpy(tok->text, p->text);
        List_Remove(&p->node);
        Mem_Free(p);
        return 1;
    }

    r = lex->reader;
    SkipSpace(r);
    while (*r->cur == '\0') {
        if (!Reader_Refill(r))
            return 0;
        SkipSpace(r);
    }

    tok->type = TOK_NONE;
    tok->op = 0;
    tok->text[0] = '\0';
    switch (*r->cur) {
    case '!':
        tok->type = TOK_BINARY;
        if (r->cur[1] == '=') {
            len = 2;
            tok->type = TOK_BINARY;
            tok->op = 0x0F;
            skip = 2;
        } else {
            /* copies two characters but only skips one */
            len = 2;
            tok->op = 0x14;
            tok->type = TOK_UNARY;
        }
        break;
    case '"':
        if (Lex_ReadToken(r->cur, r, tok->text, tok->bufsize)) {
            tok->type = TOK_STRING;
            skip = 0;
        }
        goto advance;
    default:
        if (Lex_ReadToken(r->cur, r, tok->text, tok->bufsize)) {
            tok->type = TOK_WORD;
            skip = 0;
        }
        goto advance;
    case '&':
        tok->type = TOK_BINARY;
        tok->op = 0x0A;
        len = 1;
        if (r->cur[1] == '&') {
            len = 2;
            tok->op = 0x1C;
            skip = 2;
        }
        break;
    case '(':
        tok->type = TOK_BINARY;
        tok->op = 0x15;
        len = 1;
        break;
    case ')':
        tok->type = TOK_PUNCT;
        tok->op = 0x16;
        len = 1;
        break;
    case '*':
        tok->type = TOK_BINARY;
        tok->op = 0x05;
        len = 1;
        break;
    case '+':
        tok->type = TOK_BINARY;
        tok->op = 0x01;
        len = 1;
        break;
    case ',':
        tok->type = TOK_BINARY;
        tok->op = 0x18;
        len = 1;
        break;
    case '-':
        tok->type = TOK_BINARY;
        tok->op = 0x02;
        len = 1;
        break;
    case '/':
        tok->type = TOK_BINARY;
        tok->op = 0x06;
        len = 1;
        break;
    case ':':
        tok->type = TOK_TERNARY;
        tok->op = 0x1E;
        len = 1;
        break;
    case ';':
        tok->type = TOK_PUNCT;
        tok->op = 0x17;
        len = 1;
        break;
    case '<':
        tok->type = TOK_BINARY;
        if (r->cur[1] == '<') {
            tok->op = 0x0B;
            len = skip = 2;
        } else if (r->cur[1] == '=') {
            tok->op = 0x12;
            len = skip = 2;
        } else {
            tok->op = 0x10;
            len = 1;
        }
        break;
    case '=':
        tok->type = TOK_BINARY;
        if (r->cur[1] == '=') {
            tok->op = 0x0E;
            len = skip = 2;
        } else {
            tok->op = 0x19;
            len = 1;
        }
        break;
    case '>':
        tok->type = TOK_BINARY;
        if (r->cur[1] == '>') {
            tok->op = 0x0C;
            len = skip = 2;
        } else if (r->cur[1] == '=') {
            tok->op = 0x13;
            len = skip = 2;
        } else {
            tok->op = 0x11;
            tok->text[0] = *r->cur;
            len = 1;
        }
        break;
    case '?':
        tok->type = TOK_TERNARY;
        tok->op = 0x1D;
        len = 1;
        break;
    case '^':
        tok->type = TOK_BINARY;
        tok->op = 0x09;
        len = 1;
        break;
    case '{':
        tok->op = 0x1F;
        tok->type = TOK_PUNCT;
        len = 1;
        break;
    case '|':
        tok->type = TOK_BINARY;
        tok->op = 0x08;
        len = 1;
        if (r->cur[1] == '|') {
            len = 2;
            tok->op = 0x1B;
            skip = 2;
        }
        break;
    case '}':
        tok->op = 0x20;
        tok->type = TOK_PUNCT;
        len = 1;
        break;
    case '~':
        tok->type = TOK_UNARY;
        tok->op = 0x0D;
        len = 1;
        break;
    }
    if (len < tok->bufsize)
        tok->text[len] = '\0';
    for (n = len; n; n--) {
        if (n < tok->bufsize)
            tok->text[n - 1] = r->cur[n - 1];
    }

advance:
    for (; skip; skip--) {
        if (*r->cur)
            r->cur++;
    }
    if (tok->type == TOK_WORD && Lexer_IsNumber(tok)) {
        if (Lexer_IsInteger(tok)) {
            tok->ival = Str_ToInt(tok->text);
            tok->type = TOK_INT;
        } else {
            tok->num = Str_ToFloat(tok->text);
            tok->type = TOK_FLOAT;
        }
    }
    return tok->type != TOK_NONE;
}
HOOK(0x4BFC70, Lexer_Next);

static int IsHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int Lexer_IsNumber(Token *tok)
{
    const char *s = tok->text;

    if (s[0] >= '0' && s[0] <= '9')
        return 1;
    if (s[0] == '$' && IsHexDigit(s[1]))
        return 1;
    /* any word with 'X' as its second letter counts too */
    if ((s[0] == '0' && s[1] == 'x') || s[1] == 'X')
        return 1;
    return 0;
}
HOOK(0x4C0170, Lexer_IsNumber);

int Lexer_IsInteger(Token *tok)
{
    const char *s;

    for (s = tok->text; *s; s++) {
        if (*s == '.' || *s == 'e' || *s == 'E')
            return 0;
    }
    return 1;
}
HOOK(0x4C01D0, Lexer_IsInteger);

void Lexer_Init(Lexer *lex, Reader *reader, char *buf, unsigned int bufsize)
{
    lex->reader = reader;
    lex->tok.text = buf;
    lex->tok.bufsize = bufsize;
    lex->tok.pending.tailpred = (Node *)&lex->tok.pending;
    lex->tok.pending.head = (Node *)&lex->tok.pending.tail;
    lex->tok.pending.tail = NULL;
}
HOOK(0x4C0200, Lexer_Init);

int Lexer_TextLine(Lexer *lex)
{
    return lex->reader->textline;
}
HOOK(0x4C0230, Lexer_TextLine);

char *Lexer_LineText(Lexer *lex)
{
    return lex->reader->buf;
}
HOOK(0x4C0240, Lexer_LineText);
