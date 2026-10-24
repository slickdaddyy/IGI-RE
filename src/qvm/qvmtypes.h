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

#endif
