#include "hook.h"
#include "terredit/terredit.h"

FN_EXTERN(Lensflare_Delete);
FN_EXTERN(Task_Nothing);

/* Small task types: SkyTask, LevelScript, SimBoneDynCubeObj, PushButton */

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t slot));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4B6F50, void, Text_DrawAlt, (void *text));
ORIG(0x4B6F90, void, Text_GetRect, (void *text, float *rect));
ORIG(0x4C1780, void, Debug_Log, (uint32_t a, uint32_t b));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C6890, void, DynCube_ListAdd, (Terrain *world, void *obj, int list));
ORIG(0x4D9610, uint16_t, BoneDynCubeObj_GetType, (void));
ORIG(0x4E7530, uint8_t, Screen_GetDrawEvent, (void));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x504DC0, void, MenuItemTask_AddToList, (void *list, void *item));
ORIG(0x504DF0, uint8_t, MenuItemTask_GetListEvent, (void));
ORIG(0x505C80, uint16_t, MenuItem_GetType, (void));

#define Task_Nothing (FN(0x4D3710, Task_Nothing))
#define DynCubeObj_ListRemove (FN(0x520F20, Lensflare_Delete))

/* ---- SkyTask ---- */

typedef struct SkyTask {
    Task task;
    char unk20[0x48];
    Terrain *world;
    uint32_t angle;     /* float */
    uint32_t height;    /* float */
    int unk74;
} SkyTask;

_Static_assert(sizeof(SkyTask) == 0x78, "SkyTask");

#define gSkyTaskType VAR(uint16_t, 0x54F6E4)
#define gSkyTaskParams VAR(void *, 0xA84558)

void SkyTask_Render(SkyTask *t);
void SkyTask_Create(SkyTask *t);
void SkyTask_GetParams(void *task, void **out);

void SkyTask_Init(void)
{
    SkyTask t;
    uint16_t type;

    type = TaskType_New("SkyTask", sizeof(SkyTask), Task_GetRootType(), 0, 0);
    gSkyTaskType = type;
    TaskType_SetHandler(type, 4, SkyTask_Render);
    TaskType_SetHandler(gSkyTaskType, 0, SkyTask_Create);
    TaskType_SetHandler2(gSkyTaskType, 2, DynCubeObj_ListRemove);
    TaskType_SetHandler(gSkyTaskType, Param_GetChangedEvent(), Task_Nothing);
    TaskType_SetHandler(gSkyTaskType, Param_GetTableEvent(), SkyTask_GetParams);
    gSkyTaskParams = ParamTable_New(2);
    ParamTable_Add(gSkyTaskParams, &t, &t.height, Param_Real32(), 7, NULL);
    ParamTable_Add(gSkyTaskParams, &t, &t.angle, Param_Angle(), 7, NULL);
}
HOOK(0x52C6B0, SkyTask_Init);

void SkyTask_GetParams(void *task, void **out)
{
    *out = gSkyTaskParams;
}
HOOK(0x52C790, SkyTask_GetParams);

void SkyTask_Render(SkyTask *t)
{
    Debug_Log(t->height, t->angle);
}
HOOK(0x52C7A0, SkyTask_Render);

void SkyTask_Create(SkyTask *t)
{
    t->world = gCurrentWorld;
    t->angle = 0;
    t->height = 0;
    DynCube_ListAdd(t->world, t, 0);
}
HOOK(0x52C7C0, SkyTask_Create);

void SkyTask_Shutdown(void)
{
    QTaskList_Free(gSkyTaskParams);
    TaskType_Free(gSkyTaskType);
    gSkyTaskType = 0x181;
}
HOOK(0x52C7F0, SkyTask_Shutdown);

/* ---- LevelScript ---- */

/* Runs its children in order: each child gets the LevelScript event and
   sets done when the next one may start. */
typedef struct LevelScript {
    Task task;
    int value;
    Task *current;
} LevelScript;

typedef struct LevelScriptStep {
    int value;
    char done;
} LevelScriptStep;

_Static_assert(sizeof(LevelScript) == 0x28, "LevelScript");

#define gLevelScriptType VAR(uint16_t, 0x54F6F0)
#define gLevelScriptEvent VAR(uint8_t, 0x54F6F2)
#define gLevelScriptParams VAR(void *, 0xA84564)

void LevelScript_Create(LevelScript *ls);
void LevelScript_Run(LevelScript *ls);
void LevelScript_GetParams(void *task, void **out);

void LevelScript_Init(void)
{
    LevelScript t;

    gLevelScriptType = TaskType_New("LevelScript", sizeof(LevelScript), Task_GetRootType(), 0, 0);
    gLevelScriptEvent = TaskEvent_Alloc(Task_GetRootType());
    TaskType_SetHandler(gLevelScriptType, 0, LevelScript_Create);
    TaskType_SetHandler2(gLevelScriptType, 1, LevelScript_Run);
    TaskType_SetHandler(gLevelScriptType, Param_GetChangedEvent(), Task_Nothing);
    TaskType_SetHandler(gLevelScriptType, Param_GetTableEvent(), LevelScript_GetParams);
    gLevelScriptParams = ParamTable_New(2);
    ParamTable_Add(gLevelScriptParams, &t, &t.value, Param_Int32(), 7, NULL);
}
HOOK(0x52C820, LevelScript_Init);

void LevelScript_GetParams(void *task, void **out)
{
    *out = gLevelScriptParams;
}
HOOK(0x52C8E0, LevelScript_GetParams);

void LevelScript_Create(LevelScript *ls)
{
    ls->current = NULL;
}
HOOK(0x52C8F0, LevelScript_Create);

void LevelScript_Run(LevelScript *ls)
{
    if (!ls->current) {
        Task *first = ls->task.head;

        ls->current = first->next ? first : NULL;
        CALL_CHILDREN(ls, Param_GetChangedEvent(), NULL);
    }
    for (;;) {
        LevelScriptStep step;
        Task *c = ls->current;

        step.value = ls->value;
        step.done = 0;
        if (!c)
            return;
        HANDLER(gLevelScriptEvent, c->type)(c, &step);
        if (!step.done)
            return;
        ls->current = next_sibling(ls->current);
    }
}
HOOK(0x52C900, LevelScript_Run);

void LevelScript_Shutdown(void)
{
    TaskEvent_Free(gLevelScriptEvent);
    gLevelScriptEvent = 0xFF;
    TaskType_Free(gLevelScriptType);
    QTaskList_Free(gLevelScriptParams);
}
HOOK(0x52CA00, LevelScript_Shutdown);

/* ---- SimBoneDynCubeObj ---- */

#define gSimBoneDynCubeObjType VAR(uint16_t, 0x54F700)

void SimBoneDynCubeObj_Init(void)
{
    gSimBoneDynCubeObjType = TaskType_New("SimBoneDynCubeObj", 0x140, BoneDynCubeObj_GetType(), 0, 0);
}
HOOK(0x52CA30, SimBoneDynCubeObj_Init);

void SimBoneDynCubeObj_Shutdown(void)
{
    TaskType_Free(gSimBoneDynCubeObjType);
}
HOOK(0x52CA60, SimBoneDynCubeObj_Shutdown);

/* ---- PushButton ---- */

typedef struct PushButton {
    Task task;
    char unk20[0x34];
    char item[8];       /* what the menu list gets */
    float rect[4];
    int unk6C[2];
    void *owner;
    void *text;
} PushButton;

_Static_assert(sizeof(PushButton) == 0x7C, "PushButton");

#define gPushButtonType VAR(uint16_t, 0x54F718)

void PushButton_Draw(PushButton *b);
void PushButton_AddToList(PushButton *b, void *list);

void PushButton_Init(void)
{
    gPushButtonType = TaskType_New("PushButton", sizeof(PushButton), MenuItem_GetType(), 0, 0);
    TaskType_SetHandler2(gPushButtonType, Screen_GetDrawEvent(), PushButton_Draw);
    TaskType_SetHandler(gPushButtonType, MenuItemTask_GetListEvent(), PushButton_AddToList);
}
HOOK(0x52CA70, PushButton_Init);

void PushButton_Draw(PushButton *b)
{
    if (b->text)
        Text_DrawAlt(b->text);
}
HOOK(0x52CAD0, PushButton_Draw);

void PushButton_AddToList(PushButton *b, void *list)
{
    if (b->text) {
        b->owner = b;
        Text_GetRect(b->text, b->rect);
        MenuItemTask_AddToList(list, b->item);
    }
}
HOOK(0x52CAF0, PushButton_AddToList);

void PushButton_Shutdown(void)
{
    TaskType_Free(gPushButtonType);
}
HOOK(0x52CB20, PushButton_Shutdown);
