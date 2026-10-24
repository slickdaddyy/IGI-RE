/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <math.h>

#include "hook.h"
#include "props/props.h"

ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x416920, void *, Game_GetWorld, (void));
ORIG(0x4B0D40, void, Mem_SetDefaultMode, (void));
ORIG(0x4B0D50, void, Mem_ClearMode, (void));
ORIG(0x4B0DB0, char, Mem_GetMode, (void));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *room, void *obj, void **model));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *room, void *obj));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4ECDB0, void, Body_Update, (void *body));
ORIG(0x4ECF50, void, Body_AddForce, (void *body, void *force));
ORIG(0x4EE140, PhysType *, PhysicsObjType_Find, (const char *name));
ORIG(0x4F0F60, void *, Param_Real32x3, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1000, void *, Param_Bool8, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4FB5E0, uint16_t, PhysicsObj_GetType, (void));
ORIG(0x4FB5F0, void, PhysicsObj_Init, (void *obj));
ORIG(0x4FB700, void, PhysicsObj_SetPhysType, (void *obj, PhysType *phys));
ORIG(0x4FB980, void, PhysicsObj_Free, (void *obj));
ORIG(0x4FC0E0, void, PhysicsObj_Collide, (void *obj, float a, float b, float c, void *world, int event, CollMsg *msg));
ORIG(0x4FC7B0, int, PhysicsObj_IsResting, (void *obj));
ORIG(0x5042A0, uint16_t, PhysicsMagicObj_GetType, (void));

extern uint16_t gGenericPhysicsMagicObjType;
extern uint16_t gGenericPhysicsObjType;
extern void *gGenericPhysicsObjParams;

extern float kHundred;
extern float kTwoHundred;
extern float kFifth;

/* GenericPhysicsObj is PhysicsObj with two more script flags */
typedef struct GenericPhysicsObj {
    PhysObj p;
} GenericPhysicsObj;

/* the force given to Body_AddForce */
typedef struct Force {
    Vec3f force;
    uint32_t point[3];
} Force;

/* Game event 9 (a hit) and event 11 (an explosion) */
typedef struct HitMsg {
    RigidDynCubeObj *src;
    Vec3d *center;      /* event 11 */
    Vec3f dir;          /* event 9 */
} HitMsg;

void GenericPhysicsMagicObj_Run(PhysObj *o);
void GenericPhysicsObj_Delete(PhysObj *o);
void GenericPhysicsMagicObj_Hit(PhysObj *o, HitMsg *msg);
void GenericPhysicsObj_Explode(PhysObj *o, HitMsg *msg);
void GenericPhysicsObj_Create(PhysObj *o);
void GenericPhysicsObj_Run(PhysObj *o);
void GenericPhysicsObj_GetParams(void *task, void **out);
void GenericPhysicsObj_ParamsChanged(PhysObj *o);
void GenericPhysicsObj_Hit(PhysObj *o, HitMsg *msg);

void GenericPhysicsMagicObj_Open(void)
{
    gGenericPhysicsMagicObjType = TaskType_New("GenericPhysicsMagicObj", 0x768, PhysicsMagicObj_GetType(), 0);
    TaskType_SetHandler2(gGenericPhysicsMagicObjType, 1, GenericPhysicsMagicObj_Run);
    TaskType_SetHandler2(gGenericPhysicsMagicObjType, 2, GenericPhysicsObj_Delete);
    TaskType_SetHandler(gGenericPhysicsMagicObjType, Game_GetEvent9(), GenericPhysicsMagicObj_Hit);
    TaskType_SetHandler(gGenericPhysicsMagicObjType, Game_GetEvent11(), GenericPhysicsObj_Explode);
    Script_RegisterConstant("TASKTYPE_GENERICPHYSICSMAGICOBJ", gGenericPhysicsMagicObjType);
}
HOOK(0x43E4F0, GenericPhysicsMagicObj_Open);

void GenericPhysicsMagicObj_Collide(PhysObj *o);

void GenericPhysicsMagicObj_Run(PhysObj *o)
{
    GenericPhysicsMagicObj_Collide(o);
    BROADCAST(&o->task, 1);
}
HOOK(0x43E580, GenericPhysicsMagicObj_Run);

void GenericPhysicsMagicObj_Collide(PhysObj *o)
{
    CollMsg msg;

    if (PhysicsObj_IsResting(o))
        return;
    msg.obj = o;
    PhysicsObj_Collide(o, 0x1.64ce9ep+5f, 0x1.eb851ep-1f, 0x1.d70a3ep-1f, Game_GetWorld(), 0xC, &msg);
}
HOOK(0x43E600, GenericPhysicsMagicObj_Collide);

void GenericPhysicsObj_Delete(PhysObj *o)
{
    PhysicsObj_Free(o);
    DynCube_Remove(Game_GetWorld(), o);
}
HOOK(0x43E650, GenericPhysicsObj_Delete);

/* pushes the object along v (object space), scaled to mass * k */
static void push(PhysObj *o, Force *f, float v0, float v1, float v2, float k)
{
    long double s = o->phys->mass * k / sqrtl(((long double)v0 * v0 + (long double)v1 * v1) + (long double)v2 * v2);

    f->force.x = v0 * s;
    f->force.y = v1 * s;
    f->force.z = v2 * s;
}

void GenericPhysicsMagicObj_Hit(PhysObj *o, HitMsg *msg)
{
    const float *m = o->matrix.m;
    long double d0 = msg->dir.x, d1 = msg->dir.y, d2 = msg->dir.z;
    float v0, v1, v2;
    Force f;

    memcpy(f.point, &msg->src->pos, sizeof(f.point));
    v0 = (d2 * m[6] + d1 * m[3]) + d0 * m[0];
    v1 = (d2 * m[7] + d1 * m[4]) + d0 * m[1];
    v2 = (d2 * m[8] + d1 * m[5]) + d0 * m[2];
    push(o, &f, v0, v1, v2, kHundred);
    Body_AddForce(&o->body, &f);
}
HOOK(0x43E670, GenericPhysicsMagicObj_Hit);

void GenericPhysicsObj_Explode(PhysObj *o, HitMsg *msg)
{
    const float *m = o->matrix.m;
    long double dx = o->pos.x - msg->center->x;
    double dy = o->pos.y - msg->center->y;
    double dz = o->pos.z - msg->center->z;
    float v0, v1, v2;
    Force f;

    v0 = (dz * m[6] + dy * m[3]) + dx * m[0];
    v1 = (dz * m[7] + dy * m[4]) + dx * m[1];
    v2 = (dz * m[8] + dy * m[5]) + dx * m[2];
    push(o, &f, v0, v1, v2, kTwoHundred);
    memset(f.point, 0, sizeof(f.point));
    Body_AddForce(&o->body, &f);
}
HOOK(0x43E7A0, GenericPhysicsObj_Explode);

void GenericPhysicsMagicObj_Close(void)
{
    TaskType_Free(gGenericPhysicsMagicObjType);
}
HOOK(0x43E8C0, GenericPhysicsMagicObj_Close);

void GenericPhysicsObj_Open(void)
{
    PhysObj o;

    gGenericPhysicsObjType = TaskType_New("GenericPhysicsObj", 0x720, PhysicsObj_GetType(), 0);
    TaskType_SetHandler(gGenericPhysicsObjType, 0, GenericPhysicsObj_Create);
    TaskType_SetHandler2(gGenericPhysicsObjType, 1, GenericPhysicsObj_Run);
    TaskType_SetHandler2(gGenericPhysicsObjType, 2, GenericPhysicsObj_Delete);
    TaskType_SetHandler(gGenericPhysicsObjType, Param_GetTableEvent(), GenericPhysicsObj_GetParams);
    TaskType_SetHandler(gGenericPhysicsObjType, Param_GetChangedEvent(), GenericPhysicsObj_ParamsChanged);
    TaskType_SetHandler(gGenericPhysicsObjType, Game_GetEvent9(), GenericPhysicsObj_Hit);
    TaskType_SetHandler(gGenericPhysicsObjType, Game_GetEvent11(), GenericPhysicsObj_Explode);
    gGenericPhysicsObjParams = ParamTable_New(0x14);
    ParamTable_Add(gGenericPhysicsObjParams, &o, &o.startPos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gGenericPhysicsObjParams, &o, &o.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gGenericPhysicsObjParams, &o, &o.startVel, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gGenericPhysicsObjParams, &o, o.name, Param_String16(), 7, NULL);
    ParamTable_Add(gGenericPhysicsObjParams, &o, &o.collide, Param_Bool8(), 7, NULL);
    ParamTable_Add(gGenericPhysicsObjParams, &o, &o.upright, Param_Bool8(), 7, NULL);
    Script_RegisterConstant("TASKTYPE_GENERICPHYSICSOBJ", gGenericPhysicsObjType);
}
HOOK(0x43E8D0, GenericPhysicsObj_Open);

void GenericPhysicsObj_Init(PhysObj *o);

void GenericPhysicsObj_Create(PhysObj *o)
{
    GenericPhysicsObj_Init(o);
}
HOOK(0x43EAA0, GenericPhysicsObj_Create);

void GenericPhysicsObj_Init(PhysObj *o)
{
    Vec3d zero = {0, 0, 0};
    float scale, radius;

    RigidDynCubeObj_Init(Game_GetWorld(), o, NULL);
    scale = max_scale((RigidDynCubeObj *)o);
    radius = VirModel_GetRadius(o->model) * scale;
    DynCube_Add(Game_GetWorld(), o, &zero, radius, 2);
    o->name[0] = 0;
    PhysicsObj_Init(o);
}
HOOK(0x43EAB0, GenericPhysicsObj_Init);

void GenericPhysicsObj_Collide(PhysObj *o);

void GenericPhysicsObj_Run(PhysObj *o)
{
    GenericPhysicsObj_Collide(o);
    BROADCAST(&o->task, 1);
}
HOOK(0x43EBA0, GenericPhysicsObj_Run);

void GenericPhysicsObj_GetParams(void *task, void **out)
{
    *out = gGenericPhysicsObjParams;
}
HOOK(0x43EC20, GenericPhysicsObj_GetParams);

void GenericPhysicsObj_Collide(PhysObj *o)
{
    CollMsg msg;

    if (PhysicsObj_IsResting(o))
        return;
    msg.obj = o;
    PhysicsObj_Collide(o, 0x1.64ce9ep+5f, 0x1.e66666p-1f, 0x1.e66666p-1f, Game_GetWorld(), 0xC, &msg);
}
HOOK(0x43EC30, GenericPhysicsObj_Collide);

void GenericPhysicsObj_ParamsChanged(PhysObj *o)
{
    void **model;
    float scale, radius;

    o->matrix = o->startMatrix;
    model = VirModel_Find(o->name);
    if (model) {
        RigidDynCubeObj_SetModel(o, model);
        PhysicsObj_SetPhysType(o, PhysicsObjType_Find(o->name));
    } else if (o->name[0] && o->name[1] != '*') {
        char msg[128], name[128];

        crt_sprintf(msg, "Model \"%s\" not available", o->name);
        strcpy(name, o->name);
        crt_sprintf(o->name, "** %s **", name);
    }
    o->body.vel = o->startVel;
    Body_Update(&o->body);
    scale = max_scale((RigidDynCubeObj *)o);
    radius = VirModel_GetRadius(o->model) * scale;
    DynCube_Move(Game_GetWorld(), o, &o->startPos, radius);
}
HOOK(0x43EC80, GenericPhysicsObj_ParamsChanged);

void GenericPhysicsObj_Hit(PhysObj *o, HitMsg *msg)
{
    const float *m = o->matrix.m;
    long double d0 = msg->dir.x, d1 = msg->dir.y, d2 = msg->dir.z;
    float v0, v1, v2;
    Force f;

    v0 = (d2 * m[6] + d1 * m[3]) + d0 * m[0];
    v1 = (d2 * m[7] + d1 * m[4]) + d0 * m[1];
    v2 = (d2 * m[8] + d1 * m[5]) + d0 * m[2];
    f.force.x = v0 * kFifth;
    f.force.y = v1 * kFifth;
    f.force.z = v2 * kFifth;
    memset(f.point, 0, sizeof(f.point));
    Body_AddForce(&o->body, &f);
}
HOOK(0x43EDF0, GenericPhysicsObj_Hit);

void GenericPhysicsObj_Close(void)
{
    QTaskList_Free(gGenericPhysicsObjParams);
    TaskType_Free(gGenericPhysicsObjType);
}
HOOK(0x43EEF0, GenericPhysicsObj_Close);

/* drops a new object: position, matrix, model name and start velocity */
PhysObj *GenericPhysicsObj_New(void *parent, const Vec3d *pos, const Matrix *m, const char *name, const Vec3f *vel)
{
    PhysObj *o;
    TaskFn fn;

    if (!Mem_GetMode()) {
        Mem_SetDefaultMode();
        o = (PhysObj *)Task_New(parent, gGenericPhysicsObjType, NULL);
        Mem_ClearMode();
    } else {
        o = (PhysObj *)Task_New(parent, gGenericPhysicsObjType, NULL);
    }
    o->startPos = *pos;
    o->startMatrix = *m;
    memcpy(&o->startVel, vel, sizeof(Vec3f));
    strcpy(o->name, name);
    fn = HANDLER(Param_GetChangedEvent(), o->task.type);
    if (fn)
        fn(o, NULL);
    return o;
}
HOOK(0x43EF10, GenericPhysicsObj_New);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(533578) float kHundred INIT(100.0f);
DATA(533830) float kTwoHundred INIT(200.0f);
DATA(53C46C) uint16_t gGenericPhysicsMagicObjType INIT(385);
PAD(igd, 53C46C, 53C46E, 0x3A) INIT({0x00, 0x00, 0x54, 0x41, 0x53, 0x4B, 0x54, 0x59, 0x50, 0x45, 0x5F, 0x47, 0x45, 0x4E, 0x45, 0x52, 0x49, 0x43, 0x50, 0x48, 0x59, 0x53, 0x49, 0x43, 0x53, 0x4D, 0x41, 0x47, 0x49, 0x43, 0x4F, 0x42, 0x4A, 0x00, 0x47, 0x65, 0x6E, 0x65, 0x72, 0x69, 0x63, 0x50, 0x68, 0x79, 0x73, 0x69, 0x63, 0x73, 0x4D, 0x61, 0x67, 0x69, 0x63, 0x4F, 0x62, 0x6A});
DATA(53C46C) uint16_t gGenericPhysicsObjType INIT(385);
PAD(igd, 53C46C, 53C4AA, 0x2);
BSS(57C13C) void *gGenericPhysicsObjParams;
