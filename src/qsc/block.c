#include "hook.h"
#include "orig.h"
#include "qsc/qsc.h"

ORIG(0x4B9BE0, void, Expr_Free, (void *expr));
ORIG(0x4B9120, int, Expr_Eval, (void *expr));

void Block_Free(Block *b)
{
    Node *n, *next;

    if (!b)
        return;
    switch (b->type) {
    case BLOCK_LIST:
        for (n = b->stmts.head; (next = n->next); n = next) {
            List_Remove(n);
            Block_Free((Block *)n);
        }
        break;
    case BLOCK_IF:
        Expr_Free(b->cond.cond);
        Block_Free(b->cond.then);
        for (n = b->cond.elifs.head; (next = n->next); n = next) {
            Expr_Free(((Block *)n)->loop.cond);
            Block_Free(((Block *)n)->loop.body);
            List_Remove(n);
            Mem_Free(n);
        }
        if (b->cond.otherwise)
            Block_Free(b->cond.otherwise);
        break;
    case BLOCK_WHILE:
    case BLOCK_DO:
        Expr_Free(b->loop.cond);
        Block_Free(b->loop.body);
        break;
    case BLOCK_FOR:
        if (b->loopfor.init)
            Expr_Free(b->loopfor.init);
        if (b->loopfor.cond)
            Expr_Free(b->loopfor.cond);
        Block_Free(b->loopfor.body);
        if (b->loopfor.step)
            Expr_Free(b->loopfor.step);
        break;
    case BLOCK_EXPR:
        if (b->expr)
            Expr_Free(b->expr);
        break;
    default:
        Error_Show("Unknown block type.");
        for (;;)
            ;
    }
    Mem_Free(b);
}
HOOK(0x4BF8F0, Block_Free);

void Block_Exec(Block *b)
{
    Node *n, *next;

    switch (b->type) {
    case BLOCK_LIST:
        for (n = b->stmts.head; (next = n->next); n = next)
            Block_Exec((Block *)n);
        break;
    case BLOCK_IF:
        if (Expr_Eval(b->cond.cond)) {
            Block_Exec(b->cond.then);
            break;
        }
        for (n = b->cond.elifs.head; (next = n->next); n = next) {
            if (Expr_Eval(((Block *)n)->loop.cond)) {
                Block_Exec(((Block *)n)->loop.body);
                return;
            }
        }
        if (b->cond.otherwise)
            Block_Exec(b->cond.otherwise);
        break;
    case BLOCK_WHILE:
        while (Expr_Eval(b->loop.cond))
            Block_Exec(b->loop.body);
        break;
    case BLOCK_DO:
        do
            Block_Exec(b->loop.body);
        while (Expr_Eval(b->loop.cond));
        break;
    case BLOCK_FOR:
        if (b->loopfor.init)
            Expr_Eval(b->loopfor.init);
        while (!b->loopfor.cond || Expr_Eval(b->loopfor.cond)) {
            Block_Exec(b->loopfor.body);
            if (b->loopfor.step)
                Expr_Eval(b->loopfor.step);
        }
        break;
    case BLOCK_EXPR:
        if (b->expr)
            Expr_Eval(b->expr);
        break;
    default:
        Error_Show("Unknown block type.");
        for (;;)
            ;
    }
}
HOOK(0x4BFA80, Block_Exec);
