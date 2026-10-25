#pragma GCC optimize("excess-precision=standard")

#include "hook.h"
#include "elevator/elevator.h"

FN_EXTERN(Task_Nothing);

#define TASK_NOTHING (FN(0x4D3710, Task_Nothing))

ORIG(0x47E6F0, uint8_t, BulletHole_GetEvent, (void));
ORIG(0x4453E0, void, Switch_UpdateLocal, (void *obj));

/* a lamp that rides along with an elevator, lit while a script condition holds */
typedef struct ElevatorFloorIndicator {
    RigidDynCubeObj obj;
    Vec3d place;
    Matrix placeMatrix;
    Vec3d offset;
    Matrix local;
    char litModel[16];
    char model[16];
    char sound[16];
    char unk1A0[4];
    VarString lit;
    VarString ring;
    int pad244;
} ElevatorFloorIndicator;

_Static_assert(offsetof(ElevatorFloorIndicator, offset) == 0x130, "ElevatorFloorIndicator");
_Static_assert(offsetof(ElevatorFloorIndicator, lit) == 0x1A4, "ElevatorFloorIndicator");
_Static_assert(sizeof(ElevatorFloorIndicator) == 0x248, "ElevatorFloorIndicator");

extern uint16_t gElevatorFloorIndicatorType;
extern void *gElevatorFloorIndicatorParams;

/* follows the elevator: offset turned by its matrix, matrix times local */
void ElevatorFloorIndicator_Move(ElevatorFloorIndicator *f, MoveArg *arg)
{
    const float *m = arg->matrix->m;
    const float *l = f->local.m;
    Matrix r;
    MoveArg child;

    f->place.x = m[2] * f->offset.z + m[1] * f->offset.y + m[0] * f->offset.x;
    f->place.y = m[5] * f->offset.z + m[4] * f->offset.y + m[3] * f->offset.x;
    f->place.z = m[8] * f->offset.z + m[7] * f->offset.y + m[6] * f->offset.x;
    f->place.x = arg->pos->x + f->place.x;
    f->place.y = arg->pos->y + f->place.y;
    f->place.z = arg->pos->z + f->place.z;
    DynCube_Move((int)f->obj.world, f, &f->place, obj_radius(&f->obj));
    m = arg->matrix->m;
    r.m[0] = l[6] * m[2] + l[0] * m[0] + l[3] * m[1];
    r.m[1] = l[7] * m[2] + l[1] * m[0] + l[4] * m[1];
    r.m[2] = l[8] * m[2] + l[2] * m[0] + l[5] * m[1];
    r.m[3] = l[3] * m[4] + m[5] * l[6] + m[3] * l[0];
    r.m[4] = l[4] * m[4] + m[5] * l[7] + m[3] * l[1];
    r.m[5] = l[5] * m[4] + m[5] * l[8] + m[3] * l[2];
    r.m[6] = l[3] * m[7] + m[8] * l[6] + m[6] * l[0];
    r.m[7] = l[4] * m[7] + m[8] * l[7] + m[6] * l[1];
    r.m[8] = l[5] * m[7] + m[8] * l[8] + m[6] * l[2];
    r.count = arg->matrix->count + f->local.count + 1;
    f->placeMatrix = r;
    f->obj.matrix = r;
    child.pos = &f->place;
    child.matrix = &f->placeMatrix;
    CALL_CHILDREN(&f->obj.task, RigidDynCubeObj_GetEvent4(), &child);
}
HOOK(0x43C390, ElevatorFloorIndicator_Move);

void ElevatorFloorIndicator_Create(ElevatorFloorIndicator *f);
void ElevatorFloorIndicator_Run(ElevatorFloorIndicator *f);
void ElevatorFloorIndicator_Delete(ElevatorFloorIndicator *f);
void ElevatorFloorIndicator_GetParams(void *task, void **out);
void ElevatorFloorIndicator_Changed(ElevatorFloorIndicator *f);

void ElevatorFloorIndicator_Init(void)
{
    ElevatorFloorIndicator f;

    gElevatorFloorIndicatorType = TaskType_New("ElevatorFloorIndicator", sizeof(ElevatorFloorIndicator),
                                               RigidDynCubeObj_GetType(), 0);
    TaskType_SetHandler(gElevatorFloorIndicatorType, 0, ElevatorFloorIndicator_Create);
    TaskType_SetHandler2(gElevatorFloorIndicatorType, 1, ElevatorFloorIndicator_Run);
    TaskType_SetHandler2(gElevatorFloorIndicatorType, 2, ElevatorFloorIndicator_Delete);
    TaskType_SetHandler(gElevatorFloorIndicatorType, Param_GetTableEvent(), ElevatorFloorIndicator_GetParams);
    TaskType_SetHandler(gElevatorFloorIndicatorType, Param_GetChangedEvent(), ElevatorFloorIndicator_Changed);
    TaskType_SetHandler(gElevatorFloorIndicatorType, Game_GetEvent9(), TASK_NOTHING);
    TaskType_SetHandler(gElevatorFloorIndicatorType, RigidDynCubeObj_GetEvent4(), ElevatorFloorIndicator_Move);
    TaskType_SetHandler(gElevatorFloorIndicatorType, BulletHole_GetEvent(), NULL);
    gElevatorFloorIndicatorParams = ParamTable_New(7);
    ParamTable_Add(gElevatorFloorIndicatorParams, &f, &f.place, Param_ObjectPos(), 7, NULL);
    ParamTable_Add(gElevatorFloorIndicatorParams, &f, &f.placeMatrix, Param_Real32x9(), 7, NULL);
    ParamTable_Add(gElevatorFloorIndicatorParams, &f, f.litModel, Param_String16(), 7, NULL);
    ParamTable_Add(gElevatorFloorIndicatorParams, &f, f.model, Param_String16(), 7, NULL);
    ParamTable_Add(gElevatorFloorIndicatorParams, &f, f.sound, Param_String16(), 7, NULL);
    ParamTable_Add(gElevatorFloorIndicatorParams, &f, &f.lit, Param_VarString(), 7, NULL);
    ParamTable_Add(gElevatorFloorIndicatorParams, &f, &f.ring, Param_VarString(), 7, NULL);
}
HOOK(0x43C6D0, ElevatorFloorIndicator_Init);

void ElevatorFloorIndicator_Create(ElevatorFloorIndicator *f)
{
    Vec3d zero = {0, 0, 0};
    int i;

    RigidDynCubeObj_Init((int)gCurrentWorld, f, NULL);
    DynCube_Add((int)gCurrentWorld, f, &zero, obj_radius(&f->obj), 1);
    for (i = 0; i < 9; i++)
        f->obj.matrix.m[i] = i % 4 == 0 ? 1.0f : 0.0f;
    f->obj.matrix.count = 0;
    f->litModel[0] = 0;
    f->model[0] = 0;
    f->sound[0] = 0;
    f->unk1A0[0] = 0;
    VarString_Init(&f->lit);
    VarString_Init(&f->ring);
}
HOOK(0x43C8D0, ElevatorFloorIndicator_Create);

void ElevatorFloorIndicator_Run(ElevatorFloorIndicator *f)
{
    /* without an expression the original tests a stale stack slot that
       holds the task pointer, so both default to true */
    intptr_t lit = (intptr_t)f;
    intptr_t ring = (intptr_t)f;
    Task *s;

    CALL_CHILDREN(&f->obj.task, 1);
    if (VarString_HasExpr(&f->lit))
        lit = (int)round_ll(VarString_EvalReal(&f->lit, f));
    if (VarString_HasExpr(&f->ring))
        ring = (int)round_ll(VarString_EvalReal(&f->ring, f));
    RigidDynCubeObj_SetModel(f, VirModel_Find(lit ? f->litModel : f->model));
    if (ring) {
        s = Sound_Play(Level_GetDynamic(), f->sound);
        if (s)
            Sound_SetPos(s, &f->obj.pos);
    }
}
HOOK(0x43CA10, ElevatorFloorIndicator_Run);

void ElevatorFloorIndicator_Delete(ElevatorFloorIndicator *f)
{
    VarString_Free(&f->lit);
    VarString_Free(&f->ring);
    DynCube_Remove((int)f->obj.world, f);
}
HOOK(0x43CB30, ElevatorFloorIndicator_Delete);

void ElevatorFloorIndicator_GetParams(void *task, void **out)
{
    *out = gElevatorFloorIndicatorParams;
}
HOOK(0x43CB60, ElevatorFloorIndicator_GetParams);

void ElevatorFloorIndicator_Changed(ElevatorFloorIndicator *f)
{
    RigidDynCubeObj_SetModel(f, VirModel_Find(f->model));
    DynCube_Move((int)f->obj.world, f, &f->place, obj_radius(&f->obj));
    f->obj.matrix = f->placeMatrix;
    Switch_UpdateLocal(f);
    VarString_Bind(&f->lit, f);
    VarString_Bind(&f->ring, f);
}
HOOK(0x43CB70, ElevatorFloorIndicator_Changed);

void ElevatorFloorIndicator_Close(void)
{
    QTaskList_Free(gElevatorFloorIndicatorParams);
    TaskType_Free(gElevatorFloorIndicatorType);
}
HOOK(0x43CC50, ElevatorFloorIndicator_Close);

/* data in igidecomp.exe, at its IGI.exe address (tools/gendefs.py) */
BSS(57BF84) uint16_t gElevatorFloorIndicatorType;
PAD(igb, 57BF84, 57BF86, 0x2);
BSS(57BF84) void *gElevatorFloorIndicatorParams;
