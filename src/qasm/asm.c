#include "hook.h"
#include "qasm/qasm.h"

/* Operand encoders of the QVM assembler, slot 2 of the opcode table. Each
   reads its operands from the lexer, writes the instruction to the
   temporary code file and returns its size in *size. */

ORIG(0x4A5490, int, crt_atoi, (const char *s));
ORIG(0x516AD3, double, crt_atof, (const char *s));
ORIG(0x4BADC0, QvmFixup *, Asm_AddArgCountFixup, (QvmAsm *a, int offset));
ORIG(0x4BAFD0, QvmLabelRef *, Asm_AddLabelRef, (QvmAsm *a, const char *name));
ORIG(0x4BB030, int, Asm_AddString, (QvmAsm *a, const char *s));
ORIG(0x4BB180, int, Asm_AddName, (QvmAsm *a, const char *name));
ORIG(0x4BB240, int, Asm_NextToken, (QvmToken *tok, QvmLexer *lex));

unsigned int QvmAsm_ParseHex(const char *s)
{
    unsigned int v = 0;

    for (; *s; s++) {
        int c = *s;

        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'F')
            c -= 'A' - 10;
        else
            c -= 'a' - 10;
        v = v << 4 | c;
    }
    return v;
}
HOOK(0x4C08C0, QvmAsm_ParseHex);

void QvmAsm_OpPlain(int *size, QvmAsm *a, int op);
void QvmAsm_OpPush(int *size, QvmAsm *a);
void QvmAsm_OpBranch(int *size, QvmAsm *a, int op);
void QvmAsm_OpCall(int *size, QvmAsm *a);

void QvmAsm_RegisterOps(void)
{
    int op;

    for (op = 0; op < QVM_OPCODES; op++)
        Qvm_SetHandler(op, 2, QvmAsm_OpPlain);
    Qvm_SetHandler(OP_PUSH, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHB, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHW, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHF, 2, QvmAsm_OpPush);
    Qvm_SetHandler(7, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHSI, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHSIB, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHSIW, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHI, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHII, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHIIB, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_PUSHIIW, 2, QvmAsm_OpPush);
    Qvm_SetHandler(OP_BRA, 2, QvmAsm_OpBranch);
    Qvm_SetHandler(OP_BF, 2, QvmAsm_OpBranch);
    Qvm_SetHandler(OP_BT, 2, QvmAsm_OpBranch);
    Qvm_SetHandler(OP_CALL, 2, QvmAsm_OpCall);
}
HOOK(0x4C0900, QvmAsm_RegisterOps);

static void Emit(QvmAsm *a, int *size, int n, unsigned char op, const void *operand)
{
    *size = n;
    File_Write(a->file, &op, 1);
    if (n > 1)
        File_Write(a->file, operand, n - 1);
}

/* Picks the shortest encoding for the operand, whatever push was written. */
void QvmAsm_OpPush(int *size, QvmAsm *a)
{
    QvmToken tok;
    int v;

    if (!Asm_NextToken(&tok, a->lexer)) {
        a->failed = 1;
        return;
    }
    if (tok.type == TOKEN_INT) {
        v = crt_atoi(tok.text);
        if (v >= -128 && v <= 127) {
            if (v == 0)
                Emit(a, size, 1, OP_PUSH0, NULL);
            else if (v == 1)
                Emit(a, size, 1, OP_PUSH1, NULL);
            else if (v == -1)
                Emit(a, size, 1, OP_PUSHM, NULL);
            else
                Emit(a, size, 2, OP_PUSHB, &v);
        } else if (v >= -32768 && v <= 32767) {
            Emit(a, size, 3, OP_PUSHW, &v);
        } else {
            Emit(a, size, 5, OP_PUSH, &v);
        }
    } else if (tok.type == TOKEN_FLOAT) {
        double d = crt_atof(tok.text);
        volatile float f = (float)d;

        if (d == 0.0f)
            Emit(a, size, 1, OP_PUSH0, NULL);
        else if (f == 1.0f)
            Emit(a, size, 1, OP_PUSH1, NULL);
        else if (f == -1.0f)
            Emit(a, size, 1, OP_PUSHM, NULL);
        else
            Emit(a, size, 5, OP_PUSHF, (const void *)&f);
    } else if (tok.type == TOKEN_STRING || tok.type == TOKEN_WORD) {
        int str = tok.type == TOKEN_STRING;

        v = str ? Asm_AddString(a, tok.text) : Asm_AddName(a, tok.text);
        if (v < 0x100)
            Emit(a, size, 2, str ? OP_PUSHSIB : OP_PUSHIIB, &v);
        else if (v < 0x10000)
            Emit(a, size, 3, str ? OP_PUSHSIW : OP_PUSHIIW, &v);
        else
            Emit(a, size, 5, str ? OP_PUSHSI : OP_PUSHII, &v);
    } else {
        a->failed = 1;
    }
}
HOOK(0x4C0A10, QvmAsm_OpPush);

/* A label operand is patched later, @hex is an absolute address. */
void QvmAsm_OpBranch(int *size, QvmAsm *a, int op)
{
    QvmToken tok;
    unsigned char code = op;
    unsigned int v;

    if (!Asm_NextToken(&tok, a->lexer) || (tok.type != TOKEN_WORD && tok.type != TOKEN_ADDRESS)) {
        a->failed = 1;
        return;
    }
    v = 0;
    if (tok.type == TOKEN_WORD) {
        QvmLabelRef *r = Asm_AddLabelRef(a, tok.text);

        if (r) {
            r->offset = a->pc + 1;
            r->base = a->pc + 5;
        } else {
            a->failed = 1;
        }
    } else {
        v = QvmAsm_ParseHex(tok.text + 1) + (-5u - a->pc);
    }
    *size = 5;
    File_Write(a->file, &code, 1);
    File_Write(a->file, &v, 4);
}
HOOK(0x4C0E60, QvmAsm_OpBranch);

void QvmAsm_OpPlain(int *size, QvmAsm *a, int op)
{
    unsigned char code = op;

    *size = 1;
    File_Write(a->file, &code, 1);
}
HOOK(0x4C0F50, QvmAsm_OpPlain);

/* CALL (a, b, ...): a count, then one address per target. The count is
   written as a fixup. */
void QvmAsm_OpCall(int *size, QvmAsm *a)
{
    QvmToken tok;
    unsigned char code = OP_CALL;
    unsigned int v;
    int pc, n;

    if (!Asm_NextToken(&tok, a->lexer))
        goto fail;
    pc = a->pc;
    n = 0;
    v = 0;
    if (tok.type != TOKEN_OPEN)
        goto fail;
    File_Write(a->file, &code, 1);
    File_Write(a->file, &v, 4);
    if (!Asm_NextToken(&tok, a->lexer))
        goto fail;
    pc += 5;
    for (;;) {
        if (tok.type == TOKEN_WORD) {
            QvmLabelRef *r = Asm_AddLabelRef(a, tok.text);

            if (!r)
                goto fail;
            r->offset = pc;
            r->base = 0;
        } else if (tok.type == TOKEN_ADDRESS) {
            v = QvmAsm_ParseHex(tok.text + 1);
        } else {
            break;
        }
        /* a label writes whatever the last address was */
        File_Write(a->file, &v, 4);
        n++;
        pc += 4;
        if (!Asm_NextToken(&tok, a->lexer))
            goto fail;
        if (tok.type != TOKEN_COMMA)
            break;
        if (!Asm_NextToken(&tok, a->lexer))
            goto fail;
    }
    if (tok.type == TOKEN_CLOSE) {
        *size = n * 4 + 5;
        Asm_AddArgCountFixup(a, a->pc + 1)->value = n;
        return;
    }
fail:
    a->failed = 1;
}
HOOK(0x4C0F80, QvmAsm_OpCall);
