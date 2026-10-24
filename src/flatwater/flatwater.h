#ifndef FLATWATER_FLATWATER_H
#define FLATWATER_FLATWATER_H

#include <stdint.h>

#include "igi.h"
#include "param/paramtypes.h"
#include "qvertex/qvertextypes.h"
#include "transctx/transctxtypes.h"

FN_EXTERN(SubLevel_Run);

typedef void (*TaskFn1)(void *task);
typedef void (*TaskFn2)(void *task, void *arg);

extern void * gTaskHandlers[][0x180];
extern Task *gTaskStack[];
extern int gTaskStackTop;
extern char *gCurrentWorld;
extern const char gEmptyString[];

#define TASK_HANDLER(slot, type) (gTaskHandlers[slot][type])
#define SUBLEVEL_RUN (FN(0x488700, SubLevel_Run))

/* next sibling to visit after c, NULL at the end of the list */
static inline Task *next_sibling(Task *c)
{
    return c->next && c->next->next ? c->next : NULL;
}

/* calls handler SLOT on each child of a task (not deeper), keeping the next
   child on gTaskStack so handlers may delete tasks; SLOT is read per child */
#define BROADCAST(first, SLOT, ...)                                     \
    do {                                                                \
        Task *c_ = (first);                                             \
        if (!c_->next || !c_)                                           \
            break;                                                      \
        do {                                                            \
            void *fn_;                                                  \
            gTaskStack[gTaskStackTop++] = next_sibling(c_);             \
            fn_ = TASK_HANDLER(SLOT, c_->type);                         \
            if (fn_)                                                    \
                ((void (*)())fn_)(c_, ##__VA_ARGS__);                   \
            c_ = gTaskStack[--gTaskStackTop];                           \
        } while (c_);                                                   \
    } while (0)

/* fistp with the current rounding mode, low 32 bits */
static inline int x87_fistp(long double x)
{
    long long r;

    __asm__("fistpll %0" : "=m"(r) : "t"(x) : "st");
    return (int)r;
}

/* x87 "not equal" as tested with C3: false when unordered */
static inline int fne(long double a, long double b)
{
    return a < b || a > b;
}

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401AE0, Task **, TaskRef_Add, (Task *task));
ORIG(0x401B20, void, TaskRef_Remove, (Task **ref));
ORIG(0x401BE0, Task *, Task_FindChild, (void *task, uint16_t type));
ORIG(0x401C40, Task *, Task_FindNext, (void *task, uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4A5BB0, char *, crt_strstr, (const char *s, const char *find));
ORIG(0x4B7FC0, void, Script_RegisterReal, (const char *name, double *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4F0F20, void *, Param_Angle, (void));
ORIG(0x4F0F30, void *, Param_Degrees, (void));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0F80, void *, Param_Real64, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F0FE0, void *, Param_String32, (void));
ORIG(0x4F0FF0, void *, Param_String256, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
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
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, const void *range));

/* the current view: rotation rows, screen rectangle and projection scale */
extern TransContext gTransContext;
extern Scene gScene;

extern void *gQVertexTexture;
extern int gQVertexStrip;
extern void (*gRenderFuncs[])(QVertexStrip *);

extern float kZero;
extern float kOne;
extern double kOne64;

ORIG(0x498000, void, TransContext_ProjectView, (float *out, const float *in));

/* "FlatWaterLayer" and "FlatSkyLayer", 0x26C bytes: a scrolling texture */
typedef struct FlatLayer {
    Task task;
    char texture[0x24]; /* 0x20 */
    int loaded;         /* 0x44 */
    char lastName[256]; /* 0x48 */
    char name[256];     /* 0x148 */
    float scale;        /* 0x248, texture coordinates per world unit */
    float speedU;       /* 0x24C */
    float speedV;
    float offsetU;      /* 0x254, kept in 0..1 */
    float offsetV;
    float alpha;        /* 0x25C */
    float color[3];     /* 0x260 */
} FlatLayer;

_Static_assert(sizeof(FlatLayer) == 0x26C, "FlatLayer");

void FlatLayer_Run(FlatLayer *l);
void FlatLayer_Create(FlatLayer *l);
void FlatLayer_ParamsChanged(FlatLayer *l);
void FlatLayer_Delete(FlatLayer *l);

#endif
