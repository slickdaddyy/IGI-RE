#include "hook.h"
#include "igi.h"
#include "comprun/comprun.h"

FN_EXTERN(GunCABarrel_Follow);
FN_EXTERN(Human_ClearResult);

ORIG(0x4015F0, void, Task_Delete, (void *task));
ORIG(0x401400, void, TaskType_SetHandler, (uint16_t type, uint8_t slot, void *fn));
ORIG(0x401530, void, TaskType_SetHandler2, (uint16_t type, int slot, void *fn));
ORIG(0x401900, uint16_t, TaskType_New, (const char *name, int size, uint16_t parent, int pool_count));
ORIG(0x401A20, void, TaskType_Free, (uint16_t type));
ORIG(0x416830, uint8_t, Game_GetEvent7, (void));
ORIG(0x4AF810, void, Warning_Show, (const char *fmt, ...));
ORIG(0x4B8810, void, Script_RegisterConstant, (const char *name, int value));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));
ORIG(0x4C56F0, uint8_t, RigidDynCubeObj_GetEvent2, (void));
ORIG(0x4C5710, void, RigidDynCubeObj_Init, (void *world, void *obj, void **model));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4D9FA0, uint16_t, MagicObj_GetType, (void));
ORIG(0x4E0930, Task *, MagicObjContainer_New, (void *parent, void *obj));
ORIG(0x4F1A70, uint8_t, Param_GetChangedEvent, (void));

#define Gunx2_Handler472880 (FN(0x472880, GunCABarrel_Follow))
#define Gunx2_Handler45E3D0 (FN(0x45E3D0, Human_ClearResult))

/* the argument of the create handler of the vehicle parts */
typedef struct PartCreateArg {
    const char *model;
    void *world;
    Vec3d pos;
    Vec3d offset;
    Matrix matrix;
    Matrix local;
    Task *owner;
} PartCreateArg;

/* "Gunx2" (0x138 bytes), a twin gun mounted on a vehicle */
typedef struct Gunx2 {
    RigidDynCubeObj obj;
    Vec3d offset; /* 0xF0 */
    Matrix local; /* 0x108 */
    Task *owner;  /* 0x130 */
    int unk134;
} Gunx2;

_Static_assert(offsetof(PartCreateArg, owner) == 0x88, "PartCreateArg");
_Static_assert(sizeof(Gunx2) == 0x138, "Gunx2");

extern uint16_t gGunx2Type;

void Gunx2_Create(Gunx2 *t, PartCreateArg *a);
void Gunx2_Run(Gunx2 *t);
void Gunx2_Destroy(Gunx2 *t);

void Gunx2_Init(void)
{
    gGunx2Type = TaskType_New("Gunx2", sizeof(Gunx2), MagicObj_GetType(), 0);
    TaskType_SetHandler(gGunx2Type, 0, Gunx2_Create);
    TaskType_SetHandler2(gGunx2Type, 1, Gunx2_Run);
    TaskType_SetHandler2(gGunx2Type, 2, Gunx2_Destroy);
    TaskType_SetHandler(gGunx2Type, Param_GetChangedEvent(), TASK_NOTHING);
    TaskType_SetHandler(gGunx2Type, RigidDynCubeObj_GetEvent2(), Gunx2_Handler472880);
    TaskType_SetHandler(gGunx2Type, Game_GetEvent7(), Gunx2_Handler45E3D0);
    Script_RegisterConstant("TASKTYPE_GUNX2", gGunx2Type);
}
HOOK(0x46DD00, Gunx2_Init);

void Gunx2_Create(Gunx2 *t, PartCreateArg *a)
{
    void **model = VirModel_Find(a->model);

    if (!model)
        Warning_Show("VirModel \"%s\" not available", a->model);
    t->offset = a->offset;
    t->local = a->local;
    RigidDynCubeObj_Init(a->world, t, model);
    t->obj.hidden = 0;
    t->obj.matrix = a->matrix;
    t->owner = a->owner;
    t->obj.attached = MagicObjContainer_New(NULL, a->owner);
    RigidDynCubeObj_CreateMagicObjs(t->obj.attached, t, t->owner);
}
HOOK(0x46DDC0, Gunx2_Create);

void Gunx2_Run(Gunx2 *t)
{
    CALL_CHILDREN_RUN(t->obj.attached);
}
HOOK(0x46DE70, Gunx2_Run);

void Gunx2_Destroy(Gunx2 *t)
{
    Task_Delete(t->obj.attached);
}
HOOK(0x46DEE0, Gunx2_Destroy);

void Gunx2_Close(void)
{
    TaskType_Free(gGunx2Type);
}
HOOK(0x46DF00, Gunx2_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
DATA(53FE68) uint16_t gGunx2Type INIT(385);
PAD(igd, 53FE68, 53FE6A, 0x2);
