#ifndef LIST_H
#define LIST_H

#include <stddef.h>

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

void List_AddTail(List *list, Node *node);
void List_AddHead(List *list, Node *node);
void List_Remove(Node *node);

#endif
