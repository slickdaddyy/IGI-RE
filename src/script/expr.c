#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "core/mem.h"
#include "script/script.h"
#include "script/symbol.h"

ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));
ORIG(0x5169B0, char *, crt_strncat, (char *dst, const char *src, size_t n));
ORIG(0x4B7E10, void, Script_Error, (const char *fmt, ...));
ORIG(0x4BFC70, int, Lexer_Next, (ScriptLexer *lex, void *token));
ORIG(0x4C0230, int, Lexer_TextLine, (ScriptLexer *lex));
ORIG(0x4C0240, char *, Lexer_LineText, (ScriptLexer *lex));

#define gExprInTernary VAR(int, 0xA43E80)

/* x87 status bits after "fcom": C0 for below, C3 for equal, both when unordered */
#define FPU_C0 0x01
#define FPU_C3 0x40

static int Fpu_Compare(long double st0, long double m)
{
    if (__builtin_isnan(st0) || __builtin_isnan(m))
        return FPU_C0 | FPU_C3;
    return (st0 < m ? FPU_C0 : 0) | (st0 == m ? FPU_C3 : 0);
}

static int Fpu_IsZero(long double v)
{
    return (Fpu_Compare(v, 0.0) & FPU_C3) != 0;
}

static void Lexer_Error(ScriptLexer *lex, const char *fmt)
{
    char *line = Lexer_LineText(lex);

    Script_Error(fmt, Lexer_TextLine(lex), line);
}

static ScriptSymbol *Expr_Symbol(Expr *e)
{
    if (!e->ref)
        e->ref = Script_RefSymbol(e->name);
    if (e->ref && e->ref->sym)
        return e->ref->sym;
    return NULL;
}

static ScriptValue *Expr_CallSymbol(Expr *e, ScriptSymbol *sym, ScriptValue *v)
{
    return ((ScriptHandler)sym->handler)(v, sym->data, e->args);
}

static void Expr_UnknownIdentifier(Expr *e)
{
    Script_Error("Unknown identifier '%s' in expression, line %d", e->left ? e->left->name : e->name, e->line);
}

/* resolves the target of an assignment, the original hangs on anything but a name */
static ScriptSymbol *Expr_AssignTarget(Expr *e)
{
    Expr *t = e->left;

    if (t->type != EXPR_SYMBOL) {
        if (t->type != EXPR_NAME) {
            Error_Show("Internal script error. Invalid assignment, line %d", e->line);
            for (;;)
                ;
        }
        if (!t->ref)
            e->left->ref = Script_RefSymbol(t->name);
    }
    t = e->left;
    if (t->ref && t->ref->sym)
        return t->ref->sym;
    return NULL;
}

char *Expr_EvalString(Expr *e, char *buf, int size)
{
    char tmp[0x800];
    ScriptValue v;
    ScriptSymbol *sym;
    const char *src;

    if (!e) {
        memcpy(buf, " ", 2);
        return NULL;
    }
    switch (e->op) {
    case 0:
        switch (e->type) {
        case EXPR_INT:
        case EXPR_REAL:
            Script_Error("Numeric member in string expression. Line %d.", e->line);
            memcpy(buf, " ", 2);
            return NULL;
        case EXPR_STRING:
            src = e->s;
            break;
        case EXPR_SYMBOL:
        case EXPR_NAME:
            sym = Expr_Symbol(e);
            if (!sym) {
                Expr_UnknownIdentifier(e);
                goto fail;
            }
            src = Expr_CallSymbol(e, sym, &v)->s;
            break;
        case EXPR_GROUP:
            src = Expr_EvalString(e->child, tmp, sizeof(tmp));
            break;
        default:
            memcpy(buf, " ", 2);
            return NULL;
        }
        crt_strncpy(buf, src, size - 1);
        buf[size - 1] = 0;
        return buf;
    case OP_ADD:
        crt_strncpy(buf, Expr_EvalString(e->left, tmp, sizeof(tmp)), size - 1);
        crt_strncat(buf, Expr_EvalString(e->right, tmp, sizeof(tmp)), size - 1);
        buf[size - 1] = 0;
        return buf;
    case OP_COMMA:
        Expr_EvalString(e->left, buf, size);
        return Expr_EvalString(e->right, buf, size);
    case OP_ASSIGN:
        if (!e->left)
            goto fail;
        sym = Expr_AssignTarget(e);
        if (!sym) {
            Script_Error("Unknown identifier '%s' in expression, line %d", e->left->name, e->line);
            goto fail;
        }
        if (sym->kind == SCRIPT_VARIABLE) {
            Expr_Assign(e->right, sym);
            return Expr_EvalString(e->right, buf, size);
        }
        if (sym->kind == SCRIPT_CONSTANT)
            Script_Error("Expression: Cannot assign to constant, line %d", e->line);
        else
            Script_Error("Syntax error in assignment, line %d", e->line);
        goto fail;
    default:
        Script_Error("Syntax error in string expression, line %d", e->line);
        goto fail;
    }
fail:
    memcpy(buf, " ", 2);
    return NULL;
}
HOOK(0x4B8CE0, Expr_EvalString);

void Expr_Assign(Expr *e, ScriptSymbol *sym)
{
    char tmp[0x800];

    switch (sym->type) {
    case SCRIPT_INT:
        if (sym->size == 1)
            *(char *)sym->data = Expr_EvalInt(e);
        else if (sym->size == 2)
            *(short *)sym->data = Expr_EvalInt(e);
        else
            *(int *)sym->data = Expr_EvalInt(e);
        break;
    case SCRIPT_REAL:
        *(double *)sym->data = Expr_EvalReal(e);
        break;
    case SCRIPT_STRING:
        crt_strncpy(sym->data, Expr_EvalString(e, tmp, sizeof(tmp)), sym->size - 1);
        ((char *)sym->data)[sym->size - 1] = 0;
        break;
    }
}
HOOK(0x4B9040, Expr_Assign);

int Expr_EvalInt(Expr *e)
{
    ScriptValue v;
    ScriptSymbol *sym;
    int r;

    if (!e)
        return 0;
    switch (e->op) {
    case 0:
        switch (e->type) {
        case EXPR_INT:
            return e->i;
        case EXPR_REAL:
            return Script_Round(e->d);
        case EXPR_SYMBOL:
        case EXPR_NAME:
            sym = Expr_Symbol(e);
            if (sym)
                return Expr_CallSymbol(e, sym, &v)->i;
            Expr_UnknownIdentifier(e);
            return 0;
        case EXPR_GROUP:
            return Expr_EvalInt(e->child);
        }
        return 0;
    case OP_ADD:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) + r;
    case OP_SUB:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) - r;
    case OP_PLUS:
        return Expr_EvalInt(e->right);
    case OP_NEG:
        return -Expr_EvalInt(e->right);
    case OP_MUL:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) * r;
    case OP_DIV:
        r = Expr_EvalInt(e->right);
        if (r == 0) {
            gScriptError = 1;
            return 0;
        }
        return Expr_EvalInt(e->left) / r;
    case OP_OR:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) | r;
    case OP_XOR:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) ^ r;
    case OP_AND:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) & r;
    case OP_SHL:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) << (r & 31);
    case OP_SHR:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) >> (r & 31);
    case OP_NOT:
        return ~Expr_EvalInt(e->right);
    case OP_EQ:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) == r;
    case OP_NE:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) != r;
    case OP_LT:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) < r;
    case OP_GT:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) > r;
    case OP_LE:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) <= r;
    case OP_GE:
        r = Expr_EvalInt(e->right);
        return Expr_EvalInt(e->left) >= r;
    case OP_LNOT:
        return Expr_EvalInt(e->right) == 0;
    case OP_COMMA:
        Expr_EvalInt(e->left);
        return Expr_EvalInt(e->right);
    case OP_ASSIGN:
        if (!e->left) {
            Error_Show("Internal script error. Invalid assignment, line %d", e->line);
            for (;;)
                ;
        }
        sym = Expr_AssignTarget(e);
        if (!sym) {
            Script_Error("Unknown identifier '%s' in expression, line %d", e->left->name, e->line);
            return 0;
        }
        if (sym->kind == SCRIPT_VARIABLE) {
            Expr_Assign(e->right, sym);
            return Expr_EvalInt(e->right);
        }
        if (sym->kind == SCRIPT_CONSTANT)
            Script_Error("Expression: Cannot assign to constant, line %d", e->line);
        else
            Script_Error("Syntax error in assignment, line %d", e->line);
        return 0;
    case OP_LOR:
        if (Expr_EvalInt(e->left) == 0 && Expr_EvalInt(e->right) == 0)
            return 0;
        return 1;
    case OP_LAND:
        if (Expr_EvalInt(e->left) == 0 || Expr_EvalInt(e->right) == 0)
            return 0;
        return 1;
    case OP_TERNARY:
        if (Expr_EvalInt(e->child))
            return Expr_EvalInt(e->left);
        return Expr_EvalInt(e->right);
    }
    return 0;
}
HOOK(0x4B9120, Expr_EvalInt);

/* Returns the full x87 value; operands kept across a call are rounded to double like the original spills. */
long double Expr_EvalReal(Expr *e)
{
    ScriptValue v;
    ScriptSymbol *sym;
    long double x;
    double m;

    if (!e)
        return 0.0;
    switch (e->op) {
    case 0:
        switch (e->type) {
        case EXPR_INT:
            return e->i;
        case EXPR_REAL:
            return e->d;
        case EXPR_SYMBOL:
        case EXPR_NAME:
            sym = Expr_Symbol(e);
            if (sym)
                return Expr_CallSymbol(e, sym, &v)->d;
            Expr_UnknownIdentifier(e);
            return 0.0;
        case EXPR_GROUP:
            return Expr_EvalReal(e->child);
        }
        return 0.0;
    case OP_ADD:
        m = Expr_EvalReal(e->right);
        return Expr_EvalReal(e->left) + m;
    case OP_SUB:
        m = Expr_EvalReal(e->left);
        return m - Expr_EvalReal(e->right);
    case OP_PLUS:
        return Expr_EvalReal(e->right);
    case OP_NEG:
        return -Expr_EvalReal(e->right);
    case OP_MUL:
        m = Expr_EvalReal(e->right);
        return Expr_EvalReal(e->left) * m;
    case OP_DIV:
        x = Expr_EvalReal(e->right);
        m = x;
        if (Fpu_IsZero(x)) {
            gScriptError = 1;
            return 0.0;
        }
        return Expr_EvalReal(e->left) / m;
    case OP_EQ:
        m = Expr_EvalReal(e->left);
        return (Fpu_Compare(Expr_EvalReal(e->right), m) & FPU_C3) != 0;
    case OP_NE:
        m = Expr_EvalReal(e->left);
        return (Fpu_Compare(Expr_EvalReal(e->right), m) & FPU_C3) == 0;
    case OP_LT:
        m = Expr_EvalReal(e->left);
        return (Fpu_Compare(Expr_EvalReal(e->right), m) & (FPU_C0 | FPU_C3)) == 0;
    case OP_GT:
        m = Expr_EvalReal(e->left);
        return (Fpu_Compare(Expr_EvalReal(e->right), m) & FPU_C0) != 0;
    case OP_LE:
        m = Expr_EvalReal(e->left);
        return (Fpu_Compare(Expr_EvalReal(e->right), m) & FPU_C0) == 0;
    case OP_GE:
        m = Expr_EvalReal(e->left);
        return (Fpu_Compare(Expr_EvalReal(e->right), m) & (FPU_C0 | FPU_C3)) != 0;
    case OP_LNOT:
        return Fpu_IsZero(Expr_EvalReal(e->right));
    case OP_COMMA:
        Expr_EvalInt(e->left);
        return Expr_EvalInt(e->right);
    case OP_ASSIGN:
        if (!e->left)
            return 0.0;
        sym = Expr_AssignTarget(e);
        if (!sym) {
            Script_Error("Unknown identifier '%s' in expression, line %d", e->left->name, e->line);
            return 0.0;
        }
        if (sym->kind == SCRIPT_VARIABLE) {
            Expr_Assign(e->right, sym);
            return Expr_EvalReal(e->right);
        }
        if (sym->kind != SCRIPT_CONSTANT) {
            Script_Error("Syntax error in assignment, line %d", e->line);
            return 0.0;
        }
        Script_Error("Expression: Cannot assign to constant, line %d", e->line);
        return 0.0;
    case OP_LOR:
        if (!Fpu_IsZero(Expr_EvalReal(e->left)))
            return 1;
        return !Fpu_IsZero(Expr_EvalReal(e->right));
    case OP_LAND:
        if (Fpu_IsZero(Expr_EvalReal(e->left)))
            return 0;
        return !Fpu_IsZero(Expr_EvalReal(e->right));
    case OP_TERNARY:
        if (Script_Round(Expr_EvalReal(e->child)) == 0)
            return Expr_EvalReal(e->right);
        return Expr_EvalReal(e->left);
    }
    return 0.0;
}
HOOK(0x4B9660, Expr_EvalReal);

void Expr_Free(Expr *e)
{
    int i;

    if (!e)
        return;
    Expr_Free(e->left);
    Expr_Free(e->right);
    if (e->type == EXPR_GROUP || e->type == EXPR_TERNARY)
        Expr_Free(e->child);
    if (e->type == EXPR_SYMBOL || e->type == EXPR_NAME) {
        if (e->ref)
            Script_UnrefSymbol(e->ref);
        if (e->args) {
            if (e->args->count && e->args->argv[0]) {
                for (i = 0; e->args->argv[i]; i++)
                    Expr_Free(e->args->argv[i]);
            }
            if (e->args->argv) {
                Mem_Free(e->args->argv);
                e->args->argv = NULL;
            }
            Mem_Free(e->args);
            e->args = NULL;
        }
    }
    Mem_Free(e);
}
HOOK(0x4B9BE0, Expr_Free);

int Expr_Parse(ScriptLexer *lex, Expr **out)
{
    if (!Lexer_Next(lex, &lex->pending))
        return 0;
    return Expr_ParseTokens(lex, out, 0, 0);
}
HOOK(0x4B9CA0, Expr_Parse);

/*
 * Operator precedence parser. operand is the last leaf not yet hung on an
 * operator, last the last operator node. The tree is linked through parent
 * pointers and the root is found by walking up at the end.
 */
int Expr_ParseTokens(ScriptLexer *lex, Expr **out, int depth, int inArgs)
{
    Expr *operand = NULL;
    Expr *last = NULL;
    Expr *node, *cur;
    int done = 0;
    int failed = 0;
    int kind, op, leftAssoc;

    for (;;) {
        if (done)
            goto finish;
        op = lex->op;
        kind = lex->kind;
        if (inArgs && op == OP_COMMA)
            kind = TOKEN_NONE;
        else if ((unsigned int)kind > TOKEN_INT)
            goto attach;
        switch (kind) {
        case TOKEN_NONE:
            done = 1;
            continue;
        case TOKEN_OPERATOR:
        case TOKEN_UNARY:
            if (op == OP_LPAREN) {
                if (operand) {
                    if (operand->type != EXPR_NAME)
                        goto syntax;
                    operand->args = Expr_ParseArgs(lex, NULL);
                    if (!operand->args)
                        goto fail;
                    break;
                }
                if (!Lexer_Next(lex, &lex->pending)) {
                    Lexer_Error(lex, "Unexpected end of file in line %d: '%s'");
                    goto fail;
                }
                if (!Expr_ParseTokens(lex, &operand, depth + 1, 0))
                    goto fail;
                if (lex->op != OP_RPAREN) {
                    Expr_Free(operand);
                    operand = NULL;
                    Lexer_Error(lex, "Expected ')' in line %d: '%s'");
                    goto fail;
                }
                operand = Expr_New(EXPR_GROUP, operand, lex->reader->lineNo, NULL);
                break;
            }
            if (!operand || last) {
                if (op == OP_ADD || op == OP_SUB) {
                    op = op == OP_ADD ? OP_PLUS : OP_NEG;
                } else if (kind == TOKEN_OPERATOR) {
                    Lexer_Error(lex, "Syntax error in line %d, '%s'. Binary operator requires lvalue.");
                    goto fail;
                }
            }
            node = Expr_New(EXPR_OPERATOR, &op, lex->reader->lineNo, NULL);
            if (operand)
                last = operand->parent;
            if (last) {
                leftAssoc = Expr_IsLeftAssoc(node->op) != 0;
                if (Expr_Precedence(node->op) + leftAssoc > Expr_Precedence(last->op)) {
                    do {
                        cur = last->parent;
                        if (!cur) {
                            node->left = last;
                            last->parent = node;
                            operand = NULL;
                            goto placed;
                        }
                        last = cur;
                    } while (Expr_Precedence(node->op) + leftAssoc > Expr_Precedence(cur->op));
                }
                if (last->right) {
                    if (Expr_IsUnary(last->op))
                        node->right = last->right;
                    else
                        node->left = last->right;
                    last->right->parent = node;
                }
                last->right = node;
                node->parent = last;
                operand = NULL;
            } else if (operand) {
                if (Expr_IsUnary(node->op))
                    node->right = operand;
                else
                    node->left = operand;
                operand->parent = node;
                operand = NULL;
            }
placed:
            last = node;
            break;
        case TOKEN_TERNARY:
            if (!operand) {
                Lexer_Error(lex, "Syntax error in line %d: '%s'. Expected operand.");
                goto fail;
            }
            if (op != OP_TERNARY) {
                if (!gExprInTernary) {
                    Lexer_Error(lex, "Syntax error in line %d: '%s'");
                    goto fail;
                }
                done = 1;
                continue;
            }
            gExprInTernary = 1;
            while (operand->parent && Expr_Precedence(OP_TERNARY) >= operand->parent->prec)
                operand = operand->parent;
            node = Expr_New(EXPR_TERNARY, operand, lex->reader->lineNo, NULL);
            cur = operand->parent;
            if (cur) {
                if (cur->left == operand)
                    cur->left = node;
                else if (cur->right == operand)
                    cur->right = node;
                else
                    cur->child = node;
            }
            node->parent = operand->parent;
            if (!Lexer_Next(lex, &lex->pending))
                goto eof;
            if (!Expr_ParseTokens(lex, &node->left, depth + 1, 0))
                goto nested;
            if (lex->op != OP_COLON) {
                Lexer_Error(lex, "Expected ':' in line %d: '%s'");
                goto nested;
            }
            if (!Lexer_Next(lex, &lex->pending))
                goto eof;
            if (!Expr_ParseTokens(lex, &node->right, depth + 1, 0))
                goto nested;
            operand = node;
            done = 1;
            gExprInTernary = 0;
            continue;
        case TOKEN_STRING:
            if (operand) {
                if (operand->type != EXPR_STRING)
                    goto syntax;
                operand = Expr_New(EXPR_STRING, lex->text, lex->reader->lineNo, operand);
            } else {
                operand = Expr_New(EXPR_STRING, lex->text, lex->reader->lineNo, NULL);
            }
            break;
        case TOKEN_NAME:
            if (operand)
                goto syntax;
            operand = Expr_New(EXPR_NAME, lex->text, lex->reader->lineNo, NULL);
            break;
        case TOKEN_REAL:
            if (operand)
                goto syntax;
            operand = Expr_New(EXPR_REAL, &lex->real, lex->reader->lineNo, NULL);
            break;
        case TOKEN_INT:
            if (operand)
                goto syntax;
            operand = Expr_New(EXPR_INT, &lex->value, lex->reader->lineNo, NULL);
            break;
        }
attach:
        if (operand && last) {
            last->right = operand;
            operand->parent = last;
            last = NULL;
        }
        if (!Lexer_Next(lex, &lex->pending))
            done = 1;
    }

syntax:
    Lexer_Error(lex, "Syntax error in line %d: '%s'.");
    goto fail;
eof:
    Lexer_Error(lex, "Unexpected end of file in line %d: '%s'");
nested:
    failed = 1;
    goto finish;
fail:
    failed = 1;
finish:
    if (last)
        operand = last;
    if (operand) {
        while (operand->parent)
            operand = operand->parent;
        if (!failed) {
            *out = operand;
            return 1;
        }
        Expr_Free(operand);
    }
    while (lex->op != OP_END && Lexer_Next(lex, &lex->pending))
        ;
    *out = NULL;
    gExprInTernary = 0;
    return 0;
}
HOOK(0x4B9CD0, Expr_ParseTokens);

int Expr_IsUnary(int op)
{
    switch (op) {
    case OP_PLUS:
    case OP_NEG:
    case OP_NOT:
    case OP_LNOT:
        return 1;
    }
    return 0;
}
HOOK(0x4BA340, Expr_IsUnary);

int Expr_IsLeftAssoc(int op)
{
    switch (op) {
    case OP_PLUS:
    case OP_NEG:
    case OP_NOT:
    case OP_LNOT:
    case OP_ASSIGN:
    case OP_TERNARY:
        return 0;
    }
    return 1;
}
HOOK(0x4BA380, Expr_IsLeftAssoc);

/* lower binds tighter */
int Expr_Precedence(int op)
{
    switch (op) {
    case OP_ADD:
    case OP_SUB:
        return 4;
    case OP_PLUS:
    case OP_NEG:
    case OP_NOT:
    case OP_LNOT:
        return 2;
    case OP_MUL:
    case OP_DIV:
        return 3;
    case OP_OR:
        return 10;
    case OP_XOR:
        return 9;
    case OP_AND:
        return 8;
    case OP_SHL:
    case OP_SHR:
        return 5;
    case OP_EQ:
    case OP_NE:
        return 7;
    case OP_LT:
    case OP_GT:
    case OP_LE:
    case OP_GE:
        return 6;
    case OP_COMMA:
        return 15;
    case OP_ASSIGN:
        return 14;
    case OP_1A:
        return 1;
    case OP_LOR:
        return 12;
    case OP_LAND:
        return 11;
    case OP_TERNARY:
        return 13;
    }
    Error_Show("Script: Internal Error: Unknown opcode: %02X.", op);
    for (;;)
        ;
}
HOOK(0x4BA3D0, Expr_Precedence);

static ScriptArgs *Args_New(int count, Expr **argv)
{
    ScriptArgs *args = Mem_Alloc(sizeof(ScriptArgs), 4);

    args->argv = argv;
    args->vm = NULL;
    args->count = count;
    return args;
}

static Expr **Args_Copy(Expr **tmp, int n)
{
    Expr **argv = Mem_Alloc(n * 4 + 4, 4);

    memset(argv, 0, n * 4 + 4);
    memcpy(argv, tmp, n * 4);
    return argv;
}

static void Args_FreeParsed(Expr **tmp, int n)
{
    for (; n > 0; n--)
        Expr_Free(*tmp++);
}

ScriptArgs *Expr_ParseArgs(ScriptLexer *lex, ScriptSymbol *sym)
{
    Expr **tmp = Mem_Alloc(gScriptMaxArgs * 4, 4);
    Expr **p;
    int n = 0;

    if (sym && sym->size != -1) {
        for (p = tmp; n < sym->size; n++, p++) {
            if (!Lexer_Next(lex, &lex->pending)) {
                Lexer_Error(lex, "Parse error (missing \" ?) in line %d: '%s'");
                goto fail;
            }
            if (!Expr_ParseTokens(lex, p, 0, 1))
                goto fail;
            if (lex->op != OP_COMMA && n < sym->size - 1) {
                Lexer_Error(lex, "Too few arguments to functions in line %d: '%s'");
                goto fail;
            }
        }
        if (sym->size == 0) {
            if (!Lexer_Next(lex, &lex->pending)) {
                Mem_Free(tmp);
                return NULL;
            }
            if (lex->op != OP_RPAREN) {
                Lexer_Error(lex, "Too many arguments to function in line %d: '%s'");
                Mem_Free(tmp);
                return NULL;
            }
            Mem_Free(tmp);
            return Args_New(0, NULL);
        }
        if (lex->op == OP_COMMA) {
            Lexer_Error(lex, "Too many arguments to function in line %d: '%s'");
            Mem_Free(tmp);
            return NULL;
        }
        if (lex->op != OP_RPAREN) {
            Lexer_Error(lex, "Syntax error in line %d: '%s'");
            Mem_Free(tmp);
            return NULL;
        }
        p = Args_Copy(tmp, sym->size);
        Mem_Free(tmp);
        return Args_New(sym->size, p);
fail:
        Args_FreeParsed(tmp, n);
        Mem_Free(tmp);
        return NULL;
    }

    for (p = tmp; n < gScriptMaxArgs; n++, p++) {
        *p = NULL;
        if (!Lexer_Next(lex, &lex->pending)) {
            Lexer_Error(lex, "Parse error (missing \" ?) in line %d: '%s'");
            Args_FreeParsed(tmp, n);
            Mem_Free(tmp);
            return NULL;
        }
        if (lex->op == OP_RPAREN)
            goto close;
        if (Expr_ParseTokens(lex, p, 0, 1) && lex->op != OP_COMMA) {
            n++;
            goto close;
        }
    }
    goto many;
close:
    if (n < gScriptMaxArgs) {
        if (lex->op != OP_RPAREN) {
            Lexer_Error(lex, "Syntax error in line %d: '%s'");
            Mem_Free(tmp);
            return NULL;
        }
        p = Args_Copy(tmp, n);
        Mem_Free(tmp);
        return Args_New(n, p);
    }
many:
    Lexer_Error(lex, "Too many arguments to function in line %d: '%s'");
    Mem_Free(tmp);
    return NULL;
}
HOOK(0x4BA4C0, Expr_ParseArgs);

Expr *Expr_New(int type, void *data, int line, Expr *prev)
{
    size_t extra = 0;
    Expr *e, *parent;

    if (prev)
        extra = strlen(prev->s) + strlen(data) + 1;
    else if (type == EXPR_STRING || type == EXPR_NAME)
        extra = strlen(data) + 1;
    e = Mem_Alloc(extra + sizeof(Expr), 4);
    memset(e, 0, sizeof(Expr));
    e->type = type;
    e->line = line + 1;
    switch (type) {
    case EXPR_INT:
        e->i = *(int *)data;
        return e;
    case EXPR_STRING:
        e->s = e->text;
        if (!prev) {
            strcpy(e->text, data);
            return e;
        }
        strcpy(e->text, prev->s);
        strcpy(e->s + strlen(e->s), data);
        parent = prev->parent;
        if (parent) {
            if (parent->left == prev)
                parent->left = e;
            else if (parent->right == prev)
                parent->right = e;
            else if (parent->type == EXPR_GROUP)
                parent->child = e;
            else {
                Error_Show("Internal error!!!");
                for (;;)
                    ;
            }
            e->parent = prev->parent;
        }
        Expr_Free(prev);
        return e;
    case EXPR_REAL:
        memcpy(&e->d, data, sizeof(double));
        return e;
    case EXPR_NAME:
        e->ref = NULL;
        e->name = e->text;
        e->args = NULL;
        strcpy(e->text, data);
        return e;
    case EXPR_TERNARY:
        e->op = OP_TERNARY;
        e->prec = Expr_Precedence(OP_TERNARY);
        /* fall through */
    case EXPR_SYMBOL:
    case EXPR_GROUP:
        e->child = data;
        return e;
    case EXPR_OPERATOR:
        e->op = *(char *)data;
        e->prec = Expr_Precedence(e->op);
        return e;
    }
    Error_Show("Script: Internal Error: Illegal expression type.");
    for (;;)
        ;
}
HOOK(0x4BA880, Expr_New);
