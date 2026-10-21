#include "hook.h"
#include "qasm/qasm.h"

/* Fixed size array of task pointers. Removing moves the last entry into
   the gap, so the order is not kept. */
typedef struct QTaskList {
    int count;
    int max;
    void *items[];
} QTaskList;

void QTaskList_Add(QTaskList *l, void *task)
{
    if (l->count >= l->max) {
        Error_Show("QTaskList is full");
        for (;;)
            ;
    }
    l->items[l->count] = task;
    l->count++;
}
HOOK(0x4C1790, QTaskList_Add);

void QTaskList_Remove(QTaskList *l, void *task)
{
    int i;

    for (i = 0; i < l->count; i++) {
        if (l->items[i] == task) {
            l->count--;
            l->items[i] = l->items[l->count];
            return;
        }
    }
    Error_Show("Trying to remove unrigistered QTask from QTaskList");
    for (;;)
        ;
}
HOOK(0x4C17C0, QTaskList_Remove);

QTaskList *QTaskList_New(int max)
{
    QTaskList *l = Mem_Alloc(max * 4 + 8, 4);

    l->max = max;
    l->count = 0;
    return l;
}
HOOK(0x4C1800, QTaskList_New);

void QTaskList_Free(QTaskList *l)
{
    Mem_Free(l);
}
HOOK(0x4C1830, QTaskList_Free);
