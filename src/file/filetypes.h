#ifndef FILE_FILETYPES_H
#define FILE_FILETYPES_H

/* An open file. Each device allocates its own, larger, record. */
typedef struct File {
    int device;
    int unk04[5];
    char name[128];
} File;

#endif
