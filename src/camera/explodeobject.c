/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "camera/camera.h"
#include "param/paramtypes.h"
#include "magicobj/magicobjtypes.h"
#include "glass/glasstypes.h"

FN_EXTERN(Task_Nothing);

/* a placed object that explodes once its damage reaches its health, or when
   its script value or timer says so (0x238 bytes, parent SectorInteriorObject) */
typedef struct ExplodeObject {
    Task task;
    Vec3d pos;
    char unk38[0x30];
    void *world;
    void **model;
    Matrix matrix;
    float scale[3];
    char unkA4[0x38];
    int unkDC;
    char unkE0[0x10];
    Vec3d place;
    Matrix placeMatrix;
    Vec3d offset;
    Matrix local;
    char modelName[16];
    char deadModel[16];
    char exploded;
    char registered;
    char pad192[2];
    VarString trigger;
    char triggered;
    char pad1E5[3];
    float damage;
    float health;
    float maxHealth;
    int timer;
    float blastRadius;
    float unk1FC;
    float unk200;
    float unk204;
    float delay;
    int unk20C;
    int unk210;
    char sound[16];
    char pad224[0x10];
    char inWorld;
    char pad235[3];
} ExplodeObject;

/* the argument of the World event 0 handler */
typedef struct JoinArg {
    int unk0;
    void *world;
} JoinArg;

_Static_assert(offsetof(ExplodeObject, unkDC) == 0xDC, "ExplodeObject.unkDC");
_Static_assert(offsetof(ExplodeObject, local) == 0x148, "ExplodeObject.local");
_Static_assert(offsetof(ExplodeObject, trigger) == 0x194, "ExplodeObject.trigger");
_Static_assert(offsetof(ExplodeObject, sound) == 0x214, "ExplodeObject.sound");
_Static_assert(sizeof(ExplodeObject) == 0x238, "ExplodeObject");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x414E20, void, Model_GetPoint, (Vec3d *out, void *obj, int i));
ORIG(0x416840, uint8_t, Game_GetEvent9, (void));
ORIG(0x416860, uint8_t, Game_GetEvent11, (void));
ORIG(0x4168D0, void *, Game_GetTaskList3, (void));
ORIG(0x416C90, long double, Game_BlastFalloff, (void *blast, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4453E0, void, Switch_UpdateLocal, (void *obj));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4B7EE0, void, Script_RegisterBool8, (const char *name, char *value));
ORIG(0x4B80A0, void, Script_RemoveSymbol, (const char *name));
ORIG(0x4C1790, void, QTaskList_Add, (void *list, void *item));
ORIG(0x4C17C0, void, QTaskList_Remove, (void *list, void *item));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int category));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4D0D80, int, Mesh3D_GetEvent5, (void));
ORIG(0x4DCCB0, void, Sector_AddObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));
ORIG(0x4DCF00, void, Sector_MoveObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E0830, uint16_t, SectorInteriorObject_GetType, (void));
ORIG(0x4F0F50, void *, Param_Real32, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FC0, void *, Param_VarString, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
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
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

extern uint16_t gExplodeObjectType;
extern void *gExplodeObjectParams;
extern void *gCurrentWorld;
extern float gRealRange[];
extern int gIntRange[];

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

static const float kUnits = 4096.0f;
static const float kFrames = 30.0f;

/* the largest scale factor, compared like the original */
static float max_scale(const ExplodeObject *o)
{
    float m = o->scale[2] >= o->scale[1] ? o->scale[2] : o->scale[1];

    return o->scale[0] > m ? o->scale[0] : m;
}

uint16_t ExplodeObject_GetType(void)
{
    return gExplodeObjectType;
}
HOOK(0x481340, ExplodeObject_GetType);

void *ExplodeObject_GetParams(void)
{
    return gExplodeObjectParams;
}
HOOK(0x481350, ExplodeObject_GetParams);

void ExplodeObject_Setup(ExplodeObject *o, float health, float blastRadius, float unk1FC, float unk204, float delay,
                         int unk20C, int unk210, const char *sound)
{
    o->maxHealth = health;
    o->blastRadius = blastRadius;
    o->unk1FC = unk1FC;
    o->unk204 = unk204;
    o->unk200 = unk204;
    o->delay = delay;
    o->unk20C = unk20C;
    o->unk210 = unk210;
    strcpy(o->sound, sound);
}
HOOK(0x481360, ExplodeObject_Setup);

void ExplodeObject_CreateHandler(ExplodeObject *o);
void ExplodeObject_RunHandler(ExplodeObject *o);
void ExplodeObject_DeleteHandler(ExplodeObject *o);
void ExplodeObject_GetParamTableHandler(void *task, void **out);
void ExplodeObject_ParamChangedHandler(ExplodeObject *o);
void ExplodeObject_HitHandler(ExplodeObject *o, HitArg *arg);
void ExplodeObject_BlastHandler(ExplodeObject *o, void *blast);
void ExplodeObject_RegisterVarsHandler(ExplodeObject *o);
void ExplodeObject_UnregisterVarsHandler(ExplodeObject *o);
void ExplodeObject_MoveHandler(ExplodeObject *o, MoveArg *arg);
void ExplodeObject_JoinWorldHandler(ExplodeObject *o, JoinArg *arg);
void ExplodeObject_LeaveWorldHandler(ExplodeObject *o, void **world);

void ExplodeObject_Open(void)
{
    ExplodeObject o;

    gExplodeObjectType = TaskType_New("ExplodeObject", sizeof(ExplodeObject), SectorInteriorObject_GetType(), 0, 0);
    TaskType_SetHandler(gExplodeObjectType, 0, ExplodeObject_CreateHandler);
    TaskType_SetHandler2(gExplodeObjectType, 1, ExplodeObject_RunHandler);
    TaskType_SetHandler2(gExplodeObjectType, 2, ExplodeObject_DeleteHandler);
    TaskType_SetHandler(gExplodeObjectType, Param_GetTableEvent(), ExplodeObject_GetParamTableHandler);
    TaskType_SetHandler(gExplodeObjectType, Param_GetChangedEvent(), ExplodeObject_ParamChangedHandler);
    TaskType_SetHandler(gExplodeObjectType, Game_GetEvent9(), ExplodeObject_HitHandler);
    TaskType_SetHandler(gExplodeObjectType, Game_GetEvent11(), ExplodeObject_BlastHandler);
    TaskType_SetHandler2(gExplodeObjectType, Level_GetEventDA8(), ExplodeObject_RegisterVarsHandler);
    TaskType_SetHandler2(gExplodeObjectType, Level_GetEventDAC(), ExplodeObject_UnregisterVarsHandler);
    TaskType_SetHandler(gExplodeObjectType, RigidDynCubeObj_GetEvent4(), ExplodeObject_MoveHandler);
    TaskType_SetHandler(gExplodeObjectType, 6, TASK_NOTHING);
    TaskType_SetHandler(gExplodeObjectType, World_GetEvent1(), ExplodeObject_LeaveWorldHandler);
    TaskType_SetHandler(gExplodeObjectType, World_GetEvent0(), ExplodeObject_JoinWorldHandler);
    gRealRange[0] = 0.0f;
    gRealRange[1] = 100.0f;
    gIntRange[0] = 0;
    gIntRange[1] = 1000;
    gExplodeObjectParams = ParamTable_New(13);
    ParamTable_Add(gExplodeObjectParams, &o, &o.place, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gExplodeObjectParams, &o, &o.placeMatrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gExplodeObjectParams, &o, o.modelName, Param_String16(), 7, NULL);
    ParamTable_Add(gExplodeObjectParams, &o, o.deadModel, Param_String16(), 7, NULL);
    ParamTable_Add(gExplodeObjectParams, &o, &o.maxHealth, Param_Real32(), 7, gRealRange);
    ParamTable_Add(gExplodeObjectParams, &o, &o.blastRadius, Param_Real32(), 7, gRealRange);
    ParamTable_Add(gExplodeObjectParams, &o, &o.unk1FC, Param_Real32(), 7, gRealRange);
    ParamTable_Add(gExplodeObjectParams, &o, &o.unk204, Param_Real32(), 7, gRealRange);
    ParamTable_Add(gExplodeObjectParams, &o, &o.delay, Param_Real32(), 7, gRealRange);
    ParamTable_Add(gExplodeObjectParams, &o, &o.unk20C, Param_Int32(), 7, gIntRange);
    ParamTable_Add(gExplodeObjectParams, &o, &o.unk210, Param_Int32(), 7, gIntRange);
    ParamTable_Add(gExplodeObjectParams, &o, &o.trigger, Param_VarString(), 7, NULL);
    ParamTable_Add(gExplodeObjectParams, &o, o.sound, Param_String16(), 7, NULL);
}
HOOK(0x4813E0, ExplodeObject_Open);

static const Matrix kIdentity = {{1, 0, 0, 0, 1, 0, 0, 0, 1}, 0};

void ExplodeObject_CreateHandler(ExplodeObject *o)
{
    Vec3d zero = {0, 0, 0};

    QTaskList_Add(Game_GetTaskList3(), o);
    RigidDynCubeObj_Init(gCurrentWorld, o, NULL);
    DynCube_Add(gCurrentWorld, o, &zero, VirModel_GetRadius(o->model) * max_scale(o), 2);
    Sector_AddObject(o->world, o, DynCubeObj_GetPosition(o));
    o->matrix = kIdentity;
    o->placeMatrix = kIdentity;
    o->modelName[0] = 0;
    o->deadModel[0] = 0;
    o->triggered = 0;
    o->exploded = 0;
    o->registered = 0;
    o->damage = 0.0f;
    o->maxHealth = 1.0f;
    o->blastRadius = 2.0f;
    o->unk1FC = 1.5f;
    o->unk204 = 1.0f;
    o->unk200 = 1.0f;
    o->unk20C = 3;
    o->unk210 = 3;
    o->delay = 1.0f;
    o->timer = -1;
    VarString_Init(&o->trigger);
    o->sound[0] = 0;
    o->inWorld = 1;
}
HOOK(0x481780, ExplodeObject_CreateHandler);

void ExplodeObject_Explode(ExplodeObject *o);

void ExplodeObject_RunHandler(ExplodeObject *o)
{
    CALL_CHILDREN(&o->task, 1);
    if (o->exploded)
        return;
    if (VarString_HasExpr(&o->trigger))
        o->triggered = (char)round_ll(VarString_EvalReal(&o->trigger, o));
    if (o->triggered || !o->timer) {
        ExplodeObject_Explode(o);
        if (!VirModel_Find(o->deadModel))
            return;
    }
    if (o->timer > 0)
        o->timer--;
}
HOOK(0x481970, ExplodeObject_RunHandler);

/* swaps to the destroyed model and spawns the explosion */
void ExplodeObject_Explode(ExplodeObject *o)
{
    void **model = VirModel_Find(o->deadModel);
    Vec3d p = o->pos;

    if (model) {
        RigidDynCubeObj_SetModel(o, model);
        o->unkDC = 0;
        CALL_CHILDREN(&o->task, Mesh3D_GetEvent5());
    }
    Model_GetPoint(&p, o, 0);
    p.x = p.x + o->pos.x;
    p.y = p.y + o->pos.y;
    p.z = p.z + o->pos.z;
    Explosion_Spawn(&p, 1, 0, o->unk200, o->blastRadius * kUnits, o->unk1FC * kUnits, 6144.0f, o->unk20C, o->unk210,
                    NULL, "1009_01_1", o->sound, 0x15, 2, 102400.0f, 0);
    o->exploded = 1;
}
HOOK(0x481A60, ExplodeObject_Explode);

void ExplodeObject_DeleteHandler(ExplodeObject *o)
{
    QTaskList_Remove(Game_GetTaskList3(), o);
    if (Task_GetId(o) >= 0) {
        TaskFn f = gTaskHandlers[Level_GetEventDAC()][o->task.type];

        if (f)
            ((void (*)(void *))f)(o);
    }
    VarString_Free(&o->trigger);
    Sector_RemoveObject(o);
    DynCube_Remove(o->world, o);
}
HOOK(0x481BC0, ExplodeObject_DeleteHandler);

void ExplodeObject_GetParamTableHandler(void *task, void **out)
{
    *out = gExplodeObjectParams;
}
HOOK(0x481C30, ExplodeObject_GetParamTableHandler);

/* a missing model gets its name marked "** name **" */
static void mark_missing(char *name)
{
    char msg[128];
    char copy[128];

    crt_sprintf(msg, "Model \"%s\" not available", name);
    strcpy(copy, name);
    crt_sprintf(name, "** %s **", copy);
}

void ExplodeObject_ParamChangedHandler(ExplodeObject *o)
{
    void **model = VirModel_Find(o->modelName);
    void **dead = VirModel_Find(o->deadModel);

    if (!model && o->modelName[0] && o->modelName[1] != '*')
        mark_missing(o->modelName);
    if (!dead && o->deadModel[0] && o->deadModel[1] != '*')
        mark_missing(o->deadModel);
    RigidDynCubeObj_SetModel(o, model);
    DynCube_Move(o->world, o, &o->place, VirModel_GetRadius(o->model) * max_scale(o));
    Sector_MoveObject(o->world, o, DynCubeObj_GetPosition(o));
    o->matrix = o->placeMatrix;
    o->triggered = 0;
    o->exploded = 0;
    o->damage = 0.0f;
    o->timer = -1;
    o->health = o->maxHealth;
    o->unk200 = o->unk204;
    VarString_Bind(&o->trigger, o);
    Switch_UpdateLocal(o);
}
HOOK(0x481C40, ExplodeObject_ParamChangedHandler);

void ExplodeObject_HitHandler(ExplodeObject *o, HitArg *arg)
{
    long double d;

    if (o->exploded)
        return;
    d = arg->damage + o->damage;
    o->damage = d;
    if (d >= o->health)
        o->timer = 0;
}
HOOK(0x481E60, ExplodeObject_HitHandler);

void ExplodeObject_BlastHandler(ExplodeObject *o, void *blast)
{
    long double d;

    if (o->exploded || !o->inWorld)
        return;
    d = Game_BlastFalloff(blast, o, &o->pos, VirModel_GetRadius(o->model) * max_scale(o), 1) + o->damage;
    o->damage = d;
    if (d >= o->health)
        o->timer = (int)round_ll(o->delay * kFrames);
}
HOOK(0x481EA0, ExplodeObject_BlastHandler);

void ExplodeObject_RegisterVarsHandler(ExplodeObject *o)
{
    char name[64];
    char buf[128];

    if (o->registered)
        return;
    o->registered = 1;
    Task_GetName(name, o);
    crt_sprintf(buf, "%s.isExploded", name);
    Script_RegisterBool8(buf, &o->exploded);
}
HOOK(0x481F90, ExplodeObject_RegisterVarsHandler);

void ExplodeObject_UnregisterVarsHandler(ExplodeObject *o)
{
    char name[64];
    char buf[128];

    if (!o->registered)
        return;
    Task_GetName(name, o);
    crt_sprintf(buf, "%s.isExploded", name);
    Script_RemoveSymbol(buf);
    o->registered = 0;
}
HOOK(0x481FF0, ExplodeObject_UnregisterVarsHandler);

/* follows the parent: offset turned by its matrix, matrix times local */
void ExplodeObject_MoveHandler(ExplodeObject *o, MoveArg *arg)
{
    const float *m = arg->matrix->m;
    const float *l = o->local.m;
    Vec3d p;
    Matrix r;
    MoveArg child;

    p.x = m[2] * o->offset.z + m[1] * o->offset.y + m[0] * o->offset.x;
    p.y = m[5] * o->offset.z + m[4] * o->offset.y + m[3] * o->offset.x;
    p.z = m[8] * o->offset.z + m[7] * o->offset.y + m[6] * o->offset.x;
    o->place = p;
    o->place.x = arg->pos->x + o->place.x;
    o->place.y = arg->pos->y + o->place.y;
    o->place.z = arg->pos->z + o->place.z;
    m = arg->matrix->m;
    r.m[0] = m[0] * l[0] + m[2] * l[6] + m[1] * l[3];
    r.m[1] = m[0] * l[1] + m[2] * l[7] + m[1] * l[4];
    r.m[2] = m[0] * l[2] + m[2] * l[8] + m[1] * l[5];
    r.m[3] = m[5] * l[6] + m[3] * l[0] + m[4] * l[3];
    r.m[4] = m[5] * l[7] + m[3] * l[1] + m[4] * l[4];
    r.m[5] = m[5] * l[8] + m[3] * l[2] + m[4] * l[5];
    r.m[6] = m[8] * l[6] + m[6] * l[0] + m[7] * l[3];
    r.m[7] = m[8] * l[7] + m[6] * l[1] + m[7] * l[4];
    r.m[8] = m[8] * l[8] + m[6] * l[2] + m[7] * l[5];
    r.count = arg->matrix->count + o->local.count + 1;
    o->placeMatrix = r;
    DynCube_Move(o->world, o, &o->place, VirModel_GetRadius(o->model) * max_scale(o));
    o->matrix = o->placeMatrix;
    child.pos = &o->place;
    child.matrix = &o->placeMatrix;
    CALL_CHILDREN(&o->task, RigidDynCubeObj_GetEvent4(), &child);
}
HOOK(0x482050, ExplodeObject_MoveHandler);

void ExplodeObject_JoinWorldHandler(ExplodeObject *o, JoinArg *arg)
{
    DynCube_Add(arg->world, o, &o->pos, VirModel_GetRadius(o->model) * max_scale(o), 2);
    Sector_AddObject(arg->world, o, DynCubeObj_GetPosition(o));
    CALL_CHILDREN(&o->task, World_GetEvent0(), arg);
    o->inWorld = 1;
}
HOOK(0x482390, ExplodeObject_JoinWorldHandler);

void ExplodeObject_LeaveWorldHandler(ExplodeObject *o, void **world)
{
    CALL_CHILDREN(&o->task, World_GetEvent1(), world);
    Sector_RemoveObject(o);
    DynCube_Remove(*world, o);
    o->inWorld = 0;
}
HOOK(0x4824C0, ExplodeObject_LeaveWorldHandler);

void ExplodeObject_Close(void)
{
    QTaskList_Free(gExplodeObjectParams);
    TaskType_Free(gExplodeObjectType);
}
HOOK(0x482560, ExplodeObject_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(54097C) uint16_t gExplodeObjectType INIT(385);
PAD(igd, 54097C, 54097E, 0x2);
PAD(igb, 5C1170, 5C1170, 0x8);
ALIAS(gRealRange, pad_5C1170, 0);
PAD(igb, 5C1170, 5C1178, 0xC);
ALIAS(gIntRange, pad_5C1178, 0);
BSS(5C1170) void *gExplodeObjectParams;
