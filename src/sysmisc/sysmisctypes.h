#ifndef SYSMISC_SYSMISCTYPES_H
#define SYSMISC_SYSMISCTYPES_H

typedef struct DirEntry {
    char *name;
    unsigned long size;
    char isDir;
} DirEntry;

#endif
