#ifndef SCRIPT_SCRIPT_H
#define SCRIPT_SCRIPT_H

#include "script/symbol.h"

/* A loaded script, either source text or a compiled QVM. */
typedef struct Script {
    char name[128];
    void *source;
    int size;
    int unk88;
    void *qvm;
    char isQvm;
} Script;

/* Reads lines of source text into a buffer. */
typedef struct ScriptReader {
    char *pos;
    char *line;
    int unk8;
    int lineNo;
    int size;
    int unk14;
    void *readFn;
    void *source;
} ScriptReader;

enum {
    TOKEN_NONE,
    TOKEN_OPERATOR,
    TOKEN_UNARY,
    TOKEN_TERNARY,
    TOKEN_STRING,
    TOKEN_NAME,
    TOKEN_REAL,
    TOKEN_INT,
};

typedef struct ScriptLexer {
    ScriptReader *reader;
    int unk4;
    List pending;
    int kind;
    int op;
    char *text;
    double real;
    int value;
    int textSize;
} ScriptLexer;

enum {
    EXPR_INT = 1,
    EXPR_STRING,
    EXPR_REAL,
    EXPR_SYMBOL,
    EXPR_NAME,
    EXPR_GROUP,
    EXPR_TERNARY,
    EXPR_OPERATOR,
};

enum {
    OP_ADD = 1,
    OP_SUB,
    OP_PLUS,
    OP_NEG,
    OP_MUL,
    OP_DIV,
    OP_7,
    OP_OR,
    OP_XOR,
    OP_AND,
    OP_SHL,
    OP_SHR,
    OP_NOT,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_LNOT,
    OP_LPAREN,
    OP_RPAREN,
    OP_END,
    OP_COMMA,
    OP_ASSIGN,
    OP_1A,
    OP_LOR,
    OP_LAND,
    OP_TERNARY,
    OP_COLON,
};

extern int gScriptWarningLevel;
extern int gScriptBreakEnabled;
extern int gScriptBreak;
extern int gScriptError;
extern int gScriptMaxArgs;

Script *Script_Load(const char *path);
void Script_Free(Script *s);
void Script_Compile(const char *path);
void Script_Run(Script *s);
Expr *Script_ParseExpr(const char *text);
int Script_EvalExpr(const char *text);
void Script_SetWarningLevel(int level);
void Script_PushWarningLevel(int level);
void Script_PopWarningLevel(void);
void Script_SetMaxArgs(int count);
void Script_SetLineSize(int size);
void Script_SetBreakEnabled(int on);
int Script_GetArgInt(ScriptArgs *args, int i);
long double Script_GetArgReal(ScriptArgs *args, int i);
char *Script_GetArgString(ScriptArgs *args, int i, char *buf, int size);
int Script_GetArgLine(ScriptArgs *args, int i);
int Script_HasArg(ScriptArgs *args, int i);
void Script_Init(void);
ScriptValue *BreakScript(ScriptValue *out);
ScriptValue *Script_SetWarningLevelCmd(ScriptValue *out, void *data, ScriptArgs *args);
void Script_Shutdown(void);

char *Expr_EvalString(Expr *e, char *buf, int size);
void Expr_Assign(Expr *e, ScriptSymbol *sym);
int Expr_EvalInt(Expr *e);
long double Expr_EvalReal(Expr *e);
void Expr_Free(Expr *e);
int Expr_Parse(ScriptLexer *lex, Expr **out);
int Expr_ParseTokens(ScriptLexer *lex, Expr **out, int depth, int inArgs);
int Expr_IsUnary(int op);
int Expr_IsLeftAssoc(int op);
int Expr_Precedence(int op);
ScriptArgs *Expr_ParseArgs(ScriptLexer *lex, ScriptSymbol *sym);
Expr *Expr_New(int type, void *data, int line, Expr *prev);

#endif
