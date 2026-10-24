#include <string.h>

#include "hook.h"
#include "igi.h"
#include "orig.h"
#include "qvm/qvm.h"

FN_EXTERN(Qvm_OpAssign);
FN_EXTERN(Qvm_OpBinary);
FN_EXTERN(Qvm_OpBlk);
FN_EXTERN(Qvm_OpBranch);
FN_EXTERN(Qvm_OpBrk);
FN_EXTERN(Qvm_OpCall);
FN_EXTERN(Qvm_OpIllegal);
FN_EXTERN(Qvm_OpName);
FN_EXTERN(Qvm_OpNop);
FN_EXTERN(Qvm_OpNotImplemented);
FN_EXTERN(Qvm_OpPop);
FN_EXTERN(Qvm_OpPush);
FN_EXTERN(Qvm_OpPush0);
FN_EXTERN(Qvm_OpPush1);
FN_EXTERN(Qvm_OpPushB);
FN_EXTERN(Qvm_OpPushF);
FN_EXTERN(Qvm_OpPushIndexed);
FN_EXTERN(Qvm_OpPushM);
FN_EXTERN(Qvm_OpPushS);
FN_EXTERN(Qvm_OpPushW);
FN_EXTERN(Qvm_OpUnary);

ORIG(0x4B0C60, void *, Mem_Alloc, (size_t size, size_t align));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5500, char *, crt_strncpy, (char *dst, const char *src, size_t n));
ORIG(0x4B5F00, void *, Resource_Load, (const char *name, unsigned int *size));
ORIG(0x4B6380, void, Resource_Unload, (const char *name));
ORIG(0x4B63D0, void, Resource_Flush, (void *data));
ORIG(0x4BAB80, const char *, Script_FindSymbol, (const char *name, ScriptSymbol **out));
ORIG(0x4BACA0, void, Script_UnrefSymbol, (void *ref));

extern int gQvmSavedTsp;
extern int gQvmSavedSp;

typedef struct QvmHeader {
    char magic[4];      /* "LOOP" */
    int version;        /* 8 */
    int unk08;          /* 5, older files are refused */
    int symOffsets;
    int symText;
    int symTableSize;
    int symTextSize;
    int strOffsets;
    int strText;
    int strTableSize;
    int strTextSize;
    int code;
    int codeSize;
    int stackSize;
    int typeStackSize;
} QvmHeader;

int Qvm_PopString(Qvm *vm, char **out, int *len, int type);
int Qvm_Pop(Qvm *vm, int *value, int *size, int *type);
int Qvm_PopSymbol(Qvm *vm, ScriptSymbol **out);
void Qvm_PushString(Qvm *vm, const char *s, int type);
int Qvm_PopNumber(Qvm *vm, int *i, float *f);
void Qvm_Execute(Qvm *vm);
void Qvm_SaveStack(Qvm *vm);
void Qvm_RestoreStack(Qvm *vm);
void Qvm_PushInt(Qvm *vm, int v);
void Qvm_PushFloat(Qvm *vm, float v);

/* fistp in the default rounding mode, like the original */
static inline int Qvm_Round(double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* Floats are moved through the FPU in the original, which quiets NaNs. */
static inline void Qvm_CopyFloat(void *dst, const void *src)
{
    long double t;

    __asm__("flds %1" : "=t"(t) : "m"(*(const float *)src));
    __asm__("fstps %0" : "=m"(*(float *)dst) : "t"(t) : "st");
}

/* fcomp sets C3 for equal and for unordered */
static inline int Qvm_IsZero(float f)
{
    return f == 0.0f || __builtin_isnan(f);
}

void Qvm_OpNotImplemented(void)
{
    Error_Show("Instruction not implemented.");
    for (;;)
        ;
}
HOOK(0x4BC950, Qvm_OpNotImplemented);

/* Formats each stack entry into a buffer nobody reads. */
void Qvm_DumpStack(Qvm *vm)
{
    char buf[0x1000];
    int stop = 0;
    int t = vm->tsp;
    int s = vm->sp;

    if (t >= vm->typeStackSize)
        return;
    do {
        switch (vm->types[t]) {
        case QVM_INT:
            crt_sprintf(buf, "%d", vm->stack[s]);
            s++;
            break;
        case QVM_REAL:
            crt_sprintf(buf, "%f", (double)*(float *)&vm->stack[s]);
            s++;
            break;
        case QVM_STRING:
            crt_sprintf(buf, "\"%s\"", (char *)&vm->stack[s + 1]);
            s += vm->stack[s] + 1;
            break;
        case QVM_NAME:
            crt_sprintf(buf, "%s", (char *)&vm->stack[s + 1]);
            s += vm->stack[s] + 1;
            break;
        case QVM_DATA: {
            int n = vm->stack[s];

            crt_sprintf(buf, "%d words of data.", n);
            s += n + 1;
            break;
        }
        default:
            crt_sprintf(buf, "UNKNOWN DATA TYPE");
            stop = 1;
            break;
        }
        t++;
    } while (!stop && t < vm->typeStackSize);
}
HOOK(0x4BC960, Qvm_DumpStack);

int Qvm_GetArg(void *out, int size, ScriptArgs *args, int index, int type);

int Qvm_GetIntArg(ScriptArgs *args, int index)
{
    int v = 0;

    Qvm_GetArg(&v, 4, args, index, 1);
    return v;
}
HOOK(0x4BCAB0, Qvm_GetIntArg);

/* Runs the code of one argument, which ends in BRK, and pops its value. */
int Qvm_GetArg(void *out, int size, ScriptArgs *args, int index, int type)
{
    Qvm *vm = args->vm;
    int sp = vm->sp;
    int pc = vm->pc;
    int tsp = vm->tsp;
    int ok = 0;
    int i;
    float f;
    char *s;
    int len;

    vm->pc = args->argPc[index];
    Qvm_Execute(args->vm);
    vm = args->vm;
    if (vm->tsp < tsp && vm->error == 0) {
        switch (type) {
        case 1:
            if (!Qvm_PopNumber(vm, &i, &f))
                goto done;
            *(int *)out = i;
            break;
        case 2:
            if (!Qvm_PopNumber(vm, &i, &f))
                goto done;
            Qvm_CopyFloat(out, &f);
            break;
        case 3:
            if (!Qvm_PopString(vm, &s, &len, QVM_STRING))
                goto done;
            crt_strncpy(out, s, size - 1);
            ((char *)out)[size - 1] = 0;
            break;
        default:
            Error_Show("Unknown data type.");
            for (;;)
                ;
        }
        ok = 1;
    }
done:
    args->vm->pc = pc;
    args->vm->tsp = tsp;
    args->vm->sp = sp;
    return ok;
}
HOOK(0x4BCAE0, Qvm_GetArg);

int Qvm_PopString(Qvm *vm, char **out, int *len, int type)
{
    ScriptSymbol *sym;
    int size;

    if (vm->types[vm->tsp] == type) {
        Qvm_Pop(vm, (int *)out, &size, NULL);
        *len = strlen(*out) + 1;
        return 1;
    }
    if (Qvm_PopSymbol(vm, &sym) && (sym->kind == 0 || sym->kind == 2)) {
        if (sym->type == QVM_STRING) {
            *out = sym->data;
            *len = strlen(sym->data) + 1;
        }
        return 1;
    }
    *out = NULL;
    *len = 0;
    return 0;
}
HOOK(0x4BCC00, Qvm_PopString);

/* value gets the word, or a pointer to the text of a string. */
int Qvm_Pop(Qvm *vm, int *value, int *size, int *type)
{
    int n;

    if (type)
        *type = vm->types[vm->tsp];
    switch (vm->types[vm->tsp]) {
    case QVM_INT:
    case QVM_REAL:
    case 5:
    case QVM_SYMBOL:
        if (value)
            *value = vm->stack[vm->sp];
        vm->tsp++;
        n = 4;
        vm->sp++;
        break;
    case QVM_STRING:
    case QVM_NAME:
    case QVM_DATA:
        if (value)
            *value = (int)(intptr_t)&vm->stack[vm->sp + 1];
        n = vm->stack[vm->sp] + 1;
        vm->tsp++;
        vm->sp += vm->stack[vm->sp] + 1;
        break;
    default:
        Error_Show("Unknown data type.");
        for (;;)
            ;
    }
    if (size)
        *size = n;
    return 1;
}
HOOK(0x4BCCC0, Qvm_Pop);

int Qvm_PopSymbol(Qvm *vm, ScriptSymbol **out)
{
    int t = vm->types[vm->tsp];
    char *name;
    int len;

    if (t == QVM_SYMBOL)
        return Qvm_Pop(vm, (int *)out, NULL, NULL);
    if (t == QVM_NAME) {
        Qvm_PopString(vm, &name, &len, QVM_NAME);
        if (Script_FindSymbol(name, out))
            return 1;
        vm->error = QVM_ERR_UNKNOWN_NAME;
        vm->errorPc = vm->pc;
        Qvm_PushString(vm, name, QVM_NAME);
        strcpy(vm->name, name);
    }
    return 0;
}
HOOK(0x4BCD90, Qvm_PopSymbol);

/* Text first, then its length in words on top. Padding is left as it was. */
void Qvm_PushString(Qvm *vm, const char *s, int type)
{
    int words = (strlen(s) + 4) >> 2;

    vm->tsp--;
    vm->types[vm->tsp] = type;
    vm->sp -= words;
    memmove(&vm->stack[vm->sp], s, strlen(s) + 1);
    vm->sp--;
    vm->stack[vm->sp] = words;
}
HOOK(0x4BCE40, Qvm_PushString);

/* Returns 1 for an int, 2 for a real, 0 if the top is not a number. */
int Qvm_PopNumber(Qvm *vm, int *i, float *f)
{
    int t = vm->types[vm->tsp];
    ScriptSymbol *sym;
    ScriptValue v, *r;

    if (t == QVM_INT) {
        Qvm_Pop(vm, i, NULL, NULL);
        *f = (float)*i;
        return 1;
    }
    if (t == QVM_REAL) {
        Qvm_Pop(vm, (int *)f, NULL, NULL);
        *i = Qvm_Round(*f);
        return 2;
    }
    if (Qvm_PopSymbol(vm, &sym) && (sym->kind == 0 || sym->kind == 2)) {
        r = ((ScriptHandler)sym->handler)(&v, sym->data, NULL);
        v = *r;
        if (sym->type == 1) {
            *i = v.i;
            *f = (float)v.i;
            return 1;
        }
        if (sym->type == 2) {
            *i = Qvm_Round(v.d);
            *f = (float)v.d;
            return 2;
        }
    }
    return 0;
}
HOOK(0x4BCEC0, Qvm_PopNumber);

void Qvm_Execute(Qvm *vm)
{
    vm->stop = 0;
    while (!vm->error) {
        int op = vm->code[vm->pc];

        gQvmHandlers[op][0](vm, 0, op, 0);
        if (vm->stop)
            break;
    }
}
HOOK(0x4BCFD0, Qvm_Execute);

float Qvm_GetRealArg(ScriptArgs *args, int index)
{
    float v = 0.0f;

    Qvm_GetArg(&v, 4, args, index, 2);
    return v;
}
HOOK(0x4BD010, Qvm_GetRealArg);

char *Qvm_GetStringArg(ScriptArgs *args, int index, char *buf, int size)
{
    *buf = 0;
    Qvm_GetArg(buf, size, args, index, 3);
    return buf;
}
HOOK(0x4BD040, Qvm_GetStringArg);

/* line numbers are not kept in this build */
int Qvm_GetArgLine(ScriptArgs *args, int index)
{
    (void)args;
    (void)index;
    return 0;
}
HOOK(0x4BD070, Qvm_GetArgLine);

/* Shared "return 0" stub, too short to hook */
int sub_4BD070(void)
{
    return 0;
}

int Qvm_Run(Qvm *vm)
{
    const char *msg;

    vm->pc = 0;
    vm->tsp = vm->typeStackSize;
    vm->sp = vm->stackSize;
    vm->stop = 0;
    vm->error = 0;
    vm->errorPc = 0;
    Qvm_Execute(vm);
    if (!vm->error)
        return 0;
    switch (vm->error) {
    case QVM_ERR_UNDERFLOW:
        msg = "Stack underflow";
        break;
    case QVM_ERR_OVERFLOW:
        msg = "Stack overflow";
        break;
    case QVM_ERR_INTERNAL:
        msg = "Internal error (read: BUG!!!).";
        break;
    case QVM_ERR_ILLEGAL:
        msg = "Illegal instruction";
        break;
    case QVM_ERR_OPERAND:
        msg = "Illegal operand(s) for instruction";
        break;
    case QVM_ERR_UNKNOWN_NAME:
        crt_sprintf(vm->message, "Unknown identifier: %s", vm->name);
        Qvm_DumpStack(vm);
        return vm->error;
    case QVM_ERR_NOT_VARIABLE:
        msg = "Identifier not a variable";
        break;
    case QVM_ERR_NOT_FUNCTION:
        msg = "Identifier not a function";
        break;
    case QVM_ERR_NO_FUNCTION:
        msg = "No function on stack";
        break;
    case QVM_ERR_SYNTAX:
        msg = "Syntax error";
        break;
    case QVM_ERR_ARGS:
        crt_sprintf(vm->message, "Function argument overflow: %d arguments.", *(int *)(vm->code + vm->errorPc + 1));
        Qvm_DumpStack(vm);
        return vm->error;
    case QVM_ERR_DATABLOCK:
        msg = "Datablock not present";
        break;
    case QVM_ERR_RANGE:
        msg = "Index out of range";
        break;
    default:
        msg = "Undefined error";
        break;
    }
    crt_sprintf(vm->message, msg);
    Qvm_DumpStack(vm);
    return vm->error;
}
HOOK(0x4BD080, Qvm_Run);

/* Sizes of 0 take the values from the file, then 0x800 bytes and 0x100 slots. */
Qvm *Qvm_Load(const char *name, int stackBytes, int typeCount)
{
    unsigned int size;
    char *file = Resource_Load(name, &size);
    QvmHeader *h = (QvmHeader *)file;
    unsigned int words, bytes;
    Qvm *vm;

    if (!file || size < sizeof(QvmHeader) || file[0] != 'L' || file[1] != 'O' || file[2] != 'O' || file[3] != 'P' || h->version != 8)
        return NULL;
    if (h->unk08 < 5) {
        Resource_Unload(name);
        return NULL;
    }
    if (!stackBytes)
        stackBytes = h->stackSize;
    if (!typeCount)
        typeCount = h->typeStackSize;
    if (!stackBytes)
        stackBytes = 0x800;
    if (!typeCount)
        typeCount = 0x100;
    words = (unsigned int)(stackBytes + 3) >> 2;
    bytes = (words + h->symTableSize * 2 + typeCount) * 4 + sizeof(Qvm);
    vm = Mem_Alloc(bytes, 4);
    memset(vm, 0, bytes);
    vm->code = (unsigned char *)file + h->code;
    vm->codeSize = h->codeSize;
    vm->stack = (int *)(vm + 1);
    vm->file = file;
    vm->types = vm->stack + words;
    vm->message[0] = 0;
    vm->name[0] = 0;
    vm->symVars = (void **)(vm->types + typeCount);
    vm->sp = words;
    vm->tsp = typeCount;
    vm->stackSize = words;
    vm->symRefs = vm->symVars + h->symTableSize;
    vm->typeStackSize = typeCount;
    if (h->symTableSize) {
        vm->symOffsets = (int *)(file + h->symOffsets);
        vm->symText = file + h->symText;
        vm->symTableSize = h->symTableSize;
        vm->symTextSize = h->symTextSize;
    }
    if (h->strTableSize) {
        vm->strOffsets = (int *)(file + h->strOffsets);
        vm->strText = file + h->strText;
        vm->strTableSize = h->strTableSize;
        vm->strTextSize = h->strTextSize;
    }
    return vm;
}
HOOK(0x4BD220, Qvm_Load);

void Qvm_Free(Qvm *vm)
{
    int i;

    for (i = 0; i < vm->symTableSize; i++)
        if (vm->symRefs[i])
            Script_UnrefSymbol(vm->symRefs[i]);
    Resource_Flush(vm->file);
    Mem_Free(vm);
}
HOOK(0x4BD380, Qvm_Free);

void Qvm_SetHandler(int op, int slot, void *fn)
{
    gQvmHandlers[op][slot] = (QvmHandler)fn;
}
HOOK(0x4BD3D0, Qvm_SetHandler);

void Qvm_CallHandler(int op, int slot, void *a, void *b)
{
    if (op >= 0 && op < QVM_OPCODES && slot >= 0 && slot < QVM_SLOTS && gQvmHandlers[op][slot])
        gQvmHandlers[op][slot](a, b, op, slot);
}
HOOK(0x4BD3F0, Qvm_CallHandler);

#define Qvm_OpError (FN(0x4BEC90, Qvm_OpIllegal))
#define Qvm_OpName (FN(0x4BECB0, Qvm_OpName))
#define Qvm_OpArith (FN(0x4BDC40, Qvm_OpBinary))
#define Qvm_OpUnary (FN(0x4BE6A0, Qvm_OpUnary))
#define Qvm_OpPushName (FN(0x4BEA30, Qvm_OpPushIndexed))
#define Qvm_OpPushString (FN(0x4BE9D0, Qvm_OpPushS))

/* Run and name handlers. Assemble and disassemble ones are set elsewhere. */
void Qvm_Init(void)
{
    int op, slot;

    for (slot = 0; slot < QVM_SLOTS; slot++)
        for (op = 0; op < QVM_OPCODES; op++)
            Qvm_SetHandler(op, slot, FN(0x4BC950, Qvm_OpNotImplemented));
    for (op = 0; op < QVM_OPCODES; op++) {
        Qvm_SetHandler(op, 0, Qvm_OpError);
        Qvm_SetHandler(op, 1, Qvm_OpName);
    }
    Qvm_SetHandler(OP_BRK, 0, FN(0x4BEC80, Qvm_OpBrk));
    Qvm_SetHandler(OP_NOP, 0, FN(0x4BEC70, Qvm_OpNop));
    Qvm_SetHandler(OP_PUSHB, 0, FN(0x4BE870, Qvm_OpPushB));
    Qvm_SetHandler(OP_PUSHW, 0, FN(0x4BE8B0, Qvm_OpPushW));
    Qvm_SetHandler(OP_PUSH, 0, FN(0x4BE8F0, Qvm_OpPush));
    Qvm_SetHandler(OP_PUSHF, 0, FN(0x4BE9A0, Qvm_OpPushF));
    Qvm_SetHandler(OP_PUSHA, 0, FN(0x4BE8F0, Qvm_OpPush));
    Qvm_SetHandler(OP_PUSHS, 0, Qvm_OpPushString);
    Qvm_SetHandler(OP_PUSHI, 0, Qvm_OpPushString);
    Qvm_SetHandler(OP_PUSH0, 0, FN(0x4BE830, Qvm_OpPush0));
    Qvm_SetHandler(OP_PUSH1, 0, FN(0x4BE810, Qvm_OpPush1));
    Qvm_SetHandler(OP_PUSHM, 0, FN(0x4BE850, Qvm_OpPushM));
    Qvm_SetHandler(OP_RET, 0, Qvm_OpError);
    Qvm_SetHandler(OP_JSR, 0, Qvm_OpError);
    Qvm_SetHandler(OP_CALL, 0, FN(0x4BD880, Qvm_OpCall));
    for (op = OP_ADD; op <= OP_GE; op++)
        Qvm_SetHandler(op, 0, Qvm_OpArith);
    for (op = OP_PLUS; op <= OP_NOT; op++)
        Qvm_SetHandler(op, 0, Qvm_OpUnary);
    Qvm_SetHandler(OP_ASSIGN, 0, FN(0x4BDA60, Qvm_OpAssign));
    Qvm_SetHandler(OP_POP, 0, FN(0x4BD860, Qvm_OpPop));
    Qvm_SetHandler(OP_BRA, 0, FN(0x4BD730, Qvm_OpBranch));
    Qvm_SetHandler(OP_BT, 0, FN(0x4BD730, Qvm_OpBranch));
    Qvm_SetHandler(OP_BF, 0, FN(0x4BD730, Qvm_OpBranch));
    Qvm_SetHandler(OP_BLK, 0, FN(0x4BE980, Qvm_OpBlk));
    Qvm_SetHandler(OP_PUSHII, 0, Qvm_OpPushName);
    Qvm_SetHandler(OP_PUSHIIB, 0, Qvm_OpPushName);
    Qvm_SetHandler(OP_PUSHIIW, 0, Qvm_OpPushName);
    Qvm_SetHandler(OP_PUSHSI, 0, Qvm_OpPushName);
    Qvm_SetHandler(OP_PUSHSIB, 0, Qvm_OpPushName);
    Qvm_SetHandler(OP_PUSHSIW, 0, Qvm_OpPushName);
    VAR(int, 0xA442F8) = 0;
    VAR(int, 0xA442EC) = 0;
    VAR(int, 0xA44300) = 0;
    VAR(int, 0xA442FC) = 0;
    VAR(int, 0xA44308) = 0;
    VAR(int, 0xA442F4) = 0;
}
HOOK(0x4BD430, Qvm_Init);

/* BRA, BF and BT, followed by a 4 byte displacement from the opcode. */
void Qvm_OpBranch(Qvm *vm)
{
    unsigned char *p = vm->code + vm->pc;
    int op = p[0];
    int disp = *(int *)(p + 1);
    int i, r;
    float f;

    switch (op) {
    case OP_BRA:
        break;
    case OP_BF:
        r = Qvm_PopNumber(vm, &i, &f);
        if (r == 1) {
            if (i) {
                vm->pc += 5;
                return;
            }
            break;
        }
        if (r == 2) {
            if (!Qvm_IsZero(f)) {
                vm->pc += 5;
                return;
            }
            break;
        }
        goto bad;
    case OP_BT:
        r = Qvm_PopNumber(vm, &i, &f);
        if (r == 1) {
            if (!i) {
                vm->pc += 5;
                return;
            }
            break;
        }
        if (r == 2) {
            if (Qvm_IsZero(f)) {
                vm->pc += 5;
                return;
            }
            break;
        }
        goto bad;
    default:
        vm->error = QVM_ERR_INTERNAL;
        vm->errorPc = vm->pc;
        vm->pc += 5;
        return;
    }
    vm->pc += disp + 5;
    return;
bad:
    vm->error = QVM_ERR_CONDITION;
    vm->errorPc = vm->pc;
    vm->pc += 5;
}
HOOK(0x4BD730, Qvm_OpBranch);

void Qvm_OpPop(Qvm *vm)
{
    Qvm_Pop(vm, NULL, NULL, NULL);
    vm->pc++;
}
HOOK(0x4BD860, Qvm_OpPop);

/* CALL n, then n words with the code address of each argument. */
void Qvm_OpCall(Qvm *vm)
{
    ScriptSymbol *sym;

    Qvm_SaveStack(vm);
    if (Qvm_PopSymbol(vm, &sym)) {
        if (sym->kind == 1) {
            int pc = vm->pc;
            unsigned char *p = vm->code + pc;
            int tsp = vm->tsp;
            int sp = vm->sp;
            int argc = *(int *)(p + 1);
            ScriptArgs args;
            ScriptValue v, *r;

            args.count = argc;
            args.isQvm = 1;
            args.unk08 = NULL;
            args.vm = vm;
            args.argPc = (int *)(p + 5);
            r = ((ScriptHandler)sym->handler)(&v, sym->data, &args);
            v = *r;
            if (vm->error)
                return;
            vm->stop = 0;
            vm->tsp = tsp;
            vm->pc = pc + argc * 4 + 5;
            vm->sp = sp;
            if (sym->type == 3) {
                Qvm_PushString(vm, v.s, 3);
                return;
            }
            if (sym->type == 2) {
                Qvm_PushFloat(vm, (float)v.d);
                return;
            }
            Qvm_PushInt(vm, v.i);
            return;
        }
        vm->error = QVM_ERR_NOT_FUNCTION;
        vm->errorPc = vm->pc;
    }
    if (!vm->error) {
        vm->error = QVM_ERR_NO_FUNCTION;
        vm->errorPc = vm->pc;
        Qvm_RestoreStack(vm);
    }
}
HOOK(0x4BD880, Qvm_OpCall);

void Qvm_SaveStack(Qvm *vm)
{
    gQvmSavedTsp = vm->tsp;
    gQvmSavedSp = vm->sp;
}
HOOK(0x4BD9C0, Qvm_SaveStack);

void Qvm_RestoreStack(Qvm *vm)
{
    vm->tsp = gQvmSavedTsp;
    vm->sp = gQvmSavedSp;
}
HOOK(0x4BD9E0, Qvm_RestoreStack);

void Qvm_PushInt(Qvm *vm, int v)
{
    vm->sp--;
    vm->tsp--;
    vm->types[vm->tsp] = QVM_INT;
    vm->stack[vm->sp] = v;
}
HOOK(0x4BDA00, Qvm_PushInt);

void Qvm_PushFloat(Qvm *vm, float v)
{
    vm->sp--;
    vm->tsp--;
    vm->types[vm->tsp] = QVM_REAL;
    Qvm_CopyFloat(&vm->stack[vm->sp], &v);
}
HOOK(0x4BDA30, Qvm_PushFloat);

/* value, variable, ASSIGN: stores the value and leaves it on the stack. */
void Qvm_OpAssign(Qvm *vm)
{
    ScriptSymbol *sym;
    int i, r, v;
    float f;
    char *s;
    int len;

    Qvm_SaveStack(vm);
    r = Qvm_PopNumber(vm, &i, &f);
    if (!r) {
        r = Qvm_PopString(vm, &s, &len, QVM_STRING) ? 3 : 0;
        if (!r)
            goto bad;
    }
    if (!Qvm_PopSymbol(vm, &sym))
        goto bad;
    if (sym->kind != 0) {
        vm->error = QVM_ERR_NOT_VARIABLE;
        vm->errorPc = vm->pc;
        Qvm_RestoreStack(vm);
        return;
    }
    if (sym->type == 1) {
        if (r == 1) {
            if (sym->size == 1) {
                *(char *)sym->data = (char)i;
                v = (signed char)i;
            } else if (sym->size == 2) {
                *(short *)sym->data = (short)i;
                v = (short)i;
            } else {
                *(int *)sym->data = i;
                v = i;
            }
        } else if (r == 2) {
            *(int *)sym->data = Qvm_Round(f);
            v = Qvm_Round(f);
        } else {
            goto bad;
        }
        Qvm_PushInt(vm, v);
        vm->pc++;
        return;
    }
    if (sym->type == 2) {
        if (r == 1) {
            *(double *)sym->data = (double)i;
            Qvm_PushFloat(vm, (float)i);
            vm->pc++;
            return;
        }
        if (r == 2) {
            *(double *)sym->data = f;
            Qvm_PushFloat(vm, f);
            vm->pc++;
            return;
        }
    } else if (sym->type == 3 && r == 3) {
        crt_strncpy(sym->data, s, sym->size - 1);
        ((char *)sym->data)[sym->size - 1] = 0;
        Qvm_PushString(vm, sym->data, 3);
        vm->pc++;
        return;
    }
bad:
    if (!vm->error) {
        vm->error = QVM_ERR_SYNTAX;
        vm->errorPc = vm->pc;
        Qvm_RestoreStack(vm);
    }
}
HOOK(0x4BDA60, Qvm_OpAssign);
