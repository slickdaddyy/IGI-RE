#include "hook.h"
#include "litmap/litmap.h"
#include "flatwater/flatwatertypes.h"

ORIG(0x4B3360, char, Vec3d_Equal, (const Vec3d *a, const Vec3d *b));
ORIG(0x4CEC10, void **, VirModel_Find, (const char *name));
ORIG(0x4C48D0, void, RigidDynCubeObj_SetModel, (void *obj, void **model));
ORIG(0x51FBA0, void, MoveRigidObj_Apply, (void *obj, void *obj2, char force));
ORIG(0x401780, void, Task_DeleteChildren, (void *task));
ORIG(0x4C4950, void, RigidDynCubeObj_CreateMagicObjs, (void *a, void *b, void *c));

extern uint16_t gMoveRigidObjType;
extern void *gMoveRigidObjParams;

/* jumps to the last key that was edited */
void MoveRigidObj_Changed(MoveRigidObj *m, const char *arg)
{
    int i;

    for (i = 1; i >= 0; i--) {
        if (!Vec3d_Equal(&m->savedKey[i], &m->keyPos[i]) || fne(m->alpha[i], m->savedAlpha[i]) ||
            fne(m->beta[i], m->savedBeta[i]) || fne(m->gamma[i], m->savedGamma[i])) {
            m->savedKey[i] = m->keyPos[i];
            m->savedAlpha[i] = m->alpha[i];
            m->savedBeta[i] = m->beta[i];
            m->savedGamma[i] = m->gamma[i];
            m->value = i;
        }
    }
    RigidDynCubeObj_SetModel(m, VirModel_Find(m->model));
    MoveRigidObj_Apply(m, m, 1);
    Task_DeleteChildren(m->obj.attached);
    RigidDynCubeObj_CreateMagicObjs(m->obj.attached, m, m);
    if (!arg || !*arg)
        BROADCAST(m->obj.task.head, Param_GetChangedEvent(), 0);
    VarString_Bind(&m->expr, m);
}
HOOK(0x51FD60, MoveRigidObj_Changed);

void MoveRigidObj_Shutdown(void)
{
    QTaskList_Free(gMoveRigidObjParams);
    TaskType_Free(gMoveRigidObjType);
}
HOOK(0x51FED0, MoveRigidObj_Shutdown);
