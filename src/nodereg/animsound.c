/* AnimSound: an animation trigger that plays a sound, either on the
   object or as a free sound at the trigger position. */
#include <string.h>

#include "hook.h"
#include "nodereg/nodereg.h"

/* the trigger data, 0x21 bytes */
typedef struct AnimSoundDef {
    char name[32];
    char attached;
} AnimSoundDef;

typedef struct AnimSoundParse {
    AnimSoundDef *def;
    void *args;
    int index;
} AnimSoundParse;

typedef struct AnimSoundFire {
    void *owner;
    AnimSoundDef *def;
    Vec3d *pos;
} AnimSoundFire;

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A80, void, Script_GetArgString, (void *args, int i, char *out, int size));
ORIG(0x4E6B00, void *, Sound_Play, (void *owner, AnimSoundDef *def));
ORIG(0x4E6C30, void, Sound_SetPos, (void *sound, Vec3d *pos));
ORIG(0x4EC040, uint8_t, AnimTrigger_GetFireEvent, (void));
ORIG(0x4EC050, uint8_t, AnimTrigger_GetSizeEvent, (void));
ORIG(0x4EC060, uint8_t, AnimTrigger_GetParseEvent, (void));
ORIG(0x4F0EE0, void *, Level_GetDynamic, (void));

#define TASK_DEBUG_LOG ((void *)0x4C1780)

#define gAnimSoundType VAR(uint16_t, 0xA83764)

void AnimSound_Parse(Task *t, AnimSoundParse *p);
void AnimSound_GetSize(Task *t, int *size);
void AnimSound_Fire(Task *t, AnimSoundFire *f);

void AnimSound_Open(void)
{
    uint16_t t = TaskType_New("AnimSound", 4, Task_GetRootType(), 0, 0);

    gAnimSoundType = t;
    TaskType_SetHandler(t, 0, TASK_DEBUG_LOG);
    TaskType_SetHandler2(gAnimSoundType, 2, TASK_DEBUG_LOG);
    TaskType_SetHandler(gAnimSoundType, AnimTrigger_GetParseEvent(), AnimSound_Parse);
    TaskType_SetHandler(gAnimSoundType, AnimTrigger_GetSizeEvent(), AnimSound_GetSize);
    TaskType_SetHandler(gAnimSoundType, AnimTrigger_GetFireEvent(), AnimSound_Fire);
    Script_RegisterConstant("TASKTYPE_ANIMSOUND", gAnimSoundType);
}
HOOK(0x51B400, AnimSound_Open);

void AnimSound_Parse(Task *t, AnimSoundParse *p)
{
    AnimSoundDef *d = p->def;
    int i = p->index;

    Script_GetArgString(p->args, i, d->name, sizeof(d->name));
    d->attached = Script_GetArgInt(p->args, i + 1);
}
HOOK(0x51B4B0, AnimSound_Parse);

void AnimSound_GetSize(Task *t, int *size)
{
    *size = sizeof(AnimSoundDef);
}
HOOK(0x51B4E0, AnimSound_GetSize);

void AnimSound_Fire(Task *t, AnimSoundFire *f)
{
    AnimSoundDef *d = f->def;
    void *s;

    if (d->attached) {
        Sound_Play(f->owner, d);
        return;
    }
    s = Sound_Play(Level_GetDynamic(), d);
    if (s)
        Sound_SetPos(s, f->pos);
}
HOOK(0x51B4F0, AnimSound_Fire);

void AnimSound_Close(void)
{
    TaskType_Free(gAnimSoundType);
}
HOOK(0x51B530, AnimSound_Close);
