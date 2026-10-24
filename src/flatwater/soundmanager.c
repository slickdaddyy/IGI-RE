#include <string.h>

#include "hook.h"
#include "flatwater/flatwater.h"

/* "SoundManager": plays one named sound for as long as it exists and passes
   sound queries on to its parent. */

ORIG(0x4E6610, uint8_t, Sound_GetInfoEvent, (void));
ORIG(0x4E6650, uint8_t, Sound_GetNameEvent, (void));
ORIG(0x4E6B00, Task *, Sound_Play, (void *owner, const char *name));
ORIG(0x4E6C00, void, Sound_Stop, (Task *sound));

typedef struct SoundManager {
    Task task;
    char name[32]; /* 0x20 */
    Task **sound;  /* 0x40 */
} SoundManager;

_Static_assert(sizeof(SoundManager) == 0x44, "SoundManager");

extern uint16_t gSoundManagerType;
extern void *gSoundManagerParams;

void SoundManager_Create(SoundManager *m)
{
    m->name[0] = 0;
    m->sound = NULL;
}
HOOK(0x51C090, SoundManager_Create);

void SoundManager_Delete(SoundManager *m)
{
    if (m->sound)
        TaskRef_Remove(m->sound);
}
HOOK(0x51C0A0, SoundManager_Delete);

void SoundManager_GetInfo(SoundManager *m, void *arg)
{
    Task *parent = m->task.parent;
    TaskFn2 fn = TASK_HANDLER(Sound_GetInfoEvent(), parent->type);

    if (fn)
        fn(parent, arg);
}
HOOK(0x51C0C0, SoundManager_GetInfo);

static void stop(SoundManager *m, Task **ref)
{
    Sound_Stop(*ref);
    TaskRef_Remove(ref);
    m->sound = NULL;
}

void SoundManager_ParamsChanged(SoundManager *m, char *arg)
{
    Task **ref = m->sound;
    Task *t;

    if (!m->name[0]) {
        if (ref && *ref)
            stop(m, ref);
        return;
    }
    if (!arg || !*arg)
        BROADCAST(m->task.head, Param_GetChangedEvent(), NULL);
    if (ref) {
        t = *ref;
        if (t) {
            const char *playing = NULL;
            TaskFn2 fn = TASK_HANDLER(Sound_GetNameEvent(), t->type);

            if (fn)
                fn(t, &playing);
            if (playing && strcmp(m->name, playing) == 0)
                return;
        }
        if (*ref)
            stop(m, ref);
    }
    t = Sound_Play(m, m->name);
    if (t)
        m->sound = TaskRef_Add(t);
}
HOOK(0x51C100, SoundManager_ParamsChanged);

void SoundManager_GetParams(void *task, void **out)
{
    *out = gSoundManagerParams;
}
HOOK(0x51C280, SoundManager_GetParams);

void SoundManager_Close(void)
{
    TaskType_Free(gSoundManagerType);
    QTaskList_Free(gSoundManagerParams);
}
HOOK(0x51C290, SoundManager_Close);
