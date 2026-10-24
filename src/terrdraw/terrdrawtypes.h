#ifndef TERRDRAW_TERRDRAWTYPES_H
#define TERRDRAW_TERRDRAWTYPES_H

typedef struct IffNode {
    struct IffNode *next;
    struct IffNode *parent;
    struct IffNode *child;
    int owned;
    int id;
    int type;
    int size;
    void *data;
} IffNode;

#endif
