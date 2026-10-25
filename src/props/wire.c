/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "props/props.h"
#include "math/mathvars.h"
#include "nodereg/noderegvars.h"

FN_EXTERN(Human_ClearResult);

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x414A60, void, Matrix_SetYAxis, (Matrix *m, const Vec3f *dir));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x414ED0, void, Model_GetLocalPoint, (Vec3f *out, void *obj, int i));
ORIG(0x4167F0, uint8_t, Game_GetEvent2, (void));
ORIG(0x416800, uint8_t, Game_GetEvent4, (void));
ORIG(0x416810, uint8_t, Game_GetEvent3, (void));
ORIG(0x416820, uint8_t, Game_GetEvent6, (void));
ORIG(0x416850, uint8_t, Game_GetEvent8, (void));
ORIG(0x4168A0, void *, Game_GetTaskList0, (void));
ORIG(0x416AD0, char, UseQuery_Consider, (UseQuery *q, void *obj, const Vec3d *pos));
ORIG(0x47E6F0, uint8_t, BulletHole_GetEvent, (void));
ORIG(0x4871C0, void, StatusScreen_DrawActionIcon, (Task *human, void *sprite));
ORIG(0x4B3100, void, Vec3d_Normalize, (Vec3d *v));
ORIG(0x4B3130, long double, Vec3d_Length, (const Vec3d *v));
ORIG(0x4B3C50, void, Matrix_GetEuler, (const Matrix *m, float *ax, float *ay, float *az));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *room, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *room, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1050, int, Task_GetId, (void *task));
ORIG(0x4F1210, int, Level_GetActivateEvent, (void));
ORIG(0x4F1220, int, Level_GetDeactivateEvent, (void));
ORIG(0x4F1270, void, Task_GetName, (char *out, void *task));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

/* "Wire" (0x138 bytes): a model stretched between two points that humans can
   hang on (use kind 9) */
typedef struct Wire {
    RigidDynCubeObj obj;
    Vec3d start;        /* 0xF0 */
    Vec3d end;          /* 0x108 */
    char model[0x10];   /* 0x120 */
    char isInUse;       /* 0x130 */
    char isLastInUse;   /* 0x131 */
    char registered;    /* 0x132 */
    char unk133[5];
} Wire;

_Static_assert(sizeof(Wire) == 0x138, "Wire");

extern uint16_t gWireType;
extern void *gWireParams;

extern double kWireScale; /* 1/4096 */
extern float kWireSlope;  /* 2 degrees */

void Wire_Create(Wire *w);
void Wire_Run(Wire *w);
void Wire_Delete(Wire *w);
void Wire_GetParams(void *task, void **out);
void Wire_ParamsChanged(Wire *w);
void Wire_Use(Wire *w, UseQuery *q);
void Wire_GetUseKind(Wire *w, int *out);
void Wire_SetInUse(Wire *w);
void Wire_DrawIcon(Wire *w, Task **human);
void Wire_Activate(Wire *w);
void Wire_Deactivate(Wire *w);

void Wire_Open(void)
{
    Wire w;

    gWireType = TaskType_New("Wire", sizeof(Wire), RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gWireType, 0, Wire_Create);
    TaskType_SetHandler2(gWireType, 1, Wire_Run);
    TaskType_SetHandler2(gWireType, 2, Wire_Delete);
    TaskType_SetHandler(gWireType, Param_GetTableEvent(), Wire_GetParams);
    TaskType_SetHandler(gWireType, Param_GetChangedEvent(), Wire_ParamsChanged);
    TaskType_SetHandler(gWireType, Game_GetEvent2(), Wire_Use);
    TaskType_SetHandler(gWireType, Game_GetEvent3(), Wire_GetUseKind);
    TaskType_SetHandler(gWireType, Game_GetEvent4(), Wire_SetInUse);
    TaskType_SetHandler(gWireType, Game_GetEvent6(), Wire_DrawIcon);
    TaskType_SetHandler(gWireType, Game_GetEvent8(), FN(0x45E3D0, Human_ClearResult));
    TaskType_SetHandler2(gWireType, Level_GetActivateEvent(), Wire_Activate);
    TaskType_SetHandler2(gWireType, Level_GetDeactivateEvent(), Wire_Deactivate);
    TaskType_SetHandler(gWireType, 8, NULL);
    TaskType_SetHandler(gWireType, BulletHole_GetEvent(), NULL);
    gWireParams = ParamTable_New(3);
    ParamTable_Add(gWireParams, &w, &w.start, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gWireParams, &w, &w.end, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gWireParams, &w, w.model, Param_String16(), 7, NULL);
}
HOOK(0x43EFF0, Wire_Open);

void Wire_Create(Wire *w)
{
    Vec3d zero = {0, 0, 0};
    float scale, radius;

    QTaskList_Add(Game_GetTaskList0(), w);
    RigidDynCubeObj_Init(gCurrentWorld, w, NULL);
    scale = max_scale(&w->obj);
    radius = VirModel_GetRadius(w->obj.model) * scale;
    DynCube_Add(gCurrentWorld, w, &zero, radius, 1);
    memset(&w->start, 0, sizeof(w->start));
    memset(&w->end, 0, sizeof(w->end));
    w->model[0] = 0;
    w->isInUse = 0;
    w->isLastInUse = 0;
    w->registered = 0;
}
HOOK(0x43F1F0, Wire_Create);

void Wire_Run(Wire *w)
{
    w->isLastInUse = w->isInUse;
}
HOOK(0x43F340, Wire_Run);

void Wire_Delete(Wire *w)
{
    QTaskList_Remove(Game_GetTaskList0(), w);
    if (Task_GetId(w) >= 0) {
        TaskFn fn = HANDLER(Level_GetDeactivateEvent(), w->obj.task.type);

        if (fn)
            fn(w);
    }
    DynCube_Remove(w->obj.world, w);
}
HOOK(0x43F360, Wire_Delete);

void Wire_GetParams(void *task, void **out)
{
    *out = gWireParams;
}
HOOK(0x43F3C0, Wire_GetParams);

/* stretches the model along y from start to end */
void Wire_ParamsChanged(Wire *w)
{
    Vec3d d, mid;
    Vec3f f;
    Matrix m;
    double len, sy, sz;
    long double sx;
    float scale, radius;

    d.x = w->end.x - w->start.x;
    d.y = w->end.y - w->start.y;
    d.z = w->end.z - w->start.z;
    len = Vec3d_Length(&d);
    Vec3d_Normalize(&d);
    f.x = d.x;
    f.y = d.y;
    f.z = d.z;
    Matrix_SetYAxis(&m, &f);
    w->obj.matrix = m;
    w->obj.scale[1] = len * kWireScale;
    RigidDynCubeObj_SetModel(w, VirModel_Find(w->model));
    sx = w->end.x + w->start.x;
    sy = w->start.y + w->end.y;
    sz = w->start.z + w->end.z;
    mid.x = sx * kHalfD;
    mid.y = sy * kHalfD;
    mid.z = sz * kHalfD;
    scale = max_scale(&w->obj);
    radius = VirModel_GetRadius(w->obj.model) * scale;
    DynCube_Move(w->obj.world, w, &mid, radius);
}
HOOK(0x43F3D0, Wire_ParamsChanged);

void Wire_GetMatrix(Matrix *out, Wire *w);

/* Game event 2: offers the point of the wire nearest to the human */
void Wire_Use(Wire *w, UseQuery *q)
{
    Matrix m;
    Vec3f a, b;
    Vec3d p;
    long double dx, dy, dz, t;
    double ly, wx, wy, wz;
    const float *r = m.m;

    Wire_GetMatrix(&m, w);
    dx = q->pos.x - w->obj.pos.x;
    dy = q->pos.y - w->obj.pos.y;
    dz = q->pos.z - w->obj.pos.z;
    ly = (r[7] * dz + r[4] * dy) + r[1] * dx;
    Model_GetLocalPoint(&a, w, 0);
    Model_GetLocalPoint(&b, w, 1);
    if (a.y > ly)
        t = a.y;
    else if (b.y >= ly)
        t = ly;
    else
        t = b.y;
    wx = r[1] * t;
    wy = r[4] * t;
    wz = r[7] * t;
    p.x = wx + w->obj.pos.x;
    p.y = wy + w->obj.pos.y;
    p.z = wz + w->obj.pos.z;
    UseQuery_Consider(q, w, &p);
}
HOOK(0x43F560, Wire_Use);

void Wire_GetUseKind(Wire *w, int *out)
{
    *out = 9;
}
HOOK(0x43F6C0, Wire_GetUseKind);

void Wire_SetInUse(Wire *w)
{
    w->isInUse = 1;
}
HOOK(0x43F6D0, Wire_SetInUse);

void Wire_DrawIcon(Wire *w, Task **human)
{
    StatusScreen_DrawActionIcon(*human, (char *)*human + 0x4AC);
}
HOOK(0x43F6E0, Wire_DrawIcon);

void Wire_Activate(Wire *w)
{
    char name[64], buf[128];

    if (w->registered)
        return;
    w->registered = 1;
    Task_GetName(name, w);
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RegisterBool8(buf, &w->isInUse);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RegisterBool8(buf, &w->isLastInUse);
}
HOOK(0x43F700, Wire_Activate);

void Wire_Deactivate(Wire *w)
{
    char name[64], buf[128];

    if (!w->registered)
        return;
    Task_GetName(name, w);
    w->registered = 0;
    crt_sprintf(buf, "%s.isInUse", name);
    Script_RemoveSymbol(buf);
    crt_sprintf(buf, "%s.isLastInUse", name);
    Script_RemoveSymbol(buf);
    w->registered = 0;
}
HOOK(0x43F780, Wire_Deactivate);

void Wire_Close(void)
{
    QTaskList_Free(gWireParams);
    TaskType_Free(gWireType);
}
HOOK(0x43F800, Wire_Close);

/* rotation with y along the wire model, from its point 0 to point 1 */
void Wire_GetMatrix(Matrix *out, Wire *w)
{
    Vec3d p0, p1, d;
    Vec3f f;

    Model_GetPoint(&p0, w, 0);
    Model_GetPoint(&p1, w, 1);
    d.x = p1.x - p0.x;
    d.y = p1.y - p0.y;
    d.z = p1.z - p0.z;
    Vec3d_Normalize(&d);
    f.x = d.x;
    f.y = d.y;
    f.z = d.z;
    Matrix_SetYAxis(out, &f);
}
HOOK(0x43F820, Wire_GetMatrix);

/* true when the wire is tilted by more than 2 degrees about x */
char Wire_IsSloped(Wire *w)
{
    Matrix m;
    float ax, ay, az;
    long double a;

    Wire_GetMatrix(&m, w);
    Matrix_GetEuler(&m, &ax, &ay, &az);
    a = ax;
    if (ax < kZero)
        a = -a;
    return a > kWireSlope;
}
HOOK(0x43F8A0, Wire_IsSloped);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(5334B0) double kWireScale INIT(0.000244140625);
ALIAS(kUnitsToMeters, kWireScale, 0);
DATA(5335C0) const double kHalfD INIT(0.5);
DATA(533834) float kWireSlope INIT(0.034906585f);
BSS(57C144) uint16_t gWireType;
PAD(igb, 57C144, 57C146, 0x2);
BSS(57C144) void *gWireParams;
