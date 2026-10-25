/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <math.h>
#include <string.h>

#include "hook.h"
#include "physobj/physobj.h"
#include "level/leveltypes.h"

FN_EXTERN(CameraBase_Create);
FN_EXTERN(CameraBase_Delete);
FN_EXTERN(Dynamic_Create);
FN_EXTERN(Dynamic_Delete);
FN_EXTERN(Dynamic_Run);
FN_EXTERN(EditorMagicObj_GetArgSize);
FN_EXTERN(GuardGenerator_WorldAdd);
FN_EXTERN(GuardGenerator_WorldRemove);
FN_EXTERN(HitZone_GetArgSize);
FN_EXTERN(Idle_Create);
FN_EXTERN(Idle_Delete);
FN_EXTERN(Level_Create);
FN_EXTERN(Level_Delete);
FN_EXTERN(Level_NewTask);
FN_EXTERN(Level_Run);
FN_EXTERN(ParamBool8_Copy);
FN_EXTERN(ParamBool8_Format);
FN_EXTERN(ParamBool8_Parse);
FN_EXTERN(ParamBool8_Paste);
FN_EXTERN(ParamExpression_Format);
FN_EXTERN(ParamExpression_Parse);
FN_EXTERN(ParamInt16_Copy);
FN_EXTERN(ParamInt16_Paste);
FN_EXTERN(ParamInt16_Size);
FN_EXTERN(ParamString16_Parse);
FN_EXTERN(ParamString256_Parse);
FN_EXTERN(ParamString32_Parse);
FN_EXTERN(ParamString_Copy);
FN_EXTERN(ParamString_Format);
FN_EXTERN(ParamString_Paste);
FN_EXTERN(ParamString_Size);
FN_EXTERN(ParamVarString_Copy);
FN_EXTERN(ParamVarString_Parse);
FN_EXTERN(ParamVarString_Paste);
FN_EXTERN(ParamVarString_Size);
FN_EXTERN(RocketPod_GetArgSize);
FN_EXTERN(Static_Create);
FN_EXTERN(Static_Delete);
FN_EXTERN(SubLevel_Run);
FN_EXTERN(Task_DeclareParameters);
FN_EXTERN(Task_ReturnZero);

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x4017C0, uint8_t, TaskEvent_Alloc, (uint16_t type));
ORIG(0x401810, int, TaskEvent_Alloc2, (uint16_t type));
ORIG(0x4018F0, uint16_t, Task_GetRootType, (void));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B38E0, void, Matrix_SetEuler, (float *m, float ax, float ay, float az));
ORIG(0x4B45A0, void, Quat_SetEuler, (float *q, float ax, float ay, float az));
ORIG(0x4B8890, void, Script_RegisterFunction, (const char *name, void *fn, void *data, int argc));
ORIG(0x4B8A20, int, Script_GetArgInt, (void *args, int i));
ORIG(0x4B8A50, long double, Script_GetArgReal, (void *args, int i));
ORIG(0x4C1780, void, Debug_Log, (void *a, void *b));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4F1180, const char *, TaskNote_GetText, (void *field));
ORIG(0x4F19C0, int, ParamType_New, (const char *name, int parent));
ORIG(0x4F1B60, uint8_t, ParamOp_Parse, (void));
ORIG(0x4F1B70, uint8_t, ParamOp_ArgCount, (void));
ORIG(0x4F1B80, uint8_t, ParamOp_Format, (void));
ORIG(0x4F1B90, uint8_t, ParamOp_Size, (void));
ORIG(0x4F1BA0, uint8_t, ParamOp_Copy, (void));
ORIG(0x4F1BB0, uint8_t, ParamOp_Paste, (void));
ORIG(0x4F1BC0, void, ParamType_SetOp, (int type, uint8_t op, void *fn));
ORIG(0x4F1C20, void, ParamCursor_Need, (ParamCursor *c, int count));

/* task types and events of the level */
extern uint16_t gLevelType;
extern uint16_t gStaticType;
extern uint16_t gDynamicType;
extern uint16_t gIdleType;
extern uint16_t gCameraBaseType;
extern uint8_t gLevelDebugInfoEvent;
extern uint8_t gLevelNameEvent;
extern int gLevelActivateEvent;
extern int gLevelDeactivateEvent;
extern int gLevelUnusedNewTaskEvent;

/* LevelQTaskID -> task reference */
extern void *gLevelTaskIds[];
/* 251 buckets of task references, Amiga style lists */
extern List gTaskNotes[LEVEL_NOTE_LISTS];
extern char gLevelPaused;
extern char gLevelForceRunning;
extern int gLevelUnused0;
extern int gLevelUnused1;

/* parameter type ids */
extern int gParamQuat32;
extern int gParamReal32;
extern int gParamReal32x3;
extern int gParamReal32x9;
extern int gParamReal64;
extern int gParamReal64x3;
extern int gParamInt32;
extern int gParamInt16;
extern int gParamVarString;
extern int gParamString16;
extern int gParamString32;
extern int gParamString256;
extern int gParamLevelExpression;
extern int gParamQTaskNote;
extern int gParamBool8;
extern int gParamGraph;
extern int gParamGraphEnvelope;
extern int gParamScriptFile;
extern int gParamBitfield;
extern int gParamPitchGraph;
extern int gParamRGB;
extern int gParamAngle;
extern int gParamDegrees;
extern int gParamObjectPos;
extern int gParamPushButton;
extern int gParamDropDownCombo;

static const double kTwoPi = 0x1.921fb54442d1bp+2;
static const double kGimbal = 0x1.a36e2eb1c432dp-14;   /* 0.0001 */

void ParamQuat32_Parse(float *q, ParamCursor *c);
void ParamQuat32_Format(float *q, ParamText *t);
void ParamQuat32_Copy(uint32_t *q, uint32_t **dst);
void ParamQuat32_Paste(uint32_t *q, uint32_t **src);
void ParamQuat32_Size(void *q, int *size);
void ParamReal32_Parse(float *v, ParamCursor *c);
void ParamReal32_Format(float *v, ParamText *t);
void Param32_Copy(uint32_t *v, uint32_t **dst);
void Param32_Paste(uint32_t *v, uint32_t **src);
void Param_One(void *v, int *n);
void Param_Three(void *v, int *n);
void ParamReal32x3_Parse(float *v, ParamCursor *c);
void ParamReal32x3_Format(float *v, ParamText *t);
void ParamReal32x3_Copy(uint32_t *v, uint32_t **dst);
void ParamReal32x3_Paste(uint32_t *v, uint32_t **src);
void ParamReal32x9_Parse(float *m, ParamCursor *c);
void ParamReal32x9_Format(float *m, ParamText *t);
void ParamReal32x9_Copy(uint32_t *m, uint32_t **dst);
void ParamReal32x9_Paste(uint32_t *m, uint32_t **src);
void ParamReal32x9_Size(void *m, int *size);
void ParamReal64_Parse(double *v, ParamCursor *c);
void ParamReal64_Format(uint32_t *v, ParamText *t);
void ParamReal64_Copy(uint32_t *v, uint32_t **dst);
void ParamReal64_Paste(uint32_t *v, uint32_t **src);
void ParamReal64x3_Parse(double *v, ParamCursor *c);
void ParamReal64x3_Format(uint32_t *v, ParamText *t);
void ParamReal64x3_Copy(uint32_t *v, uint32_t **dst);
void ParamReal64x3_Paste(uint32_t *v, uint32_t **src);
void ParamReal64x3_Size(void *v, int *size);
void ParamInt32_Parse(int *v, ParamCursor *c);
void ParamInt32_Format(int *v, ParamText *t);
void ParamInt16_Parse(int16_t *v, ParamCursor *c);
void ParamInt16_Format(int16_t *v, ParamText *t);
void ParamQTaskNote_Copy(void *v, char **dst);
void ParamQTaskNote_Paste(void *v, char **src);
void ParamQTaskNote_Size(void *v, int *size);
void ParamGraphEnvelope_Parse(float *v, ParamCursor *c);
void ParamGraphEnvelope_ArgCount(void *v, int *n);
void ParamGraphEnvelope_Format(float *v, ParamText *t);
void ParamGraphEnvelope_Copy(uint32_t *v, uint32_t **dst);
void ParamGraphEnvelope_Paste(uint32_t *v, uint32_t **src);
void ParamGraphEnvelope_Size(void *v, int *size);


/* parse, format, copy, paste, size, argument count; NULL is not set */
static const struct {
    const char *name;
    int *id;
    void *op[6];
} kParamTypes[] = {
    {"Quat32", &gParamQuat32, {ParamQuat32_Parse, ParamQuat32_Format, ParamQuat32_Copy, ParamQuat32_Paste,
                               ParamQuat32_Size, Param_Three}},
    {"Real32", &gParamReal32, {ParamReal32_Parse, ParamReal32_Format, Param32_Copy, Param32_Paste,
                               FN(0x427D70, RocketPod_GetArgSize), Param_One}},
    {"Real32x3", &gParamReal32x3, {ParamReal32x3_Parse, ParamReal32x3_Format, ParamReal32x3_Copy,
                                   ParamReal32x3_Paste, FN(0x51BAA0, EditorMagicObj_GetArgSize), Param_Three}},
    {"Real32x9", &gParamReal32x9, {ParamReal32x9_Parse, ParamReal32x9_Format, ParamReal32x9_Copy,
                                   ParamReal32x9_Paste, ParamReal32x9_Size, Param_Three}},
    {"Real64", &gParamReal64, {ParamReal64_Parse, ParamReal64_Format, ParamReal64_Copy, ParamReal64_Paste,
                               FN(0x427F80, HitZone_GetArgSize), Param_One}},
    {"Real64x3", &gParamReal64x3, {ParamReal64x3_Parse, ParamReal64x3_Format, ParamReal64x3_Copy,
                                   ParamReal64x3_Paste, ParamReal64x3_Size, Param_Three}},
    {"Int32", &gParamInt32, {ParamInt32_Parse, ParamInt32_Format, Param32_Copy, Param32_Paste, FN(0x427D70, RocketPod_GetArgSize),
                             Param_One}},
    {"Int16", &gParamInt16, {ParamInt16_Parse, ParamInt16_Format, FN(0x4F0170, ParamInt16_Copy), FN(0x4F0190, ParamInt16_Paste), FN(0x4F01B0, ParamInt16_Size),
                             Param_One}},
    {"VarString", &gParamVarString, {FN(0x4F01C0, ParamVarString_Parse), TASK_NOTHING, FN(0x4F0270, ParamVarString_Copy), FN(0x4F02B0, ParamVarString_Paste), FN(0x4F0320, ParamVarString_Size),
                                     Param_One}},
    {"String16", &gParamString16, {FN(0x4F0350, ParamString16_Parse), FN(0x4F0430, ParamString_Format), FN(0x4F04D0, ParamString_Copy), FN(0x4F03D0, ParamString_Paste), FN(0x4F0380, ParamString_Size), Param_One}},
    {"String32", &gParamString32, {FN(0x4F03A0, ParamString32_Parse), FN(0x4F0430, ParamString_Format), FN(0x4F04D0, ParamString_Copy), FN(0x4F03D0, ParamString_Paste), FN(0x4F0380, ParamString_Size), Param_One}},
    {"String256", &gParamString256, {FN(0x4F0400, ParamString256_Parse), FN(0x4F0430, ParamString_Format), FN(0x4F04D0, ParamString_Copy), FN(0x4F03D0, ParamString_Paste), FN(0x4F0380, ParamString_Size),
                                     Param_One}},
    {"LevelExpression", &gParamLevelExpression, {FN(0x4F0460, ParamExpression_Parse), FN(0x4F04A0, ParamExpression_Format), FN(0x4F04D0, ParamString_Copy), FN(0x4F03D0, ParamString_Paste),
                                                 FN(0x4F0380, ParamString_Size), Param_One}},
    {"QTaskNote", &gParamQTaskNote, {NULL, NULL, ParamQTaskNote_Copy, ParamQTaskNote_Paste,
                                     ParamQTaskNote_Size, Param_One}},
    {"bool8", &gParamBool8, {FN(0x4F0500, ParamBool8_Parse), FN(0x4F0530, ParamBool8_Format), FN(0x4F0560, ParamBool8_Copy), FN(0x4F0570, ParamBool8_Paste), Param_One, Param_One}},
    {"Graph", &gParamGraph, {TASK_NOTHING, TASK_NOTHING, TASK_NOTHING, TASK_NOTHING, FN(0x4F64E0, Task_ReturnZero),
                             TASK_NOTHING}},
    {"GraphEnvelope", &gParamGraphEnvelope, {ParamGraphEnvelope_Parse, ParamGraphEnvelope_Format,
                                             ParamGraphEnvelope_Copy, ParamGraphEnvelope_Paste,
                                             ParamGraphEnvelope_Size, ParamGraphEnvelope_ArgCount}},
};

/* types that share the operations of another */
static const struct {
    const char *name;
    int *id;
    int *parent;
} kParamAliases[] = {
    {"ScriptFile", &gParamScriptFile, &gParamString256},
    {"Bitfield", &gParamBitfield, &gParamInt32},
    {"PitchGraph", &gParamPitchGraph, &gParamGraphEnvelope},
    {"RGB", &gParamRGB, &gParamReal32x3},
    {"Angle", &gParamAngle, &gParamReal32},
    {"Degrees", &gParamDegrees, &gParamReal32},
    {"ObjectPos", &gParamObjectPos, &gParamReal64x3},
    {"PushButton", &gParamPushButton, &gParamBool8},
    {"DropDownCombo", &gParamDropDownCombo, &gParamVarString},
};

static uint8_t param_op(int i)
{
    switch (i) {
    case 0: return ParamOp_Parse();
    case 1: return ParamOp_Format();
    case 2: return ParamOp_Copy();
    case 3: return ParamOp_Paste();
    case 4: return ParamOp_Size();
    default: return ParamOp_ArgCount();
    }
}

static void new_type(const char *name, int size, uint16_t *out)
{
    *out = TaskType_New(name, size, Task_GetRootType(), 0, 0);
}

/* level task types, parameter types and the task id tables */
void Level_Open(void)
{
    unsigned i, k;
    List *l;

    memset(gLevelTaskIds, 0, 0x1000 * sizeof(void *));
    new_type("Level", 0x38, &gLevelType);
    new_type("Static", 0x20, &gStaticType);
    new_type("Dynamic", 0x20, &gDynamicType);
    new_type("Idle", 0x20, &gIdleType);
    new_type("CameraBase", 0x20, &gCameraBaseType);
    gLevelDebugInfoEvent = TaskEvent_Alloc(0);
    gLevelNameEvent = TaskEvent_Alloc(0);
    gLevelActivateEvent = TaskEvent_Alloc2(0);
    gLevelDeactivateEvent = TaskEvent_Alloc2(0);
    gLevelUnusedNewTaskEvent = TaskEvent_Alloc2(0);
    TaskType_SetHandler2(gLevelType, 1, FN(0x4F0580, Level_Run));
    TaskType_SetHandler(gLevelType, 0, FN(0x4F0600, Level_Create));
    TaskType_SetHandler2(gLevelType, 2, FN(0x4F0610, Level_Delete));
    TaskType_SetHandler2(gStaticType, 1, TASK_NOTHING);
    TaskType_SetHandler(gStaticType, 0, FN(0x4F0620, Static_Create));
    TaskType_SetHandler2(gStaticType, 2, FN(0x4F0640, Static_Delete));
    TaskType_SetHandler(gStaticType, World_GetEvent1(), FN(0x487E20, GuardGenerator_WorldRemove));
    TaskType_SetHandler(gStaticType, World_GetEvent0(), FN(0x487D90, GuardGenerator_WorldAdd));
    TaskType_SetHandler2(gDynamicType, 1, FN(0x4F0660, Dynamic_Run));
    TaskType_SetHandler(gDynamicType, 0, FN(0x4F06E0, Dynamic_Create));
    TaskType_SetHandler2(gDynamicType, 2, FN(0x4F06F0, Dynamic_Delete));
    TaskType_SetHandler2(gIdleType, 1, TASK_NOTHING);
    TaskType_SetHandler(gIdleType, 0, FN(0x4F0700, Idle_Create));
    TaskType_SetHandler2(gIdleType, 2, FN(0x4F0710, Idle_Delete));
    TaskType_SetHandler2(gCameraBaseType, 1, FN(0x488700, SubLevel_Run));
    TaskType_SetHandler(gCameraBaseType, 0, FN(0x4F0720, CameraBase_Create));
    TaskType_SetHandler2(gCameraBaseType, 2, FN(0x4F0730, CameraBase_Delete));
    for (i = 0; i < sizeof(kParamTypes) / sizeof(kParamTypes[0]); i++) {
        *kParamTypes[i].id = ParamType_New(kParamTypes[i].name, -1);
        for (k = 0; k < 6; k++)
            if (kParamTypes[i].op[k])
                ParamType_SetOp(*kParamTypes[i].id, param_op(k), kParamTypes[i].op[k]);
    }
    for (i = 0; i < sizeof(kParamAliases) / sizeof(kParamAliases[0]); i++)
        *kParamAliases[i].id = ParamType_New(kParamAliases[i].name, *kParamAliases[i].parent);
    Script_RegisterFunction("Task_New", FN(0x4F0780, Level_NewTask), NULL, -1);
    Script_RegisterFunction("Task_DeclareParameters", FN(0x4F0740, Task_DeclareParameters), NULL, -1);
    gLevelPaused = 0;
    gLevelForceRunning = 0;
    for (l = gTaskNotes; l < gTaskNotes + LEVEL_NOTE_LISTS; l++) {
        l->head = (Node *)&l->tail;
        l->tailpred = (Node *)l;
        l->tail = NULL;
    }
    gLevelUnused0 = 0;
    gLevelUnused1 = 0;
}
HOOK(0x4EE970, Level_Open);

/* QTaskNote: a reference to a task, saved by name */
void ParamQTaskNote_Copy(void *v, char **dst)
{
    strcpy(*dst, TaskNote_GetText(v));
}
HOOK(0x4EF7F0, ParamQTaskNote_Copy);

void ParamQTaskNote_Paste(void *v, char **src)
{
    Debug_Log(v, *src);
}
HOOK(0x4EF830, ParamQTaskNote_Paste);

void ParamQTaskNote_Size(void *v, int *size)
{
    *size = 0x41;
}
HOOK(0x4EF850, ParamQTaskNote_Size);

void ParamGraphEnvelope_Parse(float *v, ParamCursor *c)
{
    int base, i;

    ParamCursor_Need(c, 11);
    base = c->index;
    for (i = 0; i < 11; i++)
        v[i] = Script_GetArgReal(c->args, base + i);
    c->index += 11;
}
HOOK(0x4EF860, ParamGraphEnvelope_Parse);

void ParamGraphEnvelope_ArgCount(void *v, int *n)
{
    *n = 11;
}
HOOK(0x4EF8B0, ParamGraphEnvelope_ArgCount);

static char *text_end(char *p)
{
    while (*p)
        p++;
    return p;
}

static void append(char *dst, const char *src)
{
    dst = text_end(dst);
    while ((*dst++ = *src++))
        ;
}

void ParamGraphEnvelope_Format(float *v, ParamText *t)
{
    char *end = text_end(t->buf);
    char num[20];
    int i;

    for (i = 0; i < 11; i++) {
        crt_sprintf(num, "%f", (double)v[i]);
        append(end, num);
        if (i != 10)
            append(end, ", ");
    }
}
HOOK(0x4EF8C0, ParamGraphEnvelope_Format);

void ParamGraphEnvelope_Copy(uint32_t *v, uint32_t **dst)
{
    memcpy(*dst, v, 11 * 4);
}
HOOK(0x4EF990, ParamGraphEnvelope_Copy);

void ParamGraphEnvelope_Paste(uint32_t *v, uint32_t **src)
{
    memcpy(v, *src, 11 * 4);
}
HOOK(0x4EF9B0, ParamGraphEnvelope_Paste);

void ParamGraphEnvelope_Size(void *v, int *size)
{
    *size = 11 * 4;
}
HOOK(0x4EF9D0, ParamGraphEnvelope_Size);

void ParamReal32_Parse(float *v, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    *v = Script_GetArgReal(c->args, c->index);
    c->index++;
}
HOOK(0x4EF9E0, ParamReal32_Parse);

void Param_One(void *v, int *n)
{
    *n = 1;
}
HOOK(0x4EFA10, Param_One);

/* prints over the terminator, then appends a buffer it never filled */
void ParamReal32_Format(float *v, ParamText *t)
{
    char *end = text_end(t->buf);
    char num[64];

    crt_sprintf(end, "%f", (double)*v);
    __asm__("" : "=m"(num));
    append(end, num);
}
HOOK(0x4EFA20, ParamReal32_Format);

void Param32_Copy(uint32_t *v, uint32_t **dst)
{
    **dst = *v;
}
HOOK(0x4EFA90, Param32_Copy);

void Param32_Paste(uint32_t *v, uint32_t **src)
{
    *v = **src;
}
HOOK(0x4EFAA0, Param32_Paste);

void ParamReal32x3_Parse(float *v, ParamCursor *c)
{
    int i;

    ParamCursor_Need(c, 3);
    i = c->index;
    v[0] = Script_GetArgReal(c->args, i);
    v[1] = Script_GetArgReal(c->args, i + 1);
    v[2] = Script_GetArgReal(c->args, i + 2);
    c->index += 3;
}
HOOK(0x4EFAB0, ParamReal32x3_Parse);

void Param_Three(void *v, int *n)
{
    *n = 3;
}
HOOK(0x4EFB00, Param_Three);

void ParamReal32x3_Format(float *v, ParamText *t)
{
    crt_sprintf(text_end(t->buf), "%f, %f, %f", (double)v[0], (double)v[1], (double)v[2]);
}
HOOK(0x4EFB10, ParamReal32x3_Format);

void ParamReal32x3_Copy(uint32_t *v, uint32_t **dst)
{
    memcpy(*dst, v, 12);
}
HOOK(0x4EFB60, ParamReal32x3_Copy);

void ParamReal32x3_Paste(uint32_t *v, uint32_t **src)
{
    memcpy(v, *src, 12);
}
HOOK(0x4EFB80, ParamReal32x3_Paste);

/* three Euler angles */
void ParamReal32x9_Parse(float *m, ParamCursor *c)
{
    int i = c->index;
    float x, y, z;

    ParamCursor_Need(c, 3);
    x = Script_GetArgReal(c->args, i);
    y = Script_GetArgReal(c->args, i + 1);
    z = Script_GetArgReal(c->args, i + 2);
    Matrix_SetEuler(m, x, y, z);
    c->index += 3;
}
HOOK(0x4EFBA0, ParamReal32x9_Parse);

void ParamReal32x9_Format(float *m, ParamText *t)
{
    crt_sprintf(text_end(t->buf), "%f, %f, %f, %f, %f, %f, %f, %f, %f", (double)m[0], (double)m[1],
                (double)m[2], (double)m[3], (double)m[4], (double)m[5], (double)m[6], (double)m[7],
                (double)m[8]);
}
HOOK(0x4EFC10, ParamReal32x9_Format);

void ParamReal32x9_Copy(uint32_t *m, uint32_t **dst)
{
    memcpy(*dst, m, 40);
}
HOOK(0x4EFC90, ParamReal32x9_Copy);

void ParamReal32x9_Paste(uint32_t *m, uint32_t **src)
{
    memcpy(m, *src, 40);
}
HOOK(0x4EFCB0, ParamReal32x9_Paste);

void ParamReal32x9_Size(void *m, int *size)
{
    *size = 40;
}
HOOK(0x4EFCD0, ParamReal32x9_Size);

void ParamQuat32_Parse(float *q, ParamCursor *c)
{
    int i;
    float x, y, z;

    ParamCursor_Need(c, 3);
    i = c->index;
    x = Script_GetArgReal(c->args, i);
    y = Script_GetArgReal(c->args, i + 1);
    z = Script_GetArgReal(c->args, i + 2);
    Quat_SetEuler(q, x, y, z);
    c->index += 3;
}
HOOK(0x4EFCE0, ParamQuat32_Parse);

static inline long double fpatan(long double y, long double x)
{
    long double r;

    __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
    return r;
}

static long double twice(long double v)
{
    return v + v;
}

/* prints the quaternion as three angles in 0..2pi */
void ParamQuat32_Format(float *q, ParamText *t)
{
    char *end = text_end(t->buf);
    long double xx = twice((long double)q[0] * q[0]);
    float wx = twice((long double)q[3] * q[0]);
    float yy = twice((long double)q[1] * q[1]);
    float zy = twice((long double)q[2] * q[1]);
    float zz = twice((long double)q[2] * q[2]);
    float m00 = (1.0L - yy) - zz;
    float m01 = twice((long double)q[3] * q[2]) + twice((long double)q[1] * q[0]);
    long double b = 1.0L - xx;
    float m22 = b - zz;
    float m21 = (long double)zy - wx;
    float m02 = twice((long double)q[2] * q[0]) - twice((long double)q[3] * q[1]);
    float m12 = (long double)zy + wx;
    long double m11 = b - yy;
    long double r = sqrtl((long double)m01 * m01 + (long double)m00 * m00);
    double rd = r;
    long double a1, a2, a3;

    if (r > kGimbal) {
        a1 = fpatan(m12, m11);
        a2 = fpatan(-(long double)m02, rd);
        a3 = fpatan(m01, m00);
    } else {
        a1 = fpatan(-(long double)m21, m22);
        a2 = fpatan(-(long double)m02, rd);
        a3 = 0.0;
    }
    if (!(a1 >= 0))
        a1 += kTwoPi;
    if (!(a2 >= 0))
        a2 += kTwoPi;
    if (!(a3 >= 0))
        a3 += kTwoPi;
    crt_sprintf(end, "%f, %f, %f", (double)a1, (double)a2, (double)a3);
}
HOOK(0x4EFD50, ParamQuat32_Format);

void ParamQuat32_Copy(uint32_t *q, uint32_t **dst)
{
    memcpy(*dst, q, 20);
}
HOOK(0x4EFEE0, ParamQuat32_Copy);

void ParamQuat32_Paste(uint32_t *q, uint32_t **src)
{
    memcpy(q, *src, 20);
}
HOOK(0x4EFF00, ParamQuat32_Paste);

void ParamQuat32_Size(void *q, int *size)
{
    *size = 20;
}
HOOK(0x4EFF20, ParamQuat32_Size);

void ParamReal64_Parse(double *v, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    *v = Script_GetArgReal(c->args, c->index);
    c->index++;
}
HOOK(0x4EFF30, ParamReal64_Parse);

/* the double goes to sprintf as its two raw words */
void ParamReal64_Format(uint32_t *v, ParamText *t)
{
    crt_sprintf(text_end(t->buf), "%f", v[0], v[1]);
}
HOOK(0x4EFF60, ParamReal64_Format);

void ParamReal64_Copy(uint32_t *v, uint32_t **dst)
{
    memcpy(*dst, v, 8);
}
HOOK(0x4EFF90, ParamReal64_Copy);

void ParamReal64_Paste(uint32_t *v, uint32_t **src)
{
    memcpy(v, *src, 8);
}
HOOK(0x4EFFB0, ParamReal64_Paste);

void ParamReal64x3_Parse(double *v, ParamCursor *c)
{
    int i;

    ParamCursor_Need(c, 3);
    i = c->index;
    v[0] = Script_GetArgReal(c->args, i);
    v[1] = Script_GetArgReal(c->args, i + 1);
    v[2] = Script_GetArgReal(c->args, i + 2);
    c->index += 3;
}
HOOK(0x4EFFD0, ParamReal64x3_Parse);

void ParamReal64x3_Format(uint32_t *v, ParamText *t)
{
    crt_sprintf(text_end(t->buf), "%f, %f, %f", v[0], v[1], v[2], v[3], v[4], v[5]);
}
HOOK(0x4F0020, ParamReal64x3_Format);

void ParamReal64x3_Copy(uint32_t *v, uint32_t **dst)
{
    memcpy(*dst, v, 24);
}
HOOK(0x4F0060, ParamReal64x3_Copy);

void ParamReal64x3_Paste(uint32_t *v, uint32_t **src)
{
    memcpy(v, *src, 24);
}
HOOK(0x4F0080, ParamReal64x3_Paste);

void ParamReal64x3_Size(void *v, int *size)
{
    *size = 24;
}
HOOK(0x4F00A0, ParamReal64x3_Size);

void ParamInt32_Parse(int *v, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    *v = Script_GetArgInt(c->args, c->index);
    c->index++;
}
HOOK(0x4F00B0, ParamInt32_Parse);

void ParamInt32_Format(int *v, ParamText *t)
{
    crt_sprintf(text_end(t->buf), "%d", *v);
}
HOOK(0x4F00E0, ParamInt32_Format);

void ParamInt16_Parse(int16_t *v, ParamCursor *c)
{
    ParamCursor_Need(c, 1);
    *v = Script_GetArgInt(c->args, c->index);
    c->index++;
}
HOOK(0x4F0110, ParamInt16_Parse);

void ParamInt16_Format(int16_t *v, ParamText *t)
{
    crt_sprintf(text_end(t->buf), "%d", *v);
}
HOOK(0x4F0140, ParamInt16_Format);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54DD0C) uint16_t gLevelType INIT(385);
DATA(54DD0C) uint16_t gStaticType INIT(385);
DATA(54DD0C) uint16_t gDynamicType INIT(385);
DATA(54DD0C) uint16_t gIdleType INIT(385);
DATA(54DDA8) int gLevelActivateEvent INIT(255);
DATA(54DDA8) int gLevelDeactivateEvent INIT(255);
DATA(54DDA8) int gLevelUnusedNewTaskEvent INIT(255);
BSS(A70C68) int gParamReal32x3;
BSS(A70C68) int gParamReal32x9;
BSS(A70C68) int gParamDropDownCombo;
BSS(A70C78) int gParamInt16;
BSS(A70C78) int gParamInt32;
BSS(A70C78) int gParamGraphEnvelope;
BSS(A70C78) int gParamAngle;
BSS(A70C78) int gParamObjectPos;
BSS(A70C78) int gParamScriptFile;
BSS(A70C78) int gParamBool8;
BSS(A70C9C) int gParamVarString;
BSS(A70C9C) int gParamGraph;
BSS(A70C9C) int gParamDegrees;
BSS(A70C9C) List gTaskNotes[LEVEL_NOTE_LISTS];
BSS(A70C9C) int gParamString256;
BSS(A70C9C) int gParamReal32;
BSS(A70C9C) int gParamReal64;
BSS(A70C9C) int gParamQuat32;
BSS(A70C9C) int gParamPitchGraph;
BSS(A70C9C) int gParamRGB;
BSS(A70C9C) int gParamString16;
BSS(A70C9C) int gParamString32;
BSS(A70C9C) int gParamLevelExpression;
BSS(A75890) int gParamReal64x3;
BSS(A75890) int gParamQTaskNote;
BSS(A75890) int gParamPushButton;
PAD(igb, A75890, A7589C, 0x4);
BSS(A75890) int gParamBitfield;
BSS(A758BC) char gLevelInsertHead;
BSS(A758BC) char gLevelPaused;
BSS(A758BC) char gLevelForceRunning;
PAD(igb, A758BC, A758BF, 0x1);
BSS(A758BC) int gLevelUnused0;
BSS(A758BC) int gLevelUnused1;
