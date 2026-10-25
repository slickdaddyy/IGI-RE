#include "hook.h"
#include "humanai/humanai.h"
#include "humanai/humanaivars.h"

/* adds the AI to the entry for this target, or takes a free entry */
AITarget *AITarget_Add(HumanAI *ai, Task *target)
{
    AITarget *t = (AITarget *)gAITargets.head;
    AITarget *found = NULL;
    Node *next;
    int i;

    for (next = t->node.next; next; next = next->next) {
        if (t->used && target && *t->ref == target && Human_GetIndex(*t->ref) == Human_GetIndex(target)) {
            t->states[t->count] = -1;
            t->humans[t->count] = ai->task.parent;
            t->count++;
            t->pos = *DynCubeObj_GetPosition(target);
            return t;
        }
        t = (AITarget *)next;
    }
    for (i = 0; i < AI_TARGETS; i++) {
        t = &gAITargetPool[i];
        if (!t->used) {
            t->ref = TaskRef_Add(target);
            t->count = 1;
            t->used = 1;
            t->removed = 0;
            t->states[0] = -1;
            t->humans[0] = ai->task.parent;
            t->pos = *DynCubeObj_GetPosition(target);
            t->node.prev = NULL;
            t->node.next = NULL;
            List_AddTail(&gAITargets, t);
            return t;
        }
    }
    return found;
}
HOOK(0x451080, AITarget_Add);

void AITarget_Free(AITarget *t)
{
    TaskRef_Remove(t->ref);
    t->used = 0;
    List_Remove(t);
}
HOOK(0x4511D0, AITarget_Free);

void AITarget_SetState(AITarget *t, HumanAI *ai, int state)
{
    int i;

    for (i = 0; i < t->count; i++) {
        if (ai->task.parent == t->humans[i]) {
            t->states[i] = state;
            return;
        }
    }
}
HOOK(0x4511F0, AITarget_SetState);

int AITarget_GetState(AITarget *t, HumanAI *ai)
{
    int i;

    for (i = 0; i < t->count; i++)
        if (ai->task.parent == t->humans[i])
            return t->states[i];
    return -1;
}
HOOK(0x451230, AITarget_GetState);

int AITarget_CountState(AITarget *t, int state)
{
    int i, n = 0;

    for (i = 0; i < t->count; i++)
        if (t->states[i] == state)
            n++;
    return n;
}
HOOK(0x451270, AITarget_CountState);

/* takes the AI off the entry; the last one frees it */
void AITarget_Remove(AITarget *t, HumanAI *ai, char count)
{
    int i, n = t->count;

    if (n == 1) {
        t->count = 0;
        AITarget_Free(t);
        return;
    }
    for (i = 0; i < n; i++) {
        if (t->humans[i] == ai->task.parent) {
            t->count = n - 1;
            t->humans[i] = t->humans[n - 1];
            t->states[i] = t->states[t->count];
            if (count)
                t->removed++;
            return;
        }
    }
}
HOOK(0x4512A0, AITarget_Remove);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(58A9D8) List gAITargets;
