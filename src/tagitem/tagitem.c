#include <stddef.h>
#include <string.h>

#include "hook.h"
#include "tagitem/tagitem.h"

/* "TagItem": a named value task hung under another task, read back by name. */

ORIG(0x4012A0, Task *, Task_New, (Task *parent, uint16_t type, void *arg));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CF0, char, TaskType_IsA, (uint16_t type, uint16_t base));
ORIG(0x4C1780, void, sub_4C1780, (Task *task, const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1980, void *, ParamTable_Extend, (void *base, int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *extra));

#define HANDLER_4D3710 ((void *)0x4D3710)

#define gTagItemReal32Type VAR(uint16_t, 0xA7A4E0)
#define gTagItemString16Params VAR(void *, 0xA7A4E4)
#define gTagItemString256Type VAR(uint16_t, 0xA7A4E8)
#define gTagItemParams VAR(void *, 0xA7A4EC)
#define gTagItemReal32Params VAR(void *, 0xA7A4F0)
#define gTagItemEvent VAR(uint8_t, 0xA7A4F4)
#define gTagItemInt32Type VAR(uint16_t, 0xA7A4F6)
#define gTagItemInt32Params VAR(void *, 0xA7A4F8)
#define gTagItemType VAR(uint16_t, 0xA7A4FC)
#define gTagItemString16Type VAR(uint16_t, 0xA7A4FE)
#define gTagItemString256Params VAR(void *, 0xA7A500)

#define gTaskHandlers ((TaskFn (*)[0x180])0xA96AE0)

void TagItem_OnChanged(TagItem *t);
void TagItemInt32_Create(TagItem *t);
void TagItem_SetParamType(TagItem *t, void *type);
void TagItemInt32_GetParams(void *task, void **out);
void TagItemReal32_Create(TagItem *t);
void TagItemReal32_GetParams(void *task, void **out);
void TagItemString16_Create(TagItem *t);
void TagItemString16_GetParams(void *task, void **out);
void TagItemString256_Create(TagItem *t);
void TagItemString256_GetParams(void *task, void **out);

/* value is a float, copied as raw bits */
TagItem *TagItem_AddReal32(Task *parent, const char *name, int value)
{
    TagItem *t = (TagItem *)Task_New(parent, gTagItemReal32Type, NULL);

    strcpy(t->name, name);
    t->v.i = value;
    sub_4C1780(&t->task, name);
    return t;
}
HOOK(0x4FE650, TagItem_AddReal32);

TagItem *TagItem_AddString16(Task *parent, const char *name, const char *value)
{
    TagItem *t = (TagItem *)Task_New(parent, gTagItemString16Type, NULL);

    strcpy(t->name, name);
    strcpy(t->v.s, value);
    sub_4C1780(&t->task, name);
    return t;
}
HOOK(0x4FE6B0, TagItem_AddString16);

TagItem *TagItem_Find(Task *parent, const char *name)
{
    Task *t = parent->head;

    if (!t->next)
        return NULL;
    while (t) {
        if (TaskType_IsA(t->type, gTagItemType) && strcmp(((TagItem *)t)->name, name) == 0)
            return (TagItem *)t;
        t = t->next;
        if (!t || !t->next)
            return NULL;
    }
    return NULL;
}
HOOK(0x4FE730, TagItem_Find);

void TagItem_Init(void)
{
    gTagItemEvent = TaskEvent_Alloc(0);
    gTagItemType = TaskType_New("TagItem", 0x124, Task_GetRootType(), 0);
    TaskType_SetHandler(gTagItemType, 0, HANDLER_4D3710);
    TaskType_SetHandler2(gTagItemType, 1, HANDLER_4D3710);
    TaskType_SetHandler2(gTagItemType, 2, HANDLER_4D3710);
    TaskType_SetHandler(gTagItemType, Param_GetChangedEvent(), TagItem_OnChanged);
    gTagItemParams = ParamTable_New(1);
    ParamTable_Add(gTagItemParams, NULL, (void *)offsetof(TagItem, name), Param_String256(), 7, NULL);

    gTagItemInt32Type = TaskType_New("TagItemInt32", 0x128, gTagItemType, 0);
    TaskType_SetHandler(gTagItemInt32Type, 0, TagItemInt32_Create);
    TaskType_SetHandler(gTagItemInt32Type, Param_GetTableEvent(), TagItemInt32_GetParams);
    gTagItemInt32Params = ParamTable_Extend(gTagItemParams, 2);
    ParamTable_Add(gTagItemInt32Params, NULL, (void *)offsetof(TagItem, v), Param_Int32(), 7, NULL);

    gTagItemReal32Type = TaskType_New("TagItemReal32", 0x128, gTagItemType, 0);
    TaskType_SetHandler(gTagItemReal32Type, 0, TagItemReal32_Create);
    TaskType_SetHandler(gTagItemReal32Type, Param_GetTableEvent(), TagItemReal32_GetParams);
    gTagItemReal32Params = ParamTable_Extend(gTagItemParams, 2);
    ParamTable_Add(gTagItemReal32Params, NULL, (void *)offsetof(TagItem, v), Param_Real32(), 7, NULL);

    gTagItemString16Type = TaskType_New("TagItemString16", 0x134, gTagItemType, 0);
    TaskType_SetHandler(gTagItemString16Type, 0, TagItemString16_Create);
    TaskType_SetHandler(gTagItemString16Type, Param_GetTableEvent(), TagItemString16_GetParams);
    gTagItemString16Params = ParamTable_Extend(gTagItemParams, 2);
    ParamTable_Add(gTagItemString16Params, NULL, (void *)offsetof(TagItem, v), Param_String16(), 7, NULL);

    gTagItemString256Type = TaskType_New("TagItemString256", 0x224, gTagItemType, 0);
    TaskType_SetHandler(gTagItemString256Type, 0, TagItemString256_Create);
    TaskType_SetHandler(gTagItemString256Type, Param_GetTableEvent(), TagItemString256_GetParams);
    gTagItemString256Params = ParamTable_Extend(gTagItemParams, 2);
    ParamTable_Add(gTagItemString256Params, NULL, (void *)offsetof(TagItem, v), Param_String256(), 7, NULL);
}
HOOK(0x4FE7C0, TagItem_Init);

/* passes the change on to the owner: the owner's handler for the TagItem event gets (owner, item) */
void TagItem_OnChanged(TagItem *t)
{
    Task *owner = t->task.parent;
    TaskFn fn;

    if (!owner)
        return;
    fn = gTaskHandlers[gTagItemEvent][owner->type];
    if (fn)
        fn(owner, t);
}
HOOK(0x4FEA60, TagItem_OnChanged);

void TagItemInt32_Create(TagItem *t)
{
    TagItem_SetParamType(t, Param_Int32());
    t->v.i = 0;
}
HOOK(0x4FEAA0, TagItemInt32_Create);

void TagItem_SetParamType(TagItem *t, void *type)
{
    t->paramType = type;
    t->name[0] = 0;
}
HOOK(0x4FEAC0, TagItem_SetParamType);

void TagItemInt32_GetParams(void *task, void **out)
{
    *out = gTagItemInt32Params;
}
HOOK(0x4FEAE0, TagItemInt32_GetParams);

void TagItemReal32_Create(TagItem *t)
{
    TagItem_SetParamType(t, Param_Real32());
    t->v.i = 0;
}
HOOK(0x4FEAF0, TagItemReal32_Create);

void TagItemReal32_GetParams(void *task, void **out)
{
    *out = gTagItemReal32Params;
}
HOOK(0x4FEB10, TagItemReal32_GetParams);

void TagItemString16_Create(TagItem *t)
{
    TagItem_SetParamType(t, Param_String16());
    t->v.s[0] = 0;
}
HOOK(0x4FEB20, TagItemString16_Create);

void TagItemString16_GetParams(void *task, void **out)
{
    *out = gTagItemString16Params;
}
HOOK(0x4FEB40, TagItemString16_GetParams);

void TagItemString256_Create(TagItem *t)
{
    TagItem_SetParamType(t, Param_String256());
    t->v.s[0] = 0;
}
HOOK(0x4FEB50, TagItemString256_Create);

void TagItemString256_GetParams(void *task, void **out)
{
    *out = gTagItemString256Params;
}
HOOK(0x4FEB70, TagItemString256_GetParams);

void TagItem_Shutdown(void)
{
    QTaskList_Free(gTagItemString256Params);
    QTaskList_Free(gTagItemString16Params);
    QTaskList_Free(gTagItemReal32Params);
    QTaskList_Free(gTagItemInt32Params);
    QTaskList_Free(gTagItemParams);
    TaskType_Free(gTagItemType);
    TaskType_Free(gTagItemInt32Type);
    TaskType_Free(gTagItemReal32Type);
    TaskType_Free(gTagItemString16Type);
    TaskType_Free(gTagItemString256Type);
    TaskEvent_Free(gTagItemEvent);
}
HOOK(0x4FEB80, TagItem_Shutdown);
