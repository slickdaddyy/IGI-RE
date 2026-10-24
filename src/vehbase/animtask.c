/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "vehbase/vehbase.h"

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4AF8F0, void, List_AddTail, (void *list, Node *node));
ORIG(0x4AF960, void, List_Remove, (Node *node));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B7FA0, void, Script_RegisterInt2, (const char *name, int *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4F0EF0, void *, Param_PushButton, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1030, Task *, Task_FindById, (int id));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetEventDA8, (void));
ORIG(0x4F1220, int, Level_GetEventDAC, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F1340, void, VarString_Free, (VarString *v));
ORIG(0x4F1400, void, VarString_Bind, (VarString *v, void *task));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F19C0, int, ParamType_New, (const char *name, int parent));
ORIG(0x4F1A40, void, ParamType_Free, (int type));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1B60, uint8_t, ParamOp_Parse, (void));
ORIG(0x4F1B70, uint8_t, ParamOp_ArgCount, (void));
ORIG(0x4F1B80, uint8_t, ParamOp_Format, (void));
ORIG(0x4F1B90, uint8_t, ParamOp_Size, (void));
ORIG(0x4F1BA0, uint8_t, ParamOp_Copy, (void));
ORIG(0x4F1BB0, uint8_t, ParamOp_Paste, (void));
ORIG(0x4F1BC0, void, ParamType_SetOp, (int type, uint8_t op, void *fn));
ORIG(0x5034D0, uint16_t, EditorRigidObj_GetType, (void));

extern uint16_t gAnimTaskType;
extern uint8_t gVehicleControlEvent;
extern uint8_t gAnimEventDB;
extern uint8_t gVehicleInputInfoEvent;
extern uint8_t gVehicleStateSizeEvent;
extern uint8_t gVehicleSaveEvent;
extern uint8_t gVehicleLoadEvent;
extern uint8_t gVehicleKeysEvent;
extern int gAnimDataType;
extern void *gAnimTaskParams;
extern List gAnimTasks;
extern int gAnimTaskUnused1;
extern int gAnimTaskUnused2;

extern float kZero;
extern float kOne;

/* size of the state block an AnimTask saves before its vehicle's */
#define ANIM_STATE_SIZE 0xE8

/* fistp with the current rounding mode */
static inline long long round_ll(long double f)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(f) : "st");
    return r;
}

/* x87 "not equal" as tested with C3: false when unordered */
static inline int fne(long double a, long double b)
{
    return a < b || a > b;
}

typedef struct TaskParseMsg {
    int next;
    void *args;
} TaskParseMsg;

typedef struct ArgCountMsg {
    int count;
    int index;
    void *args;
} ArgCountMsg;

/* sent with the vehicle control event */
typedef struct ControlMsg {
    int tick;
    float *channels;
} ControlMsg;

uint8_t Vehicle_GetControlEvent(void);
uint8_t Vehicle_GetStateSizeEvent(void);
uint8_t Vehicle_GetSaveEvent(void);
uint8_t Vehicle_GetLoadEvent(void);

void AnimData_Parse(AnimTask *t, TaskParseMsg *msg);
void AnimData_ArgCount(void *t, ArgCountMsg *msg);
void AnimData_Copy(AnimTask *t, AnimData **out);
void AnimData_Paste(AnimTask *t, AnimData **in);
void AnimData_GetSize(void *t, int *out);

void AnimTask_Open(void)
{
    gAnimTaskType = TaskType_New("AnimTask", 0x20160, EditorRigidObj_GetType(), 0);
    gVehicleControlEvent = TaskEvent_Alloc(0);
    gAnimEventDB = TaskEvent_Alloc(0);
    gVehicleInputInfoEvent = TaskEvent_Alloc(0);
    gVehicleStateSizeEvent = TaskEvent_Alloc(0);
    gVehicleSaveEvent = TaskEvent_Alloc(0);
    gVehicleLoadEvent = TaskEvent_Alloc(0);
    gVehicleKeysEvent = TaskEvent_Alloc(0);
    gAnimDataType = ParamType_New("AnimData", -1);
    ParamType_SetOp(gAnimDataType, ParamOp_Parse(), AnimData_Parse);
    ParamType_SetOp(gAnimDataType, ParamOp_Format(), TASK_NOTHING);
    ParamType_SetOp(gAnimDataType, ParamOp_Copy(), AnimData_Copy);
    ParamType_SetOp(gAnimDataType, ParamOp_Paste(), AnimData_Paste);
    ParamType_SetOp(gAnimDataType, ParamOp_Size(), AnimData_GetSize);
    ParamType_SetOp(gAnimDataType, ParamOp_ArgCount(), AnimData_ArgCount);
}
HOOK(0x4F3440, AnimTask_Open);

/* script arguments: key count, then (channel, value) pairs */
void AnimData_Parse(AnimTask *t, TaskParseMsg *msg)
{
    int i = msg->next, k;

    t->keyCount = Script_GetArgInt(msg->args, i++);
    for (k = 0; k < t->keyCount; k++) {
        char c = Script_GetArgInt(msg->args, i++);

        t->keys[k].channel = c;
        if (c == ANIM_WAIT)
            t->keys[k].value = (int)round_ll(Script_GetArgReal(msg->args, i++));
        else
            t->keys[k].value = Script_GetArgReal(msg->args, i++);
    }
    msg->next = i;
}
HOOK(0x4F3570, AnimData_Parse);

void AnimData_ArgCount(void *t, ArgCountMsg *msg)
{
    msg->count = Script_GetArgInt(msg->args, msg->index) * 2 + 1;
}
HOOK(0x4F3610, AnimData_ArgCount);

void AnimData_Copy(AnimTask *t, AnimData **out)
{
    AnimData *d = *out;

    d->keyCount = t->keyCount;
    memcpy(d->channels, t->channels, sizeof(d->channels));
    memcpy(d->keys, t->keys, sizeof(d->keys));
}
HOOK(0x4F3630, AnimData_Copy);

void AnimData_Paste(AnimTask *t, AnimData **in)
{
    AnimData *d = *in;

    t->keyCount = d->keyCount;
    memcpy(t->channels, d->channels, sizeof(t->channels));
    memcpy(t->keys, d->keys, sizeof(t->keys));
}
HOOK(0x4F3670, AnimData_Paste);

void AnimData_GetSize(void *t, int *out)
{
    *out = sizeof(AnimData);
}
HOOK(0x4F36B0, AnimData_GetSize);

void AnimTask_Create(AnimTask *t);
void AnimTask_Delete(AnimTask *t);
void AnimTask_Run(AnimTask *t);
void AnimTask_GetParams(void *task, void **out);
void AnimTask_ParamsChanged(AnimTask *t, char *arg);
void AnimTask_GetStateSize(AnimTask *t, int *out);
void AnimTask_SaveState(AnimTask *t, void **msg);
void AnimTask_LoadState(AnimTask *t, void **msg);
void AnimTask_RegisterSymbols(AnimTask *t);
void AnimTask_RemoveSymbols(AnimTask *t);

void AnimTask_InitType(void)
{
    AnimTask t;

    TaskType_SetHandler(gAnimTaskType, 0, AnimTask_Create);
    TaskType_SetHandler2(gAnimTaskType, 2, AnimTask_Delete);
    TaskType_SetHandler2(gAnimTaskType, 1, AnimTask_Run);
    TaskType_SetHandler(gAnimTaskType, Param_GetTableEvent(), AnimTask_GetParams);
    TaskType_SetHandler(gAnimTaskType, Param_GetChangedEvent(), AnimTask_ParamsChanged);
    TaskType_SetHandler(gAnimTaskType, Vehicle_GetStateSizeEvent(), AnimTask_GetStateSize);
    TaskType_SetHandler(gAnimTaskType, Vehicle_GetSaveEvent(), AnimTask_SaveState);
    TaskType_SetHandler(gAnimTaskType, Vehicle_GetLoadEvent(), AnimTask_LoadState);
    TaskType_SetHandler2(gAnimTaskType, Level_GetEventDA8(), AnimTask_RegisterSymbols);
    TaskType_SetHandler2(gAnimTaskType, Level_GetEventDAC(), AnimTask_RemoveSymbols);
    gAnimTaskParams = ParamTable_New(8);
    ParamTable_Add(gAnimTaskParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gAnimTaskParams, &t, &t.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gAnimTaskParams, &t, &t.target, Param_Int32(), 7, NULL);
    ParamTable_Add(gAnimTaskParams, &t, &t.cond, Param_VarString(), 7, NULL);
    ParamTable_Add(gAnimTaskParams, &t, &t.isRun, Param_Bool8(), 7, NULL);
    ParamTable_Add(gAnimTaskParams, &t, &t.button[0], Param_PushButton(), 6, NULL);
    ParamTable_Add(gAnimTaskParams, &t, &t.button[1], Param_PushButton(), 6, NULL);
    /* the AnimData parameter covers the whole task */
    ParamTable_Add(gAnimTaskParams, &t, &t, (void *)gAnimDataType, 7, NULL);
    TaskType_SetHandler(RigidDynCubeObj_GetType(), gAnimEventDB, TASK_NOTHING);
    gAnimTasks.head = (Node *)&gAnimTasks.tail;
    gAnimTasks.tailpred = (Node *)&gAnimTasks.head;
    gAnimTasks.tail = NULL;
    gAnimTaskUnused1 = 60;
    gAnimTaskUnused2 = 0x400;
}
HOOK(0x4F36C0, AnimTask_InitType);

void AnimTask_Seek(AnimTask *t, int tick);

void AnimTask_ParamsChanged(AnimTask *t, char *arg)
{
    if (!arg || !*arg)
        BROADCAST(&t->task, Param_GetChangedEvent(), NULL);
    AnimTask_Seek(t, 0);
    VarString_Bind(&t->cond, t);
}
HOOK(0x4F3940, AnimTask_ParamsChanged);

void AnimTask_GetParams(void *task, void **out)
{
    *out = gAnimTaskParams;
}
HOOK(0x4F39F0, AnimTask_GetParams);

void AnimTask_Create(AnimTask *t)
{
    memset(&t->pos, 0, sizeof(t->pos));
    memset(&t->matrix, 0, sizeof(t->matrix));
    t->matrix.m[0] = t->matrix.m[4] = t->matrix.m[8] = 1.0f;
    t->tick = -1;
    t->target = -1;
    t->keyCount = 1;
    t->keys[0].channel = ANIM_WAIT;
    t->keys[0].value = 0;
    t->isRun = 1;
    t->registered = 0;
    t->ref = NULL;
    VarString_Init(&t->cond);
    memset(t->channels, 0, sizeof(t->channels));
    t->node.prev = NULL;
    t->node.next = NULL;
    List_AddTail(&gAnimTasks, &t->node);
    t->self = t;
    t->button[0] = 0;
    t->button[1] = 0;
}
HOOK(0x4F3A00, AnimTask_Create);

void AnimTask_Delete(AnimTask *t)
{
    if (Task_GetId(t) >= 0) {
        TaskFn fn = HANDLER(Level_GetEventDAC(), t->task.type);

        if (fn)
            fn(t);
    }
    VarString_Free(&t->cond);
    List_Remove(&t->node);
    if (t->ref) {
        if (*t->ref)
            Task_Delete(*t->ref);
        TaskRef_Remove(t->ref);
    }
}
HOOK(0x4F3AC0, AnimTask_Delete);

void AnimTask_Run(AnimTask *t)
{
    if (VarString_HasExpr(&t->cond))
        t->isRun = (char)round_ll(VarString_EvalReal(&t->cond, t));
    if (t->isRun)
        AnimTask_Seek(t, t->tick + 1);
}
HOOK(0x4F3B40, AnimTask_Run);

void AnimTask_RegisterSymbols(AnimTask *t)
{
    char name[64], buf[128];

    if (t->registered)
        return;
    t->registered = 1;
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.isRun", name);
    Script_RegisterBool8(buf, &t->isRun);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RegisterInt2(buf, &t->tick);
}
HOOK(0x4F3BA0, AnimTask_RegisterSymbols);

void AnimTask_RemoveSymbols(AnimTask *t)
{
    char name[64], buf[128];

    if (!t->registered)
        return;
    Task_GetName(name, t);
    t->registered = 0;
    crt_sprintf(buf, "%s.isRun", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.nTick", name);
    Script_RemoveSymbol(buf);
}
HOOK(0x4F3C20, AnimTask_RemoveSymbols);

/* the vehicle's own state follows ours */
void AnimTask_GetStateSize(AnimTask *t, int *out)
{
    Task *v = Task_FindById(t->target);

    if (v && HANDLER(Vehicle_GetStateSizeEvent(), v->type)) {
        HANDLER(Vehicle_GetStateSizeEvent(), v->type)(v, out);
        *out += ANIM_STATE_SIZE;
        return;
    }
    *out = ANIM_STATE_SIZE;
}
HOOK(0x4F3CA0, AnimTask_GetStateSize);

void AnimTask_SaveState(AnimTask *t, void **msg)
{
    Task *v = Task_FindById(t->target);
    char *buf = msg[0];
    void *m[5];
    TaskFn fn;

    memcpy(m, msg, sizeof(m));
    m[0] = buf + ANIM_STATE_SIZE;
    memcpy(buf, t->channels, ANIM_STATE_SIZE);
    if (v && (fn = HANDLER(Vehicle_GetSaveEvent(), v->type)))
        fn(v, m);
}
HOOK(0x4F3D30, AnimTask_SaveState);

void AnimTask_LoadState(AnimTask *t, void **msg)
{
    Task *v = Task_FindById(t->target);
    char *buf = msg[0];
    void *m[2];
    TaskFn fn;

    m[0] = buf + ANIM_STATE_SIZE;
    m[1] = msg[1];
    memcpy(t->channels, buf, ANIM_STATE_SIZE);
    t->tick = (int)(intptr_t)msg[1];
    if (v && (fn = HANDLER(Vehicle_GetLoadEvent(), v->type)))
        fn(v, m);
}
HOOK(0x4F3DB0, AnimTask_LoadState);

void AnimTask_Close(void)
{
    TaskType_Free(gAnimTaskType);
    gAnimTaskType = 0x181;
    ParamType_Free(gAnimDataType);
    gAnimDataType = -1;
    QTaskList_Free(gAnimTaskParams);
    TaskEvent_Free(gVehicleControlEvent);
    TaskEvent_Free(gAnimEventDB);
    TaskEvent_Free(gVehicleInputInfoEvent);
    TaskEvent_Free(gVehicleStateSizeEvent);
    TaskEvent_Free(gVehicleSaveEvent);
    TaskEvent_Free(gVehicleLoadEvent);
    TaskEvent_Free(gVehicleKeysEvent);
    /* the keys event is not reset */
    gVehicleControlEvent = 0xFF;
    gAnimEventDB = 0xFF;
    gVehicleInputInfoEvent = 0xFF;
    gVehicleStateSizeEvent = 0xFF;
    gVehicleSaveEvent = 0xFF;
    gVehicleLoadEvent = 0xFF;
}
HOOK(0x4F3E40, AnimTask_Close);

void AnimTask_Step(AnimTask *t);

/* plays the keys up to tick; going back restarts from tick 0 */
void AnimTask_Seek(AnimTask *t, int tick)
{
    if (tick != 0 && tick >= t->tick) {
        while (tick > t->tick) {
            AnimTask_Step(t);
            t->tick++;
        }
        return;
    }
    memset(t->channels, 0, sizeof(t->channels));
    t->tick = 0;
    if (tick >= 0) {
        int n = tick + 1;

        do
            AnimTask_Step(t);
        while (--n);
    }
}
HOOK(0x4F3EF0, AnimTask_Seek);

void AnimTask_SendControl(AnimTask *t);

/* one tick: count down the wait, then apply keys until one sets a wait */
void AnimTask_Step(AnimTask *t)
{
    float *ch = t->channels;

    if ((int)round_ll(ch[ANIM_POS]) < t->keyCount) {
        if (fne(ch[ANIM_WAIT], kZero)) {
            long double w = ch[ANIM_WAIT] - kOne;

            ch[ANIM_WAIT] = w;
            if (!fne(w, kZero))
                ch[ANIM_POS] = ch[ANIM_POS] + kOne;
        }
        if (!fne(ch[ANIM_WAIT], kZero)) {
            do {
                int i = (int)round_ll(ch[ANIM_POS]);
                int c;
                long double v;

                if (i >= t->keyCount)
                    break;
                c = t->keys[i].channel;
                v = t->keys[i].value;
                if (c == ANIM_WAIT)
                    v = (int)round_ll(v);
                ch[c] = v;
                if (!fne(ch[ANIM_WAIT], kZero))
                    ch[ANIM_POS] = ch[ANIM_POS] + kOne;
            } while (!fne(ch[ANIM_WAIT], kZero));
        }
    }
    AnimTask_SendControl(t);
}
HOOK(0x4F3F60, AnimTask_Step);

void AnimTask_SendControl(AnimTask *t)
{
    Task *v = Task_FindById(t->target);
    ControlMsg m;
    TaskFn fn;

    if (!v)
        return;
    m.tick = t->tick;
    m.channels = t->channels;
    fn = HANDLER(Vehicle_GetControlEvent(), v->type);
    if (fn)
        fn(v, &m);
}
HOOK(0x4F4060, AnimTask_SendControl);

uint8_t Vehicle_GetControlEvent(void)
{
    return gVehicleControlEvent;
}
HOOK(0x4F40D0, Vehicle_GetControlEvent);

uint8_t Vehicle_GetInputInfoEvent(void)
{
    return gVehicleInputInfoEvent;
}
HOOK(0x4F40E0, Vehicle_GetInputInfoEvent);

int AnimData_GetType(void)
{
    return gAnimDataType;
}
HOOK(0x4F40F0, AnimData_GetType);

uint16_t AnimTask_GetType(void)
{
    return gAnimTaskType;
}
HOOK(0x4F4100, AnimTask_GetType);

uint8_t Vehicle_GetStateSizeEvent(void)
{
    return gVehicleStateSizeEvent;
}
HOOK(0x4F4110, Vehicle_GetStateSizeEvent);

uint8_t Vehicle_GetSaveEvent(void)
{
    return gVehicleSaveEvent;
}
HOOK(0x4F4120, Vehicle_GetSaveEvent);

uint8_t Vehicle_GetLoadEvent(void)
{
    return gVehicleLoadEvent;
}
HOOK(0x4F4130, Vehicle_GetLoadEvent);

uint8_t Vehicle_GetKeysEvent(void)
{
    return gVehicleKeysEvent;
}
HOOK(0x4F4140, Vehicle_GetKeysEvent);

void *AnimTask_GetList(void)
{
    return &gAnimTasks;
}
HOOK(0x4F4150, AnimTask_GetList);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E1D8) uint16_t gAnimTaskType INIT(385);
DATA(54E1D8) uint8_t gVehicleControlEvent INIT(255);
DATA(54E1D8) uint8_t gAnimEventDB INIT(255);
DATA(54E1D8) uint8_t gVehicleInputInfoEvent INIT(255);
DATA(54E1D8) uint8_t gVehicleStateSizeEvent INIT(255);
DATA(54E1D8) uint8_t gVehicleSaveEvent INIT(255);
DATA(54E1D8) uint8_t gVehicleLoadEvent INIT(255);
DATA(54E1D8) uint8_t gVehicleKeysEvent INIT(255);
PAD(igd, 54E1D8, 54E1E1, 0x3);
DATA(54E1D8) int gAnimDataType INIT(-1);
BSS(A76C20) int gAnimTaskUnused1;
PAD(igb, A76C20, A76C24, 0x4);
BSS(A76C20) List gAnimTasks;
BSS(A76C20) int gAnimTaskUnused2;
PAD(igb, A76C20, A76C38, 0x4);
BSS(A76C20) void *gAnimTaskParams;
