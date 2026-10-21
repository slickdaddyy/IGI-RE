#include <string.h>

#include "hook.h"
#include "qasm/qasm.h"

/* Operand printers of the QVM disassembler, slot 3 of the opcode table.
   Nothing in the game calls them. */

#define Qvm_OpNotImplemented ((void *)0x4BC950)

void QvmDis_OpPlain(QvmDis *d);
void QvmDis_OpPush(QvmDis *d, QvmImage *vm);
void QvmDis_OpPushF(QvmDis *d, QvmImage *vm);
void QvmDis_OpBranch(QvmDis *d, QvmImage *vm);
void QvmDis_OpPushB(QvmDis *d, QvmImage *vm);
void QvmDis_OpPushW(QvmDis *d, QvmImage *vm);
void QvmDis_OpPushIndexed(QvmDis *d, QvmImage *vm, int op);
void QvmDis_OpCall(QvmDis *d, QvmImage *vm);

void QvmDis_RegisterOps(void)
{
    int op;

    for (op = 0; op < QVM_OPCODES; op++)
        Qvm_SetHandler(op, 3, QvmDis_OpPlain);
    Qvm_SetHandler(OP_PUSH, 3, QvmDis_OpPush);
    Qvm_SetHandler(OP_PUSHB, 3, QvmDis_OpPushB);
    Qvm_SetHandler(OP_PUSHW, 3, QvmDis_OpPushW);
    Qvm_SetHandler(OP_PUSHF, 3, QvmDis_OpPushF);
    Qvm_SetHandler(OP_PUSHSI, 3, QvmDis_OpPushIndexed);
    Qvm_SetHandler(OP_PUSHSIB, 3, QvmDis_OpPushIndexed);
    Qvm_SetHandler(OP_PUSHSIW, 3, QvmDis_OpPushIndexed);
    Qvm_SetHandler(OP_PUSHI, 3, QvmDis_OpPushIndexed);
    Qvm_SetHandler(OP_PUSHII, 3, QvmDis_OpPushIndexed);
    Qvm_SetHandler(OP_PUSHIIB, 3, QvmDis_OpPushIndexed);
    Qvm_SetHandler(OP_PUSHIIW, 3, QvmDis_OpPushIndexed);
    Qvm_SetHandler(OP_BRA, 3, QvmDis_OpBranch);
    Qvm_SetHandler(OP_BF, 3, QvmDis_OpBranch);
    Qvm_SetHandler(OP_BT, 3, QvmDis_OpBranch);
    Qvm_SetHandler(OP_CALL, 3, QvmDis_OpCall);
    Qvm_SetHandler(7, 3, Qvm_OpNotImplemented);
}
HOOK(0x4C10E0, QvmDis_RegisterOps);

static const unsigned char *Operand(QvmDis *d, QvmImage *vm)
{
    return vm->code + d->pc + 1;
}

static int Read32(const unsigned char *p)
{
    int v;

    memcpy(&v, p, 4);
    return v;
}

static void WriteText(QvmDis *d, const char *s)
{
    File_Write(d->file, s, strlen(s));
}

void QvmDis_OpPlain(QvmDis *d)
{
    File_Write(d->file, "\r\n", 2);
    d->pc++;
}
HOOK(0x4C11F0, QvmDis_OpPlain);

void QvmDis_OpPush(QvmDis *d, QvmImage *vm)
{
    char buf[32];

    crt_sprintf(buf, "%d\r\n", Read32(Operand(d, vm)));
    WriteText(d, buf);
    d->pc += 5;
}
HOOK(0x4C1210, QvmDis_OpPush);

void QvmDis_OpPushF(QvmDis *d, QvmImage *vm)
{
    char buf[32];
    float f;

    memcpy(&f, Operand(d, vm), 4);
    crt_sprintf(buf, "%f\r\n", (double)f);
    WriteText(d, buf);
    d->pc += 5;
}
HOOK(0x4C1270, QvmDis_OpPushF);

void QvmDis_OpBranch(QvmDis *d, QvmImage *vm)
{
    char buf[32];

    crt_sprintf(buf, "@%08X\r\n", d->pc + 5 + Read32(Operand(d, vm)));
    WriteText(d, buf);
    d->pc += 5;
}
HOOK(0x4C12E0, QvmDis_OpBranch);

void QvmDis_OpPushB(QvmDis *d, QvmImage *vm)
{
    char buf[32];

    crt_sprintf(buf, "%d\r\n", (signed char)*Operand(d, vm));
    WriteText(d, buf);
    d->pc += 2;
}
HOOK(0x4C1340, QvmDis_OpPushB);

void QvmDis_OpPushW(QvmDis *d, QvmImage *vm)
{
    char buf[32];
    short v;

    memcpy(&v, Operand(d, vm), 2);
    crt_sprintf(buf, "%d\r\n", v);
    WriteText(d, buf);
    d->pc += 3;
}
HOOK(0x4C13A0, QvmDis_OpPushW);

const char *QvmDis_GetText(QvmTable *t, int index);
void QvmDis_WriteString(void *f, const char *s);

void QvmDis_OpPushIndexed(QvmDis *d, QvmImage *vm, int op)
{
    const unsigned char *p = Operand(d, vm);
    const char *s;
    unsigned int index;

    switch (op) {
    case OP_PUSHSI:
    case OP_PUSHII:
        index = Read32(p);
        d->pc += 5;
        break;
    case OP_PUSHSIB:
    case OP_PUSHIIB:
        index = *p;
        d->pc += 2;
        break;
    case OP_PUSHSIW:
    case OP_PUSHIIW:
        index = p[0] | p[1] << 8;
        d->pc += 3;
        break;
    default:
        Error_Show("Opcode not implemented for PUSHSTRING");
        for (;;)
            ;
    }
    if (op <= OP_PUSHSIW) {
        s = QvmDis_GetText(&vm->strings, index);
        if (s)
            QvmDis_WriteString(d->file, s);
    } else {
        s = QvmDis_GetText(&vm->symbols, index);
        if (s)
            WriteText(d, s);
    }
    File_Write(d->file, "\r\n", 2);
}
HOOK(0x4C1400, QvmDis_OpPushIndexed);

const char *QvmDis_GetText(QvmTable *t, int index)
{
    int off;

    if (index < t->size && (off = t->offsets[index]) < t->textSize)
        return t->text + off;
    return NULL;
}
HOOK(0x4C1530, QvmDis_GetText);

/* Tab, newline and carriage return all come out as \n. */
void QvmDis_WriteString(void *f, const char *s)
{
    File_Write(f, "\"", 1);
    for (; *s; s++) {
        switch (*s) {
        case '\t':
        case '\n':
        case '\r':
            File_Write(f, "\\n", 2);
            break;
        case '"':
            File_Write(f, "\\\"", 2);
            break;
        default:
            File_Write(f, s, 1);
            break;
        }
    }
    File_Write(f, "\"", 1);
}
HOOK(0x4C1560, QvmDis_WriteString);

/* Four targets per line, further lines indented by the current width. */
void QvmDis_OpCall(QvmDis *d, QvmImage *vm)
{
    char wrap[0x200];
    char buf[32];
    unsigned int wrapLen;
    int count, i, col;

    memset(wrap, ' ', sizeof(wrap));
    memcpy(wrap, "\r\n", 2);
    wrapLen = d->indent + 2;
    if (wrapLen > sizeof(wrap))
        wrapLen = sizeof(wrap);
    count = Read32(Operand(d, vm));
    File_Write(d->file, "(", 1);
    col = 0;
    for (i = 0; i < count; i++) {
        crt_sprintf(buf, "@%08X", Read32(vm->code + d->pc + 5 + i * 4));
        if (i < count - 1)
            strcat(buf, ", ");
        else
            col = 0;
        WriteText(d, buf);
        if (col >= 4) {
            col = -1;
            File_Write(d->file, wrap, wrapLen);
        }
        col++;
    }
    File_Write(d->file, ")\r\n", 3);
    d->pc += count * 4 + 5;
}
HOOK(0x4C1600, QvmDis_OpCall);
