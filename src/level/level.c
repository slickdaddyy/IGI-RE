#include <string.h>

#include "igi.h"
#include "hook.h"
#include "level/level.h"
#include "task/taskvars.h"

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (Task *parent, uint16_t type, void *arg));
ORIG(0x4013B0, Task *, Task_NewHead, (Task *parent, uint16_t type, void *arg));
ORIG(0x401780, void, Task_DeleteChildren, (Task *task));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018E0, void, TaskEvent_Free2, (int index));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401A70, uint16_t, TaskType_Find, (const char *name));
ORIG(0x401AE0, Task **, TaskRef_Add, (Task *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401D80, void, Task_Broadcast, (Task *task, uint8_t slot, void *arg));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B80B0, void *, Script_Load, (const char *path));
ORIG(0x4B83D0, void, Script_Free, (void *script));
ORIG(0x4B85B0, void, Script_Run, (void *script));
ORIG(0x4B8930, void, Script_SetSymbolContext, (const char *name, void *context));
ORIG(0x4B8A20, int, Script_GetArgInt, (ScriptArgs *args, int i));
ORIG(0x4B8A80, char *, Script_GetArgString, (ScriptArgs *args, int i, char *buf, int size));
ORIG(0x4B8AD0, int, Script_GetArgLine, (ScriptArgs *args, int i));
ORIG(0x4B8B00, int, Script_HasArg, (ScriptArgs *args, int i));
/* empty in this build */
ORIG(0x4C1780, void, Debug_Log, (Task *task, const char *text));
ORIG(0x4CF9D0, void, ModelFile_Free, (void *models));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1230, int, Level_GetUnusedNewTaskEvent, (void));
ORIG(0x4F1A40, void, ParamType_Free, (int type));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1AF0, void, ParamType_Call, (void *field, int type, uint8_t op, void *arg));
ORIG(0x4F1B60, uint8_t, ParamOp_Parse, (void));

extern const char gEmptyString[];

static const char kTaskNew[] = "Task_New";

#define HANDLER(slot, task) ((TaskFn)gTaskHandlers[slot][(task)->type])

/* Call the run handler of each child of task. The next sibling is kept on
   the task stack so a handler may delete it. */
static void run_children(Task *task)
{
    Node *n = task->children.head;
    Node *next;

    if (!n->next || !n)
        return;
    do {
        next = n->next;
        gTaskStack[gTaskStackTop++] = next && next->next ? (Task *)next : NULL;
        if (HANDLER(1, (Task *)n))
            HANDLER(1, (Task *)n)((Task *)n);
        n = (Node *)gTaskStack[--gTaskStackTop];
    } while (n);
}

static void clear_task_ids(void)
{
    Task ***p;

    for (p = gLevelTaskIds; p < gLevelTaskIds + LEVEL_MAX_IDS; p++) {
        if (*p) {
            TaskRef_Remove(*p);
            *p = NULL;
        }
    }
}

void Level_Run(Task *task)
{
    run_children(task);
    gLevel->time += gLevel->msPerTick;
    gLevel->tick++;
}
HOOK(0x4F0580, Level_Run);

void Level_Create(Task *task)
{
    gLevel = (Level *)task;
}
HOOK(0x4F0600, Level_Create);

void Level_Delete(void)
{
    gLevel = NULL;
}
HOOK(0x4F0610, Level_Delete);

void Static_Create(Task *task)
{
    if (!gLevelStatic)
        gLevelStatic = task;
}
HOOK(0x4F0620, Static_Create);

void Static_Delete(void)
{
    if (gLevelStatic)
        gLevelStatic = NULL;
}
HOOK(0x4F0640, Static_Delete);

void Dynamic_Run(Task *task)
{
    if (!gLevelPaused)
        run_children(task);
}
HOOK(0x4F0660, Dynamic_Run);

void Dynamic_Create(Task *task)
{
    gLevelDynamic = task;
}
HOOK(0x4F06E0, Dynamic_Create);

void Dynamic_Delete(void)
{
    gLevelDynamic = NULL;
}
HOOK(0x4F06F0, Dynamic_Delete);

void Idle_Create(Task *task)
{
    gLevelIdle = task;
}
HOOK(0x4F0700, Idle_Create);

void Idle_Delete(void)
{
    gLevelIdle = NULL;
}
HOOK(0x4F0710, Idle_Delete);

void CameraBase_Create(Task *task)
{
    gLevelCameraBase = task;
}
HOOK(0x4F0720, CameraBase_Create);

void CameraBase_Delete(void)
{
    gLevelCameraBase = NULL;
}
HOOK(0x4F0730, CameraBase_Delete);

ScriptValue *Task_DeclareParameters(ScriptValue *out)
{
    ScriptValue v;

    v.i = 0;
    v.d = 0;
    v.s = gEmptyString;
    *out = v;
    return out;
}
HOOK(0x4F0740, Task_DeclareParameters);

/* Script function Task_New(id, type, name, params...): creates a task of
   the named type under the context task, registers its level id and
   parses its parameters from the remaining arguments. */
ScriptValue *Level_NewTask(ScriptValue *out, Task *parent, ScriptArgs *args)
{
    char typeName[100];
    char name[64];
    ParamCursor cur;
    ParamTable *table;
    ScriptValue v;
    Task *task;
    int id, argc, i;
    int slot;

    if (!gLevelTaskNewEnabled) {
        Error_Show("Too few parameters in line %d of file '%s'", gLevelScriptLine, gLevelScriptFile);
        for (;;);
    }
    gLevelScriptLine = Script_GetArgLine(args, 0);
    id = Script_GetArgInt(args, 0);
    Script_GetArgString(args, 1, typeName, sizeof(typeName));
    argc = 3;
    Script_GetArgString(args, 2, name, sizeof(name));
    if (gLevelInsertHead) {
        task = Task_NewHead(parent, TaskType_Find(typeName), NULL);
        gLevelInsertHead = 0;
    } else {
        task = Task_New(parent, TaskType_Find(typeName), NULL);
    }
    if (!gLevelFirstTask)
        gLevelFirstTask = task;
    Debug_Log(task, name);
    if (id >= 0) {
        if (Task_FindById(id))
            id = -1;
        else
            Level_SetTaskId(id, task);
    }
    if (gTaskHandlers[Param_GetTableEvent()][task->type]) {
        ((void (*)(Task *, ParamTable **))gTaskHandlers[Param_GetTableEvent()][task->type])(task, &table);
        if (table) {
            gLevelTaskNewEnabled = 0;
            cur.index = 3;
            cur.args = args;
            cur.file = gLevelScriptFile;
            cur.line = gLevelScriptLine;
            for (i = 0; i < table->count; i++) {
                if (table->entries[i].flags & 1) {
                    char *field = (char *)task + table->entries[i].offset;
                    uint8_t op = ParamOp_Parse();

                    ParamType_Call(field, table->entries[i].type, op, &cur);
                }
            }
            argc = cur.index;
        }
    }
    gLevelTaskNewEnabled = 1;
    if (id >= 0) {
        slot = Level_GetActivateEvent();
        if (HANDLER(slot, task))
            HANDLER(slot, task)(task);
    }
    slot = Level_GetUnusedNewTaskEvent();
    if (HANDLER(slot, task))
        HANDLER(slot, task)(task);
    Script_SetSymbolContext(kTaskNew, task);
    while (Script_HasArg(args, argc)) {
        Script_GetArgInt(args, argc);
        argc++;
    }
    Script_SetSymbolContext(kTaskNew, parent);
    v.i = argc;
    v.d = argc;
    v.s = gEmptyString;
    *out = v;
    return out;
}
HOOK(0x4F0780, Level_NewTask);

void Level_Shutdown(void)
{
    List *l;
    Node *n, *next;

    for (l = gTaskNotes; l < gTaskNotes + LEVEL_NOTE_LISTS; l++) {
        n = l->head;
        for (next = n->next; next; next = next->next) {
            TaskNote_Free((TaskNote *)n);
            n = next;
        }
    }
    ParamType_Free(gParamDropDownCombo);
    ParamType_Free(gParamScriptFile);
    ParamType_Free(gParamPushButton);
    ParamType_Free(gParamObjectPos);
    ParamType_Free(gParamAngle);
    ParamType_Free(gParamDegrees);
    ParamType_Free(gParamRGB);
    ParamType_Free(gParamPitchGraph);
    ParamType_Free(gParamBitfield);
    ParamType_Free(gParamGraphEnvelope);
    ParamType_Free(gParamGraph);
    ParamType_Free(gParamQuat32);
    ParamType_Free(gParamReal64);
    ParamType_Free(gParamReal64x3);
    ParamType_Free(gParamReal32);
    ParamType_Free(gParamReal32x3);
    ParamType_Free(gParamReal32x9);
    ParamType_Free(gParamInt32);
    ParamType_Free(gParamInt16);
    ParamType_Free(gParamVarString);
    ParamType_Free(gParamString16);
    ParamType_Free(gParamString32);
    ParamType_Free(gParamString256);
    ParamType_Free(gParamLevelExpression);
    ParamType_Free(gParamQTaskNote);
    ParamType_Free(gParamBool8);
    TaskEvent_Free(gLevelDebugInfoEvent);
    TaskEvent_Free(gLevelNameEvent);
    TaskEvent_Free2(gLevelActivateEvent);
    TaskEvent_Free2(gLevelDeactivateEvent);
    TaskEvent_Free2(gLevelUnusedNewTaskEvent);
    gLevelDebugInfoEvent = 0xFF;
    gLevelNameEvent = 0xFF;
    gLevelActivateEvent = 0xFF;
    gLevelDeactivateEvent = 0xFF;
    gLevelUnusedNewTaskEvent = 0xFF;
    TaskType_Free(gStaticType);
    TaskType_Free(gDynamicType);
    TaskType_Free(gIdleType);
    TaskType_Free(gCameraBaseType);
    TaskType_Free(gLevelType);
    clear_task_ids();
}
HOOK(0x4F0A60, Level_Shutdown);

void TaskNote_Free(TaskNote *note)
{
    TaskRef_Remove(note->ref);
    List_Remove(&note->node);
    Mem_Free(note);
}
HOOK(0x4F0C90, TaskNote_Free);

Task *Level_New(Task *parent)
{
    return Task_New(parent, gLevelType, NULL);
}
HOOK(0x4F0CB0, Level_New);

void Level_FreeModels(void)
{
    ModelFile_Free(gLevelModels);
}
HOOK(0x4F0CD0, Level_FreeModels);

void Level_SetModels(void *models)
{
    gLevelModels = models;
}
HOOK(0x4F0CE0, Level_SetModels);

/* Run a script whose Task_New calls create tasks under parent. Returns the
   first task it made. */
Task *Task_RunScript(Task *parent, const char *path, char insertHead)
{
    void *s;

    Script_SetSymbolContext(kTaskNew, parent);
    s = Script_Load(path);
    if (!s)
        return NULL;
    ScriptInit(path, 1, 0, insertHead);
    Script_Run(s);
    Script_Free(s);
    return gLevelFirstTask;
}
HOOK(0x4F0CF0, Task_RunScript);

void Level_Load(const char *path, int fps)
{
    void *s;
    char changed;

    clear_task_ids();
    Script_SetSymbolContext(kTaskNew, gLevel);
    s = Script_Load(path);
    if (!s) {
        Error_Show("Level_Load(): Couldn't load script: %s", path);
        for (;;);
    }
    ScriptInit(path, 1, 0, 0);
    Script_Run(s);
    Script_Free(s);
    changed = 1;
    Task_Broadcast(&gLevel->task, Param_GetChangedEvent(), &changed);
    gLevel->time = 0;
    gLevel->tick = 0;
    gLevel->fps = fps;
    gLevel->msPerTick = 1000 / fps;
}
HOOK(0x4F0D40, Level_Load);

void LevelLoad(const char *dir, int fps)
{
    char path[256];

    crt_sprintf(path, "%s/objects.qsc", dir);
    Level_Load(path, fps);
}
HOOK(0x4F0E10, LevelLoad);

void ScriptInit(const char *file, char taskNewEnabled, int line, char insertHead)
{
    gLevelScriptFile = file;
    gLevelTaskNewEnabled = taskNewEnabled;
    gLevelScriptLine = line;
    gLevelInsertHead = insertHead;
    gLevelFirstTask = NULL;
}
HOOK(0x4F0E50, ScriptInit);

void Level_Clear(void)
{
    if (gLevel)
        Task_DeleteChildren(&gLevel->task);
    clear_task_ids();
}
HOOK(0x4F0E90, Level_Clear);

Task *Level_GetStatic(void)
{
    return gLevelStatic;
}
HOOK(0x4F0ED0, Level_GetStatic);

Task *Level_GetDynamic(void)
{
    return gLevelDynamic;
}
HOOK(0x4F0EE0, Level_GetDynamic);

Task *Task_FindById(int id)
{
    if (id >= 0 && id < LEVEL_MAX_IDS && gLevelTaskIds[id])
        return *gLevelTaskIds[id];
    return NULL;
}
HOOK(0x4F1030, Task_FindById);

int Task_GetId(Task *task)
{
    int i;

    for (i = 0; i < LEVEL_MAX_IDS; i++)
        if (gLevelTaskIds[i] && *gLevelTaskIds[i] == task)
            return i;
    return -1;
}
HOOK(0x4F1050, Task_GetId);

int Level_GetTick(void)
{
    return gLevel->tick;
}
HOOK(0x4F1080, Level_GetTick);

void Level_SetTaskId(int id, Task *task)
{
    Task **ref = gLevelTaskIds[id];

    if (ref) {
        if (*ref) {
            Error_Show("LevelQTaskID %d already defined", id);
            for (;;);
        }
        TaskRef_Remove(ref);
    }
    gLevelTaskIds[id] = TaskRef_Add(task);
}
HOOK(0x4F1090, Level_SetTaskId);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DD14) uint16_t gCameraBaseType INIT(385);
DATA(54DD14) char gLevelTaskNewEnabled INIT(1);
PAD(igd, 54DD14, 54DD17, 0x1);
DATA(54DDB4) char str_54DDB4[2] INIT("B");
PAD(igd, 54DDB4, 54DDB6, 0x2);
DATA(54DDB4) char str_54DDB8[2] INIT("G");
PAD(igd, 54DDB4, 54DDBA, 0x2);
DATA(54DDB4) char str_54DDBC[2] INIT("R");
PAD(igd, 54DDB4, 54DDBE, 0x2);
DATA(54DDB4) char str_54DDC0[7] INIT("BIT_31");
PAD(igd, 54DDB4, 54DDC7, 0x1);
DATA(54DDB4) char str_54DDC8[7] INIT("BIT_30");
PAD(igd, 54DDB4, 54DDCF, 0x1);
DATA(54DDB4) char str_54DDD0[7] INIT("BIT_29");
PAD(igd, 54DDB4, 54DDD7, 0x1);
DATA(54DDB4) char str_54DDD8[7] INIT("BIT_28");
PAD(igd, 54DDB4, 54DDDF, 0x1);
DATA(54DDB4) char str_54DDE0[7] INIT("BIT_27");
PAD(igd, 54DDB4, 54DDE7, 0x1);
DATA(54DDB4) char str_54DDE8[7] INIT("BIT_26");
PAD(igd, 54DDB4, 54DDEF, 0x1);
DATA(54DDB4) char str_54DDF0[7] INIT("BIT_25");
PAD(igd, 54DDB4, 54DDF7, 0x1);
DATA(54DDB4) char str_54DDF8[7] INIT("BIT_24");
PAD(igd, 54DDB4, 54DDFF, 0x1);
DATA(54DDB4) char str_54DE00[7] INIT("BIT_23");
PAD(igd, 54DDB4, 54DE07, 0x1);
DATA(54DDB4) char str_54DE08[7] INIT("BIT_22");
PAD(igd, 54DDB4, 54DE0F, 0x1);
DATA(54DDB4) char str_54DE10[7] INIT("BIT_21");
PAD(igd, 54DDB4, 54DE17, 0x1);
DATA(54DDB4) char str_54DE18[7] INIT("BIT_20");
PAD(igd, 54DDB4, 54DE1F, 0x1);
DATA(54DDB4) char str_54DE20[7] INIT("BIT_19");
PAD(igd, 54DDB4, 54DE27, 0x1);
DATA(54DDB4) char str_54DE28[7] INIT("BIT_18");
PAD(igd, 54DDB4, 54DE2F, 0x1);
DATA(54DDB4) char str_54DE30[7] INIT("BIT_17");
PAD(igd, 54DDB4, 54DE37, 0x1);
DATA(54DDB4) char str_54DE38[7] INIT("BIT_16");
PAD(igd, 54DDB4, 54DE3F, 0x1);
DATA(54DDB4) char str_54DE40[7] INIT("BIT_15");
PAD(igd, 54DDB4, 54DE47, 0x1);
DATA(54DDB4) char str_54DE48[7] INIT("BIT_14");
PAD(igd, 54DDB4, 54DE4F, 0x1);
DATA(54DDB4) char str_54DE50[7] INIT("BIT_13");
PAD(igd, 54DDB4, 54DE57, 0x1);
DATA(54DDB4) char str_54DE58[7] INIT("BIT_12");
PAD(igd, 54DDB4, 54DE5F, 0x1);
DATA(54DDB4) char str_54DE60[7] INIT("BIT_11");
PAD(igd, 54DDB4, 54DE67, 0x1);
DATA(54DDB4) char str_54DE68[7] INIT("BIT_10");
PAD(igd, 54DDB4, 54DE6F, 0x1);
DATA(54DDB4) char str_54DE70[6] INIT("BIT_9");
PAD(igd, 54DDB4, 54DE76, 0x2);
DATA(54DDB4) char str_54DE78[6] INIT("BIT_8");
PAD(igd, 54DDB4, 54DE7E, 0x2);
DATA(54DDB4) char str_54DE80[6] INIT("BIT_7");
PAD(igd, 54DDB4, 54DE86, 0x2);
DATA(54DDB4) char str_54DE88[6] INIT("BIT_6");
PAD(igd, 54DDB4, 54DE8E, 0x2);
DATA(54DDB4) char str_54DE90[6] INIT("BIT_5");
PAD(igd, 54DDB4, 54DE96, 0x2);
DATA(54DDB4) char str_54DE98[6] INIT("BIT_4");
PAD(igd, 54DDB4, 54DE9E, 0x2);
DATA(54DDB4) char str_54DEA0[6] INIT("BIT_3");
PAD(igd, 54DDB4, 54DEA6, 0x2);
DATA(54DDB4) char str_54DEA8[6] INIT("BIT_2");
PAD(igd, 54DDB4, 54DEAE, 0x2);
DATA(54DDB4) char str_54DEB0[6] INIT("BIT_1");
PAD(igd, 54DDB4, 54DEB6, 0x2);
DATA(54DDB4) char str_54DEB8[6] INIT("BIT_0");
PAD(igd, 54DDB4, 54DEBE, 0x2);
BSS(A70C74) int gLevelScriptLine;
BSS(A70C94) void *gLevelModels;
BSS(A70C94) const char *gLevelScriptFile;
STORE(igb, A71890, 4) __typeof__(gLevelTaskIds[0]) store_A71890[4096];
ALIAS(gLevelTaskIds, store_A71890, 0);
BSS(A758A4) Level *gLevel;
BSS(A758A4) Task *gLevelFirstTask;
BSS(A758A4) Task *gLevelDynamic;
BSS(A758A4) Task *gLevelStatic;
BSS(A758A4) Task *gLevelIdle;
BSS(A758A4) Task *gLevelCameraBase;
