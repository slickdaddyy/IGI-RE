#include "hook.h"
#include "levelflow/levelflow.h"

ORIG(0x401780, void, Task_DeleteChildren, (void *task));
ORIG(0x4B1510, void *, File_Open, (const char *path, const char *mode));
ORIG(0x4B1690, void, File_Close, (void *f));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8930, void, Script_SetSymbolContext, (const char *name, void *value));

/* runs the script named by path under itself whenever its params change */
typedef struct SubLevel {
    Task task;
    char path[0x100];
} SubLevel;

_Static_assert(sizeof(SubLevel) == 0x120, "SubLevel");

extern uint16_t gSubLevelType;
extern ParamTable *gSubLevelParams;

void SubLevel_Create(SubLevel *s);
void SubLevel_Run(SubLevel *s);
void SubLevel_GetParams(void *task, ParamTable **out);
void SubLevel_Load(SubLevel *s);

void SubLevel_Init(void)
{
    gSubLevelType = TaskType_New("SubLevel", sizeof(SubLevel), Task_GetRootType(), 0);
    TaskType_SetHandler(gSubLevelType, 0, SubLevel_Create);
    TaskType_SetHandler2(gSubLevelType, 1, SubLevel_Run);
    TaskType_SetHandler(gSubLevelType, RigidDynCubeObj_GetEvent2(), HANDLER_4E0890);
    TaskType_SetHandler(gSubLevelType, Param_GetTableEvent(), SubLevel_GetParams);
    TaskType_SetHandler(gSubLevelType, Param_GetChangedEvent(), SubLevel_Load);
    gSubLevelParams = ParamTable_New(1);
    ParamTable_Add(gSubLevelParams, NULL, FIELD(SubLevel, path), Param_String256(), 7, NULL);
}
HOOK(0x488620, SubLevel_Init);

void SubLevel_Create(SubLevel *s)
{
    s->path[0] = 0;
}
HOOK(0x4886F0, SubLevel_Create);

void SubLevel_Run(SubLevel *s)
{
    EACH_CHILD(&s->task, 1);
}
HOOK(0x488700, SubLevel_Run);

void SubLevel_GetParams(void *task, ParamTable **out)
{
    *out = gSubLevelParams;
}
HOOK(0x488770, SubLevel_GetParams);

void SubLevel_Load(SubLevel *s)
{
    void *f, *script;

    Task_DeleteChildren(s);
    if (!s->path[0])
        return;
    f = File_Open(s->path, "r");
    if (!f)
        return;
    File_Close(f);
    script = Script_Load(s->path);
    if (!script)
        return;
    Script_SetSymbolContext("Task_New", s);
    Script_Run(script);
    Script_Free(script);
}
HOOK(0x488780, SubLevel_Load);

void SubLevel_Close(void)
{
    TaskType_Free(gSubLevelType);
    QTaskList_Free(gSubLevelParams);
}
HOOK(0x4887E0, SubLevel_Close);
