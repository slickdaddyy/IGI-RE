#include <string.h>

#include "hook.h"
#include "param/param.h"

ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401B90, const char *, TaskType_GetName, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4EE960, unsigned, TaskRef_Hash, (Task *task));
ORIG(0x4F0C90, void, TaskNote_Free, (TaskNote *note));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (Task *task));

void Level_ClearTaskId(int id)
{
    TaskRef_Remove(gLevelTaskIds[id]);
    gLevelTaskIds[id] = NULL;
}
HOOK(0x4F10E0, Level_ClearTaskId);

/* frees the level ids of a task and all its descendants */
void Level_ClearTaskIdTree(Task *task)
{
    Task *c;
    int id = Task_GetId(task);

    if (id >= 0)
        Level_ClearTaskId(id);
    c = (Task *)task->children.head;
    if (!c->node.next || !c)
        return;
    do {
        Level_ClearTaskIdTree(c);
        c = (Task *)c->node.next;
    } while (c && c->node.next && c);
}
HOOK(0x4F1110, Level_ClearTaskIdTree);

int Level_IsRunning(void)
{
    if (!gLevelA758BE && gLevelPaused)
        return 0;
    return 1;
}
HOOK(0x4F1160, Level_IsRunning);

TaskNote *TaskNote_Find(Task *task);

const char *TaskNote_GetText(Task *task)
{
    TaskNote *n = TaskNote_Find(task);

    if (n)
        return n->text;
    return gScriptEmptyString;
}
HOOK(0x4F1180, TaskNote_GetText);

/* Notes whose task has been deleted are freed on the way. */
TaskNote *TaskNote_Find(Task *task)
{
    Node *n = gTaskNotes[TaskRef_Hash(task)].head;
    Node *next;

    for (next = n->next; next; n = next, next = next->next) {
        Task *t = *((TaskNote *)n)->ref;

        if (!t)
            TaskNote_Free((TaskNote *)n);
        else if (t == task)
            return (TaskNote *)n;
    }
    return NULL;
}
HOOK(0x4F11A0, TaskNote_Find);

char Level_GetEventDA4(void)
{
    return gLevelEventDA4;
}
HOOK(0x4F11F0, Level_GetEventDA4);

char Level_GetNameEvent(void)
{
    return gLevelNameEvent;
}
HOOK(0x4F1200, Level_GetNameEvent);

int Level_GetEventDA8(void)
{
    return gLevelEventDA8;
}
HOOK(0x4F1210, Level_GetEventDA8);

int Level_GetEventDAC(void)
{
    return gLevelEventDAC;
}
HOOK(0x4F1220, Level_GetEventDAC);

int Level_GetEventDB0(void)
{
    return gLevelEventDB0;
}
HOOK(0x4F1230, Level_GetEventDB0);

/* number of ids in use before the first free one */
int Level_CountTaskIds(void)
{
    int n = 0;

    while (Task_FindById(n))
        n++;
    return n;
}
HOOK(0x4F1240, Level_CountTaskIds);

/* the name event handler's text, else "<type>_<id>" */
void Task_GetName(char *out, Task *task)
{
    char buf[64];

    if (!HANDLER(gLevelNameEvent, task)) {
        int id = Task_GetId(task);

        crt_sprintf(out, "%s_%d", TaskType_GetName(task->type), id);
        return;
    }
    ((TaskArgFn)HANDLER(gLevelNameEvent, task))(task, buf);
    strcpy(out, buf);
}
HOOK(0x4F1270, Task_GetName);

Level *Level_Get(void)
{
    return gLevel;
}
HOOK(0x4F17C0, Level_Get);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DDA4) uint8_t gLevelEventDA4 INIT(255);
DATA(54DDA4) uint8_t gLevelNameEvent INIT(255);
PAD(igd, 54DDA4, 54DDA6, 0x2);
ALIAS(gLevelEvent0, gLevelEventDA4, 0);
