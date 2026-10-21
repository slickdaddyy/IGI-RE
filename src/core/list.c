#include "hook.h"
#include "core/list.h"

void List_AddTail(List *list, Node *node)
{
    node->prev = list->tailpred;
    node->next = (Node *)&list->tail;
    list->tailpred->next = node;
    list->tailpred = node;
}
HOOK(0x4AF8F0, List_AddTail);

void List_AddHead(List *list, Node *node)
{
    node->prev = (Node *)list;
    node->next = list->head;
    list->head->prev = node;
    list->head = node;
}
HOOK(0x4AF910, List_AddHead);

Node *List_RemHead(List *list)
{
    Node *n = list->head;

    if (n == (Node *)&list->tail)
        return NULL;
    list->head = n->next;
    n->next->prev = (Node *)list;
    n->prev = NULL;
    n->next = NULL;
    return n;
}
HOOK(0x4AF930, List_RemHead);

void List_Remove(Node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = NULL;
    node->next = NULL;
}
HOOK(0x4AF960, List_Remove);

void List_InsertBefore(Node *pos, Node *node)
{
    pos->prev->next = node;
    node->prev = pos->prev;
    pos->prev = node;
    node->next = pos;
}
HOOK(0x4AF990, List_InsertBefore);

int List_Count(List *list)
{
    int count = 0;
    Node *n;

    for (n = list->head->next; n; n = n->next)
        count++;
    return count;
}
HOOK(0x4AF9B0, List_Count);
