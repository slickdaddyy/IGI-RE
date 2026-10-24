#ifndef SYSMISC_SYSMISCTYPES_H
#define SYSMISC_SYSMISCTYPES_H

typedef struct DirEntry {
    char *name;
    unsigned long size;
    char isDir;
} DirEntry;

/* one step of a route: the next node and the distance */
typedef struct GraphStep {
    int next;           /* -1: no route */
    float dist;
} GraphStep;

#endif
