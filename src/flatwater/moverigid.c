/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "flatwater/flatwater.h"

FN_EXTERN(MoveRigidObj_Changed);

/* "MoveRigidObj": a rigid object placed between two key positions and
   orientations by a script value; whole numbers are the keys and the value
   wraps around. */

ORIG(0x4B38E0, void, Matrix_SetEuler, (float *m, float ax, float ay, float az));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D0950, long double, VirModel_GetRadius2, (void **model, int unk));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *owner));

/* the RigidDynCubeObj part */
typedef struct RigidObj {
    Task task;
    Vec3d pos;         /* 0x20 */
    char unk38[0x30];
    char *world;       /* 0x68 */
    void **model;      /* 0x6C */
    float matrix[10];  /* 0x70 */
    float scale[3];    /* 0x98 */
    char unkA4[0x44];
    Task *magic;       /* 0xE8 */
    int unkEC;
} RigidObj;

typedef struct MoveRigidObj {
    RigidObj obj;
    Vec3d keyPos[2];   /* 0xF0 */
    Vec3d unk120[2];
    float alpha[2];    /* 0x150 */
    float beta[2];
    float gamma[2];
    float unk168[2];
    float unk170[2];
    float unk178[2];
    char unk180[16];
    char unk190[16];
    double value;      /* 0x1A0 */
    double last;       /* 0x1A8, value of the last placement */
    char unk1B0;
    char pad1B1[3];
    VarString expr;    /* 0x1B4 */
    char registered;   /* 0x204 */
    char pad205[3];
} MoveRigidObj;

_Static_assert(sizeof(RigidObj) == 0xF0, "RigidObj");
_Static_assert(sizeof(MoveRigidObj) == 0x208, "MoveRigidObj");

extern char gMoveBetaNames[][16];
extern uint16_t gMoveRigidObjType;
extern void *gMoveRigidObjParams;
extern char gMoveAlphaNames[][16];
extern char gMoveGammaNames[][16];
extern char gMovePosNames[][16];

extern float kOne;
extern double kZero64;
extern double kOne64;
extern double kTwo64;

#define MOVERIGID_PARAMS_CHANGED (FN(0x51FD60, MoveRigidObj_Changed))

void MoveRigidObj_Create(MoveRigidObj *t);
void MoveRigidObj_Run(MoveRigidObj *t);
void MoveRigidObj_Delete(MoveRigidObj *t);
void MoveRigidObj_GetParams(void *task, void **out);
void MoveRigidObj_Activate(MoveRigidObj *t);
void MoveRigidObj_Deactivate(MoveRigidObj *t);

void MoveRigidObj_Open(void)
{
    MoveRigidObj t;
    int i;

    gMoveRigidObjType = TaskType_New("MoveRigidObj", sizeof(MoveRigidObj), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gMoveRigidObjType, 0, MoveRigidObj_Create);
    TaskType_SetHandler2(gMoveRigidObjType, 1, MoveRigidObj_Run);
    TaskType_SetHandler2(gMoveRigidObjType, 2, MoveRigidObj_Delete);
    TaskType_SetHandler(gMoveRigidObjType, Param_GetTableEvent(), MoveRigidObj_GetParams);
    TaskType_SetHandler(gMoveRigidObjType, Param_GetChangedEvent(), MOVERIGID_PARAMS_CHANGED);
    TaskType_SetHandler2(gMoveRigidObjType, Level_GetEventDA8(), MoveRigidObj_Activate);
    TaskType_SetHandler2(gMoveRigidObjType, Level_GetEventDAC(), MoveRigidObj_Deactivate);
    gMoveRigidObjParams = ParamTable_New(20);
    for (i = 0; i < 2; i++) {
        crt_sprintf(gMovePosNames[i], "Position %d", i);
        crt_sprintf(gMoveAlphaNames[i], "Alpha %d", i);
        crt_sprintf(gMoveBetaNames[i], "Beta %d", i);
        crt_sprintf(gMoveGammaNames[i], "Gamma %d", i);
        ParamTable_Add(gMoveRigidObjParams, &t, &t.keyPos[i], Param_ObjectPos(), 7, NULL);
        ParamTable_Add(gMoveRigidObjParams, &t, &t.alpha[i], Param_Angle(), 7, NULL);
        ParamTable_Add(gMoveRigidObjParams, &t, &t.beta[i], Param_Angle(), 7, NULL);
        ParamTable_Add(gMoveRigidObjParams, &t, &t.gamma[i], Param_Angle(), 7, NULL);
    }
    ParamTable_Add(gMoveRigidObjParams, &t, &t.unk180, Param_String16(), 3, NULL);
    ParamTable_Add(gMoveRigidObjParams, &t, &t.expr, Param_VarString(), 7, NULL);
    ParamTable_Add(gMoveRigidObjParams, &t, &t.value, Param_Real64(), 3, NULL);
    ParamTable_Add(gMoveRigidObjParams, &t, &t.unk1B0, Param_Bool8(), 3, NULL);
    Script_RegisterConstant("TASKTYPE_MOVERIGIDOBJ", gMoveRigidObjType);
}
HOOK(0x51F580, MoveRigidObj_Open);

/* largest scale component; a NaN compare picks the later operand */
static float max_scale(const RigidObj *o)
{
    float m = !(o->scale[2] >= o->scale[1]) ? o->scale[1] : o->scale[2];

    return o->scale[0] > m ? o->scale[0] : m;
}

void MoveRigidObj_Create(MoveRigidObj *t)
{
    Vec3d pos;
    float scale, radius;
    int i;

    t->last = -1.0;
    pos.x = 0;
    pos.y = 0;
    pos.z = 0;
    t->obj.magic = MagicObjContainer_New(NULL, t);
    t->unk180[0] = 0;
    VarString_Init(&t->expr);
    RigidDynCubeObj_Init(gWorld, t, NULL);
    scale = max_scale(&t->obj);
    radius = VirModel_GetRadius2(t->obj.model, 2) * scale;
    DynCube_Add(gWorld, t, &pos, radius, 2);
    for (i = 0; i < 2; i++) {
        memset(&t->keyPos[i], 0, sizeof(Vec3d));
        t->alpha[i] = 0;
        t->beta[i] = 0;
        t->gamma[i] = 0;
        memset(&t->unk120[i], 0, sizeof(Vec3d));
        t->unk168[i] = -1.0f;
        t->unk170[i] = -1.0f;
        t->unk178[i] = -1.0f;
    }
    t->registered = 0;
}
HOOK(0x51F810, MoveRigidObj_Create);

void MoveRigidObj_Activate(MoveRigidObj *t)
{
    char name[64];
    char buf[128];

    if (t->registered)
        return;
    t->registered = 1;
    Task_GetName(name, t);
    crt_sprintf(buf, "%s.vValue", name);
    Script_RegisterReal(buf, &t->value);
}
HOOK(0x51F980, MoveRigidObj_Activate);

void MoveRigidObj_Deactivate(MoveRigidObj *t)
{
    char name[64];
    char buf[128];

    Task_GetName(name, t);
    t->registered = 0;
    crt_sprintf(buf, "%s.vValue", name);
    Script_RemoveSymbol(buf);
    t->registered = 0;
}
HOOK(0x51F9E0, MoveRigidObj_Deactivate);

void MoveRigidObj_Delete(MoveRigidObj *t)
{
    if (Task_GetId(t) >= 0) {
        TaskFn1 fn = TASK_HANDLER(Level_GetEventDAC(), t->obj.task.type);

        if (fn)
            fn(t);
    }
    VarString_Free(&t->expr);
    DynCube_Remove(t->obj.world, t);
    Task_Delete(t->obj.magic);
}
HOOK(0x51FA30, MoveRigidObj_Delete);

void MoveRigidObj_GetParams(void *task, void **out)
{
    *out = gMoveRigidObjParams;
}
HOOK(0x51FA90, MoveRigidObj_GetParams);

void MoveRigidObj_Apply(MoveRigidObj *t, RigidObj *o, char force);

void MoveRigidObj_Run(MoveRigidObj *t)
{
    long double x;

    BROADCAST(t->obj.task.head, 1);
    if (VarString_HasExpr(&t->expr))
        t->value = VarString_EvalReal(&t->expr, t);
    if (!(t->value >= kZero64)) {
        do {
            x = t->value + kTwo64;
            t->value = x;
        } while (!(x >= kZero64));
    }
    if (t->value > kOne64) {
        do {
            x = t->value - kOne64;
            t->value = x;
        } while (x > kOne64);
    }
    MoveRigidObj_Apply(t, &t->obj, 0);
}
HOOK(0x51FAA0, MoveRigidObj_Run);

/* places o between key i and key i+1 (wrapping to 0) by the fraction of value */
void MoveRigidObj_Apply(MoveRigidObj *t, RigidObj *o, char force)
{
    long double f, g;
    float alpha, beta, gamma, scale, radius;
    Vec3d pos;
    int i, j;

    if (!fne(t->value, t->last) && !force)
        return;
    i = x87_fistp(t->value);
    j = (i + 1) % 2;
    f = t->value - (long double)i;
    g = kOne - f;
    alpha = g * t->alpha[i] + f * t->alpha[j];
    beta = g * t->beta[i] + f * t->beta[j];
    gamma = g * t->gamma[i] + f * t->gamma[j];
    pos.x = g * t->keyPos[i].x + f * t->keyPos[j].x;
    pos.y = f * t->keyPos[j].y + g * t->keyPos[i].y;
    pos.z = f * t->keyPos[j].z + g * t->keyPos[i].z;
    Matrix_SetEuler(o->matrix, alpha, beta, gamma);
    scale = max_scale(o);
    radius = VirModel_GetRadius(o->model) * scale;
    DynCube_Move(o->world, o, &pos, radius);
    memcpy(&t->last, &t->value, sizeof(double));
}
HOOK(0x51FBA0, MoveRigidObj_Apply);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533928) double kTwo64 INIT(2.0);
STORE(igb, A837AC, 1) char store_A837AC[2][16];
ALIAS(gMoveBetaNames, store_A837AC, 0);
BSS(A837AC) uint16_t gMoveRigidObjType;
PAD(igb, A837AC, A837CE, 0x2);
BSS(A837AC) void *gMoveRigidObjParams;
STORE(igb, A837AC, 1) char store_A837D4[2][16];
ALIAS(gMoveAlphaNames, store_A837D4, 0);
STORE(igb, A837AC, 1) char store_A837F4[2][16];
ALIAS(gMoveGammaNames, store_A837F4, 0);
PAD(igb, A837AC, A83814, 0x24);
ALIAS(gMovePosNames, pad_A83814, 0);
