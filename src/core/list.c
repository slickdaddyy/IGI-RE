#include "core/list.h"

void List_AddTail(List *list, Node *node)
{
    node->prev = list->tailpred;
    node->next = (Node *)&list->tail;
    list->tailpred->next = node;
    list->tailpred = node;
}

void List_AddHead(List *list, Node *node)
{
    node->prev = (Node *)list;
    node->next = list->head;
    list->head->prev = node;
    list->head = node;
}

void List_Remove(Node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = NULL;
    node->next = NULL;
}
