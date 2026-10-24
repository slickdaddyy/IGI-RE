#ifndef QVM_QVM_H
#define QVM_QVM_H

#include "igi.h"
#include "types.h"
#include "script/scripttypes.h"
#include "qvm/qvmtypes.h"

typedef ScriptValue *(*ScriptHandler)(ScriptValue *out, void *data, ScriptArgs *args);

/* Stack slot types */
enum {
    QVM_INT = 1,
    QVM_REAL = 2,
    QVM_STRING = 3,
    QVM_NAME = 4,
    QVM_SYMBOL = 6,
    QVM_DATA = 7,
};

/* Error codes, see Qvm_Run for the texts */
enum {
    QVM_ERR_UNDERFLOW = 1,
    QVM_ERR_OVERFLOW = 2,
    QVM_ERR_INTERNAL = 3,
    QVM_ERR_ILLEGAL = 5,
    QVM_ERR_OPERAND = 6,
    QVM_ERR_UNKNOWN_NAME = 7,
    QVM_ERR_NOT_VARIABLE = 8,
    QVM_ERR_NOT_FUNCTION = 9,
    QVM_ERR_NO_FUNCTION = 10,
    QVM_ERR_SYNTAX = 11,
    QVM_ERR_CONDITION = 12,
    QVM_ERR_ARGS = 13,
    QVM_ERR_DATABLOCK = 14,
    QVM_ERR_RANGE = 15,
};

/* the ScriptArgs of a call from QVM code: the machine and the pc of each argument */
typedef struct QvmArgs {
    int count;
    int isQvm;
    void *unk08;
    Qvm *vm;
    int *argPc;
} QvmArgs;

/* 0x31 opcodes, four handlers each: run, name, assemble, disassemble */
#define QVM_OPCODES 0x31
#define QVM_SLOTS 4
typedef void (*QvmHandler)(void *a, void *b, int op, int slot);
extern QvmHandler gQvmHandlers[][6];

enum {
    OP_BRK, OP_NOP, OP_PUSH, OP_PUSHB, OP_PUSHW, OP_PUSHF, OP_PUSHA, OP_PUSHS,
    OP_PUSHSI, OP_PUSHSIB, OP_PUSHSIW, OP_PUSHI, OP_PUSHII, OP_PUSHIIB, OP_PUSHIIW,
    OP_PUSH0, OP_PUSH1, OP_PUSHM, OP_POP, OP_RET, OP_BRA, OP_BF, OP_BT, OP_JSR,
    OP_CALL, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_SHL, OP_SHR, OP_AND, OP_OR, OP_XOR,
    OP_LAND, OP_LOR, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_ASSIGN, OP_PLUS,
    OP_MINUS, OP_INV, OP_NOT, OP_BLK, OP_ILLEGAL,
};

void Qvm_SetHandler(int op, int slot, void *fn);
void Qvm_CallHandler(int op, int slot, void *a, void *b);

#endif
