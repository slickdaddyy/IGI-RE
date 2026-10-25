/* AreaActivate: a box that counts, each frame, the things that match its
   criteria mask ("<name>.nActive" in scripts). Criteria are registered by
   other modules as script constants holding one bit each. */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "editobj/editobj.h"
#include "task/taskvars.h"

/* "AreaActivate" (0xB8), an EditorRigidObj */
typedef struct AreaActivate {
    Task task;
    Vec3d min;          /* world bounding box */
    Vec3d max;
    Vec3d pos;
    Matrix matrix;
    Vec3f half;
    Vec3f size;
    int nActive;
    unsigned int mask;
    char *criteria;     /* script expression giving the mask */
    char unkB4;
    char registered;    /* "<name>.nActive" is a script symbol */
    char unkB6[2];
} AreaActivate;

typedef struct CriteriaArg {
    AreaActivate *area;
    int count;
} CriteriaArg;

typedef struct Criteria {
    char used;
    char pad[3];
    const char *name;
    void (*fn)(CriteriaArg *arg);
} Criteria;

_Static_assert(offsetof(AreaActivate, half) == 0x90, "half");
_Static_assert(sizeof(AreaActivate) == 0xB8, "AreaActivate");

ORIG(0x4AF7B0, void, Error_Show, (const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x4018C0, void, TaskEvent_Free, (uint8_t index));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B0D10, void, Mem_Free, (void *p));
ORIG(0x4B7E80, void, Script_RegisterInt, (const char *name, int *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8770, int, Script_EvalExpr, (const char *text));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4B8920, void, Script_UnregisterSymbol, (const char *name));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F60, void *, Param_Real32x3, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

uint16_t EditorRigidObj_GetType(void);

#define AREA_CRITERIA_COUNT 31

extern uint16_t gAreaActivateType;
extern uint8_t gAreaActivateEvent;
extern Criteria gAreaCriteria[];
extern void *gAreaActivateParams;

static const float kHalf = 0.5f;
static const double kNoMax = -0x1.fffffffffffffp+1023;

/* Is pos inside the box */
char AreaActivate_Contains(AreaActivate *a, Vec3d *pos)
{
    const float *m = a->matrix.m;
    long double dx = pos->x - a->pos.x;
    long double dy = pos->y - a->pos.y;
    long double dz = pos->z - a->pos.z;
    float x = dz * m[6] + dy * m[3] + dx * m[0];
    float y = dz * m[7] + dy * m[4] + dx * m[1];
    float z = dz * m[8] + dy * m[5] + dx * m[2];

    if (x > a->half.x)
        return 0;
    if (y > a->half.y)
        return 0;
    if (z > a->half.z)
        return 0;
    if (-a->half.x > x)
        return 0;
    if (-a->half.y > y)
        return 0;
    if (-a->half.z > z)
        return 0;
    return 1;
}
HOOK(0x5042B0, AreaActivate_Contains);

void AreaActivate_Create(AreaActivate *a);
void AreaActivate_Delete(AreaActivate *a);
void AreaActivate_Changed(AreaActivate *a);
void AreaActivate_GetParams(void *task, void **out);
void AreaActivate_Run(AreaActivate *a);
void AreaActivate_Activate(AreaActivate *a);
void AreaActivate_Deactivate(AreaActivate *a);

void AreaActivate_Open(void)
{
    AreaActivate a;

    gAreaActivateType = TaskType_New("AreaActivate", sizeof(AreaActivate), EditorRigidObj_GetType(), 0, 0);
    gAreaActivateEvent = TaskEvent_Alloc(0);
    TaskType_SetHandler(gAreaActivateType, 0, AreaActivate_Create);
    TaskType_SetHandler2(gAreaActivateType, 2, AreaActivate_Delete);
    TaskType_SetHandler(gAreaActivateType, Param_GetChangedEvent(), AreaActivate_Changed);
    TaskType_SetHandler(gAreaActivateType, Param_GetTableEvent(), AreaActivate_GetParams);
    TaskType_SetHandler2(gAreaActivateType, 1, AreaActivate_Run);
    TaskType_SetHandler2(gAreaActivateType, Level_GetActivateEvent(), AreaActivate_Activate);
    TaskType_SetHandler2(gAreaActivateType, Level_GetDeactivateEvent(), AreaActivate_Deactivate);
    gAreaActivateParams = ParamTable_New(4);
    ParamTable_Add(gAreaActivateParams, &a, &a.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gAreaActivateParams, &a, &a.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gAreaActivateParams, &a, &a.size, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gAreaActivateParams, &a, &a.criteria, Param_VarString(), 7, NULL);
}
HOOK(0x5043B0, AreaActivate_Open);

/* Makes "<name>.nActive" a script variable */
void AreaActivate_Activate(AreaActivate *a)
{
    char name[64];
    char text[128];

    if (a->registered)
        return;
    a->registered = 1;
    Task_GetName(name, a);
    crt_sprintf(text, "%s.nActive", name);
    Script_RegisterInt(text, &a->nActive);
}
HOOK(0x504530, AreaActivate_Activate);

void AreaActivate_Deactivate(AreaActivate *a)
{
    char name[64];
    char text[128];

    if (!a->registered)
        return;
    Task_GetName(name, a);
    a->registered = 0;
    crt_sprintf(text, "%s.nActive", name);
    Script_RemoveSymbol(text);
}
HOOK(0x504590, AreaActivate_Deactivate);

/* Reads the criteria mask and makes the world bounding box of the eight corners */
void AreaActivate_Changed(AreaActivate *a)
{
    const float *m = a->matrix.m;
    Vec3d lo = {0x1.fffffffffffffp+1023, 0x1.fffffffffffffp+1023, 0x1.fffffffffffffp+1023};
    double hix = -0x1.fffffffffffffp+1023, hiy = -0x1.fffffffffffffp+1023, hiz = kNoMax;
    float hx, hy, hz;
    int i;

    a->mask = 0;
    if (a->criteria)
        a->mask = Script_EvalExpr(a->criteria);
    a->half = a->size;
    hx = a->half.x * kHalf;
    a->half.x = hx;
    hy = a->half.y * kHalf;
    a->half.y = hy;
    hz = a->half.z * kHalf;
    a->half.z = hz;
    for (i = 0; i < 8; i++) {
        long double x = (long double)(1 - (i & 1) * 2) * hx;
        double y = (1 - (i & 2)) * (double)hy;
        double z = (1 - ((i >> 1) & 2)) * (double)hz;
        double px = m[1] * y + m[2] * z + m[0] * x;
        double py = m[4] * y + m[5] * z + m[3] * x;
        double pz = m[7] * y + m[8] * z + m[6] * x;
        long double wx = px + a->pos.x;
        long double wy = py + a->pos.y;
        double wyd = wy;
        double wz = pz + a->pos.z;

        if (lo.x >= wx)
            lo.x = wx;
        if (lo.y >= wy)
            lo.y = wy;
        if (lo.z >= wz)
            lo.z = wz;
        if (!(hix > wx))
            hix = wx;
        if (!(hiy > wyd))
            hiy = wyd;
        if (!(hiz > wz))
            hiz = wz;
    }
    a->min = lo;
    a->max = (Vec3d){hix, hiy, hiz};
}
HOOK(0x5045F0, AreaActivate_Changed);

void AreaActivate_GetParams(void *task, void **out)
{
    *out = gAreaActivateParams;
}
HOOK(0x504890, AreaActivate_GetParams);

/* Counts the matches of every criteria in the mask */
void AreaActivate_Run(AreaActivate *a)
{
    CriteriaArg arg;
    int i;

    a->nActive = 0;
    arg.area = a;
    arg.count = 0;
    for (i = 0; i < AREA_CRITERIA_COUNT; i++) {
        if (a->mask & 1 << i) {
            gAreaCriteria[i].fn(&arg);
            a->nActive += arg.count;
            arg.count = 0;
        }
    }
    CALL_CHILDREN(&a->task, 1);
}
HOOK(0x5048A0, AreaActivate_Run);

void AreaActivate_Delete(AreaActivate *a)
{
    if (Task_GetId(a) >= 0)
        ((void (*)(void *))gTaskHandlers[Level_GetDeactivateEvent()][a->task.type])(a);
    if (a->criteria) {
        Mem_Free(a->criteria);
        a->criteria = NULL;
    }
}
HOOK(0x504970, AreaActivate_Delete);

void AreaActivate_Create(AreaActivate *a)
{
    a->registered = 0;
    a->nActive = 0;
    a->mask = 0;
    a->criteria = NULL;
}
HOOK(0x5049D0, AreaActivate_Create);

void AreaActivate_Close(void)
{
    QTaskList_Free(gAreaActivateParams);
    TaskEvent_Free(gAreaActivateEvent);
    gAreaActivateEvent = 0xFF;
    TaskType_Free(gAreaActivateType);
    gAreaActivateType = 0x181;
}
HOOK(0x5049F0, AreaActivate_Close);

/* Gives fn the next free bit and registers name as a script constant for it */
int AreaActivate_AddCriteria(const char *name, void (*fn)(CriteriaArg *arg))
{
    int i, bit;

    for (i = 0; i < AREA_CRITERIA_COUNT; i++) {
        if (!gAreaCriteria[i].used) {
            gAreaCriteria[i].used = 1;
            gAreaCriteria[i].name = name;
            gAreaCriteria[i].fn = fn;
            bit = 1 << i;
            Script_RegisterConstant(name, bit);
            return bit;
        }
    }
    Error_Show("Unable to register AreaActivate criteria");
    for (;;)
        ;
}
HOOK(0x504A30, AreaActivate_AddCriteria);

void AreaActivate_RemoveCriteria(int bit)
{
    int i = 0;

    for (; !(bit & 1); bit >>= 1)
        i++;
    gAreaCriteria[i].used = 0;
    Script_UnregisterSymbol(gAreaCriteria[i].name);
}
HOOK(0x504A90, AreaActivate_RemoveCriteria);

unsigned int AreaActivate_GetMask(AreaActivate *a)
{
    return a->mask;
}
HOOK(0x504AC0, AreaActivate_GetMask);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54E69C) uint16_t gAreaActivateType INIT(385);
DATA(54E69C) uint8_t gAreaActivateEvent INIT(255);
PAD(igd, 54E69C, 54E69F, 0x1);
STORE(igb, A7A640, 4) __typeof__(gAreaCriteria[0]) store_A7A640[31];
ALIAS(gAreaCriteria, store_A7A640, 0);
BSS(A7A640) void *gAreaActivateParams;
