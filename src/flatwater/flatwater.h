#ifndef FLATWATER_FLATWATER_H
#define FLATWATER_FLATWATER_H

#include <stdint.h>

#include "igi.h"

/* the first 0x20 bytes of every task */
typedef struct Task {
    struct Task *next;
    struct Task *prev;
    struct Task *head;
    void *tail;
    void *tailpred;
    struct Task *parent;
    int refs;
    uint16_t type;
    char pooled;
    char unk1F;
} Task;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

typedef struct Vec3d {
    double x, y, z;
} Vec3d;

typedef struct VarString {
    char *text;
    char *expanded;
    void *expr;
    void *refs[16];
    int refCount;
} VarString;

_Static_assert(sizeof(VarString) == 0x50, "VarString");

typedef void (*TaskFn1)(void *task);
typedef void (*TaskFn2)(void *task, void *arg);

#define gTaskHandlers ((void *(*)[0x180])0xA96AE0)
#define gTaskStack ((Task **)0xAFA6E0)
#define gTaskStackTop VAR(int, 0xAFA7E0)
#define gWorld VAR(char *, 0xA44344)
#define gScriptEmptyString ((const char *)0x567C74)

#define TASK_HANDLER(slot, type) (gTaskHandlers[slot][type])
#define SUBLEVEL_RUN ((void *)0x488700)

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
#define gViewM ((float *)0xBCAAE0)
#define gViewScaleX VAR(float, 0xBCAB28)
#define gViewScaleY VAR(float, 0xBCAB2C)
#define gViewX VAR(float, 0xBCABB8)
#define gViewY VAR(float, 0xBCABBC)
#define gViewW VAR(float, 0xBCABC0)
#define gViewH VAR(float, 0xBCABC4)
#define gViewCX VAR(float, 0xBCABC8)
#define gViewCY VAR(float, 0xBCABCC)
#define gCamPos ((double *)0xBCAB08)

typedef struct QVertex {
    float x, y, z, u, v, a, r, g, b;
} QVertex;

typedef struct QVertexStrip {
    int untextured;
    unsigned int flags;
    int unk8;
    int count;
    QVertex *verts;
} QVertexStrip;

#define gQVertexTexture VAR(void *, 0xA84A50)
#define gQVertexStrip VAR(int, 0xA84A54)
#define gRenderFuncs ((void (**)(QVertexStrip *))0xA94E84)

#define kZero VAR(float, 0x5333EC)
#define kOne VAR(float, 0x5333E0)
#define kOne64 VAR(double, 0x5333F8)

ORIG(0x498000, void, View_ProjectPoint, (float *out, const float *in));

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
