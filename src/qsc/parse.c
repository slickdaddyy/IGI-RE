#include <string.h>

#include "hook.h"
#include "qsc/qsc.h"

ORIG(0x4B9CD0, int, Expr_Parse, (Lexer *lex, void **out, int unk3, int unk4));
ORIG(0x4B9CA0, int, Expr_ParseNext, (Lexer *lex, void **out, int unk3));

static Block *NewBlock(size_t size, int type)
{
    Block *b = Mem_Alloc(size, 4);

    memset(b, 0, size);
    b->type = type;
    return b;
}

int Parse_Block(Lexer *lex, Block **out)
{
    Block *b, *stmt;
    void *expr;
    int single;

    *out = NULL;
    b = Mem_Alloc(0x18, 4);
    b->type = BLOCK_LIST;
    b->node.prev = NULL;
    b->node.next = NULL;
    b->stmts.head = (Node *)&b->stmts.tail;
    b->stmts.tailpred = (Node *)&b->stmts;
    b->stmts.tail = NULL;

    if (Parse_Keyword(lex) == KW_OPEN) {
        if (!Lexer_Next(lex, &lex->tok)) {
            Script_Error("Unexpected end of file.");
            Block_Free(b);
            return 0;
        }
        single = 0;
    } else {
        single = 1;
    }

    for (;;) {
        switch (Parse_Keyword(lex)) {
        case KW_NONE:
            if (!Expr_Parse(lex, &expr, 0, 0)) {
                if (gScriptError)
                    Script_Error("Script error: Aborting.\n");
                goto fail;
            }
            stmt = Mem_Alloc(0x10, 4);
            stmt->type = BLOCK_EXPR;
            stmt->node.prev = NULL;
            stmt->node.next = NULL;
            stmt->expr = expr;
            List_AddTail(&b->stmts, &stmt->node);
            break;
        case KW_CLOSE:
            if (single) {
                Script_Error("Unexpected '}' in line %d", Lexer_Line(lex));
                Block_Free(b);
                return 0;
            }
            *out = b;
            return 1;
        default:
            if (!Parse_Statement(lex, &stmt))
                goto fail;
            List_AddTail(&b->stmts, &stmt->node);
            break;
        }
        if (single) {
            *out = b;
            return 1;
        }
        if (!Lexer_Next(lex, &lex->tok)) {
            Script_Error("Unexpected end of file.");
            goto fail;
        }
    }

fail:
    Block_Free(b);
    return 0;
}
HOOK(0x4BF1F0, Parse_Block);

int Parse_Keyword(Lexer *lex)
{
    const char *s = lex->tok.text;

    if (!strcmp(s, "{"))
        return KW_OPEN;
    if (!strcmp(s, "}"))
        return KW_CLOSE;
    if (!strcmp(s, "if"))
        return KW_IF;
    if (!strcmp(s, "else"))
        return KW_ELSE;
    if (!strcmp(s, "do"))
        return KW_DO;
    if (!strcmp(s, "while"))
        return KW_WHILE;
    if (!strcmp(s, "for"))
        return KW_FOR;
    return KW_NONE;
}
HOOK(0x4BF3A0, Parse_Keyword);

int Parse_Statement(Lexer *lex, Block **out)
{
    switch (Parse_Keyword(lex)) {
    case KW_OPEN:
        return Parse_Block(lex, out);
    case KW_IF:
        return Parse_If(lex, out);
    case KW_ELSE:
        Script_Error("'else' without matching 'if' in line %d", Lexer_Line(lex));
        return 0;
    case KW_WHILE:
        return Parse_While(lex, out);
    default:
        Script_Error("Syntax error in line %d", Lexer_Line(lex));
        return 0;
    }
}
HOOK(0x4BF580, Parse_Statement);

int Parse_If(Lexer *lex, Block **out)
{
    Token *tok = &lex->tok;
    Block *b, *e;

    Parse_Keyword(lex);
    b = NewBlock(0x24, BLOCK_IF);
    b->cond.elifs.head = (Node *)&b->cond.elifs.tail;
    b->cond.elifs.tail = NULL;
    b->cond.elifs.tailpred = (Node *)&b->cond.elifs;
    *out = b;

    if (!Lexer_Next(lex, tok))
        goto eof;
    if (tok->text[0] != '(' || !Expr_ParseNext(lex, &b->cond.cond, 0))
        goto fail;
    if (!Lexer_Next(lex, tok))
        goto eof;
    if (!Parse_Block(lex, &b->cond.then))
        goto fail;
    if (!Lexer_Next(lex, tok))
        return 1;
    if (Parse_Keyword(lex) != KW_ELSE) {
        Lexer_Unget(&tok->pending, tok);
        return 1;
    }
    if (!Lexer_Next(lex, tok)) {
        /* unlike the same case in the loop below, this keeps the block */
        Script_Error("Unexpected end of file.");
        return 1;
    }
    while (Parse_Keyword(lex) == KW_IF) {
        if (!Lexer_Next(lex, tok))
            goto eof;
        if (tok->text[0] != '(')
            goto fail;
        e = NewBlock(0x14, BLOCK_ELSEIF);
        List_AddTail(&b->cond.elifs, &e->node);
        if (!Expr_ParseNext(lex, &e->loop.cond, 0))
            goto fail;
        if (!Lexer_Next(lex, tok))
            goto eof;
        if (!Parse_Block(lex, &e->loop.body))
            goto fail;
        if (!Lexer_Next(lex, tok))
            return 1;
        if (Parse_Keyword(lex) != KW_ELSE) {
            Lexer_Unget(&tok->pending, tok);
            return 1;
        }
        if (!Lexer_Next(lex, tok))
            goto eof;
    }
    if (Parse_Block(lex, &b->cond.otherwise))
        return 1;
    goto fail;

eof:
    Script_Error("Unexpected end of file.");
fail:
    Block_Free(*out);
    *out = NULL;
    return 0;
}
HOOK(0x4BF620, Parse_If);

int Parse_While(Lexer *lex, Block **out)
{
    Block *b;

    Parse_Keyword(lex);
    b = NewBlock(0x14, BLOCK_WHILE);
    *out = b;
    /* on failure the block is neither freed nor cleared */
    return Lexer_Next(lex, &lex->tok) && lex->tok.text[0] == '(' &&
           Expr_ParseNext(lex, &b->loop.cond, 0) && Lexer_Next(lex, &lex->tok) &&
           Parse_Block(lex, &b->loop.body);
}
HOOK(0x4BF850, Parse_While);
