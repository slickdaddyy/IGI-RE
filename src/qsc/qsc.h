#ifndef QSC_H
#define QSC_H

#include <stddef.h>

#include "igi.h"
#include "qsc/qsctypes.h"

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4AF8F0, void, List_AddTail, (List *list, Node *node));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4B7E10, void, Script_Error, (const char *fmt, ...));

/* Script source split into lines. Only line based refill exists. */
typedef struct Reader {
    char *cur;
    char *buf;
    int line;
    int textline; /* line the text in buf started on */
    int size;
    int pos;
    int (*refill)(struct Reader *r);
    const char *src;
} Reader;

enum {
    TOK_PUNCT,
    TOK_BINARY,
    TOK_UNARY,
    TOK_TERNARY,
    TOK_STRING,
    TOK_WORD,
    TOK_FLOAT,
    TOK_INT,
    TOK_NONE
};

typedef struct Token {
    List pending; /* tokens pushed back with Lexer_Unget */
    int type;
    int op;
    char *text;
    double num;
    int ival;
    unsigned int bufsize;
} Token;

typedef struct PendingToken {
    Node node;
    int type;
    int op;
    char *text;
    int unk14;
    double num;
    int ival;
    int unk24;
    char buf[];
} PendingToken;

typedef struct Lexer {
    Reader *reader;
    int unk04;
    Token tok;
} Lexer;

enum {
    KW_NONE,
    KW_OPEN,
    KW_CLOSE,
    KW_IF,
    KW_ELSE,
    KW_DO,
    KW_WHILE,
    KW_FOR
};

enum {
    BLOCK_LIST = 1,
    BLOCK_IF,
    BLOCK_ELSEIF,
    BLOCK_WHILE,
    BLOCK_DO,
    BLOCK_FOR,
    BLOCK_EXPR
};

Reader *Reader_Init(Reader *r, const char *src, int size, int (*refill)(Reader *r));
void Reader_Free(Reader *r);
int Reader_ReadLine(Reader *r);
Reader *Reader_InitMem(Reader *r, const char *src, int size);
int Reader_Refill(Reader *r);

int Lexer_Line(Lexer *lex);
void Lexer_Unget(List *pending, Token *tok);
int Lexer_Next(Lexer *lex, Token *tok);
int Lexer_IsNumber(Token *tok);
int Lexer_IsInteger(Token *tok);
void Lexer_Init(Lexer *lex, Reader *reader, char *buf, unsigned int bufsize);
int Lexer_TextLine(Lexer *lex);
char *Lexer_LineText(Lexer *lex);

int Parse_Block(Lexer *lex, Block **out);
int Parse_Keyword(Lexer *lex);
int Parse_Statement(Lexer *lex, Block **out);
int Parse_If(Lexer *lex, Block **out);
int Parse_While(Lexer *lex, Block **out);

void Block_Free(Block *b);
void Block_Exec(Block *b);

#endif
