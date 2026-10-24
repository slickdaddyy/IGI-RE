/* AnimScaleTask: an AnimTask that plays its keys at a variable speed,
   ten ticks per unit of speed. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "animcam/animcam.h"
#include "param/paramtypes.h"
#include "vehbase/vehbasetypes.h"

enum {
    ANIM_CHANNELS = 0x22,
    ANIM_WAIT = 0x20,
    ANIM_KEYS = 0x4000,
};

/* "AnimScaleTask" (0x20168 bytes), an AnimTask (0x20160) */
typedef struct AnimScaleTask {
    Task task;
    Vec3d pos;          /* 0x20 */
    Matrix matrix;      /* 0x38 */
    Node node;          /* 0x60 */
    struct AnimScaleTask *self; /* 0x68 */
    float channels[ANIM_CHANNELS]; /* 0x6C */
    AnimKey keys[ANIM_KEYS]; /* 0xF4 */
    char isRun;         /* 0x200F4 */
    char registered;    /* 0x200F5 */
    char unk200F6[2];
    VarString cond;     /* 0x200F8 */
    Task **ref;         /* 0x20148 */
    int target;         /* 0x2014C */
    int keyCount;       /* 0x20150 */
    int tick;           /* 0x20154 */
    char button[2];     /* 0x20158 */
    char unk2015A[6];
    float speed;        /* 0x20160 */
    float frac;         /* 0x20164, part of a tick left over */
} AnimScaleTask;

_Static_assert(offsetof(AnimScaleTask, isRun) == 0x200F4, "isRun");
_Static_assert(offsetof(AnimScaleTask, target) == 0x2014C, "target");
_Static_assert(sizeof(AnimScaleTask) == 0x20168, "AnimScaleTask");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4AF8F0, void, List_AddTail, (void *list, Node *node));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1320, void, VarString_Init, (VarString *v));
ORIG(0x4F16C0, char, VarString_HasExpr, (VarString *v));
ORIG(0x4F16E0, long double, VarString_EvalReal, (VarString *v, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F3EF0, void, AnimTask_Seek, (void *t, int tick));
ORIG(0x4F40F0, void *, AnimData_GetType, (void));
ORIG(0x4F4100, uint16_t, AnimTask_GetType, (void));
ORIG(0x4F4150, void *, AnimTask_GetList, (void));

extern uint16_t gAnimScaleTaskType;
extern void *gAnimScaleTaskParams;

static const float kTickStep = 0x1.99999ap-4f;

void AnimScaleTask_Create(AnimScaleTask *t);
void AnimScaleTask_Run(AnimScaleTask *t);

/* the table is never handed out: the GetParams handler is AnimTask's */
void AnimScaleTask_Open(void)
{
    AnimScaleTask t;
    uint16_t type = TaskType_New("AnimScaleTask", sizeof(AnimScaleTask), AnimTask_GetType(), 0, 0);

    gAnimScaleTaskType = type;
    TaskType_SetHandler(type, 0, AnimScaleTask_Create);
    TaskType_SetHandler2(gAnimScaleTaskType, 1, AnimScaleTask_Run);
    gAnimScaleTaskParams = ParamTable_New(10);
    ParamTable_Add(gAnimScaleTaskParams, &t, &t.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gAnimScaleTaskParams, &t, &t.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gAnimScaleTaskParams, &t, &t.target, Param_Int32(), 7, NULL);
    ParamTable_Add(gAnimScaleTaskParams, &t, &t.cond, Param_String256(), 7, NULL);
    ParamTable_Add(gAnimScaleTaskParams, &t, &t.isRun, Param_Bool8(), 7, NULL);
    ParamTable_Add(gAnimScaleTaskParams, &t, &t, AnimData_GetType(), 7, NULL);
}
HOOK(0x52AD10, AnimScaleTask_Open);

void AnimScaleTask_Create(AnimScaleTask *t)
{
    t->tick = -1;
    t->target = -1;
    t->pos.x = t->pos.y = t->pos.z = 0.0;
    t->matrix.m[0] = 1.0f;
    t->matrix.m[1] = t->matrix.m[2] = t->matrix.m[3] = 0.0f;
    t->matrix.m[4] = 1.0f;
    t->matrix.m[5] = t->matrix.m[6] = t->matrix.m[7] = 0.0f;
    t->matrix.m[8] = 1.0f;
    t->matrix.count = 0;
    t->keyCount = 1;
    t->keys[0].channel = ANIM_WAIT;
    t->keys[0].value = 0.0f;
    t->isRun = 1;
    t->registered = 0;
    t->ref = NULL;
    VarString_Init(&t->cond);
    memset(t->channels, 0, sizeof(t->channels));
    t->node.prev = NULL;
    t->node.next = NULL;
    List_AddTail(AnimTask_GetList(), &t->node);
    t->self = t;
    t->speed = 1.0f;
    t->frac = 0.0f;
}
HOOK(0x52AE50, AnimScaleTask_Create);

void AnimScaleTask_Run(AnimScaleTask *t)
{
    long double f;
    int n = 0;

    if (VarString_HasExpr(&t->cond))
        t->isRun = (char)round_ll(VarString_EvalReal(&t->cond, t));
    if (!t->isRun)
        return;
    f = (long double)t->speed + t->frac;
    t->frac = f;
    while (f >= kTickStep) {
        f = (long double)t->frac - kTickStep;
        n++;
        t->frac = f;
    }
    AnimTask_Seek(t, t->tick + n);
}
HOOK(0x52AF10, AnimScaleTask_Run);

void AnimScaleTask_Close(void)
{
    QTaskList_Free(gAnimScaleTaskParams);
    TaskType_Free(gAnimScaleTaskType);
    gAnimScaleTaskType = 0x181;
}
HOOK(0x52AFB0, AnimScaleTask_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54F68C) uint16_t gAnimScaleTaskType INIT(385);
PAD(igd, 54F68C, 54F68E, 0x2);
BSS(A84394) void *gAnimScaleTaskParams;
