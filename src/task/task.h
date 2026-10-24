#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#include "core/list.h"
#include "task/tasktypes.h"

#define TASK_MAX_TYPES 0x180
#define TASK_MAX_SLOTS 254
#define TASK_NO_TYPE 0x181

enum {
    TASK_SLOT_CREATE,
    TASK_SLOT_RUN,
    TASK_SLOT_DELETE,
    TASK_SLOT_PREDELETE,
};

typedef struct TaskEvent {
    uint16_t type;
    char used;
    char flag;
    uint16_t unk4;
} TaskEvent;

typedef void (*TaskHandler)(Task *task, void *arg);

void Task_InitSystem(void);
void Task_ShutdownSystem(void);
void TaskType_ResetAll(void);
void TaskEvent_ResetAll(void);
void TaskType_WarnLeaks(void);
void TaskEvent_WarnLeaks(void);
Task *Task_New(Task *parent, uint16_t type, void *arg);
Task *Task_Alloc(Task *parent, uint16_t type);
Task *Task_NewHead(Task *parent, uint16_t type, void *arg);
void TaskType_SetHandler(uint16_t type, uint8_t slot, TaskHandler fn);
void TaskType_UpdateMasks(void);
uint16_t TaskType_GetMask(uint16_t type);
void TaskType_SetHandler2(uint16_t type, int slot, TaskHandler fn);
void Task_Unlink(Task *task);
void Task_Delete(Task *task);
void Task_DeleteChildren(Task *task);
int TaskEvent_Alloc(uint16_t type);
int TaskEvent_Alloc2(uint16_t type);
void TaskEvent_Set(uint16_t type, uint8_t index);
void TaskEvent_Set2(uint16_t type, int index);
void TaskEvent_Free(uint8_t index);
void TaskEvent_Free2(uint8_t index);
uint16_t Task_GetRootType(void);
uint16_t TaskType_New(const char *name, int size, uint16_t parent, int pool_count);
void TaskType_Free(uint16_t type);
int Task_GetCount(void);
uint16_t TaskType_Find(const char *name);
Task **TaskRef_Add(Task *task);
void TaskRef_Remove(Task **ref);
const char *TaskType_GetName(uint16_t type);
void Task_MoveAfter(Task *task, Task *after);
Task *Task_FindChild(Task *task, uint16_t type);
Task *Task_FindLastChild(Task *task, uint16_t type);
Task *Task_FindNext(Task *task, uint16_t type);
Task *Task_FindPrev(Task *task, uint16_t type);
Task *Task_FindAncestor(Task *task, uint16_t type);
char TaskType_IsA(uint16_t type, uint16_t base);
void TaskEvent_MapSlot(uint8_t slot, int bit);
void TaskEvent_UnmapSlot(uint8_t slot);
void Task_Broadcast(Task *task, uint8_t slot, void *arg);
void Task_BroadcastChildren(Task *task, uint8_t slot, void *arg);
void Task_BroadcastType(Task *task, uint8_t slot, uint16_t type, void *arg);
void Task_SetUnk567C60(int value);
void Task_ClearPair(int *a, int *b);
void Task_SetGlobalFlags(uint32_t flags);
void Task_ClearGlobalFlags(uint32_t flags);
void TaskType_SetFlags(uint16_t type, uint32_t flags);

#endif
