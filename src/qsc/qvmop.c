#include <string.h>

#include "hook.h"
#include "orig.h"
#include "qsc/qsc.h"

/* Strings or symbol names of a compiled program, found by index. */
typedef struct QvmStrings {
    int unk00;
    int unk04;
    int count;
    int size;
    int *offsets;
    char *data;
} QvmStrings;

typedef struct ScriptSymbol {
    int kind;
    int flags1;
    int flags2;
} ScriptSymbol;

/* Node from the script reference pool, see Script_RefSymbol. */
typedef struct ScriptRef {
    Node node;
    ScriptSymbol *symbol;
} ScriptRef;

typedef struct Qvm {
    unsigned char *code;
    int unk04;
    int unk08;
    int unk0C;
    int errpc;
    int pc;
    int sp;    /* value stack, grows down */
    int tsp;   /* type stack, grows down */
    int error;
    int *stack;
    int *types;
    int halt;
    QvmStrings strings;
    QvmStrings names;
    ScriptSymbol **symbols;
    ScriptRef **refs;
    char errtext[];
} Qvm;

enum {
    VAL_INT = 1,
    VAL_FLOAT,
    VAL_STRING,
    VAL_IDENT,
    VAL_ADDRESS,
    VAL_SYMBOL
};

enum {
    QVM_ERR_OPCODE = 3,
    QVM_ERR_ILLEGAL = 5,
    QVM_ERR_TYPE = 6,
    QVM_ERR_SYMBOL = 7,
    QVM_ERR_INDEX = 15
};

enum {
    OP_PUSH = 2,
    OP_PUSHB,
    OP_PUSHW,
    OP_PUSHA = 6,
    OP_PUSHS,
    OP_PUSHSI,
    OP_PUSHSIB,
    OP_PUSHSIW,
    OP_PUSHI,
    OP_PUSHII,
    OP_PUSHIIB,
    OP_PUSHIIW,
    OP_ADD = 0x19,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_SHL,
    OP_SHR,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_LAND,
    OP_LOR,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_PLUS = 0x2B,
    OP_MINUS,
    OP_INV,
    OP_NOT
};

ORIG(0x4BD9C0, void, Qvm_SaveStack, (Qvm *vm));
ORIG(0x4BD9E0, void, Qvm_RestoreStack, (Qvm *vm));
ORIG(0x4BDA00, void, Qvm_PushInt, (Qvm *vm, int v));
ORIG(0x4BDA30, void, Qvm_PushFloat, (Qvm *vm, float v));
ORIG(0x4BCEC0, int, Qvm_PopNumber, (Qvm *vm, int *i, float *f));
ORIG(0x4BCC00, int, Qvm_PopString, (Qvm *vm, char **s, unsigned int *size, int type));
ORIG(0x4BCE40, void, Qvm_PushString, (Qvm *vm, const char *s, int type));
ORIG(0x4BAB80, const char *, Script_FindSymbol, (const char *name, ScriptSymbol **out));
ORIG(0x4BABB0, ScriptRef *, Script_RefSymbol, (const char *name));
ORIG(0x4C10E0, void, sub_4C10E0, (void));

static int Read32(const unsigned char *p)
{
    int v;

    memcpy(&v, p, 4);
    return v;
}

static void Fail(Qvm *vm, int error)
{
    vm->error = error;
    vm->errpc = vm->pc;
    Qvm_RestoreStack(vm);
}

/* x87 compare flags: "zero" and "equal" also hold for NaN. */
static int NonZero(float f)
{
    return f < 0.0f || f > 0.0f;
}

int Qvm_PopNumbers(Qvm *vm, int *ib, int *ia, float *fb, float *fa);

void Qvm_OpBinary(Qvm *vm)
{
    char buf[0x1000];
    char *a, *b, *s, *heap;
    unsigned int alen, blen;
    int ia, ib;
    float fa, fb;
    int kind;

    Qvm_SaveStack(vm);
    kind = Qvm_PopNumbers(vm, &ib, &ia, &fb, &fa);
    if (kind == VAL_INT) {
        switch (vm->code[vm->pc]) {
        case OP_ADD: Qvm_PushInt(vm, ia + ib); break;
        case OP_SUB: Qvm_PushInt(vm, ia - ib); break;
        case OP_MUL: Qvm_PushInt(vm, ia * ib); break;
        case OP_DIV: Qvm_PushInt(vm, ia / ib); break;
        case OP_SHL: Qvm_PushInt(vm, (int)((unsigned int)ia << (ib & 31))); break;
        case OP_SHR: Qvm_PushInt(vm, ia >> (ib & 31)); break;
        case OP_AND: Qvm_PushInt(vm, ia & ib); break;
        case OP_OR: Qvm_PushInt(vm, ia | ib); break;
        case OP_XOR: Qvm_PushInt(vm, ia ^ ib); break;
        case OP_LAND: Qvm_PushInt(vm, ia && ib); break;
        case OP_LOR: Qvm_PushInt(vm, ia || ib); break;
        case OP_EQ: Qvm_PushInt(vm, ia == ib); break;
        case OP_NE: Qvm_PushInt(vm, ia != ib); break;
        case OP_LT: Qvm_PushInt(vm, ia < ib); break;
        case OP_LE: Qvm_PushInt(vm, ia <= ib); break;
        case OP_GT: Qvm_PushInt(vm, ia > ib); break;
        case OP_GE: Qvm_PushInt(vm, ia >= ib); break;
        default: Fail(vm, QVM_ERR_OPCODE); break;
        }
    } else if (kind) {
        switch (vm->code[vm->pc]) {
        case OP_ADD: Qvm_PushFloat(vm, fa + fb); break;
        case OP_SUB: Qvm_PushFloat(vm, fa - fb); break;
        case OP_MUL: Qvm_PushFloat(vm, fa * fb); break;
        case OP_DIV: Qvm_PushFloat(vm, fa / fb); break;
        case OP_SHL:
        case OP_SHR:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
            Fail(vm, QVM_ERR_TYPE);
            break;
        case OP_LAND: Qvm_PushInt(vm, NonZero(fa) && NonZero(fb)); break;
        case OP_LOR: Qvm_PushInt(vm, NonZero(fa) || NonZero(fb)); break;
        case OP_EQ: Qvm_PushInt(vm, !(fa < fb || fa > fb)); break;
        case OP_NE: Qvm_PushInt(vm, fa < fb || fa > fb); break;
        case OP_LT: Qvm_PushInt(vm, !(fa >= fb)); break;
        case OP_LE: Qvm_PushInt(vm, !(fa > fb)); break;
        case OP_GT: Qvm_PushInt(vm, fa > fb); break;
        case OP_GE: Qvm_PushInt(vm, fa >= fb); break;
        default: Fail(vm, QVM_ERR_OPCODE); break;
        }
    } else {
        heap = NULL;
        if (!Qvm_PopString(vm, &b, &blen, VAL_STRING) || !Qvm_PopString(vm, &a, &alen, VAL_STRING)) {
            if (!vm->error)
                Fail(vm, QVM_ERR_TYPE);
            vm->pc++;
            return;
        }
        switch (vm->code[vm->pc]) {
        case OP_ADD:
            if (alen + blen <= sizeof(buf))
                s = buf;
            else
                s = heap = Mem_Alloc(alen + blen, 4);
            strcpy(s, a);
            strcat(s, b);
            Qvm_PushString(vm, s, VAL_STRING);
            if (heap)
                Mem_Free(heap);
            break;
        case OP_EQ: Qvm_PushInt(vm, strcmp(a, b) == 0); break;
        case OP_NE: Qvm_PushInt(vm, strcmp(a, b) != 0); break;
        case OP_LT: Qvm_PushInt(vm, strcmp(a, b) < 0); break;
        case OP_LE: Qvm_PushInt(vm, strcmp(a, b) <= 0); break;
        case OP_GT: Qvm_PushInt(vm, strcmp(a, b) > 0); break;
        case OP_GE: Qvm_PushInt(vm, strcmp(a, b) >= 0); break;
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_SHL:
        case OP_SHR:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_LAND:
        case OP_LOR:
            Fail(vm, QVM_ERR_TYPE);
            break;
        default:
            /* the stack is not restored here */
            vm->errpc = vm->pc;
            vm->error = QVM_ERR_OPCODE;
            break;
        }
    }
    vm->pc++;
}
HOOK(0x4BDC40, Qvm_OpBinary);

/* Pops the right then the left operand. Returns 2 if either is a float. */
int Qvm_PopNumbers(Qvm *vm, int *ib, int *ia, float *fb, float *fa)
{
    int kb, ka;

    kb = Qvm_PopNumber(vm, ib, fb);
    if (!kb)
        return 0;
    ka = Qvm_PopNumber(vm, ia, fa);
    if (!ka) {
        if (kb == VAL_FLOAT)
            Qvm_PushFloat(vm, *fb);
        else
            Qvm_PushInt(vm, *ib);
        return 0;
    }
    return kb == VAL_FLOAT ? VAL_FLOAT : ka;
}
HOOK(0x4BE620, Qvm_PopNumbers);

void Qvm_OpUnary(Qvm *vm)
{
    int i;
    float f;
    int kind;

    Qvm_SaveStack(vm);
    kind = Qvm_PopNumber(vm, &i, &f);
    if (!kind) {
        if (!vm->error)
            Fail(vm, QVM_ERR_TYPE);
        return;
    }
    if (kind == VAL_INT) {
        switch (vm->code[vm->pc]) {
        case OP_PLUS: Qvm_PushInt(vm, i); break;
        case OP_MINUS: Qvm_PushInt(vm, -i); break;
        case OP_INV: Qvm_PushInt(vm, ~i); break;
        case OP_NOT: Qvm_PushInt(vm, i == 0); break;
        default: Fail(vm, QVM_ERR_OPCODE); break;
        }
    } else {
        switch (vm->code[vm->pc]) {
        case OP_PLUS: Qvm_PushFloat(vm, f); break;
        case OP_MINUS: Qvm_PushFloat(vm, -f); break;
        case OP_INV:
        case OP_NOT:
            Fail(vm, QVM_ERR_TYPE);
            break;
        default: Fail(vm, QVM_ERR_OPCODE); break;
        }
    }
    vm->pc++;
}
HOOK(0x4BE6A0, Qvm_OpUnary);

void Qvm_OpPush1(Qvm *vm)
{
    Qvm_PushInt(vm, 1);
    vm->pc++;
}
HOOK(0x4BE810, Qvm_OpPush1);

void Qvm_OpPush0(Qvm *vm)
{
    Qvm_PushInt(vm, 0);
    vm->pc++;
}
HOOK(0x4BE830, Qvm_OpPush0);

void Qvm_OpPushM(Qvm *vm)
{
    Qvm_PushInt(vm, -1);
    vm->pc++;
}
HOOK(0x4BE850, Qvm_OpPushM);

void Qvm_OpPushB(Qvm *vm)
{
    unsigned char *p = vm->code + vm->pc;

    if (p[0] != OP_PUSHB) {
        Error_Show("Internal error.");
        for (;;)
            ;
    }
    Qvm_PushInt(vm, (signed char)p[1]);
    vm->pc += 2;
}
HOOK(0x4BE870, Qvm_OpPushB);

void Qvm_OpPushW(Qvm *vm)
{
    unsigned char *p = vm->code + vm->pc;
    short v;

    if (p[0] != OP_PUSHW) {
        Error_Show("Internal error.");
        for (;;)
            ;
    }
    memcpy(&v, p + 1, 2);
    Qvm_PushInt(vm, v);
    vm->pc += 3;
}
HOOK(0x4BE8B0, Qvm_OpPushW);

void Qvm_PushAddress(Qvm *vm, int v);

/* PUSH and PUSHA */
void Qvm_OpPush(Qvm *vm)
{
    unsigned char *p = vm->code + vm->pc;
    int v = Read32(p + 1);

    if (p[0] == OP_PUSH) {
        Qvm_PushInt(vm, v);
    } else if (p[0] == OP_PUSHA) {
        Qvm_PushAddress(vm, v);
    } else {
        Error_Show("Internal error.");
        for (;;)
            ;
    }
    vm->pc += 5;
}
HOOK(0x4BE8F0, Qvm_OpPush);

void Qvm_PushAddress(Qvm *vm, int v)
{
    vm->sp--;
    vm->tsp--;
    vm->types[vm->tsp] = VAL_ADDRESS;
    vm->stack[vm->sp] = v;
}
HOOK(0x4BE950, Qvm_PushAddress);

/* BLK skips the data that follows it */
void Qvm_OpBlk(Qvm *vm)
{
    vm->pc += 5 + Read32(vm->code + vm->pc + 1);
}
HOOK(0x4BE980, Qvm_OpBlk);

void Qvm_OpPushF(Qvm *vm)
{
    float f;

    memcpy(&f, vm->code + vm->pc + 1, 4);
    Qvm_PushFloat(vm, f);
    vm->pc += 5;
}
HOOK(0x4BE9A0, Qvm_OpPushF);

/* PUSHS and PUSHI, the string follows the opcode */
void Qvm_OpPushS(Qvm *vm)
{
    unsigned char *p = vm->code + vm->pc;
    int type;

    if (p[0] == OP_PUSHS) {
        type = VAL_STRING;
    } else if (p[0] == OP_PUSHI) {
        type = VAL_IDENT;
    } else {
        Error_Show("Unhandled opcode.");
        for (;;)
            ;
    }
    Qvm_PushString(vm, (char *)p + 1, type);
    vm->pc += strlen((char *)vm->code + vm->pc + 1) + 2;
}
HOOK(0x4BE9D0, Qvm_OpPushS);

void Qvm_PushSymbol(Qvm *vm, ScriptSymbol *s);
char *Qvm_GetString(QvmStrings *t, int i);

/* PUSHSI* push a string and PUSHII* a symbol, both by index */
void Qvm_OpPushIndexed(Qvm *vm)
{
    unsigned char *p = vm->code + vm->pc;
    ScriptSymbol *sym;
    ScriptRef *ref;
    char *s;
    int idx, n;

    switch (p[0]) {
    case OP_PUSHSI:
        idx = Read32(p + 1);
        n = 4;
        break;
    case OP_PUSHSIB:
        idx = p[1];
        n = 1;
        break;
    case OP_PUSHSIW:
        idx = p[1] | p[2] << 8;
        n = 2;
        break;
    case OP_PUSHII:
        idx = Read32(p + 1);
        n = 4;
        goto symbol;
    case OP_PUSHIIB:
        idx = p[1];
        n = 1;
        goto symbol;
    case OP_PUSHIIW:
        idx = p[1] | p[2] << 8;
        n = 2;
        goto symbol;
    default:
        Error_Show("Unhandled opcode.");
        for (;;)
            ;
    }

    s = Qvm_GetString(&vm->strings, idx);
    if (!s)
        goto bad_index;
    Qvm_PushString(vm, s, VAL_STRING);
    vm->pc += n + 1;
    return;

symbol:
    sym = vm->symbols[idx];
    if (!sym) {
        ref = vm->refs[idx];
        if (ref) {
            sym = ref->symbol;
            /* with no name a NULL symbol is pushed */
            if (!sym && (s = Qvm_GetString(&vm->names, idx)))
                goto unknown;
        } else {
            s = Qvm_GetString(&vm->names, idx);
            if (!s)
                goto bad_index;
            if (!Script_FindSymbol(s, &sym))
                goto unknown;
            if (!sym->flags2)
                vm->symbols[idx] = sym;
            else
                vm->refs[idx] = Script_RefSymbol(s);
        }
    }
    Qvm_PushSymbol(vm, sym);
    vm->pc += n + 1;
    return;

unknown:
    vm->error = QVM_ERR_SYMBOL;
    vm->errpc = vm->pc;
    strcpy(vm->errtext, s);
    return;

bad_index:
    vm->error = QVM_ERR_INDEX;
    vm->errpc = vm->pc;
}
HOOK(0x4BEA30, Qvm_OpPushIndexed);

void Qvm_PushSymbol(Qvm *vm, ScriptSymbol *s)
{
    vm->sp--;
    vm->tsp--;
    vm->types[vm->tsp] = VAL_SYMBOL;
    vm->stack[vm->sp] = (int)(intptr_t)s;
}
HOOK(0x4BEC10, Qvm_PushSymbol);

char *Qvm_GetString(QvmStrings *t, int i)
{
    if (i < t->count && t->offsets[i] < t->size)
        return t->data + t->offsets[i];
    return NULL;
}
HOOK(0x4BEC40, Qvm_GetString);

void Qvm_OpNop(Qvm *vm)
{
    vm->pc++;
}
HOOK(0x4BEC70, Qvm_OpNop);

void Qvm_OpBrk(Qvm *vm)
{
    vm->halt = 1;
}
HOOK(0x4BEC80, Qvm_OpBrk);

/* Also used for RET and JSR. */
void Qvm_OpIllegal(Qvm *vm)
{
    vm->error = QVM_ERR_ILLEGAL;
    vm->errpc = vm->pc;
}
HOOK(0x4BEC90, Qvm_OpIllegal);

static const char *const opnames[] = {
    "BRK", "NOP", "PUSH", "PUSHB", "PUSHW", "PUSHF", "PUSHA", "PUSHS",
    "PUSHSI", "PUSHSIB", "PUSHSIW", "PUSHI", "PUSHII", "PUSHIIB", "PUSHIIW", "PUSH0",
    "PUSH1", "PUSHM", "POP", "RET", "BRA", "BF", "BT", "JSR",
    "CALL", "ADD", "SUB", "MUL", "DIV", "SHL", "SHR", "AND",
    "OR", "XOR", "LAND", "LOR", "EQ", "NE", "LT", "LE",
    "GT", "GE", "ASSIGN", "PLUS", "MINUS", "INV", "NOT", "BLK",
};

/* The disassembly handler of every opcode. */
void Qvm_OpName(char *out, Qvm *vm, unsigned int op)
{
    strcpy(out, op < sizeof(opnames) / sizeof(opnames[0]) ? opnames[op] : "ILLEGAL");
}
HOOK(0x4BECB0, Qvm_OpName);

void sub_4C0250(void)
{
    sub_4C10E0();
}
HOOK(0x4C0250, sub_4C0250);
