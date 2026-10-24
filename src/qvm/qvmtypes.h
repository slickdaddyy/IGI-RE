#ifndef QVM_QVMTYPES_H
#define QVM_QVMTYPES_H

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

/* Strings or symbol names of a compiled program, found by index. */
typedef struct QvmStrings {
    int unk00;
    int unk04;
    int count;
    int size;
    int *offsets;
    char *data;
} QvmStrings;

/* A loaded program. Both stacks grow down; the data stack is words. */
typedef struct Qvm {
    unsigned char *code;    /* 0x00 */
    int codeSize;           /* 0x04 */
    int stackSize;          /* 0x08 */
    int typeStackSize;      /* 0x0C */
    int errorPc;            /* 0x10 */
    int pc;                 /* 0x14 */
    int sp;                 /* 0x18 */
    int tsp;                /* 0x1C */
    int error;              /* 0x20 */
    int *stack;             /* 0x24 */
    int *types;             /* 0x28 */
    int stop;               /* 0x2C */
    QvmStrings strings;     /* 0x30 */
    QvmStrings names;       /* 0x48 */
    struct ScriptSymbol **symVars; /* 0x60, per name */
    struct ScriptRef **symRefs; /* 0x64, refs for names with dynamic set */
    char name[0x200];       /* 0x68 */
    char message[0x200];    /* 0x268 */
    char *file;             /* 0x468 */
} Qvm;
_Static_assert(sizeof(Qvm) == 0x46C, "Qvm size");

#endif
