/* float expressions are kept in x87 precision and rounded at each assignment,
   like the original */
#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "vehbase/vehbase.h"

ORIG(0x4012A0, Task *, Task_New, (void *parent, int type, void *arg));
ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x401CC0, RigidObj *, Task_FindAncestor, (void *task, uint16_t type));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C3440, Vec3d *, DynCubeObj_GetPosition, (void *obj));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (Task *parts, void *obj, void *owner));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C5800, uint8_t, RigidDynCubeObj_GetEvent4, (void));
ORIG(0x4C6940, void, DynCube_Move, (void *world, void *obj, Vec3d *pos, float radius));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int n));
ORIG(0x4C7560, void, DynCube_Remove, (void *world, void *obj));
ORIG(0x4CEA10, uint8_t, World_GetEvent0, (void));
ORIG(0x4CEA20, uint8_t, World_GetEvent1, (void));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model));
ORIG(0x4DCCB0, void, Sector_AddObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4DCDD0, void, Sector_RemoveObject, (void *obj));
ORIG(0x4DCF00, void, Sector_MoveObject, (void *world, void *obj, Vec3d *pos));
ORIG(0x4E0830, uint16_t, SectorInteriorObject_GetType, (void));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *owner));
ORIG(0x4F0F40, void *, Param_RGB, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1010, void *, Param_ObjectPos, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));
ORIG(0x4F1C90, void *, Param_Range02, (void));

#define gEditRigidObjType VAR(uint16_t, 0x54E1AC)
#define gEditRigidObjParams VAR(void *, 0xA76C14)
#define gWorld VAR(void *, 0xA44344)

/* out = a * b. Each element sums a[i][k] * b[k][j] for k = 0..2; last[i] is
   the k whose product row i adds last. */
static inline void mat_mul(Matrix *out, const Matrix *a, const Matrix *b, const char *last)
{
    Matrix n;
    int i, j;

    for (i = 0; i < 3; i++) {
        int k0 = last[i] == 0 ? 1 : 0, k1 = last[i] == 2 ? 1 : 2, k2 = last[i];

        for (j = 0; j < 3; j++)
            n.m[i * 3 + j] = (a->m[i * 3 + k0] * b->m[k0 * 3 + j] + a->m[i * 3 + k1] * b->m[k1 * 3 + j]) +
                             a->m[i * 3 + k2] * b->m[k2 * 3 + j];
    }
    n.unk24 = a->unk24 + b->unk24 + 1;
    *out = n;
}

static float radius(EditRigidObj *o)
{
    float s = max_scale(&o->obj);

    return VirModel_GetRadius(o->obj.model) * s;
}

/* RigidDynCubeObj move event: follow the object this one sits on */
void EditRigidObj_Move(EditRigidObj *o, MoveMsg *msg)
{
    static const char last[3] = {1, 1, 1};
    const float *m = msg->matrix->m;
    MoveMsg out;
    Vec3d p;

    p.x = (m[2] * o->offset.z + m[1] * o->offset.y) + m[0] * o->offset.x;
    p.y = (m[5] * o->offset.z + m[4] * o->offset.y) + m[3] * o->offset.x;
    p.z = (m[8] * o->offset.z + m[7] * o->offset.y) + m[6] * o->offset.x;
    o->pos = p;
    o->pos.x = msg->pos->x + o->pos.x;
    o->pos.y = msg->pos->y + o->pos.y;
    o->pos.z = msg->pos->z + o->pos.z;
    mat_mul(&o->matrix, msg->matrix, &o->local, last);
    DynCube_Move(o->obj.room, o, &o->pos, radius(o));
    o->obj.matrix = o->matrix;
    out.pos = &o->pos;
    out.matrix = &o->matrix;
    BROADCAST(&o->obj.task, RigidDynCubeObj_GetEvent4(), &out);
}
HOOK(0x4F2400, EditRigidObj_Move);

void EditRigidObj_Create(EditRigidObj *o);
void EditRigidObj_Run(EditRigidObj *o);
void EditRigidObj_Delete(EditRigidObj *o);
void EditRigidObj_GetParams(void *task, void **out);
void EditRigidObj_ParamsChanged(EditRigidObj *o, void *arg);
void EditRigidObj_Activate(EditRigidObj *o, WorldMsg *msg);
void EditRigidObj_Deactivate(EditRigidObj *o, WorldMsg *msg);

void EditRigidObj_Open(void)
{
    EditRigidObj o;
    void *range;

    gEditRigidObjType = TaskType_New("EditRigidObj", 0x1A0, SectorInteriorObject_GetType(), 0);
    TaskType_SetHandler(gEditRigidObjType, 0, EditRigidObj_Create);
    TaskType_SetHandler2(gEditRigidObjType, 1, EditRigidObj_Run);
    TaskType_SetHandler2(gEditRigidObjType, 2, EditRigidObj_Delete);
    TaskType_SetHandler(gEditRigidObjType, Param_GetTableEvent(), EditRigidObj_GetParams);
    TaskType_SetHandler(gEditRigidObjType, Param_GetChangedEvent(), EditRigidObj_ParamsChanged);
    TaskType_SetHandler(gEditRigidObjType, RigidDynCubeObj_GetEvent4(), EditRigidObj_Move);
    TaskType_SetHandler(gEditRigidObjType, 6, TASK_NOTHING);
    TaskType_SetHandler(gEditRigidObjType, World_GetEvent1(), EditRigidObj_Deactivate);
    TaskType_SetHandler(gEditRigidObjType, World_GetEvent0(), EditRigidObj_Activate);
    gEditRigidObjParams = ParamTable_New(5);
    ParamTable_Add(gEditRigidObjParams, &o, &o.pos, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gEditRigidObjParams, &o, &o.matrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gEditRigidObjParams, &o, o.model, Param_String16(), 7, NULL);
    range = Param_Range02();
    ParamTable_Add(gEditRigidObjParams, &o, &o.rgb[0], Param_RGB(), 7, range);
    range = Param_Range02();
    ParamTable_Add(gEditRigidObjParams, &o, &o.rgb[1], Param_RGB(), 7, range);
    Script_RegisterConstant("TASKTYPE_EDITRIGIDOBJ", gEditRigidObjType);
}
HOOK(0x4F2740, EditRigidObj_Open);

void EditRigidObj_Create(EditRigidObj *o)
{
    Vec3d zero = {0, 0, 0};

    RigidDynCubeObj_Init(gWorld, o, NULL);
    DynCube_Add(gWorld, o, &zero, radius(o), 2);
    Sector_AddObject(gWorld, o, DynCubeObj_GetPosition(o));
    memset(&o->pos, 0, sizeof(o->pos));
    memset(&o->matrix, 0, sizeof(o->matrix));
    o->matrix.m[0] = o->matrix.m[4] = o->matrix.m[8] = 1.0f;
    o->model[0] = 0;
    o->rgb[0].x = o->rgb[0].y = o->rgb[0].z = 1.0f;
    memset(&o->rgb[1], 0, sizeof(o->rgb[1]));
    o->active = 1;
}
HOOK(0x4F2930, EditRigidObj_Create);

void EditRigidObj_Run(EditRigidObj *o)
{
    Task *parts = o->obj.parts;

    if (parts)
        HANDLER(1, parts->type)(parts);
    BROADCAST(&o->obj.task, 1);
}
HOOK(0x4F2AC0, EditRigidObj_Run);

void EditRigidObj_Delete(EditRigidObj *o)
{
    if (o->obj.parts)
        Task_Delete(o->obj.parts);
    Sector_RemoveObject(o);
    DynCube_Remove(o->obj.room, o);
}
HOOK(0x4F2B50, EditRigidObj_Delete);

void EditRigidObj_GetParams(void *task, void **out)
{
    *out = gEditRigidObjParams;
}
HOOK(0x4F2B80, EditRigidObj_GetParams);

void EditRigidObj_Attach(EditRigidObj *o);

void EditRigidObj_ParamsChanged(EditRigidObj *o, void *arg)
{
    void **model;

    o->obj.rgb[0] = o->rgb[0];
    o->obj.rgb[1] = o->rgb[1];
    o->obj.matrix = o->matrix;
    model = VirModel_Find(o->model);
    if (model)
        RigidDynCubeObj_SetModel(o, model);
    DynCube_Move(o->obj.room, o, &o->pos, radius(o));
    Sector_MoveObject(o->obj.room, o, &o->pos);
    if (o->obj.parts)
        Task_Delete(o->obj.parts);
    o->obj.parts = MagicObjContainer_New(NULL, o);
    RigidDynCubeObj_CreateMagicObjs(o->obj.parts, o, o);
    BROADCAST(o->obj.parts, Param_GetChangedEvent(), NULL);
    EditRigidObj_Attach(o);
}
HOOK(0x4F2B90, EditRigidObj_ParamsChanged);

/* keeps the place relative to the RigidDynCubeObj above, if any */
void EditRigidObj_Attach(EditRigidObj *o)
{
    static const char last[3] = {2, 1, 0};
    RigidObj *a = Task_FindAncestor(o, RigidDynCubeObj_GetType());
    const uint32_t *s;
    uint32_t *d;
    long double dz;
    const float *m;
    Vec3d p;

    if (!a) {
        memset(&o->offset, 0, sizeof(o->offset));
        memset(&o->local, 0, sizeof(o->local));
        o->local.m[0] = o->local.m[4] = o->local.m[8] = 1.0f;
        return;
    }
    m = a->matrix.m;
    o->offset.x = o->pos.x - a->pos.x;
    o->offset.y = o->pos.y - a->pos.y;
    dz = o->pos.z - a->pos.z;
    o->offset.z = dz;
    p.x = (m[0] * o->offset.x + m[3] * o->offset.y) + m[6] * dz;
    p.y = (m[1] * o->offset.x + m[7] * dz) + m[4] * o->offset.y;
    p.z = (m[2] * o->offset.x + m[8] * dz) + m[5] * o->offset.y;
    o->offset = p;
    /* the local matrix starts as the transpose of the ancestor's */
    s = (const uint32_t *)&a->matrix;
    d = (uint32_t *)&o->local;
    d[0] = s[0], d[1] = s[3], d[2] = s[6];
    d[3] = s[1], d[4] = s[4], d[5] = s[7];
    d[6] = s[2], d[7] = s[5], d[8] = s[8];
    d[9] = s[9];
    mat_mul(&o->local, &o->matrix, &o->local, last);
}
HOOK(0x4F2D50, EditRigidObj_Attach);

void EditRigidObj_Activate(EditRigidObj *o, WorldMsg *msg)
{
    DynCube_Add(msg->world, o, &o->pos, radius(o), 2);
    Sector_AddObject(msg->world, o, DynCubeObj_GetPosition(o));
    BROADCAST(&o->obj.task, World_GetEvent0(), msg);
    if (o->obj.parts)
        BROADCAST(o->obj.parts, World_GetEvent0(), msg);
    o->active = 1;
}
HOOK(0x4F30A0, EditRigidObj_Activate);

void EditRigidObj_Deactivate(EditRigidObj *o, WorldMsg *msg)
{
    BROADCAST(&o->obj.task, World_GetEvent1(), msg);
    if (o->obj.parts)
        BROADCAST(o->obj.parts, World_GetEvent1(), msg);
    Sector_RemoveObject(o);
    DynCube_Remove(msg->unk0, o);
    o->active = 0;
}
HOOK(0x4F3240, EditRigidObj_Deactivate);

void EditRigidObj_Close(void)
{
    QTaskList_Free(gEditRigidObjParams);
    TaskType_Free(gEditRigidObjType);
}
HOOK(0x4F3360, EditRigidObj_Close);

uint16_t EditRigidObj_GetType(void)
{
    return gEditRigidObjType;
}
HOOK(0x4F3380, EditRigidObj_GetType);

EditRigidObj *EditRigidObj_New(void *parent, const char *model, Vec3d *pos, Matrix *matrix, Vec3f *rgb0,
                               Vec3f *rgb1)
{
    EditRigidObj *o = (EditRigidObj *)Task_New(parent, gEditRigidObjType, NULL);

    o->pos = *pos;
    o->matrix = *matrix;
    strcpy(o->model, model);
    o->rgb[0] = *rgb0;
    o->rgb[1] = *rgb1;
    EditRigidObj_ParamsChanged(o, NULL);
    return o;
}
HOOK(0x4F3390, EditRigidObj_New);
