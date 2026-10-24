/* EditRigidObjAttachedToBone: an editor model placed at an offset from a bone. */
#pragma GCC optimize("excess-precision=standard")

#include <string.h>

#include "hook.h"
#include "editobj/editobj.h"

/* "EditRigidObjAttachedToBone" (0x138), a RigidDynCubeObj */
typedef struct EditRigidObjAttachedToBone {
    Obj obj;
    Vec3f offset;
    Matrix local;
    int unk124;
    char model[16];     /* "*name" when the model was not found */
} EditRigidObjAttachedToBone;

_Static_assert(offsetof(EditRigidObjAttachedToBone, model) == 0x128, "model");
_Static_assert(sizeof(EditRigidObjAttachedToBone) == 0x138, "EditRigidObjAttachedToBone");

ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count, int unk));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x4A53B3, int, crt_sprintf, (char *buf, const char *fmt, ...));
ORIG(0x4C1780, void, Debug_Log, (void *task));
ORIG(0x4C1830, void, QTaskList_Free, (void *list));
ORIG(0x4C48C0, uint16_t, RigidDynCubeObj_GetType, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4C7140, void, DynCube_Add, (void *world, void *obj, Vec3d *pos, float radius, int unk));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D0950, long double, VirModel_GetRadius, (void **model, int unk));
ORIG(0x4F0F60, void *, Param_Real32x3, (void));
ORIG(0x4F0F70, void *, Param_Real32x9, (void));
ORIG(0x4F0FA0, void *, Param_Int32, (void));
ORIG(0x4F0FD0, void *, Param_String16, (void));
ORIG(0x4F1950, void *, ParamTable_New, (int count));
ORIG(0x4F1A60, uint8_t, Param_GetTableEvent, (void));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));
ORIG(0x4F1A80, void, ParamTable_Add, (void *table, void *base, void *field, void *type, int flags, void *range));

#define MAGICOBJ_DELETE ((void *)0x51B940)

#define gAttachedToBoneParams VAR(void *, 0xBA8080)
#define gAttachedToBoneType VAR(uint16_t, 0xBA8084)

void EditRigidObjAttachedToBone_Create(EditRigidObjAttachedToBone *o)
{
    Vec3d zero = {0, 0, 0};

    o->offset = (Vec3f){0.0f, 0.0f, 0.0f};
    o->local = (Matrix){{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}, 0};
    o->unk124 = 0;
    memcpy(o->model, "*", 2);
    RigidDynCubeObj_Init(gCurrentWorld, o, NULL);
    DynCube_Add(o->obj.world, o, &zero, VirModel_GetRadius(o->obj.model, 2), 2);
}
HOOK(0x504AD0, EditRigidObjAttachedToBone_Create);

/* Loads the model by name; a missing one gets a '*' in front of its name */
void EditRigidObjAttachedToBone_Changed(EditRigidObjAttachedToBone *o)
{
    char name[16];

    if (o->model[0] != '*' && o->model[0]) {
        void **model = VirModel_Find(o->model);

        if (model) {
            o->obj.model = model;
            Debug_Log(o);
            return;
        }
        strcpy(name, o->model);
        crt_sprintf(o->model, "*%s", name);
    }
    Debug_Log(o);
}
HOOK(0x504BB0, EditRigidObjAttachedToBone_Changed);

void EditRigidObjAttachedToBone_GetParams(void *task, void **out)
{
    *out = gAttachedToBoneParams;
}
HOOK(0x504C40, EditRigidObjAttachedToBone_GetParams);

void EditRigidObjAttachedToBone_Open(void)
{
    EditRigidObjAttachedToBone o;
    uint16_t t = TaskType_New("EditRigidObjAttachedToBone", sizeof(o), RigidDynCubeObj_GetType(), 0, 0);

    gAttachedToBoneType = t;
    TaskType_SetHandler(t, 0, EditRigidObjAttachedToBone_Create);
    TaskType_SetHandler2(gAttachedToBoneType, 2, MAGICOBJ_DELETE);
    TaskType_SetHandler2(gAttachedToBoneType, 1, Debug_Log);
    TaskType_SetHandler(gAttachedToBoneType, Param_GetChangedEvent(), EditRigidObjAttachedToBone_Changed);
    TaskType_SetHandler(gAttachedToBoneType, Param_GetTableEvent(), EditRigidObjAttachedToBone_GetParams);
    gAttachedToBoneParams = ParamTable_New(4);
    ParamTable_Add(gAttachedToBoneParams, &o, &o.offset, Param_Real32x3(), 7, NULL);
    ParamTable_Add(gAttachedToBoneParams, &o, &o.local, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gAttachedToBoneParams, &o, &o.unk124, Param_Int32(), 7, NULL);
    ParamTable_Add(gAttachedToBoneParams, &o, o.model, Param_String16(), 7, NULL);
}
HOOK(0x504C50, EditRigidObjAttachedToBone_Open);

void EditRigidObjAttachedToBone_Close(void)
{
    TaskType_Free(gAttachedToBoneType);
    QTaskList_Free(gAttachedToBoneParams);
}
HOOK(0x504D90, EditRigidObjAttachedToBone_Close);

uint16_t EditRigidObjAttachedToBone_GetType(void)
{
    return gAttachedToBoneType;
}
HOOK(0x504DB0, EditRigidObjAttachedToBone_GetType);
