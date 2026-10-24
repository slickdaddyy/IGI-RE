#ifndef TYPES_H
#define TYPES_H

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

/* 3x3 rotation, row major */
typedef struct Matrix {
    float m[9];
    int count; /* sum of the inputs' values plus one on each multiply */
} Matrix;
_Static_assert(sizeof(Matrix) == 0x28, "Matrix");

typedef struct Quat {
    float x, y, z, w;
    int unk10; /* always 0 */
} Quat;

typedef struct Random {
    unsigned int a, b, c;
} Random;

typedef struct Node {
    struct Node *next;
    struct Node *prev;
} Node;

/* head, tail and tailpred overlap two sentinel nodes, tail is always NULL */
typedef struct List {
    Node *head;
    Node *tail;
    Node *tailpred;
} List;

/* count fixed size slots plus a table of free slot indices after them */
typedef struct Pool {
    char *data;
    int *free;
    int used;
    int count;
    int stride;
    int size;
} Pool;
_Static_assert(sizeof(Pool) == 0x18, "Pool");

#endif
